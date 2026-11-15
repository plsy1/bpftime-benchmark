// c1check: part (b) of check C1. Does state the entry probe writes reach the
// exit probe of the same thread, through the kernel-scope carry, in the code
// the dev passes produce?
//
//   c1check <basic|streams|graph|spin> <OUTDIR> [key=value ...]
//
// Driver API only. Loads build/b/b_module.cubin (ptxas -arch=sm_87 of the
// patched module, see tools/build_b.py) and its manifest. Every kernel has
// three maps in map_info: rec (1502: per-thread ground truth and counters, in
// device memory), store (the storage under test; its map type picks the
// variant at run time) and cnt (1503: NULL counters).
//
// Variants (map_info[store].map_type; helper-form kernels):
//   carry    1540  the thread's kernel-scope .local carry       (under test)
//   slot     1543  (smid, warpid, laneid) slot, no tag          (control: may fail)
//   slottag  1542  the same slot with an owner tag              (fallback design)
//   shared   1544  one slot for every thread                    (must fail)
//   lane     1545  a slot per %laneid only                      (must fail)
//   frame    1541  the wrapper's own frame (dangling)           (unsound; reported)
// Direct-form kernels (<name>_d) always use the carry ("carry_direct").
//
// For every trial the program checks, from rec: entries == exits == launches
// x threads, mismatches (exit read != this thread's entry write or != the
// ground truth), NULL storage at entry/exit, and the addresses the exit probe
// used. Output: OUTDIR/trials.csv (one row per trial), plus per case
// OUTDIR/spin_warps.csv, spin_gaps.csv (spin), streams.csv, meta.json.
#include <cuda.h>

#include <algorithm>
#include <chrono>
#include <cinttypes>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <random>
#include <set>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <iterator>
#include <vector>

#define CK(call)                                                                       \
	do {                                                                           \
		CUresult rc_ = (call);                                                 \
		if (rc_ != CUDA_SUCCESS) {                                             \
			const char *s_ = nullptr;                                      \
			cuGetErrorString(rc_, &s_);                                    \
			fprintf(stderr, "%s:%d: %s -> %d (%s)\n", __FILE__, __LINE__, #call, \
				(int)rc_, s_ ? s_ : "?");                              \
			exit(10);                                                      \
		}                                                                      \
	} while (0)

// trampoline/default_trampoline.cu MapBasicInfo (40 B)
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

enum { MT_PERTHREAD = 1502, MT_GPU_ARRAY = 1503, MT_CARRY = 1540, MT_FRAME = 1541,
       MT_SLOTTAG = 1542, MT_SLOT = 1543, MT_SHARED = 1544, MT_LANE = 1545 };
static const int REC_VALUE = 64; // struct rec: 8 x u64
static const int WREC = 10;      // kernels_b.cu

static std::string fmt(const char *f, ...)
{
	char b[2048];
	va_list ap;
	va_start(ap, f);
	vsnprintf(b, sizeof b, f, ap);
	va_end(ap);
	return b;
}

struct Kern {
	std::string name;
	int rec = 0, store = 0, cnt = 0, direct = 0, carry = 0;
	CUfunction fn = nullptr;
	uint64_t max_t = 0;
	CUdeviceptr recbuf = 0, cntbuf = 0;
	int regs = 0, local = 0, occ512 = 0;
};

struct G {
	CUmodule mod = nullptr;
	CUdeviceptr map_info = 0;
	CUdeviceptr slotbuf = 0; // shared by every kernel's store map
	uint64_t slot_entries = 0;
	std::map<std::string, Kern> k;
	std::map<std::string, std::string> opt;
	FILE *trials = nullptr;
	int nsm = 0, max_thr_sm = 0;
} g;

static uint64_t optu(const char *k, uint64_t d)
{
	auto it = g.opt.find(k);
	return it == g.opt.end() ? d : strtoull(it->second.c_str(), nullptr, 0);
}
static std::string opts(const char *k, const std::string &d)
{
	auto it = g.opt.find(k);
	return it == g.opt.end() ? d : it->second;
}

static void set_map(int fd, const MapBasicInfo &m)
{
	CK(cuMemcpyHtoD(g.map_info + (size_t)fd * sizeof m, &m, sizeof m));
}

static int variant_type(const std::string &v)
{
	if (v == "carry" || v == "carry_direct")
		return MT_CARRY;
	if (v == "slot")
		return MT_SLOT;
	if (v == "slottag")
		return MT_SLOTTAG;
	if (v == "shared")
		return MT_SHARED;
	if (v == "lane")
		return MT_LANE;
	if (v == "frame")
		return MT_FRAME;
	fprintf(stderr, "unknown variant %s\n", v.c_str());
	exit(2);
}

// Points the kernel's three maps at its buffers, the store map at the
// variant. The context is synchronized first: no kernel may run while
// map_info changes.
static void configure(Kern &k, const std::string &variant)
{
	CK(cuCtxSynchronize());
	MapBasicInfo m = {};
	m.enabled = true;
	m.key_size = 4;
	m.value_size = REC_VALUE;
	m.max_entries = 2;
	m.map_type = MT_PERTHREAD;
	m.extra_buffer = (void *)(uintptr_t)k.recbuf;
	m.max_thread_count = k.max_t;
	set_map(k.rec, m);
	m = {};
	m.enabled = true;
	m.key_size = 4;
	m.value_size = 8;
	m.max_entries = 8;
	m.map_type = MT_GPU_ARRAY;
	m.extra_buffer = (void *)(uintptr_t)k.cntbuf;
	set_map(k.cnt, m);
	m = {};
	m.enabled = true;
	m.key_size = 4;
	m.map_type = variant_type(variant);
	m.value_size = 16;
	m.extra_buffer = (void *)(uintptr_t)g.slotbuf;
	switch (m.map_type) {
	case MT_CARRY:
		m.max_entries = 1;
		m.extra_buffer = nullptr;
		if (k.carry < 16) {
			fprintf(stderr, "%s: carry %d B < 16\n", k.name.c_str(), k.carry);
			exit(2);
		}
		break;
	case MT_SLOTTAG:
		m.value_size = 24; // tag + 16 B
		m.max_entries = (int)g.slot_entries;
		break;
	case MT_SLOT:
		m.max_entries = (int)g.slot_entries;
		break;
	case MT_SHARED:
		m.max_entries = 1;
		break;
	case MT_LANE:
		m.max_entries = 32;
		break;
	case MT_FRAME:
		m.max_entries = 1;
		m.extra_buffer = nullptr;
		break;
	}
	set_map(k.store, m);
}

static void reset(Kern &k)
{
	CK(cuCtxSynchronize());
	CK(cuMemsetD8(k.recbuf, 0, 2 * k.max_t * REC_VALUE));
	CK(cuMemsetD8(k.cntbuf, 0, 64));
	CK(cuMemsetD8(g.slotbuf, 0, g.slot_entries * 24));
	CK(cuCtxSynchronize());
}

struct Tally {
	uint64_t threads = 0, launches = 0, expect = 0;
	uint64_t entries = 0, entry_null = 0, exits = 0, exit_null = 0, mism = 0;
	uint64_t bad_bits[4] = { 0, 0, 0, 0 }; // last launch, per bit
	uint64_t threads_mism = 0, threads_count_err = 0, rec_null = 0;
	std::set<uint64_t> addrs;
};

// Reads rec of threads [0, T) and sums the counters.
static Tally tally(Kern &k, uint64_t T, uint64_t launches)
{
	CK(cuCtxSynchronize());
	std::vector<uint64_t> r0(T * 8), r1(T * 8), cnt(8);
	CK(cuMemcpyDtoH(r0.data(), k.recbuf, T * REC_VALUE));
	CK(cuMemcpyDtoH(r1.data(), k.recbuf + k.max_t * REC_VALUE, T * REC_VALUE));
	CK(cuMemcpyDtoH(cnt.data(), k.cntbuf, 64));
	Tally t;
	t.threads = T;
	t.launches = launches;
	t.expect = T * launches;
	t.rec_null = cnt[0];
	for (uint64_t i = 0; i < T; i++) {
		const uint64_t *a = &r0[i * 8], *o = &r1[i * 8];
		t.entries += a[2];
		t.entry_null += a[3];
		t.exits += o[4];
		t.mism += o[5];
		t.exit_null += o[6];
		for (int b = 0; b < 4; b++)
			if (o[3] & (1ull << b))
				t.bad_bits[b]++;
		if (o[5])
			t.threads_mism++;
		if (a[2] != launches || o[4] != launches)
			t.threads_count_err++;
		if (t.addrs.size() < 64)
			t.addrs.insert(o[7]);
	}
	return t;
}

static void trial_row(const std::string &cs, const std::string &kname,
		      const std::string &variant, const std::string &shape, unsigned blocks,
		      unsigned tpb, const Tally &t, double ms, const std::string &extra)
{
	std::string addrs;
	int n = 0;
	for (uint64_t a : t.addrs) {
		if (n++ < 3)
			addrs += fmt("%s%" PRIx64, addrs.empty() ? "" : " ", a);
	}
	fprintf(g.trials,
		"%s,%s,%s,%s,%u,%u,%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64
		",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64
		",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%zu,%s,%.3f,%s\n",
		cs.c_str(), kname.c_str(), variant.c_str(), shape.c_str(), blocks, tpb, t.threads,
		t.launches, t.expect, t.entries, t.exits, t.entry_null, t.exit_null, t.mism,
		t.threads_mism, t.threads_count_err, t.bad_bits[0], t.bad_bits[1], t.bad_bits[2],
		t.bad_bits[3], t.rec_null, t.addrs.size(), addrs.c_str(), ms, extra.c_str());
	fflush(g.trials);
	fprintf(stderr,
		"%-7s %-8s %-12s %-7s T=%-7" PRIu64 " L=%-4" PRIu64 " entries=%" PRIu64
		" exits=%" PRIu64 " mism=%" PRIu64 " null=%" PRIu64 "/%" PRIu64 " addrs=%zu %s\n",
		cs.c_str(), kname.c_str(), variant.c_str(), shape.c_str(), t.threads, t.launches,
		t.entries, t.exits, t.mism, t.entry_null, t.exit_null, t.addrs.size(),
		extra.c_str());
}

static double now_ms()
{
	return std::chrono::duration<double, std::milli>(
		       std::chrono::steady_clock::now().time_since_epoch())
		.count();
}

// ------------------------------------------------------------------ inputs
struct Shape {
	std::string name;
	unsigned blocks, tpb;
	uint64_t T() const { return (uint64_t)blocks * tpb; }
};

static std::vector<uint32_t> gen_input(uint64_t words, uint64_t seed)
{
	std::vector<uint32_t> v(words);
	std::mt19937_64 rng(seed);
	for (auto &x : v)
		x = (uint32_t)rng();
	return v;
}

// kmr: in (u32 per thread), kout, spin_ns, caps. kdiv: in, kout, iters,
// nthreads, spin_ns, caps. caps counts spin-cap hits (both kernels).
struct AppBufs {
	std::vector<CUdeviceptr> in; // one per launch
	CUdeviceptr kout = 0, caps = 0;
	uint32_t iters = 0;
	uint64_t spin_ns = 0;
	uint32_t nthreads = 0;
};

static AppBufs make_app(const std::string &fam, const Shape &s, unsigned launches,
			uint64_t seed, uint32_t iters, uint64_t spin_ns)
{
	AppBufs a;
	a.iters = iters;
	a.spin_ns = spin_ns;
	a.nthreads = (uint32_t)s.T();
	const uint64_t words = fam == "kmr" ? s.T() : (uint64_t)(iters + 2) * s.T();
	for (unsigned l = 0; l < launches; l++) {
		CUdeviceptr p;
		CK(cuMemAlloc(&p, words * 4));
		auto v = gen_input(words, seed * 1000003ull + l);
		CK(cuMemcpyHtoD(p, v.data(), words * 4));
		a.in.push_back(p);
	}
	CK(cuMemAlloc(&a.kout, s.T() * 8));
	CK(cuMemAlloc(&a.caps, 64));
	CK(cuMemsetD8(a.caps, 0, 64));
	// cuMemcpyHtoD/cuMemsetD8 run on the legacy stream, which does not order
	// against the non-blocking streams the kernels use: wait for them.
	CK(cuCtxSynchronize());
	return a;
}
static void free_app(AppBufs &a)
{
	for (auto p : a.in)
		cuMemFree(p);
	cuMemFree(a.kout);
	cuMemFree(a.caps);
}

static void launch_app(Kern &k, const std::string &fam, const Shape &s, AppBufs &a,
		       unsigned l, CUstream st)
{
	if (fam == "kmr") {
		void *args[] = { &a.in[l], &a.kout, &a.spin_ns, &a.caps };
		CK(cuLaunchKernel(k.fn, s.blocks, 1, 1, s.tpb, 1, 1, 0, st, args, nullptr));
	} else {
		void *args[] = { &a.in[l], &a.kout, &a.iters, &a.nthreads, &a.spin_ns, &a.caps };
		CK(cuLaunchKernel(k.fn, s.blocks, 1, 1, s.tpb, 1, 1, 0, st, args, nullptr));
	}
}

static std::vector<std::string> split(const std::string &s)
{
	std::vector<std::string> v;
	size_t p = 0;
	while (p <= s.size()) {
		size_t q = s.find(',', p);
		if (q == std::string::npos)
			q = s.size();
		if (q > p)
			v.push_back(s.substr(p, q - p));
		p = q + 1;
	}
	return v;
}

static Kern &K(const std::string &n)
{
	auto it = g.k.find(n);
	if (it == g.k.end()) {
		fprintf(stderr, "kernel %s not in the manifest\n", n.c_str());
		exit(2);
	}
	return it->second;
}

static std::vector<Shape> app_shapes()
{
	std::vector<Shape> all = { { "full512", 24, 512 }, { "w48", 256, 48 },
				   { "anchor", 512, 512 } };
	std::vector<Shape> out;
	for (auto &n : split(opts("shapes", "full512,w48,anchor")))
		for (auto &s : all)
			if (s.name == n)
				out.push_back(s);
	return out;
}

// ------------------------------------------------------------------ basic
// kmr and kdiv (helper form: every variant; direct form: carry), every shape,
// `launches` back-to-back launches with a new input each.
static void case_basic()
{
	const unsigned launches = (unsigned)optu("launches", 3);
	const uint32_t iters = (uint32_t)optu("iters", 16);
	const uint64_t spin_ns = optu("spin_ns", 20000);
	auto variants = split(opts("variants", "carry,slot,slottag,shared,lane,frame"));
	uint64_t seed = optu("seed", 20261115);
	for (const char *fam : { "kmr", "kdiv" }) {
		for (auto &s : app_shapes()) {
			std::vector<std::pair<std::string, std::string>> runs;
			for (auto &v : variants)
				runs.push_back({ fam, v });
			runs.push_back({ std::string(fam) + "_d", "carry_direct" });
			for (auto &[kn, v] : runs) {
				Kern &k = K(kn);
				AppBufs a = make_app(fam, s, launches, seed++, iters, spin_ns);
				configure(k, v);
				reset(k);
				CUstream st;
				CK(cuStreamCreate(&st, CU_STREAM_NON_BLOCKING));
				const double t0 = now_ms();
				for (unsigned l = 0; l < launches; l++)
					launch_app(k, fam, s, a, l, st);
				CK(cuStreamSynchronize(st));
				const double ms = now_ms() - t0;
				uint32_t caps = 0;
				CK(cuMemcpyDtoH(&caps, a.caps, 4));
				Tally t = tally(k, s.T(), launches);
				trial_row("basic", kn, v, s.name, s.blocks, s.tpb, t, ms,
					  fmt("spin_caps=%u", caps));
				CK(cuStreamDestroy(st));
				free_app(a);
			}
		}
	}
}

// ------------------------------------------------------------------ graph
// [kmr, kdiv] captured from a stream into one graph, replayed `replays`
// times per variant (map_info switched between replay batches, context
// synchronized), and [kmr_d, kdiv_d] likewise.
static void case_graph()
{
	const unsigned replays = (unsigned)optu("replays", 1000);
	const uint32_t iters = (uint32_t)optu("iters", 8);
	const Shape s = { "full512", 24, 512 };
	auto variants = split(opts("variants", "carry,slot,slottag,shared"));
	for (int direct = 0; direct < 2; direct++) {
		const std::string sfx = direct ? "_d" : "";
		Kern &km = K("kmr" + sfx), &kd = K("kdiv" + sfx);
		AppBufs am = make_app("kmr", s, 1, 77 + direct, iters, 2000);
		AppBufs ad = make_app("kdiv", s, 1, 99 + direct, iters, 0);
		CUstream st;
		CK(cuStreamCreate(&st, CU_STREAM_NON_BLOCKING));
		CUgraph gr;
		CUgraphExec ge;
		CK(cuStreamBeginCapture(st, CU_STREAM_CAPTURE_MODE_RELAXED));
		launch_app(km, "kmr", s, am, 0, st);
		launch_app(kd, "kdiv", s, ad, 0, st);
		CK(cuStreamEndCapture(st, &gr));
		size_t nn = 0;
		CK(cuGraphGetNodes(gr, nullptr, &nn));
		CK(cuGraphInstantiate(&ge, gr, 0));
		std::vector<std::string> vs = direct ? std::vector<std::string>{ "carry_direct" } :
						       variants;
		for (auto &v : vs) {
			configure(km, v);
			configure(kd, v);
			reset(km);
			reset(kd);
			const double t0 = now_ms();
			for (unsigned r = 0; r < replays; r++)
				CK(cuGraphLaunch(ge, st));
			CK(cuStreamSynchronize(st));
			const double ms = now_ms() - t0;
			Tally tm = tally(km, s.T(), replays), td = tally(kd, s.T(), replays);
			uint32_t cm = 0, cd = 0;
			CK(cuMemcpyDtoH(&cm, am.caps, 4));
			CK(cuMemcpyDtoH(&cd, ad.caps, 4));
			trial_row("graph", km.name, v, s.name, s.blocks, s.tpb, tm, ms,
				  fmt("graph_nodes=%zu spin_caps=%u", nn, cm));
			trial_row("graph", kd.name, v, s.name, s.blocks, s.tpb, td, ms,
				  fmt("graph_nodes=%zu spin_caps=%u", nn, cd));
		}
		CK(cuGraphExecDestroy(ge));
		CK(cuGraphDestroy(gr));
		CK(cuStreamDestroy(st));
		free_app(am);
		free_app(ad);
	}
}

// ------------------------------------------------------------------ spin
struct SpinBufs {
	CUdeviceptr wrec = 0, glog = 0;
	uint64_t warps = 0;
	uint32_t logcap = 0;
};
static SpinBufs make_spin(uint64_t T, uint32_t logcap)
{
	SpinBufs b;
	b.warps = (T + 31) / 32;
	b.logcap = logcap;
	CK(cuMemAlloc(&b.wrec, b.warps * WREC * 8));
	CK(cuMemAlloc(&b.glog, b.warps * logcap * 16));
	CK(cuMemsetD8(b.wrec, 0, b.warps * WREC * 8));
	CK(cuMemsetD8(b.glog, 0, b.warps * logcap * 16));
	CK(cuCtxSynchronize()); // legacy-stream memsets vs non-blocking streams
	return b;
}
static void launch_spin(Kern &k, unsigned blocks, unsigned tpb, SpinBufs &b, uint64_t spin_ns,
			uint64_t gap_ns, uint64_t inj_ns, CUstream st)
{
	void *args[] = { &b.wrec, &b.glog, &spin_ns, &gap_ns, &b.logcap, &inj_ns };
	CK(cuLaunchKernel(k.fn, blocks, 1, 1, tpb, 1, 1, 0, st, args, nullptr));
}

// Writes per-warp records and logged gaps; returns (warp-gaps, max gap ns,
// capped warps, warps whose injected stall was seen as a gap).
struct SpinSum {
	uint64_t warp_gaps = 0, max_gap = 0, capped = 0, inj_seen = 0, warps = 0;
	uint64_t t0min = UINT64_MAX, t1max = 0;
	std::set<uint32_t> sms;
	uint64_t id_changes = 0;
};
static SpinSum dump_spin(const std::string &tag, SpinBufs &b, uint64_t inj_ns, FILE *fw,
			 FILE *fg)
{
	CK(cuCtxSynchronize());
	std::vector<uint64_t> w(b.warps * WREC), gl(b.warps * b.logcap * 2);
	CK(cuMemcpyDtoH(w.data(), b.wrec, w.size() * 8));
	CK(cuMemcpyDtoH(gl.data(), b.glog, gl.size() * 8));
	SpinSum s;
	s.warps = b.warps;
	for (uint64_t i = 0; i < b.warps; i++) {
		const uint64_t *r = &w[i * WREC];
		fprintf(fw, "%s,%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64
			    ",%" PRIu64 ",%u,%u,%u,%u,%" PRIu64 ",%" PRIu64 "\n",
			tag.c_str(), i, r[0], r[1], r[2], r[3], r[4], r[5], (unsigned)(r[6] >> 32),
			(unsigned)(r[6] & 0xffffffff), (unsigned)(r[7] >> 32),
			(unsigned)(r[7] & 0xffffffff), r[8], r[9]);
		s.warp_gaps += r[2];
		s.max_gap = std::max(s.max_gap, r[3]);
		s.capped += r[5];
		s.t0min = std::min(s.t0min, r[0]);
		s.t1max = std::max(s.t1max, r[1]);
		s.sms.insert((unsigned)(r[6] >> 32));
		if (r[6] != r[7])
			s.id_changes++;
		const uint64_t n = std::min<uint64_t>(r[2], b.logcap);
		bool inj = false;
		for (uint64_t j = 0; j < n; j++) {
			const uint64_t p = gl[(i * b.logcap + j) * 2], q = gl[(i * b.logcap + j) * 2 + 1];
			fprintf(fg, "%s,%" PRIu64 ",%" PRIu64 ",%" PRIu64 "\n", tag.c_str(), i, p, q);
			if (inj_ns && r[8] && p <= r[8] + 1 && q >= r[9] && q - p >= inj_ns)
				inj = true;
		}
		if (inj)
			s.inj_seen++;
	}
	fflush(fw);
	fflush(fg);
	return s;
}

static unsigned spin_blocks(Kern &k, unsigned tpb)
{
	int occ = 0;
	CK(cuOccupancyMaxActiveBlocksPerMultiprocessor(&occ, k.fn, tpb, 0));
	return (unsigned)occ * g.nsm;
}

// kspin (helper form: listed variants) and kspin_d: `reps` launches each of
// spin_ms, sized to the kernel's own full residency, one at a time.
static void case_spin(const std::string &out)
{
	const unsigned reps = (unsigned)optu("reps", 2);
	const uint64_t spin_ns = optu("spin_ms", 300) * 1000000ull;
	const uint64_t gap_ns = optu("gap_us", 50) * 1000ull;
	const uint64_t inj_ns = optu("inj_us", 200) * 1000ull;
	const uint32_t logcap = (uint32_t)optu("logcap", 256);
	const unsigned tpb = 512;
	auto variants = split(opts("variants", "carry,slot,slottag,shared"));
	FILE *fw = fopen((out + "/spin_warps.csv").c_str(), "w");
	FILE *fg = fopen((out + "/spin_gaps.csv").c_str(), "w");
	FILE *fs = fopen((out + "/spin_launches.csv").c_str(), "w");
	fputs("tag,warp,t0,t1,gaps,max_gap,iters,capped,sm0,w0,sm1,w1,inj0,inj1\n", fw);
	fputs("tag,warp,t_prev,t_now\n", fg);
	fputs("tag,kernel,variant,rep,blocks,tpb,warps,warp_gaps,max_gap_ns,capped_warps,"
	      "inj_seen_warps,id_changes,t0min,t1max,host_start_ms,host_end_ms\n",
	      fs);
	std::vector<std::pair<std::string, std::string>> runs;
	for (auto &v : variants)
		runs.push_back({ "kspin", v });
	runs.push_back({ "kspin_d", "carry_direct" });
	const double h0 = now_ms();
	for (auto &[kn, v] : runs) {
		Kern &k = K(kn);
		const unsigned blocks = spin_blocks(k, tpb);
		const uint64_t T = (uint64_t)blocks * tpb;
		if (T > k.max_t) {
			fprintf(stderr, "%s: %" PRIu64 " threads > map capacity\n", kn.c_str(), T);
			exit(2);
		}
		configure(k, v);
		reset(k);
		CUstream st;
		CK(cuStreamCreate(&st, CU_STREAM_NON_BLOCKING));
		for (unsigned r = 0; r < reps; r++) {
			SpinBufs b = make_spin(T, logcap);
			const double hs = now_ms() - h0;
			launch_spin(k, blocks, tpb, b, spin_ns, gap_ns, inj_ns, st);
			CK(cuStreamSynchronize(st));
			const double he = now_ms() - h0;
			const std::string tag = fmt("%s/%s/%u", kn.c_str(), v.c_str(), r);
			SpinSum s = dump_spin(tag, b, inj_ns, fw, fg);
			fprintf(fs,
				"%s,%s,%s,%u,%u,%u,%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64
				",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%" PRIu64 ",%.1f,%.1f\n",
				tag.c_str(), kn.c_str(), v.c_str(), r, blocks, tpb, s.warps, s.warp_gaps,
				s.max_gap, s.capped, s.inj_seen, s.id_changes, s.t0min, s.t1max, hs, he);
			fflush(fs);
			cuMemFree(b.wrec);
			cuMemFree(b.glog);
		}
		Tally t = tally(k, T, reps);
		trial_row("spin", kn, v, fmt("occ%u", blocks / g.nsm), blocks, tpb, t, 0,
			  fmt("spin_ms=%" PRIu64, spin_ns / 1000000ull));
		CK(cuStreamDestroy(st));
	}
	fclose(fw);
	fclose(fg);
	fclose(fs);
}

// ------------------------------------------------------------------ streams
// ks2a on one stream and ks2b on another, each with half of its own resident
// capacity, launched back to back so they run together; both kernels' store
// maps share the one slot array.
static void case_streams(const std::string &out)
{
	const uint64_t spin_ns = optu("spin_ms", 30) * 1000000ull;
	const uint64_t gap_ns = optu("gap_us", 50) * 1000ull;
	const unsigned reps = (unsigned)optu("reps", 3);
	const unsigned tpb = 512;
	auto variants = split(opts("variants", "carry,slot,slottag,shared"));
	FILE *fs = fopen((out + "/streams.csv").c_str(), "w");
	fputs("variant,rep,kernel_a,kernel_b,blocks_a,blocks_b,overlap_ns,sms_a,sms_b,"
	      "sms_both,capped\n",
	      fs);
	std::vector<std::string> vs = variants;
	vs.push_back("carry_direct");
	for (auto &v : vs) {
		const bool d = v == "carry_direct";
		Kern &ka = K(d ? "ks2a_d" : "ks2a"), &kb = K(d ? "ks2b_d" : "ks2b");
		const unsigned ba = std::max(1u, spin_blocks(ka, tpb) / 2),
			       bb = std::max(1u, spin_blocks(kb, tpb) / 2);
		configure(ka, v);
		configure(kb, v);
		reset(ka);
		reset(kb);
		CUstream sa, sb;
		CK(cuStreamCreate(&sa, CU_STREAM_NON_BLOCKING));
		CK(cuStreamCreate(&sb, CU_STREAM_NON_BLOCKING));
		FILE *nul = fopen("/dev/null", "w");
		for (unsigned r = 0; r < reps; r++) {
			SpinBufs xa = make_spin((uint64_t)ba * tpb, 4), xb = make_spin((uint64_t)bb * tpb, 4);
			launch_spin(ka, ba, tpb, xa, spin_ns, gap_ns, 0, sa);
			launch_spin(kb, bb, tpb, xb, spin_ns, gap_ns, 0, sb);
			CK(cuCtxSynchronize());
			SpinSum a = dump_spin("a", xa, 0, nul, nul), b = dump_spin("b", xb, 0, nul, nul);
			const int64_t ov = (int64_t)std::min(a.t1max, b.t1max) -
					   (int64_t)std::max(a.t0min, b.t0min);
			std::vector<uint32_t> both;
			std::set_intersection(a.sms.begin(), a.sms.end(), b.sms.begin(), b.sms.end(),
					      std::back_inserter(both));
			fprintf(fs, "%s,%u,%s,%s,%u,%u,%" PRId64 ",%zu,%zu,%zu,%" PRIu64 "\n", v.c_str(), r,
				ka.name.c_str(), kb.name.c_str(), ba, bb, ov, a.sms.size(), b.sms.size(),
				both.size(), a.capped + b.capped);
			fflush(fs);
			cuMemFree(xa.wrec);
			cuMemFree(xa.glog);
			cuMemFree(xb.wrec);
			cuMemFree(xb.glog);
		}
		fclose(nul);
		Tally ta = tally(ka, (uint64_t)ba * tpb, reps), tb = tally(kb, (uint64_t)bb * tpb, reps);
		trial_row("streams", ka.name, v, "half", ba, tpb, ta, 0, "");
		trial_row("streams", kb.name, v, "half", bb, tpb, tb, 0, "");
		CK(cuStreamDestroy(sa));
		CK(cuStreamDestroy(sb));
	}
	fclose(fs);
}

int main(int argc, char **argv)
{
	if (argc < 3) {
		fprintf(stderr, "usage: %s <basic|streams|graph|spin> <OUTDIR> [key=value ...]\n",
			argv[0]);
		return 2;
	}
	const std::string cs = argv[1], out = argv[2];
	for (int i = 3; i < argc; i++) {
		std::string a = argv[i];
		auto eq = a.find('=');
		if (eq == std::string::npos) {
			fprintf(stderr, "bad option %s\n", a.c_str());
			return 2;
		}
		g.opt[a.substr(0, eq)] = a.substr(eq + 1);
	}
	const std::string D = opts("dir", ".");
	const std::string cubin = opts("cubin", D + "/build/b/b_module.cubin");
	const std::string manifest = opts("manifest", D + "/build/b/b_manifest.txt");
	CK(cuInit(0));
	CUdevice dev;
	CK(cuDeviceGet(&dev, 0));
	CUcontext ctx;
	CK(cuDevicePrimaryCtxRetain(&ctx, dev));
	CK(cuCtxSetCurrent(ctx));
	CK(cuDeviceGetAttribute(&g.nsm, CU_DEVICE_ATTRIBUTE_MULTIPROCESSOR_COUNT, dev));
	CK(cuDeviceGetAttribute(&g.max_thr_sm, CU_DEVICE_ATTRIBUTE_MAX_THREADS_PER_MULTIPROCESSOR,
				dev));
	int preempt = 0;
	CK(cuDeviceGetAttribute(&preempt, CU_DEVICE_ATTRIBUTE_COMPUTE_PREEMPTION_SUPPORTED, dev));
	// (SM, warp slot, lane) slots: SMs x warps per SM x 32 (= 8 x 48 x 32)
	g.slot_entries = (uint64_t)g.nsm * (g.max_thr_sm / 32) * 32;
	CK(cuModuleLoad(&g.mod, cubin.c_str()));
	size_t mib = 0;
	CK(cuModuleGetGlobal(&g.map_info, &mib, g.mod, "map_info"));
	if (mib != 1024 * sizeof(MapBasicInfo)) {
		fprintf(stderr, "map_info is %zu bytes\n", mib);
		return 4;
	}
	CK(cuMemsetD8(g.map_info, 0, mib));
	CK(cuMemAlloc(&g.slotbuf, g.slot_entries * 24));
	FILE *mf = fopen(manifest.c_str(), "r");
	if (!mf) {
		perror(manifest.c_str());
		return 3;
	}
	char name[256];
	Kern k;
	while (fscanf(mf, "%255s %d %d %d %d %d", name, &k.rec, &k.store, &k.cnt, &k.direct,
		      &k.carry) == 6) {
		k.name = name;
		CK(cuModuleGetFunction(&k.fn, g.mod, name));
		cuFuncGetAttribute(&k.regs, CU_FUNC_ATTRIBUTE_NUM_REGS, k.fn);
		cuFuncGetAttribute(&k.local, CU_FUNC_ATTRIBUTE_LOCAL_SIZE_BYTES, k.fn);
		CK(cuOccupancyMaxActiveBlocksPerMultiprocessor(&k.occ512, k.fn, 512, 0));
		const bool big = k.name.rfind("kmr", 0) == 0 || k.name.rfind("kdiv", 0) == 0;
		k.max_t = big ? 512ull * 512 : (uint64_t)g.nsm * g.max_thr_sm;
		CK(cuMemAlloc(&k.recbuf, 2 * k.max_t * REC_VALUE));
		CK(cuMemAlloc(&k.cntbuf, 64));
		g.k[k.name] = k;
	}
	fclose(mf);
	for (size_t i = 1; i <= out.size(); i++) // mkdir -p
		if (i == out.size() || out[i] == '/')
			mkdir(out.substr(0, i).c_str(), 0755);
	{
		FILE *m = fopen((out + "/meta.json").c_str(), "w");
		if (!m) {
			perror((out + "/meta.json").c_str());
			return 3;
		}
		fprintf(m, "{\n \"case\": \"%s\",\n \"pid\": %d,\n \"sm_count\": %d,\n"
			   " \"max_threads_per_sm\": %d,\n \"compute_preemption_supported\": %d,\n"
			   " \"slot_entries\": %" PRIu64 ",\n \"cubin\": \"%s\",\n \"kernels\": [\n",
			cs.c_str(), (int)getpid(), g.nsm, g.max_thr_sm, preempt, g.slot_entries,
			cubin.c_str());
		size_t i = 0;
		for (auto &[n, x] : g.k)
			fprintf(m,
				"  {\"name\": \"%s\", \"regs\": %d, \"local_bytes\": %d, "
				"\"occ_blocks_512\": %d, \"direct\": %d, \"carry\": %d}%s\n",
				n.c_str(), x.regs, x.local, x.occ512, x.direct, x.carry,
				++i < g.k.size() ? "," : "");
		fprintf(m, " ],\n \"options\": {");
		i = 0;
		for (auto &[a, b] : g.opt)
			fprintf(m, "%s\"%s\": \"%s\"", i++ ? ", " : "", a.c_str(), b.c_str());
		fprintf(m, "}\n}\n");
		fclose(m);
	}
	g.trials = fopen((out + "/trials.csv").c_str(), "w");
	fputs("case,kernel,variant,shape,blocks,tpb,threads,launches,expect,entries,exits,"
	      "entry_null,exit_null,mismatches,threads_mism,threads_count_err,bad_id,bad_truth,"
	      "bad_truth_self,bad_time,rec_null,addr_distinct,addr_sample,host_ms,extra\n",
	      g.trials);
	const double t0 = now_ms();
	if (cs == "basic")
		case_basic();
	else if (cs == "graph")
		case_graph();
	else if (cs == "spin")
		case_spin(out);
	else if (cs == "streams")
		case_streams(out);
	else {
		fprintf(stderr, "unknown case %s\n", cs.c_str());
		return 2;
	}
	fclose(g.trials);
	fprintf(stderr, "%s done in %.1f s\n", cs.c_str(), (now_ms() - t0) / 1000);
	return 0;
}
