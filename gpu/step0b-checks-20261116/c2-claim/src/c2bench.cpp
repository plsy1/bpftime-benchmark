// c2bench: cost of the self-claim (check C2, step 0b), with step0/e2-cost's bench2 method.
//
// Driver API only. Two modules, both launched with cuLaunchKernel:
//   kernels.cubin   c_* cost kernels: one entry helper call and one exit helper call per thread
//   refprobe.cubin  today's empty bpftime probe pair (kprobe_entry + kretprobe passes, empty eBPF)
// Every kernel has the signature (u64 *aux, u32 mode); mode bit 0 = validation counting.
//
// Kept from bench2 (step0/e2-cost/src/bench2.cpp): the k_hold launch gate, the shapes
// (anchor 512 x 512, large 8,192 x 512, full 24 x 512), one validation launch per item, N
// calibrated to about 4 ms per batch, ROUNDS rounds each running every item once in an order
// shuffled by mt19937_64(seed + round): 1 untimed launch, then N launches between two events.
// Added: before every batch the item's pool is re-initialised (outside the timed region); the
// pool counters are read before and after the batch (misses, cap hits, tag mismatches per batch)
// and the pool's end state is checked after it (every slot free again).
// Output: OUT/raw.csv (one row per round and item), OUT/items.json.
#include "c2host.h"

#include <algorithm>
#include <chrono>
#include <map>
#include <random>
#include <regex>
#include <sstream>
#include <unistd.h>

struct Shape {
	std::string name;
	unsigned blocks, tpb;
	uint64_t threads() const { return (uint64_t)blocks * tpb; }
	uint64_t warps() const { return threads() / 32; }
};

struct Item {
	std::string id, kname, module; // module: k (kernels) or r (refprobe)
	int kind = -1;                 // pool kind, -1 = no pool
	double factor = 1;
	const Shape *shape = nullptr;
	CUfunction fn = nullptr;
	unsigned n = 2;
	double cal_ms = 0;
	int occ_blocks = 0, regs = 0, local_bytes = 0;
	std::string valid = "not-run", valid_detail;
	std::vector<uint64_t> vctr;
};

struct Ctx {
	CUmodule mk = nullptr, mr = nullptr;
	CUstream st = nullptr;
	CUdeviceptr aux = 0;
	C2Pool pool;
	bool use_hold = true;
	uint64_t *hold_host = nullptr;
	CUdeviceptr hold_dev = 0;
	CUfunction hold_fn = nullptr;
	uint64_t hold_cap_ns = 200000000ull;
};

static void launch(Ctx &c, Item &it, uint32_t mode)
{
	uint64_t aux = c.aux;
	void *args[] = { &aux, &mode };
	C2_CU(cuLaunchKernel(it.fn, it.shape->blocks, 1, 1, it.shape->tpb, 1, 1, 0, c.st, args, nullptr));
}

static void prepare(Ctx &c, Item &it)
{
	c.pool.setup(it.kind >= 0 ? it.kind : C2_BMSM, it.factor);
}

static double time_batch(Ctx &c, Item &it, unsigned n, CUevent e0, CUevent e1, int *capped = nullptr)
{
	uint64_t caps0 = 0;
	if (c.use_hold) {
		caps0 = __atomic_load_n(&c.hold_host[1], __ATOMIC_ACQUIRE);
		__atomic_store_n(&c.hold_host[0], 0, __ATOMIC_RELEASE);
		void *args[] = { &c.hold_dev, &c.hold_cap_ns };
		C2_CU(cuLaunchKernel(c.hold_fn, 1, 1, 1, 32, 1, 1, 0, c.st, args, nullptr));
	}
	C2_CU(cuEventRecord(e0, c.st));
	for (unsigned i = 0; i < n; i++)
		launch(c, it, 0);
	C2_CU(cuEventRecord(e1, c.st));
	if (c.use_hold)
		__atomic_store_n(&c.hold_host[0], 1, __ATOMIC_RELEASE);
	C2_CU(cuEventSynchronize(e1));
	float ms = 0;
	C2_CU(cuEventElapsedTime(&ms, e0, e1));
	if (capped)
		*capped = c.use_hold && __atomic_load_n(&c.hold_host[1], __ATOMIC_ACQUIRE) != caps0;
	return ms;
}

// One launch in validation mode on a fresh pool; every lane must be accounted for.
static void validate(Ctx &c, Item &it)
{
	prepare(c, it);
	launch(c, it, 1);
	C2_CU(cuStreamSynchronize(c.st));
	const auto v = c.pool.counters();
	it.vctr = v;
	const uint64_t T = it.shape->threads();
	bool ok = true;
	std::string d;
	auto expect = [&](const char *what, uint64_t got, uint64_t want) {
		d += c2fmt("%s=%" PRIu64 "/%" PRIu64 " ", what, got, want);
		if (got != want)
			ok = false;
	};
	if (it.kind >= 0) {
		const auto e = c.pool.end_state(true, false);
		expect("lanes", v[CT_CLAIM_LANES] + v[CT_MISS_LANES], T);
		expect("exit_lanes", v[CT_EXIT_LANES], v[CT_CLAIM_LANES]);
		expect("releases", v[CT_RELEASES], v[CT_CLAIMS]);
		expect("tag_bad", v[CT_TAG_BAD], 0);
		expect("cap_push", v[CT_CAP_PUSH], 0);
		expect("cap_spin", v[CT_CAP_SPIN], 0);
		expect("end_clean", e.clean ? 1 : 0, 1);
		d += c2fmt("misses=%" PRIu64 " cap_claim=%" PRIu64 " ", v[CT_MISSES], v[CT_CAP_CLAIM]);
	} else if (it.kname == "c_wid") {
		d += c2fmt("tag_bad=%" PRIu64 " (a %%warpid collision would show here) ", v[CT_TAG_BAD]);
	} else {
		d += "none ";
	}
	it.valid = ok ? "ok" : "FAIL";
	it.valid_detail = d;
}

static std::string json_str(const std::string &s)
{
	std::string o = "\"";
	for (char ch : s) {
		if (ch == '"' || ch == '\\')
			o += '\\';
		o += ch;
	}
	return o + "\"";
}

int main(int argc, char **argv)
{
	std::string out = "out", kcubin = "build/kernels.cubin", rcubin = "build/ref/refprobe.cubin", only = "",
		    shapes_arg = "anchor,large,full";
	int rounds = 10;
	double target_ms = 4.0, max_wall_s = 1e9;
	unsigned nmax = 200;
	uint64_t seed = 20261115;
	bool use_hold = true;
	for (int i = 1; i < argc; i++) {
		std::string a = argv[i];
		auto val = [&]() -> std::string {
			if (i + 1 >= argc) {
				fprintf(stderr, "missing value for %s\n", a.c_str());
				exit(2);
			}
			return argv[++i];
		};
		if (a == "--out")
			out = val();
		else if (a == "--rounds")
			rounds = atoi(val().c_str());
		else if (a == "--target-ms")
			target_ms = atof(val().c_str());
		else if (a == "--nmax")
			nmax = (unsigned)atoi(val().c_str());
		else if (a == "--shapes")
			shapes_arg = val();
		else if (a == "--only")
			only = val();
		else if (a == "--seed")
			seed = strtoull(val().c_str(), nullptr, 0);
		else if (a == "--kernels")
			kcubin = val();
		else if (a == "--ref")
			rcubin = val();
		else if (a == "--max-wall-s")
			max_wall_s = atof(val().c_str());
		else if (a == "--no-hold")
			use_hold = false;
		else {
			fprintf(stderr, "unknown option %s\n", a.c_str());
			return 2;
		}
	}
	auto split = [](const std::string &s) {
		std::vector<std::string> v;
		std::stringstream ss(s);
		std::string t;
		while (std::getline(ss, t, ','))
			if (!t.empty())
				v.push_back(t);
		return v;
	};
	const auto t_start = std::chrono::steady_clock::now();
	auto wall = [&]() {
		return std::chrono::duration<double>(std::chrono::steady_clock::now() - t_start).count();
	};
	// step0/e2-cost's shapes
	const std::vector<Shape> all_shapes = {
		{ "full", 24, 512 },
		{ "anchor", 512, 512 },
		{ "large", 8192, 512 },
	};
	std::vector<Shape> shapes;
	for (auto &n : split(shapes_arg))
		for (auto &s : all_shapes)
			if (s.name == n)
				shapes.push_back(s);

	{ // the pid first, so run_all.sh can remove CUDA's /dev/shm leftovers even if this is killed
		FILE *m = fopen((out + "/meta_bench.json").c_str(), "w");
		if (m) {
			fprintf(m, "{\"pid\": %d}\n", (int)getpid());
			fclose(m);
		}
	}
	Ctx c;
	C2_CU(cuInit(0));
	CUdevice dev;
	C2_CU(cuDeviceGet(&dev, 0));
	CUcontext ctx;
	C2_CU(cuDevicePrimaryCtxRetain(&ctx, dev));
	C2_CU(cuCtxSetCurrent(ctx));
	C2_CU(cuStreamCreate(&c.st, CU_STREAM_NON_BLOCKING));
	C2_CU(cuModuleLoad(&c.mk, kcubin.c_str()));
	C2_CU(cuModuleLoad(&c.mr, rcubin.c_str()));
	C2_CU(cuMemAlloc(&c.aux, 4096));
	c.pool.alloc(c.mk);
	c.use_hold = use_hold;
	{ // timing gate: one pinned, device-mapped page (cuMemHostAlloc, no POSIX shm)
		void *h = nullptr;
		C2_CU(cuMemHostAlloc(&h, 4096, CU_MEMHOSTALLOC_DEVICEMAP | CU_MEMHOSTALLOC_PORTABLE));
		memset(h, 0, 4096);
		c.hold_host = (uint64_t *)h;
		C2_CU(cuMemHostGetDevicePointer(&c.hold_dev, h, 0));
		C2_CU(cuModuleGetFunction(&c.hold_fn, c.mk, "k_hold"));
	}
	// the items
	struct Def {
		const char *k;
		const char *mod;
		int kind;
		double factor;
	};
	std::vector<Def> defs = {
		{ "c_empty", "k", -1, 1 },	{ "c_probe", "k", -1, 1 }, { "c_wid", "k", -1, 1 },
		{ "rp_empty", "r", -1, 1 },	{ "rp_pair", "r", -1, 1 }, { "rp_entry", "r", -1, 1 },
		{ "rp_exit", "r", -1, 1 },
	};
	for (const char *p : { "bmsm", "bmglob", "stksm", "stkglob" })
		for (const char *x : { "o_lane", "r_lane", "oc_lane", "o_agg", "r_agg" })
			defs.push_back({ strdup(c2fmt("c_%s_%s", p, x).c_str()), "k", c2_pool_kind(p), 1 });
	for (const char *p : { "lnsm", "lnglob", "lfsm", "lfglob" })
		for (const char *x : { "o_lane", "r_lane" })
			defs.push_back({ strdup(c2fmt("c_%s_%s", p, x).c_str()), "k", c2_pool_kind(p), 1 });
	// the pool at twice the resident count (misses from releases still in flight, if any)
	for (const char *k : { "c_bmsm_o_lane", "c_bmsm_oc_lane", "c_stkglob_o_lane", "c_lnsm_o_lane", "c_lfsm_o_lane" }) {
		const std::string p = std::string(k).substr(2, std::string(k).find('_', 2) - 2);
		defs.push_back({ k, "k", c2_pool_kind(p), 2 });
	}
	std::vector<Item> items;
	items.reserve(defs.size() * shapes.size());
	for (auto &s : shapes)
		for (auto &d : defs) {
			Item it;
			it.kname = d.k;
			it.module = d.mod;
			it.kind = d.kind;
			it.factor = d.factor;
			it.shape = &s;
			it.id = c2fmt("%s|%s|f%g", s.name.c_str(), d.k, d.factor);
			if (!only.empty() && !std::regex_search(it.id, std::regex(only)))
				continue;
			C2_CU(cuModuleGetFunction(&it.fn, d.mod[0] == 'k' ? c.mk : c.mr, d.k));
			items.push_back(it);
		}
	fprintf(stderr, "items: %zu, rounds: %d\n", items.size(), rounds);

	CUevent e0, e1;
	C2_CU(cuEventCreate(&e0, CU_EVENT_DEFAULT));
	C2_CU(cuEventCreate(&e1, CU_EVENT_DEFAULT));
	int nfail = 0;
	for (auto &it : items) {
		cuFuncGetAttribute(&it.regs, CU_FUNC_ATTRIBUTE_NUM_REGS, it.fn);
		cuFuncGetAttribute(&it.local_bytes, CU_FUNC_ATTRIBUTE_LOCAL_SIZE_BYTES, it.fn);
		C2_CU(cuOccupancyMaxActiveBlocksPerMultiprocessor(&it.occ_blocks, it.fn, it.shape->tpb, 0));
		validate(c, it);
		if (it.valid == "FAIL") {
			nfail++;
			fprintf(stderr, "VALIDATION FAIL %s: %s\n", it.id.c_str(), it.valid_detail.c_str());
		}
		prepare(c, it);
		launch(c, it, 0); // warm
		C2_CU(cuStreamSynchronize(c.st));
		const double t1 = time_batch(c, it, 1, e0, e1);
		const double t3 = time_batch(c, it, 3, e0, e1) / 3;
		it.cal_ms = std::min(t1, t3);
		it.n = (unsigned)std::clamp<double>(std::ceil(target_ms / std::max(it.cal_ms, 1e-4)),
						    it.cal_ms > 50 ? 1 : 2, nmax);
	}
	fprintf(stderr, "validation done (%d failures), calibration done, %.1f s\n", nfail, wall());

	FILE *csv = fopen((out + "/raw.csv").c_str(), "w");
	if (!csv) {
		perror("raw.csv");
		return 5;
	}
	fprintf(csv, "round,pos,id,kernel,module,kind,factor,shape,blocks,tpb,threads,n_launch,batch_ms,"
		     "per_launch_us,misses,miss_lanes,cap_claim,cap_spin,tag_bad,spin_extra,retry,end_clean,"
		     "hold_capped\n");
	int done_rounds = 0;
	uint64_t hold_capped_rows = 0, unclean_rows = 0;
	for (int r = 0; r < rounds; r++) {
		if (wall() > max_wall_s) {
			fprintf(stderr, "max wall time reached before round %d\n", r);
			break;
		}
		std::vector<size_t> order(items.size());
		for (size_t i = 0; i < order.size(); i++)
			order[i] = i;
		std::mt19937_64 rng(seed + (uint64_t)r);
		std::shuffle(order.begin(), order.end(), rng);
		for (size_t pos = 0; pos < order.size(); pos++) {
			Item &it = items[order[pos]];
			prepare(c, it);
			if (it.cal_ms < 20)
				launch(c, it, 0); // untimed
			C2_CU(cuStreamSynchronize(c.st));
			const auto c0 = c.pool.counters();
			int capped = 0;
			const double ms = time_batch(c, it, it.n, e0, e1, &capped);
			const auto c1 = c.pool.counters();
			bool clean = true;
			if (it.kind >= 0)
				clean = c.pool.end_state(true, false).clean;
			unclean_rows += !clean;
			hold_capped_rows += capped;
			auto d = [&](int i) { return c1[i] - c0[i]; };
			fprintf(csv,
				"%d,%zu,%s,%s,%s,%s,%g,%s,%u,%u,%" PRIu64 ",%u,%.6f,%.4f,%" PRIu64 ",%" PRIu64
				",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%d,%d\n",
				r, pos, it.id.c_str(), it.kname.c_str(), it.module.c_str(),
				it.kind >= 0 ? c2_pool_name(it.kind) : "none", it.factor, it.shape->name.c_str(),
				it.shape->blocks, it.shape->tpb, it.shape->threads(), it.n, ms, ms * 1000.0 / it.n,
				d(CT_MISSES), d(CT_MISS_LANES), d(CT_CAP_CLAIM), d(CT_CAP_SPIN), d(CT_TAG_BAD),
				d(CT_SPIN_EXTRA), d(CT_RETRY), clean ? 1 : 0, capped);
		}
		fflush(csv);
		done_rounds++;
		fprintf(stderr, "round %d done, %.1f s\n", r, wall());
	}
	fclose(csv);

	FILE *j = fopen((out + "/items.json").c_str(), "w");
	fprintf(j,
		"{\n \"rounds_done\": %d,\n \"rounds_requested\": %d,\n \"target_ms\": %g,\n \"seed\": %" PRIu64
		",\n \"sms\": %d,\n \"resident_warps\": %d,\n \"wall_s\": %.2f,\n \"validation_failures\": %d,\n"
		" \"hold_gate\": %s,\n \"hold_capped_rows\": %" PRIu64 ",\n \"unclean_rows\": %" PRIu64
		",\n \"items\": [\n",
		done_rounds, rounds, target_ms, seed, c.pool.dev.sms, c.pool.dev.resident_warps, wall(), nfail,
		c.use_hold ? "true" : "false", hold_capped_rows, unclean_rows);
	for (size_t i = 0; i < items.size(); i++) {
		auto &it = items[i];
		fprintf(j,
			"  {\"id\": %s, \"kernel\": %s, \"module\": %s, \"pool\": %s, \"factor\": %g, \"shape\": %s, "
			"\"blocks\": %u, \"tpb\": %u, \"threads\": %" PRIu64 ", \"warps\": %" PRIu64
			", \"n\": %u, \"cal_ms\": %.6f, \"regs\": %d, \"local_bytes\": %d, \"occ_blocks_per_sm\": %d, "
			"\"valid\": %s, \"valid_detail\": %s, \"vctr\": %s}%s\n",
			json_str(it.id).c_str(), json_str(it.kname).c_str(), json_str(it.module).c_str(),
			json_str(it.kind >= 0 ? c2_pool_name(it.kind) : "none").c_str(), it.factor,
			json_str(it.shape->name).c_str(), it.shape->blocks, it.shape->tpb, it.shape->threads(),
			it.shape->warps(), it.n, it.cal_ms, it.regs, it.local_bytes, it.occ_blocks,
			json_str(it.valid).c_str(), json_str(it.valid_detail).c_str(),
			c2_json_counters(it.vctr.empty() ? std::vector<uint64_t>(CT_N, 0) : it.vctr).c_str(),
			i + 1 < items.size() ? "," : "");
	}
	fprintf(j, " ]\n}\n");
	fclose(j);
	C2_CU(cuStreamSynchronize(c.st));
	cuModuleUnload(c.mk);
	cuModuleUnload(c.mr);
	c.pool.free_all();
	cuMemFree(c.aux);
	cuMemFreeHost(c.hold_host);
	fprintf(stderr, "done: %d rounds, %.1f s, out %s\n", done_rounds, wall(), out.c_str());
	return nfail ? 6 : 0;
}
