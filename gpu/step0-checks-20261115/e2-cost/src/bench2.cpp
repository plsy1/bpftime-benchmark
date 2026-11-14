// bench2: step 0, check 2 (placement and atomic cost) on the Orin.
//
// Driver API only. Two modules, both launched with cuLaunchKernel:
//   kernels.cubin  the cases (src/kernels.cu)
//   ref1512.cubin  today's 1512 lookup-and-increment, built from the dev
//                  build's kretprobe pass and trampoline (ref/build_ref.py)
// Memory: 128 MiB registered the way bpftime registers its segment
// (common/step0_regmem.h) and 128 MiB of cuMemAlloc memory.
//
// For every item (case x memory x fence x shape x R [x k]): one validation
// launch (results in items.json), then a calibrated batch size N; then
// ROUNDS rounds, each running every item once in a deterministic shuffled
// order: 1 untimed launch, then N launches between two CUDA events.
// Output: OUT/raw.csv (one row per round and item), OUT/items.json.
#include "../../common/step0_regmem.h"

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

static constexpr size_t BUF_BYTES = 128ull << 20;
static constexpr uint64_t BUF_WORDS = BUF_BYTES / 8;
static constexpr size_t AUX_BYTES = 8ull << 20;
static constexpr uint64_t MAX_THREADS = 4194304; // largest shape
// kernels.cu
static constexpr int AUX_CAPHITS = 0, AUX_PART = 64, AUX_LOGN = 128,
		     AUX_LOG = 256;
static constexpr uint64_t RING_BASE_WORDS = 8192, RING_SLOTS = 4096,
			  RING_SLOT_WORDS = 8;
// ref/build_ref.py
static constexpr int HOST_FD = 5, DEV_FD = 6;

// trampoline/default_trampoline.cu MapBasicInfo (40 B, same ABI on host)
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

struct Shape {
	std::string name;
	unsigned blocks, tpb;
	uint32_t rbig;
	bool ksweep;
	uint64_t threads() const { return (uint64_t)blocks * tpb; }
	uint64_t warps() const { return threads() / 32; }
};

struct Item {
	std::string id, kase, mem; // mem: reg, dev, none
	int fence = 0;
	const Shape *shape = nullptr;
	uint32_t R = 1, k = 0;
	CUfunction fn = nullptr;
	bool ref = false;
	// filled in
	unsigned n = 2;
	double cal_ms = 0;
	int occ_blocks = 0, regs = 0, local_bytes = 0;
	std::string valid = "not-run";
	std::string valid_detail;
	double part_total = 0; // participating warps (atom_sm/cas_sm)
	double kmax_conc = 0, kmean_conc = 0; // max/mean concurrency per SM
	int max_warpid = -1;
	uint64_t cap_hits = 0;
};

static std::string fmt(const char *f, ...)
{
	char b[1024];
	va_list ap;
	va_start(ap, f);
	vsnprintf(b, sizeof b, f, ap);
	va_end(ap);
	return b;
}

struct Ctx {
	step0_regmem reg;
	CUdeviceptr dbuf = 0, aux = 0;
	CUmodule mk = nullptr, mr = nullptr;
	CUstream st = nullptr;
	// timing gate (k_hold): [0] release word set by the host once the
	// batch is queued, [1] cap hits counted by the kernel
	bool use_hold = true;
	uint64_t *hold_host = nullptr;
	CUdeviceptr hold_dev = 0;
	CUfunction hold_fn = nullptr;
	uint64_t hold_cap_ns = 200000000ull; // 200 ms
};

static CUdeviceptr buf_of(Ctx &c, const std::string &mem)
{
	return mem == "reg" ? c.reg.dev : c.dbuf;
}

static void launch(Ctx &c, Item &it, uint32_t mode)
{
	uint64_t buf = buf_of(c, it.mem), mask = BUF_WORDS - 1, aux = c.aux;
	uint32_t R = it.R, k = it.k;
	void *args[] = { &buf, &mask, &R, &k, &aux, &mode };
	STEP0_CU(cuLaunchKernel(it.fn, it.shape->blocks, 1, 1, it.shape->tpb,
				1, 1, 0, c.st, args, nullptr));
}

static void read_words(Ctx &c, const std::string &mem, uint64_t first,
		       uint64_t count, std::vector<uint64_t> &out)
{
	out.resize(count);
	if (count == 0)
		return;
	if (mem == "reg") {
		memcpy(out.data(), (uint64_t *)c.reg.host + first, count * 8);
	} else {
		STEP0_CU(cuMemcpyDtoH(out.data(), c.dbuf + first * 8,
				      count * 8));
	}
}

static void reset_buffers(Ctx &c, const std::string &mem, uint64_t words)
{
	words = std::min<uint64_t>(words, BUF_WORDS);
	if (mem == "reg" || mem == "dev")
		STEP0_CU(cuMemsetD32Async(buf_of(c, mem), 0, words * 2, c.st));
	STEP0_CU(cuMemsetD32Async(c.aux, 0, AUX_BYTES / 4, c.st));
	STEP0_CU(cuStreamSynchronize(c.st));
}

// Words of buf a case touches from a zeroed buffer (for the reset).
static uint64_t footprint_words(const Item &it)
{
	const uint64_t T = it.shape->threads(), W = it.shape->warps(),
		       R = it.R;
	const std::string &k = it.kase;
	if (it.ref)
		return T;
	if (k == "st_lane32" || k == "st_lane1")
		return R * T;
	if (k == "rec_lane0" || k == "rec_lane4")
		return R * W * 4;
	if (k == "rec_thread")
		return R * T * 4;
	if (k == "atom_lane")
		return T;
	if (k == "atom_warp_all" || k == "atom_warp_leader" ||
	    k == "atom_warp_leader_sys" || k == "red_warp_leader")
		return W * 4;
	if (k == "atom_sm" || k == "cas_sm")
		return 64 * 16;
	if (k == "comp_rec")
		return RING_BASE_WORDS + 64 * RING_SLOTS * RING_SLOT_WORDS;
	return 0;
}

static uint64_t sum_words(Ctx &c, const std::string &mem, uint64_t first,
			  uint64_t count, uint64_t stride = 1)
{
	std::vector<uint64_t> v;
	read_words(c, mem, first, count, v);
	uint64_t s = 0;
	for (uint64_t i = 0; i < count; i += stride)
		s += v[i];
	return s;
}

// One launch in validation mode on zeroed memory, then checks that every
// operation ran (atomic sums) or that the last iteration's stores landed.
static void validate(Ctx &c, Item &it)
{
	const uint64_t T = it.shape->threads(), W = it.shape->warps(),
		       R = it.R;
	const std::string mem = it.mem;
	reset_buffers(c, mem == "none" ? "dev" : mem,
		      std::max<uint64_t>(footprint_words(it), 1024));
	launch(c, it, 1);
	STEP0_CU(cuStreamSynchronize(c.st));
	std::vector<uint64_t> auxv(AUX_BYTES / 8);
	STEP0_CU(cuMemcpyDtoH(auxv.data(), c.aux, AUX_BYTES));
	it.cap_hits = auxv[AUX_CAPHITS];
	const std::string &k = it.kase;
	bool ok = true;
	std::string d;
	auto expect = [&](const char *what, uint64_t got, uint64_t want) {
		d += fmt("%s=%" PRIu64 "/%" PRIu64 " ", what, got, want);
		if (got != want)
			ok = false;
	};
	auto sample_stores = [&](uint64_t units, uint64_t stride_units,
				 uint64_t words_per_unit, bool lane0_only) {
		// last iteration r = R-1; unit index = r*units + u (no wrap)
		const uint64_t r = R - 1;
		if ((r * units + units) * words_per_unit > BUF_WORDS) {
			d += "wrapped(not-checked) ";
			return;
		}
		uint64_t bad = 0, n = 0;
		std::vector<uint64_t> v;
		const uint64_t step = std::max<uint64_t>(1, units / 4096);
		for (uint64_t u = 0; u < units; u += step) {
			if (lane0_only && (u % 32) != 0)
				continue;
			const uint64_t w = (r * units + u) * words_per_unit;
			read_words(c, mem, w, words_per_unit, v);
			for (uint64_t j = 0; j < words_per_unit; j++) {
				uint64_t want;
				if (words_per_unit == 1)
					want = u + r + 1;
				else
					want = ((u + r + 1) << 2) | j;
				n++;
				if (v[j] != want)
					bad++;
			}
		}
		(void)stride_units;
		expect("sample_mismatch", bad, 0);
		d += fmt("sampled=%" PRIu64 " ", n);
	};
	if (it.ref) {
		expect("sum", sum_words(c, mem, 0, T), T * R);
	} else if (k == "empty" || k == "ref_empty") {
		d += "none ";
	} else if (k == "st_lane32") {
		sample_stores(T, 1, 1, false);
	} else if (k == "st_lane1") {
		sample_stores(T, 32, 1, true);
	} else if (k == "rec_lane0" || k == "rec_lane4") {
		sample_stores(W, 1, 4, false);
	} else if (k == "rec_thread") {
		sample_stores(T, 1, 4, false);
	} else if (k == "atom_lane") {
		expect("sum", sum_words(c, mem, 0, T), T * R);
	} else if (k == "atom_warp_all") {
		expect("sum", sum_words(c, mem, 0, W * 4), T * R);
	} else if (k == "atom_warp_leader" || k == "atom_warp_leader_sys" ||
		   k == "red_warp_leader") {
		expect("sum", sum_words(c, mem, 0, W * 4), W * R);
	} else if (k == "atom_sm" || k == "cas_sm") {
		uint64_t part = 0;
		for (int s = 0; s < 64; s++)
			part += auxv[AUX_PART + s];
		it.part_total = (double)part;
		expect("sum", sum_words(c, mem, 0, 64 * 16), part * R);
		expect("cap_hits", it.cap_hits, 0);
		// concurrency per SM from the participant log (t0, t1)
		const uint64_t n = std::min<uint64_t>(
			auxv[AUX_LOGN], (AUX_BYTES / 8 - AUX_LOG) / 4);
		std::map<unsigned, std::vector<std::pair<uint64_t, int>>> ev;
		std::map<unsigned, std::set<unsigned>> wids;
		int maxw = -1;
		for (uint64_t e = 0; e < n; e++) {
			const uint64_t *p = &auxv[AUX_LOG + e * 4];
			const unsigned sm = (unsigned)(p[0] >> 32),
				       wid = (unsigned)(p[0] & 0xffffffff);
			ev[sm].push_back({ p[1], +1 });
			ev[sm].push_back({ p[2] + 1, -1 });
			wids[sm].insert(wid);
			maxw = std::max(maxw, (int)wid);
		}
		double maxc = 0, sumc = 0;
		for (auto &[sm, v] : ev) {
			std::sort(v.begin(), v.end());
			int cur = 0, m = 0;
			for (auto &e : v) {
				cur += e.second;
				m = std::max(m, cur);
			}
			maxc = std::max<double>(maxc, m);
			sumc += m;
		}
		it.kmax_conc = maxc;
		it.kmean_conc = ev.empty() ? 0 : sumc / ev.size();
		it.max_warpid = maxw;
		d += fmt("logged=%" PRIu64 " sms=%zu ", n, ev.size());
	} else if (k == "comp_rec") {
		expect("cursor_sum", sum_words(c, mem, 0, 64 * 16), W * R);
	}
	it.valid = ok ? "ok" : "FAIL";
	it.valid_detail = d;
}

// Operations a batch performed on the per-SM words (atom_sm, cas_sm) or
// cursors (comp_rec): the words count them, so before/after sums give the
// exact number, whatever the scheduler did.
static bool counts_ops(const Item &it)
{
	return it.kase == "atom_sm" || it.kase == "cas_sm" ||
	       it.kase == "comp_rec";
}
static uint64_t ops_words_sum(Ctx &c, const Item &it)
{
	return sum_words(c, it.mem, 0, 64 * 16, 16);
}

// N back-to-back launches between two events. With the gate (default), a
// k_hold launch in front keeps the stream waiting until all N launches and
// e1 are queued, so the events time GPU execution only; without it, kernels
// shorter than the host's per-launch cost (about 3.5 us) are timed at the
// host's launch rate. *capped is set when the gate hit its cap (the batch
// is then not fully queued in front of the GPU and is marked in raw.csv).
static double time_batch(Ctx &c, Item &it, unsigned n, CUevent e0, CUevent e1,
			 int *capped = nullptr)
{
	uint64_t caps0 = 0;
	if (c.use_hold) {
		caps0 = __atomic_load_n(&c.hold_host[1], __ATOMIC_ACQUIRE);
		__atomic_store_n(&c.hold_host[0], 0, __ATOMIC_RELEASE);
		void *args[] = { &c.hold_dev, &c.hold_cap_ns };
		STEP0_CU(cuLaunchKernel(c.hold_fn, 1, 1, 1, 32, 1, 1, 0, c.st,
					args, nullptr));
	}
	STEP0_CU(cuEventRecord(e0, c.st));
	for (unsigned i = 0; i < n; i++)
		launch(c, it, 0);
	STEP0_CU(cuEventRecord(e1, c.st));
	if (c.use_hold)
		__atomic_store_n(&c.hold_host[0], 1, __ATOMIC_RELEASE);
	STEP0_CU(cuEventSynchronize(e1));
	float ms = 0;
	STEP0_CU(cuEventElapsedTime(&ms, e0, e1));
	if (capped)
		*capped = c.use_hold &&
			  __atomic_load_n(&c.hold_host[1], __ATOMIC_ACQUIRE) !=
				  caps0;
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
	std::string out = "out", kcubin = "build/kernels.cubin",
		    rcubin = "build/ref/ref1512.cubin", only = "",
		    shapes_arg = "tiny,full,anchor,large", ks_arg =
							       "1,2,4,8,16,32,48",
		    ksweep_arg = "full,large";
	int rounds = 10;
	double target_ms = 4.0, max_wall_s = 1e9;
	unsigned nmax = 200;
	uint64_t seed = 20261115;
	bool use_hold = true;
	std::map<std::string, uint32_t> rbig = {
		{ "tiny", 64 }, { "full", 16 }, { "anchor", 8 }, { "large", 2 }
	};
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
		else if (a == "--ks")
			ks_arg = val();
		else if (a == "--ksweep-shapes")
			ksweep_arg = val();
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
		else if (a == "--no-hold") // time without the k_hold gate
			use_hold = false;
		else if (a == "--rbig") { // name=R,name=R
			std::stringstream ss(val());
			std::string tok;
			while (std::getline(ss, tok, ',')) {
				auto eq = tok.find('=');
				rbig[tok.substr(0, eq)] =
					(uint32_t)atoi(tok.c_str() + eq + 1);
			}
		} else {
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

	// shapes: 64 threads; full occupancy 8 SMs x 1,536 = 24 x 512;
	// the rms_norm shape behind the 0.6 ns figure (512 blocks of 512);
	// the largest llama.cpp launch (4,194,304 threads)
	std::vector<Shape> all_shapes = {
		{ "tiny", 1, 64, 0, false },
		{ "full", 24, 512, 0, false },
		{ "anchor", 512, 512, 0, false },
		{ "large", 8192, 512, 0, false },
	};
	std::vector<Shape> shapes;
	std::set<std::string> ksw;
	for (auto &s : split(ksweep_arg))
		ksw.insert(s);
	for (auto &n : split(shapes_arg))
		for (auto s : all_shapes)
			if (s.name == n) {
				s.rbig = rbig.count(n) ? rbig[n] : 4;
				s.ksweep = ksw.count(n) > 0;
				shapes.push_back(s);
			}
	std::vector<uint32_t> ks;
	for (auto &s : split(ks_arg))
		ks.push_back((uint32_t)atoi(s.c_str()));

	Ctx c;
	step0_primary_ctx(0);
	STEP0_CU(cuStreamCreate(&c.st, CU_STREAM_NON_BLOCKING));
	const std::string shm_name = fmt("step0_e2_%d", (int)getpid());
	if (!step0_regmem_create(c.reg, shm_name, BUF_BYTES, false)) {
		fprintf(stderr, "registered memory failed\n");
		return 3;
	}
	STEP0_CU(cuMemAlloc(&c.dbuf, BUF_BYTES));
	STEP0_CU(cuMemAlloc(&c.aux, AUX_BYTES));
	STEP0_CU(cuModuleLoad(&c.mk, kcubin.c_str()));
	STEP0_CU(cuModuleLoad(&c.mr, rcubin.c_str()));
	c.use_hold = use_hold;
	{ // timing gate: one pinned, device-mapped page (not shm)
		void *h = nullptr;
		STEP0_CU(cuMemHostAlloc(&h, 4096,
					CU_MEMHOSTALLOC_DEVICEMAP |
						CU_MEMHOSTALLOC_PORTABLE));
		memset(h, 0, 4096);
		c.hold_host = (uint64_t *)h;
		STEP0_CU(cuMemHostGetDevicePointer(&c.hold_dev, h, 0));
		STEP0_CU(cuModuleGetFunction(&c.hold_fn, c.mk, "k_hold"));
	}
	{ // map_info[HOST_FD] -> registered, map_info[DEV_FD] -> device
		CUdeviceptr mi;
		size_t bytes;
		STEP0_CU(cuModuleGetGlobal(&mi, &bytes, c.mr, "map_info"));
		if (bytes != 1024 * sizeof(MapBasicInfo)) {
			fprintf(stderr, "map_info is %zu bytes\n", bytes);
			return 4;
		}
		MapBasicInfo m = {};
		m.enabled = true;
		m.key_size = 4;
		m.value_size = 8;
		m.max_entries = 1;
		m.map_type = 1512; // BPF_MAP_TYPE_PERGPUTD_ARRAY_HOST_MAP
		m.max_thread_count = MAX_THREADS;
		m.extra_buffer = (void *)(uintptr_t)c.reg.dev;
		STEP0_CU(cuMemcpyHtoD(mi + HOST_FD * sizeof m, &m, sizeof m));
		m.extra_buffer = (void *)(uintptr_t)c.dbuf;
		STEP0_CU(cuMemcpyHtoD(mi + DEV_FD * sizeof m, &m, sizeof m));
	}
	memset(c.reg.host, 0, BUF_BYTES);
	STEP0_CU(cuMemsetD32(c.dbuf, 0, BUF_BYTES / 4));

	auto fn = [&](CUmodule m, const std::string &name) {
		CUfunction f;
		STEP0_CU(cuModuleGetFunction(&f, m, name.c_str()));
		return f;
	};
	const std::vector<std::string> base_cases = {
		"st_lane32",	  "st_lane1",	     "rec_lane0",
		"rec_lane4",	  "rec_thread",	     "atom_lane",
		"atom_warp_all",  "atom_warp_leader", "atom_warp_leader_sys",
		"red_warp_leader", "comp_rec"
	};
	std::vector<Item> items;
	for (auto &s : shapes) {
		const Shape *sp = nullptr;
		for (auto &x : shapes)
			if (x.name == s.name)
				sp = &x;
		auto add = [&](Item it) {
			it.shape = sp;
			it.id = fmt("%s|%s|%s|f%d|R%u|k%u", sp->name.c_str(),
				    it.kase.c_str(), it.mem.c_str(), it.fence,
				    it.R, it.k);
			if (!only.empty() &&
			    !std::regex_search(it.id, std::regex(only)))
				return;
			items.push_back(it);
		};
		{
			Item e;
			e.kase = "empty";
			e.mem = "none";
			e.fn = fn(c.mk, "k_empty");
			add(e);
			Item re;
			re.kase = "ref_empty";
			re.mem = "none";
			re.fn = fn(c.mr, "ref_empty");
			add(re);
		}
		for (uint32_t R : { 1u, s.rbig }) {
			for (const char *mem : { "reg", "dev" }) {
				const std::string h =
					std::string(mem) == "reg" ? "host" :
								    "dev";
				Item r;
				r.kase = "ref1512";
				r.mem = mem;
				r.fence = 1;
				r.R = R;
				r.ref = true;
				r.fn = fn(c.mr, R == 1 ? "ref_" + h + "_r1" :
							 "ref_" + h + "_rn");
				add(r);
				if (R == 1) { // the loop form at R = 1
					Item l = r;
					l.kase = "ref1512_loop";
					l.fn = fn(c.mr, "ref_" + h + "_rn");
					add(l);
				}
				for (int f : { 0, 1 }) {
					for (auto &cs : base_cases) {
						Item it;
						it.kase = cs;
						it.mem = mem;
						it.fence = f;
						it.R = R;
						it.fn = fn(c.mk,
							   fmt("k_%s_f%d",
							       cs.c_str(), f));
						add(it);
					}
					if (!s.ksweep)
						continue;
					// With more warps than fit at once,
					// warps are replaced and %warpid < k
					// picks a scheduler-dependent subset
					// (smoke: 72-28,506 of 131,072 warps):
					// there, only k = 48 (every warp).
					const bool replaced =
						s.threads() > 8ull * 1536;
					for (const char *cs :
					     { "atom_sm", "cas_sm" })
						for (uint32_t k : ks) {
							if (replaced && k < 48)
								continue;
							Item it;
							it.kase = cs;
							it.mem = mem;
							it.fence = f;
							it.R = R;
							it.k = k;
							it.fn = fn(
								c.mk,
								fmt("k_%s_f%d",
								    cs, f));
							add(it);
						}
				}
			}
		}
		if (s.rbig == 1) { // R list had a duplicate
			std::set<std::string> seen;
			std::vector<Item> u;
			for (auto &it : items)
				if (seen.insert(it.id).second)
					u.push_back(it);
			items.swap(u);
		}
	}
	fprintf(stderr, "items: %zu, rounds: %d, shm %s (unlinked)\n",
		items.size(), rounds, shm_name.c_str());

	// attributes, validation, calibration
	CUevent e0, e1;
	STEP0_CU(cuEventCreate(&e0, CU_EVENT_DEFAULT));
	STEP0_CU(cuEventCreate(&e1, CU_EVENT_DEFAULT));
	int nfail = 0;
	for (auto &it : items) {
		cuFuncGetAttribute(&it.regs, CU_FUNC_ATTRIBUTE_NUM_REGS, it.fn);
		cuFuncGetAttribute(&it.local_bytes,
				   CU_FUNC_ATTRIBUTE_LOCAL_SIZE_BYTES, it.fn);
		STEP0_CU(cuOccupancyMaxActiveBlocksPerMultiprocessor(
			&it.occ_blocks, it.fn, it.shape->tpb, 0));
		validate(c, it);
		if (it.valid == "FAIL") {
			nfail++;
			fprintf(stderr, "VALIDATION FAIL %s: %s\n",
				it.id.c_str(), it.valid_detail.c_str());
		}
		launch(c, it, 0); // warm
		STEP0_CU(cuStreamSynchronize(c.st));
		const double t1 = time_batch(c, it, 1, e0, e1);
		const double t3 = time_batch(c, it, 3, e0, e1) / 3;
		it.cal_ms = std::min(t1, t3);
		it.n = (unsigned)std::clamp<double>(
			std::ceil(target_ms / std::max(it.cal_ms, 1e-4)),
			it.cal_ms > 50 ? 1 : 2, nmax);
	}
	fprintf(stderr, "validation done (%d failures), calibration done, %.1f s\n",
		nfail, wall());

	// rounds
	std::string csvp = out + "/raw.csv";
	FILE *csv = fopen(csvp.c_str(), "w");
	if (!csv) {
		perror(csvp.c_str());
		return 5;
	}
	fprintf(csv, "round,pos,id,case,mem,fence,shape,blocks,tpb,threads,R,k,"
		     "n_launch,batch_ms,per_launch_us,ops_batch,hold_capped\n");
	int done_rounds = 0;
	uint64_t hold_capped_rows = 0;
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
			if (it.cal_ms < 20) {
				launch(c, it, 0); // untimed
			}
			uint64_t ops0 = 0, ops = 0;
			if (counts_ops(it)) {
				STEP0_CU(cuStreamSynchronize(c.st));
				ops0 = ops_words_sum(c, it);
			}
			int capped = 0;
			const double ms =
				time_batch(c, it, it.n, e0, e1, &capped);
			if (counts_ops(it))
				ops = ops_words_sum(c, it) - ops0;
			hold_capped_rows += capped;
			fprintf(csv,
				"%d,%zu,%s,%s,%s,%d,%s,%u,%u,%" PRIu64
				",%u,%u,%u,%.6f,%.4f,%" PRIu64 ",%d\n",
				r, pos, it.id.c_str(), it.kase.c_str(),
				it.mem.c_str(), it.fence,
				it.shape->name.c_str(), it.shape->blocks,
				it.shape->tpb, it.shape->threads(), it.R, it.k,
				it.n, ms, ms * 1000.0 / it.n, ops, capped);
		}
		fflush(csv);
		done_rounds++;
		fprintf(stderr, "round %d done, %.1f s\n", r, wall());
	}
	fclose(csv);

	// items.json
	std::string jp = out + "/items.json";
	FILE *j = fopen(jp.c_str(), "w");
	int dev = 0, smc = 0;
	CUdevice d;
	cuDeviceGet(&d, 0);
	cuDeviceGetAttribute(&smc, CU_DEVICE_ATTRIBUTE_MULTIPROCESSOR_COUNT, d);
	(void)dev;
	fprintf(j, "{\n \"rounds_done\": %d,\n \"rounds_requested\": %d,\n"
		   " \"target_ms\": %g,\n \"seed\": %" PRIu64 ",\n"
		   " \"buf_bytes\": %zu,\n \"sm_count\": %d,\n"
		   " \"wall_s\": %.2f,\n \"validation_failures\": %d,\n"
		   " \"hold_gate\": %s,\n \"hold_capped_rows\": %" PRIu64 ",\n"
		   " \"hold_cap_hits_total\": %" PRIu64 ",\n"
		   " \"items\": [\n",
		done_rounds, rounds, target_ms, seed, BUF_BYTES, smc, wall(),
		nfail, c.use_hold ? "true" : "false", hold_capped_rows,
		c.hold_host[1]);
	for (size_t i = 0; i < items.size(); i++) {
		auto &it = items[i];
		fprintf(j,
			"  {\"id\": %s, \"case\": %s, \"mem\": %s, \"fence\": %d, "
			"\"shape\": %s, \"blocks\": %u, \"tpb\": %u, "
			"\"threads\": %" PRIu64 ", \"warps\": %" PRIu64
			", \"R\": %u, \"k\": %u, \"n\": %u, \"cal_ms\": %.6f, "
			"\"regs\": %d, \"local_bytes\": %d, "
			"\"occ_blocks_per_sm\": %d, \"valid\": %s, "
			"\"valid_detail\": %s, \"participants\": %.0f, "
			"\"k_max_concurrent\": %.0f, \"k_mean_max_concurrent\": "
			"%.2f, \"max_warpid\": %d, \"cap_hits\": %" PRIu64
			"}%s\n",
			json_str(it.id).c_str(), json_str(it.kase).c_str(),
			json_str(it.mem).c_str(), it.fence,
			json_str(it.shape->name).c_str(), it.shape->blocks,
			it.shape->tpb, it.shape->threads(), it.shape->warps(),
			it.R, it.k, it.n, it.cal_ms, it.regs, it.local_bytes,
			it.occ_blocks, json_str(it.valid).c_str(),
			json_str(it.valid_detail).c_str(), it.part_total,
			it.kmax_conc, it.kmean_conc, it.max_warpid, it.cap_hits,
			i + 1 < items.size() ? "," : "");
	}
	fprintf(j, " ]\n}\n");
	fclose(j);

	STEP0_CU(cuStreamSynchronize(c.st));
	cuModuleUnload(c.mk);
	cuModuleUnload(c.mr);
	cuMemFree(c.dbuf);
	cuMemFree(c.aux);
	cuMemFreeHost(c.hold_host);
	step0_regmem_destroy(c.reg);
	fprintf(stderr, "done: %d rounds, %.1f s, out %s\n", done_rounds,
		wall(), out.c_str());
	return nfail ? 6 : 0;
}
