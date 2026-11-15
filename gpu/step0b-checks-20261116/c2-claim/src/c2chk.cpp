// c2chk: correctness runs of check C2 (step 0b). Driver API only.
//   c2chk --module build/kernels.cubin|build/kernels_jit.ptx --out DIR [--path native|jit]
//         [--cases LIST] [--pools LIST] [--exits LIST] [--factors LIST] [--kases LIST]
//         [--min-claims N] [--ctl-min-claims N] [--aba-min-claims N] [--reuse-min N]
//         [--max-launches N] [--seed N] [--aba ROUNDS,J,NS] [--mask-launches N]
//         [--hold-ms X] [--hold-launches N] [--graph-replays N] [--stream-launches N]
//         [--full-reps N]
// A module path ending in .ptx is loaded with cuModuleLoadData (driver JIT).
// cases: div (early,lowlanes,nested,loops,split,entrydiv,churn), aba, full, streams, graph, hold,
//        mask (the __activemask() reproducer)
// exits (div): o_lane r_lane oc_lane o_agg o_first oc_first; full/streams/graph/hold run o_lane,
// r_lane and oc_lane. oc_* (constructed mask; oc_first = its matched first-exit control) only
// run where the claim is the kernel's first statement (not split, entrydiv, churn). Kernels
// missing from the module are skipped.
// Writes DIR/results.jsonl (one line per run) and prints a line per run.
#include "c2host.h"

#include <algorithm>
#include <chrono>
#include <map>
#include <sstream>
#include <unistd.h>

static std::vector<std::string> split(const std::string &s)
{
	std::vector<std::string> v;
	std::stringstream ss(s);
	std::string t;
	while (std::getline(ss, t, ','))
		if (!t.empty())
			v.push_back(t);
	return v;
}
static uint64_t splitmix(uint64_t &x)
{
	uint64_t z = (x += 0x9e3779b97f4a7c15ull);
	z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
	z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
	return z ^ (z >> 31);
}
static double now_s()
{
	return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

struct G {
	CUmodule mod = nullptr;
	C2Pool pool;
	CUstream st = nullptr, st2 = nullptr;
	CUdeviceptr in = 0, tl = 0, arrive = 0;
	uint32_t nin = 1u << 22;
	FILE *out = nullptr;
	std::string path = "native";
	uint64_t rng = 20261115;
	int nfail_run = 0;
};
static G g;
static uint32_t g_aba_rounds = 64, g_aba_J = 1, g_aba_ns = 5000;
static uint64_t g_reuse_min = 100; // claim target >= g_reuse_min x pool slots (non-control div runs)

static bool get_fn(const std::string &name, CUfunction *f)
{
	return cuModuleGetFunction(f, g.mod, name.c_str()) == CUDA_SUCCESS;
}
static int occ_blocks(CUfunction f, int bs)
{
	int occ = 0;
	C2_CU(cuOccupancyMaxActiveBlocksPerMultiprocessor(&occ, f, bs, 0));
	return occ;
}
static XArgs base_args()
{
	XArgs a{};
	a.in = g.in;
	a.nin = g.nin;
	a.check_every = 1;
	return a;
}
static void launch(CUfunction f, unsigned blocks, unsigned bs, XArgs a, CUstream s)
{
	void *args[] = { &a };
	C2_CU(cuLaunchKernel(f, blocks, 1, 1, bs, 1, 1, 0, s, args, nullptr));
}

static const char *kase_name(uint32_t k)
{
	static const char *n[] = { "early",  "lowlanes", "nested",   "loops",
				   "split",  "churn",    "entrydiv", "churnsync" };
	return k < 8 ? n[k] : "?";
}

// one JSON line per run
static void record(const std::string &kase, const std::string &kernel, int kind, const std::string &exit_s,
		   double factor, uint64_t launches, unsigned blocks, unsigned bs, const std::string &extra,
		   const std::vector<uint64_t> &c, const C2Pool::End &e, double wall)
{
	const uint64_t viol = c[CT_DBL_CLAIM] + c[CT_FOREIGN] + c[CT_FREED_LIVE] + c[CT_LANETAG_BAD] +
			      c[CT_BAD_REL] + c[CT_BAD_CLEAR] + c[CT_TAG_BAD];
	fprintf(g.out,
		"{\"case\": \"%s\", \"kernel\": \"%s\", \"path\": \"%s\", \"pool\": \"%s\", \"exit\": \"%s\", "
		"\"factor\": %g, \"nslots\": %u, \"launches_issued\": %" PRIu64 ", \"blocks\": %u, \"bs\": %u, "
		"\"violations\": %" PRIu64 ", \"wall_s\": %.3f, \"extra\": {%s}, \"ctr\": %s, \"end\": %s}\n",
		kase.c_str(), kernel.c_str(), g.path.c_str(), c2_pool_name(kind), exit_s.c_str(), factor,
		g.pool.pd.nslots, launches, blocks, bs, viol, wall, extra.c_str(), c2_json_counters(c).c_str(),
		c2_json_end(e).c_str());
	fflush(g.out);
	printf("%-8s %-24s %-6s f=%-4g launches=%-5" PRIu64 " claims=%-9" PRIu64 " misses=%-8" PRIu64
	       " split=%-8" PRIu64 " viol=%-8" PRIu64 " caps=%" PRIu64 "/%" PRIu64 "/%" PRIu64
	       " end=%s %.2fs\n",
	       kase.c_str(), kernel.c_str(), g.path.c_str(), factor, launches, c[CT_CLAIMS], c[CT_MISSES],
	       c[CT_SPLIT_EXIT], viol, c[CT_CAP_CLAIM], c[CT_CAP_PUSH], c[CT_CAP_SPIN],
	       e.clean ? "clean" : e.detail.c_str(), wall);
	fflush(stdout);
}

// ---- div: divergent exits ------------------------------------------------------------------------
static void run_div(int kind, const std::string &exit_s, uint32_t kase, double factor, uint64_t min_claims,
		    int max_launches, uint32_t aba_ns, const std::string &case_label)
{
	const std::string kname = c2fmt("x_div_%s_%s", c2_pool_name(kind), exit_s.c_str());
	CUfunction f;
	if (!get_fn(kname, &f))
		return;
	if (exit_s.rfind("oc", 0) == 0 && (kase == K_SPLIT || kase == K_CHURN || kase == K_ENTRYDIV))
		return; // the constructed mask needs every lane of the warp at the claim
	const unsigned bs = 128;
	const unsigned resident_blocks = (unsigned)(occ_blocks(f, bs) * g.pool.dev.sms);
	const unsigned blocks = 4 * resident_blocks; // grid >> resident
	g.pool.setup(kind, factor, aba_ns);
	const bool control = exit_s.size() > 6 && exit_s.compare(exit_s.size() - 6, 6, "_first") == 0;
	if (!control && case_label.empty()) // every slot reused >= 100 times on average
		min_claims = std::max<uint64_t>(min_claims, g_reuse_min * g.pool.pd.nslots);
	XArgs a = base_args();
	a.kase = kase;
	a.nthreads = blocks * bs;
	a.iters = (kase == K_LOOPS) ? 32 : 64;
	a.rounds = 16;
	a.J = 8;
	if (aba_ns) { // ABA case: short rounds so a popped slot comes back while a pop sleeps
		a.rounds = g_aba_rounds;
		a.J = g_aba_J;
	}
	const double t0 = now_s();
	uint64_t launches = 0;
	// one launch, then as many as the claim target needs (at most max_launches), in chunks
	while ((int)launches < max_launches) {
		const uint64_t have = launches ? g.pool.counters()[CT_CLAIMS] : 0;
		if (launches && have >= min_claims)
			break;
		uint64_t want = 1;
		if (launches) {
			const uint64_t per = std::max<uint64_t>(1, have / launches);
			want = std::min<uint64_t>(10, (min_claims - have + per - 1) / per);
		}
		for (uint64_t i = 0; i < want && (int)launches < max_launches; i++, launches++) {
			a.off = (uint32_t)splitmix(g.rng);
			launch(f, blocks, bs, a, g.st);
		}
		C2_CU(cuStreamSynchronize(g.st));
	}
	const auto c = g.pool.counters();
	const auto e = g.pool.end_state(!control, true);
	record(case_label.empty() ? kase_name(kase) : case_label, kname, kind, exit_s, factor, launches,
	       blocks, bs,
	       c2fmt("\"kase\": \"%s\", \"resident_blocks\": %u, \"aba_ns\": %u, \"occ_blocks_per_sm\": %d",
		     kase_name(kase), resident_blocks, aba_ns, occ_blocks(f, bs)),
	       c, e, now_s() - t0);
}

// ---- full: full occupancy with a capped all-warp barrier ------------------------------------------
static void run_full(int kind, const std::string &exit_s, int reps, bool with_oversub)
{
	const std::string kname = c2fmt("x_full_%s_%s", c2_pool_name(kind), exit_s.c_str());
	CUfunction f;
	if (!get_fn(kname, &f))
		return;
	const unsigned sizes[] = { 32, 48, 64, 96, 128, 256, 512, 1024 };
	struct L {
		unsigned bs, blocks;
		bool oversub;
	};
	std::vector<L> ls;
	for (int r = 0; r < reps; r++)
		for (unsigned bs : sizes)
			ls.push_back({ bs, (unsigned)(occ_blocks(f, bs) * g.pool.dev.sms), false });
	if (with_oversub) // control: twice the resident capacity; the barrier must hit its cap
		ls.push_back({ 128, (unsigned)(2 * occ_blocks(f, 128) * g.pool.dev.sms), true });
	for (auto &l : ls) {
		g.pool.setup(kind, 1.0);
		C2_CU(cuMemsetD32(g.arrive, 0, 1));
		XArgs a = base_args();
		const unsigned wpb = (l.bs + 31) / 32;
		a.total_warps = l.blocks * wpb;
		a.bar_cap_iters = 1u << 22;
		a.bar_cap_ns = l.oversub ? 50000000ull : 2000000000ull; // 50 ms / 2 s
		a.arrive = g.arrive;
		const double t0 = now_s();
		launch(f, l.blocks, l.bs, a, g.st);
		C2_CU(cuStreamSynchronize(g.st));
		const auto c = g.pool.counters();
		const auto e = g.pool.end_state(true, true);
		const int occ = occ_blocks(f, l.bs);
		record(l.oversub ? "full_oversub" : "full", kname, kind, exit_s, 1.0, 1, l.blocks, l.bs,
		       c2fmt("\"total_warps\": %u, \"occ_blocks_per_sm\": %d, \"resident_warps_per_sm\": %u, "
			     "\"oversub\": %s",
			     a.total_warps, occ, (unsigned)occ * wpb, l.oversub ? "true" : "false"),
		       c, e, now_s() - t0);
	}
}

// ---- streams: concurrent kernels on two streams sharing one pool ----------------------------------
static void run_streams(int kind, const std::string &exit_s, int per_stream)
{
	const std::string kname = c2fmt("x_div_%s_%s", c2_pool_name(kind), exit_s.c_str());
	CUfunction f;
	if (!get_fn(kname, &f))
		return;
	const unsigned bs = 128;
	const unsigned blocks = 2 * (unsigned)(occ_blocks(f, bs) * g.pool.dev.sms);
	g.pool.setup(kind, 1.0);
	const int nl = 2 * per_stream;
	std::vector<uint64_t> tl(2 * (size_t)nl);
	for (int i = 0; i < nl; i++) {
		tl[2 * i] = ~0ull;
		tl[2 * i + 1] = 0;
	}
	C2_CU(cuMemcpyHtoD(g.tl, tl.data(), tl.size() * 8));
	const uint32_t kases[3] = { K_EARLY, K_NESTED, K_LOWLANES };
	const double t0 = now_s();
	for (int i = 0; i < per_stream; i++)
		for (int s = 0; s < 2; s++) {
			XArgs a = base_args();
			a.kase = kases[(i + s) % 3];
			a.nthreads = blocks * bs;
			a.iters = 64;
			a.off = (uint32_t)splitmix(g.rng);
			a.tl = g.tl;
			a.launch_id = (uint32_t)(2 * i + s);
			launch(f, blocks, bs, a, s ? g.st2 : g.st);
		}
	C2_CU(cuStreamSynchronize(g.st));
	C2_CU(cuStreamSynchronize(g.st2));
	const double wall = now_s() - t0;
	C2_CU(cuMemcpyDtoH(tl.data(), g.tl, tl.size() * 8));
	// pairs (stream 0 launch, stream 1 launch) whose [first entry, last exit] intervals overlap
	uint64_t overlap_pairs = 0, overlap_ns = 0, missing = 0;
	for (int i = 0; i < per_stream; i++) {
		const uint64_t a0 = tl[4 * i], a1 = tl[4 * i + 1];
		if (a0 == ~0ull || a1 == 0)
			missing++;
		for (int j = 0; j < per_stream; j++) {
			const uint64_t b0 = tl[4 * j + 2], b1 = tl[4 * j + 3];
			const uint64_t lo = std::max(a0, b0), hi = std::min(a1, b1);
			if (hi > lo) {
				overlap_pairs++;
				overlap_ns += hi - lo;
			}
		}
	}
	const auto c = g.pool.counters();
	const auto e = g.pool.end_state(true, true);
	record("streams", kname, kind, exit_s, 1.0, (uint64_t)nl, blocks, bs,
	       c2fmt("\"overlap_pairs\": %" PRIu64 ", \"overlap_ms\": %.3f, \"launches_without_times\": %" PRIu64,
		     overlap_pairs, overlap_ns / 1e6, missing),
	       c, e, wall);
}

// ---- graph: a captured 3-kernel graph replayed many times ------------------------------------------
static void run_graph(int kind, const std::string &exit_s, int replays)
{
	const std::string kname = c2fmt("x_div_%s_%s", c2_pool_name(kind), exit_s.c_str());
	CUfunction f;
	if (!get_fn(kname, &f))
		return;
	const unsigned bs = 128;
	const unsigned blocks = 2 * (unsigned)(occ_blocks(f, bs) * g.pool.dev.sms);
	g.pool.setup(kind, 1.0);
	C2_CU(cuStreamBeginCapture(g.st, CU_STREAM_CAPTURE_MODE_THREAD_LOCAL));
	const uint32_t kases[3] = { K_EARLY, K_LOWLANES, K_NESTED };
	for (int n = 0; n < 3; n++) {
		XArgs a = base_args();
		a.kase = kases[n];
		a.nthreads = blocks * bs;
		a.iters = 64;
		a.off = (uint32_t)splitmix(g.rng);
		launch(f, blocks, bs, a, g.st);
	}
	CUgraph graph;
	CUgraphExec exec;
	C2_CU(cuStreamEndCapture(g.st, &graph));
	C2_CU(cuGraphInstantiate(&exec, graph, 0));
	const double t0 = now_s();
	for (int r = 0; r < replays; r++)
		C2_CU(cuGraphLaunch(exec, g.st));
	C2_CU(cuStreamSynchronize(g.st));
	const double wall = now_s() - t0;
	const auto c = g.pool.counters();
	const auto e = g.pool.end_state(true, true);
	record("graph", kname, kind, exit_s, 1.0, (uint64_t)3 * replays, blocks, bs,
	       c2fmt("\"replays\": %d, \"nodes\": 3, \"expected_launches\": %d", replays, 3 * replays), c, e,
	       wall);
	C2_CU(cuGraphExecDestroy(exec));
	C2_CU(cuGraphDestroy(graph));
}

// ---- hold: warps hold their slots for milliseconds (run next to the hog for time-slicing) ----------
static void run_hold(int kind, const std::string &exit_s, int nlaunch, double hold_ms)
{
	const std::string kname = c2fmt("x_hold_%s_%s", c2_pool_name(kind), exit_s.c_str());
	CUfunction f;
	if (!get_fn(kname, &f))
		return;
	const unsigned bs = 128;
	const unsigned blocks = 2 * (unsigned)(occ_blocks(f, bs) * g.pool.dev.sms); // two waves
	g.pool.setup(kind, 1.0);
	XArgs a = base_args();
	a.hold_ns = (uint64_t)(hold_ms * 1e6);
	a.hold_step_ns = a.hold_ns / 4;
	a.hold_jit_ns = a.hold_ns;
	a.sleep_ns = 2000;
	a.gap_ns = 200000; // 200 us
	a.check_every = 16;
	const double t0 = now_s();
	for (int i = 0; i < nlaunch; i++)
		launch(f, blocks, bs, a, g.st);
	C2_CU(cuStreamSynchronize(g.st));
	const double wall = now_s() - t0;
	const auto c = g.pool.counters();
	const auto e = g.pool.end_state(true, true);
	record("hold", kname, kind, exit_s, 1.0, (uint64_t)nlaunch, blocks, bs,
	       c2fmt("\"hold_ms\": %.3f, \"gap_us\": 200, \"warps_per_launch\": %u", hold_ms, blocks * bs / 32),
	       c, e, wall);
}

// ---- mask: the __activemask() reproducer (churn structure, helper in place of the claim) ----------
static void run_mask(const std::string &shape, int nlaunch)
{
	const std::string kname = "x_mask_" + shape;
	CUfunction f;
	if (!get_fn(kname, &f))
		return;
	const unsigned bs = 128;
	const unsigned blocks = 4 * (unsigned)(occ_blocks(f, bs) * g.pool.dev.sms);
	g.pool.setup(C2_BMSM, 1.0); // only for the counters
	XArgs a = base_args();
	a.nthreads = blocks * bs;
	a.rounds = 16;
	a.J = 8;
	const double t0 = now_s();
	for (int i = 0; i < nlaunch; i++) {
		a.off = (uint32_t)splitmix(g.rng);
		launch(f, blocks, bs, a, g.st);
	}
	C2_CU(cuStreamSynchronize(g.st));
	const auto c = g.pool.counters();
	C2Pool::End e; // no pool involved
	record("mask", kname, -1, shape, 1.0, (uint64_t)nlaunch, blocks, bs,
	       c2fmt("\"shape\": \"%s\"", shape.c_str()), c, e, now_s() - t0);
}

int main(int argc, char **argv)
{
	std::string module = "build/kernels.cubin", outdir = "out";
	std::string cases = "mask,div,aba,full,streams,graph",
		    pools = "bmsm,bmglob,stksm,stkglob,lnsm,lnglob,lfsm,lfglob",
		    exits = "o_lane,r_lane,oc_lane,o_agg,o_first,oc_first", factors = "0.5,1", kases = "";
	uint64_t min_claims = 100000, ctl_min_claims = 20000, aba_min_claims = 1000000;
	int mask_launches = 20;
	int max_launches = 600, hold_launches = 3, graph_replays = 200, stream_launches = 30, full_reps = 3;
	double hold_ms = 8;
	for (int i = 1; i < argc; i++) {
		std::string k = argv[i];
		auto val = [&]() -> std::string {
			if (i + 1 >= argc) {
				fprintf(stderr, "missing value for %s\n", k.c_str());
				exit(2);
			}
			return argv[++i];
		};
		if (k == "--module")
			module = val();
		else if (k == "--out")
			outdir = val();
		else if (k == "--path")
			g.path = val();
		else if (k == "--cases")
			cases = val();
		else if (k == "--pools")
			pools = val();
		else if (k == "--exits")
			exits = val();
		else if (k == "--factors")
			factors = val();
		else if (k == "--kases")
			kases = val();
		else if (k == "--min-claims")
			min_claims = strtoull(val().c_str(), nullptr, 0);
		else if (k == "--ctl-min-claims")
			ctl_min_claims = strtoull(val().c_str(), nullptr, 0);
		else if (k == "--aba-min-claims")
			aba_min_claims = strtoull(val().c_str(), nullptr, 0);
		else if (k == "--reuse-min")
			g_reuse_min = strtoull(val().c_str(), nullptr, 0);
		else if (k == "--aba")  // rounds,J,ns
			sscanf(val().c_str(), "%u,%u,%u", &g_aba_rounds, &g_aba_J, &g_aba_ns);
		else if (k == "--mask-launches")
			mask_launches = atoi(val().c_str());
		else if (k == "--max-launches")
			max_launches = atoi(val().c_str());
		else if (k == "--seed")
			g.rng = strtoull(val().c_str(), nullptr, 0);
		else if (k == "--hold-ms")
			hold_ms = atof(val().c_str());
		else if (k == "--hold-launches")
			hold_launches = atoi(val().c_str());
		else if (k == "--graph-replays")
			graph_replays = atoi(val().c_str());
		else if (k == "--stream-launches")
			stream_launches = atoi(val().c_str());
		else if (k == "--full-reps")
			full_reps = atoi(val().c_str());
		else {
			fprintf(stderr, "unknown option %s\n", k.c_str());
			return 2;
		}
	}
	C2_CU(cuInit(0));
	CUdevice dev;
	C2_CU(cuDeviceGet(&dev, 0));
	CUcontext ctx;
	C2_CU(cuDevicePrimaryCtxRetain(&ctx, dev));
	C2_CU(cuCtxSetCurrent(ctx));
	const double tl0 = now_s();
	if (module.size() > 4 && module.substr(module.size() - 4) == ".ptx") {
		FILE *pf = fopen(module.c_str(), "rb");
		if (!pf) {
			perror(module.c_str());
			return 2;
		}
		std::string ptx;
		char buf[65536];
		size_t k;
		while ((k = fread(buf, 1, sizeof buf, pf)) > 0)
			ptx.append(buf, k);
		fclose(pf);
		C2_CU(cuModuleLoadData(&g.mod, ptx.c_str())); // driver JIT, as bpftime loads patched PTX
	} else {
		C2_CU(cuModuleLoad(&g.mod, module.c_str()));
	}
	const double load_s = now_s() - tl0;
	g.pool.alloc(g.mod);
	C2_CU(cuStreamCreate(&g.st, CU_STREAM_NON_BLOCKING));
	C2_CU(cuStreamCreate(&g.st2, CU_STREAM_NON_BLOCKING));
	{
		std::vector<uint32_t> in(g.nin);
		uint64_t x = g.rng ^ 0x5eedull;
		for (auto &w : in)
			w = (uint32_t)splitmix(x);
		C2_CU(cuMemAlloc(&g.in, 4ull * g.nin));
		C2_CU(cuMemcpyHtoD(g.in, in.data(), 4ull * g.nin));
	}
	C2_CU(cuMemAlloc(&g.tl, 8 * 4096));
	C2_CU(cuMemAlloc(&g.arrive, 64));
	if (system(("mkdir -p '" + outdir + "'").c_str()) != 0)
		return 2;
	g.out = fopen((outdir + "/results.jsonl").c_str(), "a");
	if (!g.out) {
		perror("results.jsonl");
		return 2;
	}
	{
		FILE *m = fopen((outdir + "/meta_" + g.path + ".json").c_str(), "w");
		fprintf(m,
			"{\"pid\": %d, \"module\": \"%s\", \"path\": \"%s\", \"module_load_s\": %.3f, "
			"\"sms\": %d, \"max_threads_per_sm\": %d, \"warps_per_sm\": %d, \"resident_warps\": %d, "
			"\"max_blocks_per_sm\": %d, \"min_claims\": %" PRIu64 ", \"max_launches\": %d, "
			"\"seed\": %" PRIu64 "}\n",
			(int)getpid(), module.c_str(), g.path.c_str(), load_s, g.pool.dev.sms,
			g.pool.dev.max_threads_per_sm, g.pool.dev.warps_per_sm, g.pool.dev.resident_warps,
			g.pool.dev.max_blocks_per_sm, min_claims, max_launches, g.rng);
		fclose(m);
	}
	printf("module %s (%s), loaded in %.2f s; %d SMs x %d warps = %d resident warps\n", module.c_str(),
	       g.path.c_str(), load_s, g.pool.dev.sms, g.pool.dev.warps_per_sm, g.pool.dev.resident_warps);
	const auto cs = split(cases), ps = split(pools), xs = split(exits), fs = split(factors);
	auto has = [&](const std::string &c) { return std::find(cs.begin(), cs.end(), c) != cs.end(); };
	std::vector<uint32_t> klist;
	if (kases.empty())
		klist = { K_EARLY, K_LOWLANES, K_NESTED, K_LOOPS, K_SPLIT, K_ENTRYDIV, K_CHURN, K_CHURNSYNC };
	else
		for (auto &k : split(kases))
			for (uint32_t i = 0; i < 8; i++)
				if (k == kase_name(i))
					klist.push_back(i);
	if (has("mask"))
		for (const char *sh : { "loop_ni", "one_ni", "loop_inl" })
			run_mask(sh, mask_launches);
	if (has("div"))
		for (auto &p : ps)
			for (auto &x : xs)
				for (auto &fa : fs)
					for (uint32_t k : klist)
						run_div(c2_pool_kind(p), x, k, atof(fa.c_str()),
							(x == "o_first" || x == "oc_first") ? ctl_min_claims : min_claims,
							max_launches, 0, "");
	if (has("aba")) // churnsync with a widened pop window (constructed-mask claims, so the
			// __activemask() effect cannot mix in); versioned stacks and no-version controls
		for (const char *p : { "stksm", "stkglob", "stksmnv", "stkglobnv" })
			run_div(c2_pool_kind(p), "oc_lane", K_CHURNSYNC, 0.5, aba_min_claims, max_launches, g_aba_ns, "aba");
	for (auto &p : ps)
		for (const char *x : { "o_lane", "r_lane", "oc_lane" }) {
			if (std::find(xs.begin(), xs.end(), x) == xs.end())
				continue;
			if (has("full"))
				run_full(c2_pool_kind(p), x, full_reps, p == ps[0] && std::string(x) == "o_lane");
			if (has("streams"))
				run_streams(c2_pool_kind(p), x, stream_launches);
			if (has("graph"))
				run_graph(c2_pool_kind(p), x, graph_replays);
			if (has("hold"))
				run_hold(c2_pool_kind(p), x, hold_launches, hold_ms);
		}
	fclose(g.out);
	C2_CU(cuCtxSynchronize());
	g.pool.free_all();
	cuModuleUnload(g.mod);
	return 0;
}
