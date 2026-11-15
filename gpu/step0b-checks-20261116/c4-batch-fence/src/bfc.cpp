// bfc: step 0b, check C4 (batched publication cost) on the Orin.
//
// Driver API only, derived from step 0's bench2 (e2-cost/src/bench2.cpp):
// the same launch path (cuLaunchKernel), the same timing gate (k_hold), the
// same calibration (about 4 ms per batch), interleaved rounds in an order
// shuffled by mt19937_64(seed + round), and the delta against the empty
// kernel of the same module and shape (analyze.py).
//
// Memory: 128 MiB registered the bpftime way (common/step0b_regmem.h) for the
// records, the flags and the registered-counter items; 8 MiB of cuMemAlloc
// memory for the counters (the CPU never reads them); 4 KiB of device memory
// for validation counts.
//
// For every item (shape x layout x variant x G x counter memory): two
// validation launches on zeroed memory (epochs 1 and 2: every record word,
// every counter, every flag and the completer count are checked; items.json),
// then a calibrated batch size N; then ROUNDS rounds, each running every item
// once in a deterministic shuffled order. A batch: counters zeroed, one
// untimed launch (epoch 1), the gate, N launches (epochs 2..N+1) between two
// events, then a post-check of the first and the last chunk's counter and
// flag (outside the timed window; column post_ok).
// Output: OUT/raw.csv (one row per round and item), OUT/items.json.
#include "../common/step0b_regmem.h"

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

#define CU STEP0B_CU

static constexpr size_t REG_BYTES = 128ull << 20;
static constexpr size_t REC_OFF = 0; // 32 MiB: 131,072 warps x 256 B
static constexpr size_t FLAG_OFF = 32ull << 20; // 16 MiB: 131,072 x 128 B
static constexpr size_t CREG_OFF = 48ull << 20; // 4 MiB: 131,072 x 32 B
static constexpr size_t DCNT_BYTES = 8ull << 20;
static constexpr size_t AUX_BYTES = 4096;
static constexpr uint64_t FLAG_STRIDE = 16, CNT_STRIDE = 4; // words (kernels.cu)
static constexpr uint64_t MAX_WARPS = 131072;

struct Params { // kernels.cu
	uint64_t rec, cnt, flags, aux;
	uint64_t magic;
	uint32_t G, epoch, mode, pad;
};
static_assert(sizeof(Params) == 56, "Params layout");

struct Shape {
	std::string name;
	unsigned blocks, tpb;
	uint64_t threads() const { return (uint64_t)blocks * tpb; }
	uint64_t warps() const { return threads() / 32; }
};

// kinds of items
enum Kind { K_EMPTY, K_FONLY, K_REC, K_RF, K_PW, K_BATCH };

struct Item {
	std::string id, layout, variant, cntmem; // cntmem: dev, reg, -
	Kind kind = K_EMPTY;
	uint32_t G = 0;
	const Shape *shape = nullptr;
	CUfunction fn = nullptr;
	bool ar32 = false;
	// filled in
	unsigned n = 2;
	double cal_ms = 0;
	int occ_blocks = 0, regs = 0, local_bytes = 0;
	std::string valid = "not-run", valid_detail;
	uint64_t nchunks() const
	{
		return G ? (shape->warps() + G - 1) / G : 0;
	}
	uint32_t rec_words() const { return layout == "w32" ? 4 : 32; }
};

static std::string fmt(const char *f, ...)
{
	char b[2048];
	va_list ap;
	va_start(ap, f);
	vsnprintf(b, sizeof b, f, ap);
	va_end(ap);
	return b;
}

struct Ctx {
	step0b_regmem reg;
	CUdeviceptr dcnt = 0, aux = 0;
	CUmodule mk = nullptr;
	CUstream st = nullptr;
	uint64_t *hold_host = nullptr;
	CUdeviceptr hold_dev = 0;
	CUfunction hold_fn = nullptr;
	uint64_t hold_cap_ns = 200000000ull; // 200 ms
	uint64_t magic[1024] = {};
};

static uint64_t magic_for(uint32_t G)
{
	return ((1ull << 40) + G - 1) / G; // ceil(2^40 / G)
}

static CUdeviceptr cnt_of(Ctx &c, const Item &it)
{
	return it.cntmem == "reg" ? c.reg.dev + CREG_OFF : c.dcnt;
}

static void launch(Ctx &c, Item &it, uint32_t epoch, uint32_t mode)
{
	Params p{};
	p.rec = c.reg.dev + REC_OFF;
	p.cnt = cnt_of(c, it);
	p.flags = c.reg.dev + FLAG_OFF;
	p.aux = c.aux;
	p.G = it.G ? it.G : 1;
	p.magic = magic_for(p.G);
	p.epoch = epoch;
	p.mode = mode;
	void *args[] = { &p };
	CU(cuLaunchKernel(it.fn, it.shape->blocks, 1, 1, it.shape->tpb, 1, 1, 0,
			  c.st, args, nullptr));
}

static void reset_counters(Ctx &c, Item &it)
{
	if (it.kind != K_BATCH)
		return;
	CU(cuMemsetD32Async(cnt_of(c, it), 0,
			    (unsigned)(it.nchunks() * CNT_STRIDE * 2), c.st));
}

static uint64_t read_cnt(Ctx &c, Item &it, uint64_t chunk)
{
	uint64_t v = 0;
	if (it.cntmem == "reg")
		v = __atomic_load_n((uint64_t *)((uint8_t *)c.reg.host + CREG_OFF) +
					    chunk * CNT_STRIDE,
				    __ATOMIC_ACQUIRE);
	else
		CU(cuMemcpyDtoH(&v, c.dcnt + chunk * CNT_STRIDE * 8, 8));
	return v;
}
static uint64_t read_flag(Ctx &c, uint64_t idx)
{
	return __atomic_load_n((uint64_t *)((uint8_t *)c.reg.host + FLAG_OFF) +
				       idx * FLAG_STRIDE,
			       __ATOMIC_ACQUIRE);
}
static uint64_t rec_value(uint32_t epoch, uint32_t w, uint32_t lane, uint32_t j)
{
	return ((uint64_t)epoch << 40) | ((uint64_t)w << 8) | (lane << 2) | j;
}

// Two launches (epochs 1, 2) in validation mode on zeroed memory, then every
// record word, counter, flag and the completer/increment counts.
static void validate(Ctx &c, Item &it)
{
	const uint64_t W = it.shape->warps(), NC = it.nchunks();
	CU(cuMemsetD32Async(c.reg.dev + REC_OFF, 0,
			    (unsigned)(W * 32 * 2), c.st));
	CU(cuMemsetD32Async(c.reg.dev + FLAG_OFF, 0,
			    (unsigned)(W * FLAG_STRIDE * 2), c.st));
	CU(cuMemsetD32Async(c.dcnt, 0, (unsigned)(DCNT_BYTES / 4), c.st));
	CU(cuMemsetD32Async(c.reg.dev + CREG_OFF, 0,
			    (unsigned)(MAX_WARPS * CNT_STRIDE * 2), c.st));
	CU(cuMemsetD32Async(c.aux, 0, AUX_BYTES / 4, c.st));
	CU(cuStreamSynchronize(c.st));
	launch(c, it, 1, 1);
	launch(c, it, 2, 1);
	CU(cuStreamSynchronize(c.st));
	uint64_t aux[2];
	CU(cuMemcpyDtoH(aux, c.aux, sizeof aux));
	bool ok = true;
	std::string d;
	auto expect = [&](const char *what, uint64_t got, uint64_t want) {
		d += fmt("%s=%" PRIu64 "/%" PRIu64 " ", what, got, want);
		if (got != want)
			ok = false;
	};
	if (it.kind == K_EMPTY || it.kind == K_FONLY) {
		expect("aux_completers", aux[0], 0);
		it.valid = ok ? "ok" : "FAIL";
		it.valid_detail = d;
		return;
	}
	// records: every word of every warp's record, last epoch (2)
	{
		const uint64_t *r =
			(const uint64_t *)((uint8_t *)c.reg.host + REC_OFF);
		uint64_t bad = 0;
		for (uint64_t w = 0; w < W; w++) {
			if (it.layout == "w32") {
				for (uint32_t j = 0; j < 4; j++)
					bad += r[w * 4 + j] !=
					       rec_value(2, (uint32_t)w, 0, j);
			} else {
				for (uint32_t l = 0; l < 32; l++)
					bad += r[w * 32 + l] !=
					       rec_value(2, (uint32_t)w, l, 0);
			}
		}
		expect("record_words_bad", bad, 0);
	}
	if (it.kind == K_REC || it.kind == K_RF) {
		uint64_t nz = 0;
		for (uint64_t w = 0; w < W; w++)
			nz += read_flag(c, w) != 0;
		expect("flags_nonzero", nz, 0);
		expect("aux_completers", aux[0], 0);
	} else if (it.kind == K_PW) {
		uint64_t bad = 0;
		for (uint64_t w = 0; w < W; w++)
			bad += read_flag(c, w) != ((2ull << 32) | w);
		expect("flags_bad", bad, 0);
		expect("completers", aux[0], 2 * W);
		expect("increments", aux[1], 2 * W);
	} else { // K_BATCH
		std::vector<uint64_t> cnt(NC);
		if (it.cntmem == "reg") {
			const uint64_t *p = (const uint64_t *)((uint8_t *)c.reg.host +
							       CREG_OFF);
			for (uint64_t k = 0; k < NC; k++)
				cnt[k] = __atomic_load_n(p + k * CNT_STRIDE,
							 __ATOMIC_ACQUIRE);
		} else {
			std::vector<uint64_t> all(NC * CNT_STRIDE);
			CU(cuMemcpyDtoH(all.data(), c.dcnt, all.size() * 8));
			for (uint64_t k = 0; k < NC; k++)
				cnt[k] = all[k * CNT_STRIDE];
		}
		const uint64_t per = it.ar32 ? 32 : 1;
		uint64_t cbad = 0, fbad = 0;
		for (uint64_t k = 0; k < NC; k++) {
			const uint64_t E = std::min<uint64_t>(it.G, W - k * it.G);
			cbad += cnt[k] != 2 * E * per;
			const uint64_t f = read_flag(c, k), fw = f & 0xffffffffull;
			fbad += (f >> 32) != 2 || fw < k * it.G ||
				fw >= k * it.G + E;
		}
		// nothing beyond the last chunk was flagged
		uint64_t extra = 0;
		for (uint64_t k = NC; k < std::min<uint64_t>(NC + 64, W); k++)
			extra += read_flag(c, k) != 0;
		expect("counters_bad", cbad, 0);
		expect("flags_bad", fbad, 0);
		expect("flags_beyond", extra, 0);
		expect("completers", aux[0], 2 * NC);
		expect("increments", aux[1], 2 * W * per);
		// the host's view of the division the kernels do
		uint64_t dbad = 0;
		const uint64_t m = magic_for(it.G);
		for (uint64_t w = 0; w < W; w++)
			dbad += ((w * m) >> 40) != w / it.G;
		expect("division_bad", dbad, 0);
	}
	it.valid = ok ? "ok" : "FAIL";
	it.valid_detail = d;
}

// N back-to-back launches between two events behind the k_hold gate (see
// bench2). Counters are zeroed and one untimed launch (epoch 1) runs first;
// the timed launches have epochs 2..N+1.
static double time_batch(Ctx &c, Item &it, unsigned n, CUevent e0, CUevent e1,
			 int *capped, int *post_ok)
{
	reset_counters(c, it);
	// zero the flags the post-check reads, so it sees this batch's stores
	// (not an earlier batch's of the same item, which has the same last epoch)
	if (it.kind == K_BATCH || it.kind == K_PW) {
		const uint64_t last = it.kind == K_BATCH ? it.nchunks() - 1 :
							   it.shape->warps() - 1;
		for (uint64_t k : { (uint64_t)0, last })
			CU(cuMemsetD32Async(c.reg.dev + FLAG_OFF + k * FLAG_STRIDE * 8,
					    0, 2, c.st));
	}
	launch(c, it, 1, 0);
	const uint64_t caps0 = __atomic_load_n(&c.hold_host[1], __ATOMIC_ACQUIRE);
	__atomic_store_n(&c.hold_host[0], 0, __ATOMIC_RELEASE);
	{
		void *args[] = { &c.hold_dev, &c.hold_cap_ns };
		CU(cuLaunchKernel(c.hold_fn, 1, 1, 1, 32, 1, 1, 0, c.st, args,
				  nullptr));
	}
	CU(cuEventRecord(e0, c.st));
	for (unsigned i = 0; i < n; i++)
		launch(c, it, 2 + i, 0);
	CU(cuEventRecord(e1, c.st));
	__atomic_store_n(&c.hold_host[0], 1, __ATOMIC_RELEASE);
	CU(cuEventSynchronize(e1));
	float ms = 0;
	CU(cuEventElapsedTime(&ms, e0, e1));
	*capped = __atomic_load_n(&c.hold_host[1], __ATOMIC_ACQUIRE) != caps0;
	// post-check: the last launch completed the first and the last chunk
	*post_ok = 1;
	const uint64_t last_epoch = n + 1;
	if (it.kind == K_BATCH) {
		const uint64_t W = it.shape->warps(), NC = it.nchunks();
		for (uint64_t k : { (uint64_t)0, NC - 1 }) {
			const uint64_t E = std::min<uint64_t>(it.G, W - k * it.G);
			const uint64_t want = last_epoch * E * (it.ar32 ? 32 : 1);
			if (read_cnt(c, it, k) != want ||
			    (read_flag(c, k) >> 32) != last_epoch)
				*post_ok = 0;
		}
	} else if (it.kind == K_PW) {
		if ((read_flag(c, 0) >> 32) != last_epoch ||
		    (read_flag(c, it.shape->warps() - 1) >> 32) != last_epoch)
			*post_ok = 0;
	}
	return ms;
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
	std::string out = "out", kcubin = "build/kernels.cubin", only = "",
		    shapes_arg = "full,anchor,large",
		    gs_arg = "1,2,4,8,16,32,48,96,384,1536,8192";
	int rounds = 10;
	double target_ms = 4.0, max_wall_s = 1e9;
	unsigned nmax = 200;
	uint64_t seed = 20261116;
	for (int i = 1; i < argc; i++) {
		std::string a = argv[i];
		auto val = [&]() -> std::string {
			if (i + 1 >= argc) {
				fprintf(stderr, "missing value for %s\n",
					a.c_str());
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
		else if (a == "--gs")
			gs_arg = val();
		else if (a == "--only")
			only = val();
		else if (a == "--seed")
			seed = strtoull(val().c_str(), nullptr, 0);
		else if (a == "--kernels")
			kcubin = val();
		else if (a == "--max-wall-s")
			max_wall_s = atof(val().c_str());
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
		return std::chrono::duration<double>(
			       std::chrono::steady_clock::now() - t_start)
			.count();
	};
	// step 0's shapes: full occupancy (8 SMs x 1,536), rms_norm's pp512
	// launch (the anchor), the largest llama.cpp launch
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
	std::vector<uint32_t> gs;
	for (auto &s : split(gs_arg))
		gs.push_back((uint32_t)atoi(s.c_str()));

	Ctx c;
	step0b_primary_ctx(0);
	CU(cuStreamCreate(&c.st, CU_STREAM_NON_BLOCKING));
	const std::string shm_name = fmt("step0b_c4_%d", (int)getpid());
	if (!step0b_regmem_create(c.reg, shm_name, REG_BYTES, false)) {
		fprintf(stderr, "registered memory failed\n");
		return 3;
	}
	CU(cuMemAlloc(&c.dcnt, DCNT_BYTES));
	CU(cuMemAlloc(&c.aux, AUX_BYTES));
	CU(cuModuleLoad(&c.mk, kcubin.c_str()));
	{ // timing gate: one pinned, device-mapped page (not shm)
		void *h = nullptr;
		CU(cuMemHostAlloc(&h, 4096,
				  CU_MEMHOSTALLOC_DEVICEMAP |
					  CU_MEMHOSTALLOC_PORTABLE));
		memset(h, 0, 4096);
		c.hold_host = (uint64_t *)h;
		CU(cuMemHostGetDevicePointer(&c.hold_dev, h, 0));
		CU(cuModuleGetFunction(&c.hold_fn, c.mk, "k_hold"));
	}
	memset(c.reg.host, 0, REG_BYTES);
	CU(cuMemsetD32(c.dcnt, 0, DCNT_BYTES / 4));

	auto fn = [&](const std::string &name) {
		CUfunction f;
		CU(cuModuleGetFunction(&f, c.mk, name.c_str()));
		return f;
	};
	const std::vector<std::string> rf = { "rf_scgpu", "rf_argpu", "rf_scsys",
					      "rf_arsys" };
	const std::vector<std::string> batch = { "nofence", "nosys",  "sysonly",
						 "tf_sc",   "ar_sc",  "rel_sc",
						 "ar_ar",   "rel_ar", "ar_aa",
						 "rmw_ar" };
	std::vector<Item> items;
	items.reserve(4096);
	for (auto &s : shapes) {
		const Shape *sp = &s;
		auto add = [&](Item it) {
			it.shape = sp;
			it.id = fmt("%s|%s|%s|G%u|%s", sp->name.c_str(),
				    it.layout.c_str(), it.variant.c_str(), it.G,
				    it.cntmem.c_str());
			if (!only.empty() &&
			    !std::regex_search(it.id, std::regex(only)))
				return;
			items.push_back(it);
		};
		{
			Item e;
			e.kind = K_EMPTY;
			e.layout = "-";
			e.variant = "empty";
			e.cntmem = "-";
			e.fn = fn("k_empty");
			add(e);
		}
		for (const char *f : { "scgpu", "argpu", "scsys", "arsys" }) {
			Item it;
			it.kind = K_FONLY;
			it.layout = "-";
			it.variant = std::string("fonly_") + f;
			it.cntmem = "-";
			it.fn = fn(std::string("k_fonly_") + f);
			add(it);
		}
		for (const char *L : { "w32", "l8" }) {
			const std::string l = L;
			{
				Item it;
				it.kind = K_REC;
				it.layout = l;
				it.variant = "rec";
				it.cntmem = "-";
				it.fn = fn("k_" + l + "_rec");
				add(it);
			}
			for (auto &v : rf) {
				Item it;
				it.kind = K_RF;
				it.layout = l;
				it.variant = v;
				it.cntmem = "-";
				it.fn = fn("k_" + l + "_" + v);
				add(it);
			}
			for (const char *v : { "pw_sc", "pw_ar" }) {
				Item it;
				it.kind = K_PW;
				it.layout = l;
				it.variant = v;
				it.cntmem = "-";
				it.fn = fn("k_" + l + "_" + v);
				add(it);
			}
			for (uint32_t G : gs) {
				if (G == 0 || G > s.warps())
					continue;
				std::vector<std::string> vs = batch;
				if (l == "l8") {
					vs.push_back("ar32_sc");
					vs.push_back("ar32_ar");
					vs.push_back("ar32_aa");
				}
				for (auto &v : vs) {
					Item it;
					it.kind = K_BATCH;
					it.layout = l;
					it.variant = v;
					it.G = G;
					it.cntmem = "dev";
					it.ar32 = v.rfind("ar32_", 0) == 0;
					it.fn = fn("k_" + l + "_" + v);
					add(it);
				}
				if (l == "w32") { // the counter in registered memory
					Item it;
					it.kind = K_BATCH;
					it.layout = l;
					it.variant = "ar_sc";
					it.G = G;
					it.cntmem = "reg";
					it.fn = fn("k_w32_ar_sc");
					add(it);
				}
			}
		}
	}
	fprintf(stderr, "items: %zu, rounds: %d, shm %s (unlinked)\n",
		items.size(), rounds, shm_name.c_str());

	CUevent e0, e1;
	CU(cuEventCreate(&e0, CU_EVENT_DEFAULT));
	CU(cuEventCreate(&e1, CU_EVENT_DEFAULT));
	int nfail = 0;
	for (auto &it : items) {
		cuFuncGetAttribute(&it.regs, CU_FUNC_ATTRIBUTE_NUM_REGS, it.fn);
		cuFuncGetAttribute(&it.local_bytes,
				   CU_FUNC_ATTRIBUTE_LOCAL_SIZE_BYTES, it.fn);
		CU(cuOccupancyMaxActiveBlocksPerMultiprocessor(
			&it.occ_blocks, it.fn, it.shape->tpb, 0));
		validate(c, it);
		if (it.valid == "FAIL") {
			nfail++;
			fprintf(stderr, "VALIDATION FAIL %s: %s\n", it.id.c_str(),
				it.valid_detail.c_str());
		}
		int capped = 0, post_ok = 0;
		const double t1 = time_batch(c, it, 1, e0, e1, &capped, &post_ok);
		const double t3 =
			time_batch(c, it, 3, e0, e1, &capped, &post_ok) / 3;
		it.cal_ms = std::min(t1, t3);
		it.n = (unsigned)std::clamp<double>(
			std::ceil(target_ms / std::max(it.cal_ms, 1e-4)),
			it.cal_ms > 50 ? 1 : 2, nmax);
		if (wall() > max_wall_s) {
			fprintf(stderr, "max wall time reached in validation\n");
			return 6;
		}
	}
	fprintf(stderr,
		"validation done (%d failures), calibration done, %.1f s\n",
		nfail, wall());
	int smc = 0;
	{
		CUdevice d;
		CU(cuDeviceGet(&d, 0));
		CU(cuDeviceGetAttribute(&smc, CU_DEVICE_ATTRIBUTE_MULTIPROCESSOR_COUNT,
					d));
	}
	// items.json is written now and rewritten at the end, so that a run cut
	// short (timeout) still leaves the item list next to its raw.csv rows
	auto write_items = [&](int done_rounds, bool final_) {
		std::string jp = out + "/items.json";
		FILE *j = fopen(jp.c_str(), "w");
		if (!j) {
			perror(jp.c_str());
			return;
		}
		fprintf(j, "{\"rounds_done\": %d, \"rounds_requested\": %d, \"final\": %s, "
			   "\"sms\": %d, \"seed\": %" PRIu64 ", \"validation_failures\": %d, "
			   "\"wall_s\": %.1f, \"items\": [\n",
			done_rounds, rounds, final_ ? "true" : "false", smc, seed, nfail,
			wall());
		for (size_t i = 0; i < items.size(); i++) {
			auto &it = items[i];
			fprintf(j,
				"  {\"id\": %s, \"shape\": %s, \"layout\": %s, \"variant\": %s, "
				"\"G\": %u, \"cnt\": %s, \"warps\": %" PRIu64
				", \"chunks\": %" PRIu64 ", \"n\": %u, \"cal_ms\": %.4f, "
				"\"regs\": %d, \"local_bytes\": %d, \"occ_blocks\": %d, "
				"\"valid\": %s, \"valid_detail\": %s}%s\n",
				json_str(it.id).c_str(), json_str(it.shape->name).c_str(),
				json_str(it.layout).c_str(), json_str(it.variant).c_str(),
				it.G, json_str(it.cntmem).c_str(), it.shape->warps(),
				it.nchunks(), it.n, it.cal_ms, it.regs, it.local_bytes,
				it.occ_blocks, json_str(it.valid).c_str(),
				json_str(it.valid_detail).c_str(),
				i + 1 < items.size() ? "," : "");
		}
		fprintf(j, "]}\n");
		fclose(j);
	};
	write_items(0, false);

	std::string csvp = out + "/raw.csv";
	FILE *csv = fopen(csvp.c_str(), "w");
	if (!csv) {
		perror(csvp.c_str());
		return 5;
	}
	fprintf(csv, "round,pos,id,shape,layout,variant,G,cnt,warps,threads,"
		     "chunks,n_launch,batch_ms,per_launch_us,hold_capped,post_ok\n");
	int done_rounds = 0;
	for (int r = 0; r < rounds; r++) {
		if (wall() > max_wall_s) {
			fprintf(stderr, "max wall time reached before round %d\n",
				r);
			break;
		}
		std::vector<size_t> order(items.size());
		for (size_t i = 0; i < order.size(); i++)
			order[i] = i;
		std::mt19937_64 rng(seed + (uint64_t)r);
		std::shuffle(order.begin(), order.end(), rng);
		for (size_t pos = 0; pos < order.size(); pos++) {
			Item &it = items[order[pos]];
			int capped = 0, post_ok = 0;
			const double ms =
				time_batch(c, it, it.n, e0, e1, &capped, &post_ok);
			fprintf(csv,
				"%d,%zu,%s,%s,%s,%s,%u,%s,%" PRIu64 ",%" PRIu64
				",%" PRIu64 ",%u,%.6f,%.4f,%d,%d\n",
				r, pos, it.id.c_str(), it.shape->name.c_str(),
				it.layout.c_str(), it.variant.c_str(), it.G,
				it.cntmem.c_str(), it.shape->warps(),
				it.shape->threads(), it.nchunks(), it.n, ms,
				ms * 1000.0 / it.n, capped, post_ok);
		}
		fflush(csv);
		done_rounds++;
		write_items(done_rounds, false);
		fprintf(stderr, "round %d done, %.1f s\n", r, wall());
	}
	fclose(csv);
	write_items(done_rounds, true);
	step0b_regmem_destroy(c.reg);
	fprintf(stderr, "done, %.1f s\n", wall());
	return nfail ? 7 : 0;
}
