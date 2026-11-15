// c1bench: part (c) of check C1, cost of carrying 8 B from the entry to the
// exit probe, per thread execution. Adapted from step0/e2-cost/src/bench2.cpp
// (the same launch gate k_hold, calibration to about 4 ms per batch, rounds
// with every item once in an order shuffled by mt19937_64(seed + round),
// CUDA events around N back-to-back launches, validation before timing).
//
// Module: build/c/c_module.cubin (tools/build_c.py); gate: build/gate.cubin.
// Items: kernel x shape, R = 1. Shapes as e2: anchor 512 x 512, large
// 8,192 x 512, full 24 x 512.
//   c_none     no probes (the delta reference: same module, same shape)
//   c_empty    empty probes              c_value    entry computes, no storage
//   c_local    carry, helper form          c_local_d  carry, direct form
//   c_slottag  slot + owner tag (device)   c_1512     1512 path (registered)
//   c_ref      e2's reference (threadhist exit-only lookup-and-increment)
// Validation (once per item and shape, before timing): the counters of the
// kernel's cnt map (NULL storage, mismatches) must be 0; the sabotaged twins
// (*_bad, not timed) must count a mismatch in every thread; c_1512 must have
// written every thread's slot; c_ref must have incremented every slot once;
// the exit-only carry kernels must find the prologue's mark (0: a mismatch
// in every thread; the _mark1 twin's 1: none); the no-mark control is
// reported.
// Output: OUT/raw.csv, OUT/items.json.
#include "step0b_regmem.h"

#include <algorithm>
#include <chrono>
#include <cinttypes>
#include <cmath>
#include <cstdarg>
#include <map>
#include <random>
#include <regex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

static constexpr uint64_t MAX_THREADS = 4194304; // large shape
static constexpr size_t REG_BYTES = 2 * MAX_THREADS * 8; // 1512 + ref regions
static constexpr uint64_t SLOTS = 12288;

struct MapBasicInfo {
	bool enabled;
	int key_size;
	int value_size;
	int max_entries;
	int map_type;
	void *extra_buffer;
	uint64_t max_thread_count;
};
static_assert(sizeof(MapBasicInfo) == 40, "MapBasicInfo layout");
enum { MT_ARRAY_DEV = 1503, MT_1512 = 1512, MT_CARRY = 1540, MT_SLOTTAG = 1542 };

static std::string fmt(const char *f, ...)
{
	char b[1024];
	va_list ap;
	va_start(ap, f);
	vsnprintf(b, sizeof b, f, ap);
	va_end(ap);
	return b;
}

struct Shape {
	std::string name;
	unsigned blocks, tpb;
	uint64_t threads() const { return (uint64_t)blocks * tpb; }
};
struct KDef {
	std::string name, kind, mem;
	int store = 0, cnt = 0;
	CUfunction fn = nullptr;
	CUdeviceptr cntbuf = 0;
};
struct Item {
	std::string id;
	KDef *k = nullptr;
	const Shape *s = nullptr;
	bool timed = true;
	unsigned n = 2;
	double cal_ms = 0;
	int regs = 0, local_bytes = 0, occ = 0;
	std::string valid = "not-run", valid_detail;
};

struct Ctx {
	step0_regmem reg;
	CUdeviceptr dbuf = 0, aux = 0, slotbuf = 0, map_info = 0;
	CUmodule mod = nullptr, gate = nullptr;
	CUstream st = nullptr;
	uint64_t *hold_host = nullptr;
	CUdeviceptr hold_dev = 0;
	CUfunction hold_fn = nullptr;
	uint64_t hold_cap_ns = 200000000ull;
	bool use_hold = true;
} c;

static void set_map(int fd, const MapBasicInfo &m)
{
	STEP0_CU(cuMemcpyHtoD(c.map_info + (size_t)fd * sizeof m, &m, sizeof m));
}

static void launch(Item &it)
{
	uint64_t buf = c.dbuf, mask = 0, aux = c.aux;
	uint32_t R = 1, k = 0, mode = 0;
	void *args[] = { &buf, &mask, &R, &k, &aux, &mode };
	STEP0_CU(cuLaunchKernel(it.k->fn, it.s->blocks, 1, 1, it.s->tpb, 1, 1, 0, c.st, args,
				nullptr));
}

// as bench2.cpp time_batch
static double time_batch(Item &it, unsigned n, CUevent e0, CUevent e1, int *capped = nullptr)
{
	uint64_t caps0 = 0;
	if (c.use_hold) {
		caps0 = __atomic_load_n(&c.hold_host[1], __ATOMIC_ACQUIRE);
		__atomic_store_n(&c.hold_host[0], 0, __ATOMIC_RELEASE);
		void *args[] = { &c.hold_dev, &c.hold_cap_ns };
		STEP0_CU(cuLaunchKernel(c.hold_fn, 1, 1, 1, 32, 1, 1, 0, c.st, args, nullptr));
	}
	STEP0_CU(cuEventRecord(e0, c.st));
	for (unsigned i = 0; i < n; i++)
		launch(it);
	STEP0_CU(cuEventRecord(e1, c.st));
	if (c.use_hold)
		__atomic_store_n(&c.hold_host[0], 1, __ATOMIC_RELEASE);
	STEP0_CU(cuEventSynchronize(e1));
	float ms = 0;
	STEP0_CU(cuEventElapsedTime(&ms, e0, e1));
	if (capped)
		*capped = c.use_hold &&
			  __atomic_load_n(&c.hold_host[1], __ATOMIC_ACQUIRE) != caps0;
	return ms;
}

static KDef *g_dirty = nullptr; // c_local_d: leaves nonzero timers in the carry

static void validate(Item &it)
{
	KDef &k = *it.k;
	const uint64_t T = it.s->threads();
	if (k.name.find("_noentry") != std::string::npos && g_dirty) {
		// first leave stale nonzero bytes where the carry lives (same frame
		// layout, 656 B, checked in build/c/ptxas.txt), so that an exit
		// without its entry could read them as valid if no mark were written
		Item d = it;
		d.k = g_dirty;
		launch(d);
		STEP0_CU(cuCtxSynchronize());
	}
	std::string d;
	bool ok = true;
	auto expect = [&](const char *what, uint64_t got, uint64_t want) {
		d += fmt("%s=%" PRIu64 "/%" PRIu64 " ", what, got, want);
		if (got != want)
			ok = false;
	};
	STEP0_CU(cuCtxSynchronize());
	if (k.cntbuf)
		STEP0_CU(cuMemsetD8(k.cntbuf, 0, 64));
	STEP0_CU(cuMemsetD8(c.slotbuf, 0, SLOTS * 16));
	memset(c.reg.host, 0, REG_BYTES);
	STEP0_CU(cuCtxSynchronize());
	launch(it);
	STEP0_CU(cuCtxSynchronize());
	uint64_t cnt[8] = { 0 };
	if (k.cntbuf)
		STEP0_CU(cuMemcpyDtoH(cnt, k.cntbuf, 64));
	// sabotaged twins (entry writes 0) and exit-only kernels (the exit finds
	// the prologue's valid mark, 0): every thread must count a mismatch.
	// The _mark1 twin (prologue writes 1, exit only) must count none: the
	// exit reads what the prologue wrote, so the mark check can fail.
	const bool mark1 = k.name.find("_mark1") != std::string::npos;
	const bool bad = !mark1 && (k.name.find("_bad") != std::string::npos ||
				    k.name.find("_noentry") != std::string::npos);
	const bool report_only = k.name.find("_noinit") != std::string::npos;
	if (k.kind == "none" || k.name == "c_empty" || k.name == "c_value") {
		d += "none ";
	} else if (k.name == "c_ref") {
		const uint64_t *p = (const uint64_t *)c.reg.host + MAX_THREADS;
		uint64_t s = 0;
		for (uint64_t i = 0; i < T; i++)
			s += p[i];
		expect("sum", s, T);
	} else if (report_only) {
		d += fmt("null=%" PRIu64 " mismatch=%" PRIu64 "/%" PRIu64 " (stale bytes read as valid: %" PRIu64 ") ",
			 cnt[1], cnt[2], T, T - cnt[2]);
	} else {
		expect("null", cnt[1], 0);
		expect("mismatch", cnt[2], bad ? T : 0);
		if (k.name == "c_1512" || k.name == "c_1512_bad") {
			const uint64_t *p = (const uint64_t *)c.reg.host;
			uint64_t nz = 0;
			for (uint64_t i = 0; i < T; i++)
				nz += p[i] != 0;
			expect("written", nz, bad ? 0 : T);
		}
	}
	it.valid = report_only ? "report" : ok ? "ok" : "FAIL";
	it.valid_detail = d;
}

int main(int argc, char **argv)
{
	std::string out = "out", D = ".", only = "", shapes_arg = "anchor,large,full";
	int rounds = 10;
	double target_ms = 4.0, max_wall_s = 1e9;
	unsigned nmax = 200;
	uint64_t seed = 20261115;
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
		else if (a == "--dir")
			D = val();
		else if (a == "--rounds")
			rounds = atoi(val().c_str());
		else if (a == "--target-ms")
			target_ms = atof(val().c_str());
		else if (a == "--shapes")
			shapes_arg = val();
		else if (a == "--only")
			only = val();
		else if (a == "--seed")
			seed = strtoull(val().c_str(), nullptr, 0);
		else if (a == "--max-wall-s")
			max_wall_s = atof(val().c_str());
		else if (a == "--no-hold")
			c.use_hold = false;
		else {
			fprintf(stderr, "unknown option %s\n", a.c_str());
			return 2;
		}
	}
	const auto t_start = std::chrono::steady_clock::now();
	auto wall = [&]() {
		return std::chrono::duration<double>(std::chrono::steady_clock::now() - t_start)
			.count();
	};
	std::vector<Shape> all = { { "full", 24, 512 }, { "anchor", 512, 512 },
				   { "large", 8192, 512 } };
	std::vector<Shape> shapes;
	{
		std::stringstream ss(shapes_arg);
		std::string t;
		while (std::getline(ss, t, ','))
			for (auto &s : all)
				if (s.name == t)
					shapes.push_back(s);
	}
	step0_primary_ctx(0);
	STEP0_CU(cuStreamCreate(&c.st, CU_STREAM_NON_BLOCKING));
	const std::string shm_name = fmt("step0b_c1_%d", (int)getpid());
	if (!step0_regmem_create(c.reg, shm_name, REG_BYTES, false)) {
		fprintf(stderr, "registered memory failed\n");
		return 3;
	}
	STEP0_CU(cuMemAlloc(&c.dbuf, 1 << 20));
	STEP0_CU(cuMemAlloc(&c.aux, 1 << 20));
	STEP0_CU(cuMemAlloc(&c.slotbuf, SLOTS * 16));
	STEP0_CU(cuModuleLoad(&c.mod, (D + "/build/c/c_module.cubin").c_str()));
	STEP0_CU(cuModuleLoad(&c.gate, (D + "/build/gate.cubin").c_str()));
	{
		void *h = nullptr;
		STEP0_CU(cuMemHostAlloc(&h, 4096, CU_MEMHOSTALLOC_DEVICEMAP | CU_MEMHOSTALLOC_PORTABLE));
		memset(h, 0, 4096);
		c.hold_host = (uint64_t *)h;
		STEP0_CU(cuMemHostGetDevicePointer(&c.hold_dev, h, 0));
		STEP0_CU(cuModuleGetFunction(&c.hold_fn, c.gate, "k_hold"));
	}
	size_t mib = 0;
	STEP0_CU(cuModuleGetGlobal(&c.map_info, &mib, c.mod, "map_info"));
	if (mib != 1024 * sizeof(MapBasicInfo)) {
		fprintf(stderr, "map_info is %zu bytes\n", mib);
		return 4;
	}
	STEP0_CU(cuMemsetD8(c.map_info, 0, mib));
	// kernels and their maps
	std::vector<KDef> ks;
	{
		FILE *mf = fopen((D + "/build/c/c_manifest.txt").c_str(), "r");
		if (!mf) {
			perror("c_manifest.txt");
			return 3;
		}
		char name[128], kind[32];
		int s, cn;
		while (fscanf(mf, "%127s %d %d %31s", name, &s, &cn, kind) == 4) {
			KDef k;
			k.name = name;
			k.kind = kind;
			k.store = s;
			k.cnt = cn;
			STEP0_CU(cuModuleGetFunction(&k.fn, c.mod, name));
			ks.push_back(k);
		}
		fclose(mf);
	}
	for (auto &k : ks) {
		if (k.kind == "none")
			continue;
		MapBasicInfo m = {};
		m.enabled = true;
		m.key_size = 4;
		if (k.cnt) {
			STEP0_CU(cuMemAlloc(&k.cntbuf, 64));
			MapBasicInfo n = {};
			n.enabled = true;
			n.key_size = 4;
			n.value_size = 8;
			n.max_entries = 8;
			n.map_type = MT_ARRAY_DEV;
			n.extra_buffer = (void *)(uintptr_t)k.cntbuf;
			set_map(k.cnt, n);
		}
		const std::string &nm = k.name;
		if (nm == "c_empty" || nm == "c_value") {
			k.mem = "none";
			m.map_type = MT_ARRAY_DEV; // never looked up
			m.value_size = 8;
			m.max_entries = 1;
			m.extra_buffer = (void *)(uintptr_t)c.slotbuf;
		} else if (nm.rfind("c_local", 0) == 0) {
			k.mem = "local";
			m.map_type = MT_CARRY;
			m.value_size = 8;
			m.max_entries = 1;
		} else if (nm.rfind("c_slottag", 0) == 0) {
			k.mem = "dev";
			m.map_type = MT_SLOTTAG;
			m.value_size = 16;
			m.max_entries = (int)SLOTS;
			m.extra_buffer = (void *)(uintptr_t)c.slotbuf;
		} else if (nm.rfind("c_1512", 0) == 0) {
			k.mem = "reg";
			m.map_type = MT_1512;
			m.value_size = 8;
			m.max_entries = 1;
			m.max_thread_count = MAX_THREADS;
			m.extra_buffer = (void *)(uintptr_t)c.reg.dev;
		} else if (nm == "c_ref") {
			k.mem = "reg";
			m.map_type = MT_1512;
			m.value_size = 8;
			m.max_entries = 1;
			m.max_thread_count = MAX_THREADS;
			m.extra_buffer = (void *)(uintptr_t)(c.reg.dev + MAX_THREADS * 8);
		}
		set_map(k.store, m);
	}
	for (auto &k : ks) {
		if (k.kind == "none")
			k.mem = "none";
		if (k.name == "c_local_d")
			g_dirty = &k;
	}
	std::vector<Item> items;
	for (auto &s : shapes)
		for (auto &k : ks) {
			Item it;
			it.k = &k;
			it.s = &s;
			it.id = fmt("%s|%s|%s", s.name.c_str(), k.name.c_str(), k.mem.c_str());
			it.timed = k.name.find("_bad") == std::string::npos &&
				   k.name.find("_noentry") == std::string::npos;
			if (!only.empty() && !std::regex_search(it.id, std::regex(only)))
				continue;
			items.push_back(it);
		}
	CUevent e0, e1;
	STEP0_CU(cuEventCreate(&e0, CU_EVENT_DEFAULT));
	STEP0_CU(cuEventCreate(&e1, CU_EVENT_DEFAULT));
	int nfail = 0;
	for (auto &it : items) {
		cuFuncGetAttribute(&it.regs, CU_FUNC_ATTRIBUTE_NUM_REGS, it.k->fn);
		cuFuncGetAttribute(&it.local_bytes, CU_FUNC_ATTRIBUTE_LOCAL_SIZE_BYTES, it.k->fn);
		STEP0_CU(cuOccupancyMaxActiveBlocksPerMultiprocessor(&it.occ, it.k->fn, it.s->tpb, 0));
		validate(it);
		if (it.valid == "FAIL") {
			nfail++;
			fprintf(stderr, "VALIDATION FAIL %s: %s\n", it.id.c_str(), it.valid_detail.c_str());
		}
		if (!it.timed)
			continue;
		launch(it); // warm
		STEP0_CU(cuStreamSynchronize(c.st));
		const double t1 = time_batch(it, 1, e0, e1);
		const double t3 = time_batch(it, 3, e0, e1) / 3;
		it.cal_ms = std::min(t1, t3);
		it.n = (unsigned)std::clamp<double>(std::ceil(target_ms / std::max(it.cal_ms, 1e-4)),
						    it.cal_ms > 50 ? 1 : 2, nmax);
	}
	fprintf(stderr, "items %zu, validation done (%d failures), calibration done, %.1f s\n",
		items.size(), nfail, wall());
	FILE *csv = fopen((out + "/raw.csv").c_str(), "w");
	if (!csv) {
		perror("raw.csv");
		return 5;
	}
	fprintf(csv, "round,pos,id,kernel,mem,shape,blocks,tpb,threads,n_launch,batch_ms,"
		     "per_launch_us,hold_capped\n");
	int done_rounds = 0;
	uint64_t hold_capped_rows = 0;
	std::vector<size_t> timed;
	for (size_t i = 0; i < items.size(); i++)
		if (items[i].timed)
			timed.push_back(i);
	for (int r = 0; r < rounds; r++) {
		if (wall() > max_wall_s) {
			fprintf(stderr, "max wall time reached before round %d\n", r);
			break;
		}
		std::vector<size_t> order = timed;
		std::mt19937_64 rng(seed + (uint64_t)r);
		std::shuffle(order.begin(), order.end(), rng);
		for (size_t pos = 0; pos < order.size(); pos++) {
			Item &it = items[order[pos]];
			if (it.cal_ms < 20)
				launch(it); // untimed
			int capped = 0;
			const double ms = time_batch(it, it.n, e0, e1, &capped);
			hold_capped_rows += capped;
			fprintf(csv, "%d,%zu,%s,%s,%s,%s,%u,%u,%" PRIu64 ",%u,%.6f,%.4f,%d\n", r, pos,
				it.id.c_str(), it.k->name.c_str(), it.k->mem.c_str(), it.s->name.c_str(),
				it.s->blocks, it.s->tpb, it.s->threads(), it.n, ms, ms * 1000.0 / it.n,
				capped);
		}
		fflush(csv);
		done_rounds++;
		fprintf(stderr, "round %d done, %.1f s\n", r, wall());
	}
	fclose(csv);
	FILE *j = fopen((out + "/items.json").c_str(), "w");
	fprintf(j, "{\n \"pid\": %d,\n \"rounds_done\": %d,\n \"rounds_requested\": %d,\n \"target_ms\": %g,\n"
		   " \"seed\": %" PRIu64 ",\n \"wall_s\": %.2f,\n \"validation_failures\": %d,\n"
		   " \"hold_gate\": %s,\n \"hold_capped_rows\": %" PRIu64 ",\n \"items\": [\n",
		(int)getpid(), done_rounds, rounds, target_ms, seed, wall(), nfail,
		c.use_hold ? "true" : "false", hold_capped_rows);
	for (size_t i = 0; i < items.size(); i++) {
		auto &it = items[i];
		fprintf(j,
			"  {\"id\": \"%s\", \"kernel\": \"%s\", \"mem\": \"%s\", \"shape\": \"%s\", "
			"\"threads\": %" PRIu64 ", \"timed\": %s, \"n\": %u, \"cal_ms\": %.6f, "
			"\"regs\": %d, \"local_bytes\": %d, \"occ_blocks_per_sm\": %d, "
			"\"valid\": \"%s\", \"valid_detail\": \"%s\"}%s\n",
			it.id.c_str(), it.k->name.c_str(), it.k->mem.c_str(), it.s->name.c_str(),
			it.s->threads(), it.timed ? "true" : "false", it.n, it.cal_ms, it.regs,
			it.local_bytes, it.occ, it.valid.c_str(), it.valid_detail.c_str(),
			i + 1 < items.size() ? "," : "");
	}
	fprintf(j, " ]\n}\n");
	fclose(j);
	STEP0_CU(cuStreamSynchronize(c.st));
	step0_regmem_destroy(c.reg);
	fprintf(stderr, "done: %d rounds, %.1f s, out %s\n", done_rounds, wall(), out.c_str());
	return nfail ? 6 : 0;
}
