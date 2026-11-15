// bench3: step 0b, check C3 (cost parts of today's per-thread map op, slot
// addressing, fence kinds) on the Orin.
//
// A copy of step 0's e2-cost/src/bench2.cpp. Unchanged: the memory setup
// (128 MiB registered as bpftime registers its segment, 128 MiB cuMemAlloc),
// the launch path (cuLaunchKernel, driver API only), the timing gate k_hold,
// calibration to about 4 ms per batch, rounds with a deterministic shuffle
// (mt19937_64(seed + round)), CUDA events around N back-to-back launches.
// Changed: the item list (C3's items only, plus step 0's anchors), several
// reference modules instead of one, the validation of C3's cases, and the
// output columns (module, fence kind).
//
// Modules, all launched with cuLaunchKernel:
//   k  build/kernels.cubin   step 0's kernels + C3 (b), (d) cases
//   F  build/ref/refF.cubin  today's 1512 op (byte-identical PTX to step 0's)
//   N  build/ref/refN.cubin  the same, the 1512 branch's membar.sys removed
//   X  build/ref/refX.cubin  E, I, IF, IC, K, M, M64 (ref/build_variants.py)
// Each module has its own empty kernel; deltas are taken per module.
// Output: OUT/raw.csv (one row per round and item), OUT/items.json.
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

static constexpr size_t BUF_BYTES = 128ull << 20;
static constexpr uint64_t BUF_WORDS = BUF_BYTES / 8;
static constexpr size_t AUX_BYTES = 8ull << 20;
static constexpr uint64_t MAX_THREADS = 4194304; // largest shape
// kernels.cu
static constexpr int AUX_CAPHITS = 0, AUX_MISS = 2, AUX_OOR = 3, AUX_HITS = 4,
		     AUX_CFAIL = 5;
static constexpr uint64_t TIDW_SLOTS = 16384;
// ref/build_variants.py (same fds as step 0's build_ref.py)
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
	uint64_t threads() const { return (uint64_t)blocks * tpb; }
	uint64_t warps() const { return threads() / 32; }
};

struct Item {
	std::string id, kase, mem, mod; // mem: reg, dev, none; mod: k F N X
	int fk = 0; // fence kind: 0 none, 1 membar.sys, 2 membar.gl,
		    // 3 fence.acq_rel.gpu, 4 fence.acq_rel.sys
	std::string vkind; // validation: ref, st32, st1, slot_e, tid_e,
			   // tidw_e, slot_ex, badtag, slot_cx, collide, none
	const Shape *shape = nullptr;
	uint32_t R = 1, k = 0, mode_extra = 0;
	bool validate_only = false;
	CUfunction fn = nullptr;
	// filled in
	unsigned n = 2;
	double cal_ms = 0;
	int occ_blocks = 0, regs = 0, local_bytes = 0;
	std::string valid = "not-run";
	std::string valid_detail;
	uint64_t cap_hits = 0, slot_miss = 0, slot_oor = 0, slot_hits = 0,
		 claim_fail = 0, inline_miss = 0;
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
	CUmodule mk = nullptr;
	std::map<std::string, CUmodule> mref; // F, N, X
	CUdeviceptr inline_miss = 0; // X: __c3_inline_miss
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
	mode |= it.mode_extra;
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

// Words of buf an item touches from a zeroed buffer (for the reset).
static uint64_t footprint_words(const Item &it)
{
	const uint64_t T = it.shape->threads(), R = it.R;
	const std::string &v = it.vkind;
	if (v == "ref")
		return T;
	if (v == "st32" || v == "st1")
		return R * T;
	if (v == "slot_e" || v == "slot_ex" || v == "badtag" || v == "tidw_e")
		return 2 * TIDW_SLOTS;
	if (v == "tid_e")
		return 2 * T;
	return 0;
}

// The claim table of slot_cx / slot_cxr: the top TIDW_SLOTS x 16 B of the
// device buffer (kernels.cu scratch_claim), which no other item writes.
static constexpr uint64_t CLAIM_FIRST_WORD = BUF_WORDS - 2 * TIDW_SLOTS;
static void reset_claim_table(Ctx &c)
{
	STEP0_CU(cuMemsetD32Async(c.dbuf + CLAIM_FIRST_WORD * 8, 0,
				  2 * TIDW_SLOTS * 2, c.st));
	STEP0_CU(cuStreamSynchronize(c.st));
}

static uint64_t read_inline_miss(Ctx &c)
{
	uint64_t v = 0;
	if (c.inline_miss)
		STEP0_CU(cuMemcpyDtoH(&v, c.inline_miss, 8));
	return v;
}

// One launch in validation mode on zeroed memory, then the checks of the
// item's kind (README "Validation").
static void validate(Ctx &c, Item &it)
{
	const uint64_t T = it.shape->threads(), R = it.R;
	const std::string mem = it.mem;
	reset_buffers(c, mem == "none" ? "dev" : mem,
		      std::max<uint64_t>(footprint_words(it), 1024));
	if (it.vkind == "slot_cx" || it.vkind == "collide")
		reset_claim_table(c);
	const uint64_t im0 = read_inline_miss(c);
	launch(c, it, 1);
	STEP0_CU(cuStreamSynchronize(c.st));
	it.inline_miss = read_inline_miss(c) - im0;
	std::vector<uint64_t> auxv(64);
	STEP0_CU(cuMemcpyDtoH(auxv.data(), c.aux, auxv.size() * 8));
	it.cap_hits = auxv[AUX_CAPHITS];
	it.slot_miss = auxv[AUX_MISS];
	it.slot_oor = auxv[AUX_OOR];
	it.slot_hits = auxv[AUX_HITS];
	it.claim_fail = auxv[AUX_CFAIL];
	const std::string &v = it.vkind;
	bool ok = true;
	std::string d;
	auto expect = [&](const char *what, uint64_t got, uint64_t want) {
		d += fmt("%s=%" PRIu64 "/%" PRIu64 " ", what, got, want);
		if (got != want)
			ok = false;
	};
	auto sample_stores = [&](uint64_t units, bool lane0_only) {
		// step 0's check: last iteration r = R-1, word r*units + u
		const uint64_t r = R - 1;
		if (r * units + units > BUF_WORDS) {
			d += "wrapped(not-checked) ";
			return;
		}
		uint64_t bad = 0, n = 0;
		std::vector<uint64_t> w;
		const uint64_t step = std::max<uint64_t>(1, units / 4096);
		for (uint64_t u = 0; u < units; u += step) {
			if (lane0_only && (u % 32) != 0)
				continue;
			read_words(c, mem, r * units + u, 1, w);
			n++;
			if (w[0] != u + r + 1)
				bad++;
		}
		expect("sample_mismatch", bad, 0);
		d += fmt("sampled=%" PRIu64 " ", n);
	};
	// slot tables: {tag, state}; tag = (k+1)<<48 | (r & 0xff)<<40 | tid
	const uint64_t ktag = (uint64_t)(it.k + 1) << 48;
	auto tag_of = [&](uint64_t tid) {
		return ktag | ((uint64_t)((R - 1) & 0xff) << 40) | tid;
	};
	if (v == "ref") {
		// every thread's own word must equal R (README "Validation"),
		// not only their sum
		std::vector<uint64_t> w;
		read_words(c, mem, 0, T, w);
		uint64_t s = 0, notR = 0;
		for (uint64_t u = 0; u < T; u++) {
			s += w[u];
			notR += w[u] != R;
		}
		expect("sum", s, T * R);
		expect("words_not_R", notR, 0);
		expect("inline_miss", it.inline_miss, 0);
	} else if (v == "st32") {
		sample_stores(T, false);
	} else if (v == "st1") {
		sample_stores(T, true);
	} else if (v == "slot_ex") {
		expect("hits", it.slot_hits, T * R);
		expect("miss", it.slot_miss, 0);
		expect("oor", it.slot_oor, 0);
	} else if (v == "badtag") { // control: every exit check must miss
		expect("miss", it.slot_miss, T * R);
		expect("hits", it.slot_hits, 0);
		expect("oor", it.slot_oor, 0);
	} else if (v == "slot_cx") { // claimed, checked and released
		expect("hits", it.slot_hits, T * R);
		expect("miss", it.slot_miss, 0);
		expect("oor", it.slot_oor, 0);
		expect("claim_fail", it.claim_fail, 0);
		std::vector<uint64_t> w;
		read_words(c, mem, CLAIM_FIRST_WORD, 2 * TIDW_SLOTS, w);
		uint64_t held = 0;
		for (uint64_t s = 0; s < TIDW_SLOTS; s++)
			held += w[2 * s] != 0;
		expect("slots_still_claimed", held, 0);
	} else if (v == "collide") { // control: lanes 2j, 2j+1 share a slot
		expect("hits+miss", it.slot_hits + it.slot_miss, T * R);
		d += fmt("claim_fail=%" PRIu64 " ", it.claim_fail);
		if (it.claim_fail == 0)
			ok = false;
	} else if (v == "slot_e" || v == "tidw_e") {
		expect("oor", it.slot_oor, 0);
		std::vector<uint64_t> w;
		read_words(c, mem, 0, 2 * TIDW_SLOTS, w);
		uint64_t used = 0, bad = 0;
		for (uint64_t s = 0; s < TIDW_SLOTS; s++) {
			const uint64_t tag = w[2 * s], st = w[2 * s + 1];
			if (tag == 0 && st == 0)
				continue;
			used++;
			const uint64_t tid = tag & 0xffffffffffull;
			if (tag != tag_of(tid) || st != 0 || tid >= T ||
			    (v == "tidw_e" && (tid & (TIDW_SLOTS - 1)) != s))
				bad++;
		}
		expect("bad_slots", bad, 0);
		const uint64_t want_used =
			v == "tidw_e" ? std::min<uint64_t>(T, TIDW_SLOTS) : 0;
		if (v == "tidw_e")
			expect("used_slots", used, want_used);
		else
			d += fmt("used_slots=%" PRIu64 " ", used);
		if (used == 0)
			ok = false;
	} else if (v == "tid_e") {
		expect("oor", it.slot_oor, 0);
		uint64_t bad = 0, n = 0;
		std::vector<uint64_t> w;
		const uint64_t step = std::max<uint64_t>(1, T / 4096);
		for (uint64_t u = 0; u < T; u += step) {
			read_words(c, mem, 2 * u, 2, w);
			n++;
			if (w[0] != tag_of(u) || w[1] != 0)
				bad++;
		}
		expect("sample_mismatch", bad, 0);
		d += fmt("sampled=%" PRIu64 " ", n);
	} else {
		d += "none ";
	}
	it.valid = ok ? "ok" : "FAIL";
	it.valid_detail = d;
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
		    refdir = "build/ref", only = "",
		    shapes_arg = "full,anchor,large";
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
		else if (a == "--only")
			only = val();
		else if (a == "--seed")
			seed = strtoull(val().c_str(), nullptr, 0);
		else if (a == "--kernels")
			kcubin = val();
		else if (a == "--refdir")
			refdir = val();
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

	// step 0's shapes: full occupancy 8 SMs x 1,536 = 24 x 512; the
	// rms_norm shape behind the 0.6 ns figure (512 blocks of 512); the
	// largest llama.cpp launch (4,194,304 threads)
	std::vector<Shape> all_shapes = {
		{ "tiny", 1, 64, 0 },
		{ "full", 24, 512, 0 },
		{ "anchor", 512, 512, 0 },
		{ "large", 8192, 512, 0 },
	};
	std::vector<Shape> shapes;
	for (auto &n : split(shapes_arg))
		for (auto s : all_shapes)
			if (s.name == n) {
				s.rbig = rbig.count(n) ? rbig[n] : 4;
				shapes.push_back(s);
			}

	Ctx c;
	step0_primary_ctx(0);
	STEP0_CU(cuStreamCreate(&c.st, CU_STREAM_NON_BLOCKING));
	const std::string shm_name = fmt("step0b_c3_%d", (int)getpid());
	if (!step0_regmem_create(c.reg, shm_name, BUF_BYTES, false)) {
		fprintf(stderr, "registered memory failed\n");
		return 3;
	}
	STEP0_CU(cuMemAlloc(&c.dbuf, BUF_BYTES));
	STEP0_CU(cuMemAlloc(&c.aux, AUX_BYTES));
	STEP0_CU(cuModuleLoad(&c.mk, kcubin.c_str()));
	for (const char *m : { "F", "N", "X" }) {
		CUmodule mod;
		const std::string p = refdir + "/ref" + m + ".cubin";
		STEP0_CU(cuModuleLoad(&mod, p.c_str()));
		c.mref[m] = mod;
	}
	{
		size_t bytes = 0;
		STEP0_CU(cuModuleGetGlobal(&c.inline_miss, &bytes, c.mref["X"],
					   "__c3_inline_miss"));
		STEP0_CU(cuMemsetD32(c.inline_miss, 0, 2));
	}
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
	// map_info[HOST_FD] -> registered, map_info[DEV_FD] -> device, in
	// every reference module (as step 0's bench2 did for its one module)
	for (auto &[name, mod] : c.mref) {
		CUdeviceptr mi;
		size_t bytes;
		STEP0_CU(cuModuleGetGlobal(&mi, &bytes, mod, "map_info"));
		if (bytes != 1024 * sizeof(MapBasicInfo)) {
			fprintf(stderr, "map_info of %s is %zu bytes\n",
				name.c_str(), bytes);
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
	std::vector<Item> items;
	for (auto &s : shapes) {
		const Shape *sp = nullptr;
		for (auto &x : shapes)
			if (x.name == s.name)
				sp = &x;
		auto add = [&](Item it) {
			it.shape = sp;
			it.id = fmt("%s|%s|%s|%s|fk%d|R%u|k%u", sp->name.c_str(),
				    it.mod.c_str(), it.kase.c_str(),
				    it.mem.c_str(), it.fk, it.R, it.k);
			if (!only.empty() &&
			    !std::regex_search(it.id, std::regex(only)))
				return;
			items.push_back(it);
		};
		auto mk = [&](const std::string &mod, const std::string &kase,
			      const std::string &mem, const std::string &kname,
			      const std::string &vkind, uint32_t R, int fk) {
			Item it;
			it.mod = mod;
			it.kase = kase;
			it.mem = mem;
			it.vkind = vkind;
			it.R = R;
			it.fk = fk;
			it.fn = fn(mod == "k" ? c.mk : c.mref[mod], kname);
			return it;
		};
		// per-module empty kernels (the delta baselines)
		add(mk("k", "empty", "none", "k_empty", "none", 1, 0));
		for (const char *m : { "F", "N", "X" })
			add(mk(m, "empty", "none", "ref_empty", "none", 1, 0));
		std::vector<uint32_t> Rs = { 1u };
		if (s.rbig != 1)
			Rs.push_back(s.rbig);
		for (uint32_t R : Rs) {
			const bool r1 = R == 1;
			const std::string form = r1 ? "_r1" : "_rn";
			// (a) the cost ladder
			add(mk("X", "E", "none", "E" + form, "none", R, 0));
			for (const char *mem : { "reg", "dev" }) {
				const std::string h =
					std::string(mem) == "reg" ? "host" :
								    "dev";
				for (const char *m : { "F", "N" }) {
					add(mk(m, m, mem, "ref_" + h + form,
					       "ref", R, m[0] == 'F' ? 1 : 0));
					if (r1) // the loop form at R = 1
						add(mk(m, std::string(m) + "_loop",
						       mem, "ref_" + h + "_rn",
						       "ref", R,
						       m[0] == 'F' ? 1 : 0));
				}
				for (const char *v : { "I", "IF", "IC", "K" })
					add(mk("X", v, mem,
					       std::string(v) + "_" + h + form,
					       "ref", R,
					       std::string(v) == "IF" ? 1 : 0));
				add(mk("X", "M", mem, "M" + form, "ref", R, 0));
				add(mk("X", "M64", mem, "M64" + form, "ref", R,
				       0));
				// (d) store + fence kind; step 0's kernels for
				// none and membar.sys
				add(mk("k", "st_lane32", mem, "k_st_lane32_f0",
				       "st32", R, 0));
				add(mk("k", "st_lane32", mem, "k_st_lane32_f1",
				       "st32", R, 1));
				for (int fk : { 2, 3, 4 })
					add(mk("k", "st_lane32", mem,
					       fmt("k_st32_fk%d", fk), "st32", R,
					       fk));
				add(mk("k", "st_lane1", mem, "k_st_lane1_f0",
				       "st1", R, 0));
				add(mk("k", "st_lane1", mem, "k_st_lane1_f1",
				       "st1", R, 1));
				add(mk("k", "st_lane1", mem, "k_st1_fk2", "st1",
				       R, 2));
			}
			// (d) the fence alone (no memory: once per shape)
			for (int fk : { 1, 2, 3, 4 }) {
				add(mk("k", "fence_lane32", "none",
				       fmt("k_fal32_fk%d", fk), "none", R, fk));
				add(mk("k", "fence_lane1", "none",
				       fmt("k_fal1_fk%d", fk), "none", R, fk));
			}
			// (b) slot addressing, device memory
			add(mk("k", "slot_e", "dev", "k_slot_e", "slot_e", R, 0));
			add(mk("k", "slot_ex", "dev", "k_slot_ex", "slot_ex", R,
			       0));
			add(mk("k", "slot_excg", "dev", "k_slot_excg", "slot_ex",
			       R, 0));
			add(mk("k", "tid_e", "dev", "k_tid_e", "tid_e", R, 0));
			add(mk("k", "tid_ex", "dev", "k_tid_ex", "slot_ex", R, 0));
			add(mk("k", "tidw_e", "dev", "k_tidw_e", "tidw_e", R, 0));
			add(mk("k", "slot_cx", "dev", "k_slot_cx", "slot_cx", R, 0));
			add(mk("k", "slot_cxr", "dev", "k_slot_cxr", "slot_cx", R,
			       0));
			if (r1) { // control, validated only: claims must collide
				Item b = mk("k", "slot_cx_collide", "dev",
					    "k_slot_cx", "collide", R, 0);
				b.mode_extra = 4;
				b.validate_only = true;
				add(b);
			}
			if (r1) { // control, validated only: every check misses
				Item b = mk("k", "slot_ex_badtag", "dev",
					    "k_slot_ex", "badtag", R, 0);
				b.mode_extra = 2;
				b.validate_only = true;
				add(b);
			}
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
		if (it.validate_only)
			continue;
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
	// counters of the timed launches start from zero here, and the claim
	// table is free again (the collide control leaves slots claimed)
	reset_buffers(c, "none", 0);
	reset_claim_table(c);
	const uint64_t inline_miss0 = read_inline_miss(c);

	// rounds
	std::string csvp = out + "/raw.csv";
	FILE *csv = fopen(csvp.c_str(), "w");
	if (!csv) {
		perror(csvp.c_str());
		return 5;
	}
	fprintf(csv, "round,pos,id,module,case,mem,fk,shape,blocks,tpb,threads,"
		     "R,k,n_launch,batch_ms,per_launch_us,hold_capped\n");
	int done_rounds = 0;
	uint64_t hold_capped_rows = 0;
	std::vector<size_t> timed;
	for (size_t i = 0; i < items.size(); i++)
		if (!items[i].validate_only)
			timed.push_back(i);
	for (int r = 0; r < rounds; r++) {
		if (wall() > max_wall_s) {
			fprintf(stderr, "max wall time reached before round %d\n",
				r);
			break;
		}
		std::vector<size_t> order = timed;
		std::mt19937_64 rng(seed + (uint64_t)r);
		std::shuffle(order.begin(), order.end(), rng);
		for (size_t pos = 0; pos < order.size(); pos++) {
			Item &it = items[order[pos]];
			if (it.cal_ms < 20) {
				launch(c, it, 0); // untimed
			}
			int capped = 0;
			const double ms =
				time_batch(c, it, it.n, e0, e1, &capped);
			hold_capped_rows += capped;
			fprintf(csv,
				"%d,%zu,%s,%s,%s,%s,%d,%s,%u,%u,%" PRIu64
				",%u,%u,%u,%.6f,%.4f,%d\n",
				r, pos, it.id.c_str(), it.mod.c_str(),
				it.kase.c_str(), it.mem.c_str(), it.fk,
				it.shape->name.c_str(), it.shape->blocks,
				it.shape->tpb, it.shape->threads(), it.R, it.k,
				it.n, ms, ms * 1000.0 / it.n, capped);
		}
		fflush(csv);
		done_rounds++;
		fprintf(stderr, "round %d done, %.1f s\n", r, wall());
	}
	fclose(csv);
	// exit tag checks that missed, indices out of range and failed claims
	// during every launch after validation (all expected 0)
	std::vector<uint64_t> auxv(8);
	STEP0_CU(cuStreamSynchronize(c.st));
	STEP0_CU(cuMemcpyDtoH(auxv.data(), c.aux, auxv.size() * 8));
	const uint64_t inline_miss_timed = read_inline_miss(c) - inline_miss0;

	// items.json
	std::string jp = out + "/items.json";
	FILE *j = fopen(jp.c_str(), "w");
	int smc = 0;
	CUdevice d;
	cuDeviceGet(&d, 0);
	cuDeviceGetAttribute(&smc, CU_DEVICE_ATTRIBUTE_MULTIPROCESSOR_COUNT, d);
	fprintf(j, "{\n \"rounds_done\": %d,\n \"rounds_requested\": %d,\n"
		   " \"target_ms\": %g,\n \"seed\": %" PRIu64 ",\n"
		   " \"buf_bytes\": %zu,\n \"sm_count\": %d,\n"
		   " \"wall_s\": %.2f,\n \"validation_failures\": %d,\n"
		   " \"hold_gate\": %s,\n \"hold_capped_rows\": %" PRIu64 ",\n"
		   " \"hold_cap_hits_total\": %" PRIu64 ",\n"
		   " \"timed_slot_misses\": %" PRIu64 ",\n"
		   " \"timed_slot_oor\": %" PRIu64 ",\n"
		   " \"timed_inline_misses\": %" PRIu64 ",\n"
		   " \"timed_claim_failures\": %" PRIu64 ",\n"
		   " \"items\": [\n",
		done_rounds, rounds, target_ms, seed, BUF_BYTES, smc, wall(),
		nfail, c.use_hold ? "true" : "false", hold_capped_rows,
		c.hold_host[1], auxv[AUX_MISS], auxv[AUX_OOR],
		inline_miss_timed, auxv[AUX_CFAIL]);
	for (size_t i = 0; i < items.size(); i++) {
		auto &it = items[i];
		fprintf(j,
			"  {\"id\": %s, \"module\": %s, \"case\": %s, \"mem\": %s, "
			"\"fk\": %d, \"vkind\": %s, \"validate_only\": %s, "
			"\"shape\": %s, \"blocks\": %u, \"tpb\": %u, "
			"\"threads\": %" PRIu64 ", \"warps\": %" PRIu64
			", \"R\": %u, \"k\": %u, \"n\": %u, \"cal_ms\": %.6f, "
			"\"regs\": %d, \"local_bytes\": %d, "
			"\"occ_blocks_per_sm\": %d, \"valid\": %s, "
			"\"valid_detail\": %s, \"slot_miss\": %" PRIu64
			", \"slot_oor\": %" PRIu64 ", \"slot_hits\": %" PRIu64
			", \"claim_fail\": %" PRIu64
			", \"inline_miss\": %" PRIu64 "}%s\n",
			json_str(it.id).c_str(), json_str(it.mod).c_str(),
			json_str(it.kase).c_str(), json_str(it.mem).c_str(),
			it.fk, json_str(it.vkind).c_str(),
			it.validate_only ? "true" : "false",
			json_str(it.shape->name).c_str(), it.shape->blocks,
			it.shape->tpb, it.shape->threads(), it.shape->warps(),
			it.R, it.k, it.n, it.cal_ms, it.regs, it.local_bytes,
			it.occ_blocks, json_str(it.valid).c_str(),
			json_str(it.valid_detail).c_str(), it.slot_miss,
			it.slot_oor, it.slot_hits, it.claim_fail, it.inline_miss,
			i + 1 < items.size() ? "," : "");
	}
	fprintf(j, " ]\n}\n");
	fclose(j);

	STEP0_CU(cuStreamSynchronize(c.st));
	cuModuleUnload(c.mk);
	for (auto &[name, mod] : c.mref)
		cuModuleUnload(mod);
	cuMemFree(c.dbuf);
	cuMemFree(c.aux);
	cuMemFreeHost(c.hold_host);
	step0_regmem_destroy(c.reg);
	fprintf(stderr, "done: %d rounds, %.1f s, out %s\n", done_rounds,
		wall(), out.c_str());
	return nfail ? 6 : 0;
}
