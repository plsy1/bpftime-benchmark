// copyorder.cu: step 0, check 3, part A (standalone CUDA, no bpftime).
//
// Question: a writer kernel on stream S1 keeps filling chunks in device
// memory (payload, fence, then a per-chunk flag word holding the sequence).
// A copier on a separate non-blocking stream S2 copies the chunk area with
// cuMemcpyDtoHAsync into registered host memory made like bpftime's segment.
// If a copied flag says "sequence s complete", is every payload word of s in
// the same copy (no word older than s)?
//
//   copyorder order [opts]   ordering test, one JSON line per variant
//   copyorder bw    [opts]   DtoH latency and throughput, one JSON line per size
//
// Layouts (where the flag sits; writer order is always payload, fence, flag):
//   after  : [payload][flag line]   flag at a higher address than its payload
//   before : [flag line][payload]   flag at a lower address than its payload
//   sepff  : flags in a separate array; two copies on S2: flags, then payload
//   seppf  : same arrays; two copies on S2: payload, then flags (wrong order)
//   farafter : one allocation [all payload][all flags], one copy: each flag
//              lies far above its payload (the copy reads it much later)
//   farbefore: one allocation [all flags][all payload], one copy
// Writers:
//   normal   : every lane stores its payload words, __threadfence(),
//              __syncwarp(), lane 0 __threadfence() then stores the flag
//   reversed : can-fail control; lane 0 stores the flag first, then the
//              payload is stored (a reader can see the flag before the data)
//
// Every device loop is bounded: a sequence cap and a %globaltimer deadline,
// besides the host stop word; the exit reason of every warp is counted.
#include <cuda.h>
#include <cuda_runtime.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <vector>
#include <algorithm>

#include "../common/regmem.h"

#define RK(x)                                                                  \
	do {                                                                   \
		cudaError_t e_ = (x);                                          \
		if (e_ != cudaSuccess) {                                       \
			fprintf(stderr, "%s:%d %s -> %d %s\n", __FILE__,       \
				__LINE__, #x, (int)e_, cudaGetErrorName(e_));  \
			exit(2);                                               \
		}                                                              \
	} while (0)

enum { L_AFTER = 0, L_BEFORE = 1, L_SEPFF = 2, L_SEPPF = 3, L_FARAFTER = 4,
       L_FARBEFORE = 5, L_N = 6 };
static const char *layout_name[] = { "after", "before", "sepff",
				     "seppf", "farafter", "farbefore" };

struct WP {
	unsigned long long *area; // device: chunks (after/before) or payload (sep)
	unsigned long long *flags; // device: separate flag array (sep layouts)
	int layout;
	int reversed;
	unsigned nchunks;
	unsigned wpl; // payload words per lane; a chunk has 32 * wpl words
	unsigned stride_words; // chunk stride for after/before
	unsigned far_words; // far layouts: flag region size, nchunks rounded to 16
	unsigned long long seq_start, seq_cap;
	unsigned long long max_ns; // %globaltimer budget per launch
	const volatile unsigned *stop; // registered host memory (device pointer)
	unsigned long long *warp_last; // device: last completed sequence per warp
	unsigned *exit_reason; // device: [0] stop word, [1] seq cap, [2] deadline
	unsigned long long *span; // device: warp 0's %globaltimer at start, at exit
};

__device__ __forceinline__ unsigned long long gtimer()
{
	unsigned long long t;
	asm volatile("mov.u64 %0, %%globaltimer;" : "=l"(t));
	return t;
}

__device__ __forceinline__ unsigned long long *payload_ptr(const WP &p,
							   unsigned c)
{
	if (p.layout == L_AFTER)
		return p.area + (size_t)c * p.stride_words;
	if (p.layout == L_BEFORE)
		return p.area + (size_t)c * p.stride_words + 16;
	if (p.layout == L_FARBEFORE)
		return p.area + p.far_words + (size_t)c * 32u * p.wpl;
	return p.area + (size_t)c * 32u * p.wpl; // sep*, farafter
}

__device__ __forceinline__ volatile unsigned long long *flag_ptr(const WP &p,
								  unsigned c)
{
	if (p.layout == L_AFTER)
		return p.area + (size_t)c * p.stride_words + 32u * p.wpl;
	if (p.layout == L_BEFORE)
		return p.area + (size_t)c * p.stride_words;
	if (p.layout == L_FARAFTER)
		return p.area + (size_t)p.nchunks * 32u * p.wpl + c;
	if (p.layout == L_FARBEFORE)
		return p.area + c;
	return p.flags + c;
}

__global__ void writer(WP p)
{
	const unsigned lane = threadIdx.x & 31u;
	const unsigned warp = (blockIdx.x * blockDim.x + threadIdx.x) >> 5;
	const unsigned nwarps = (gridDim.x * blockDim.x) >> 5;
	const unsigned long long t_start = gtimer();
	const unsigned long long t_end = t_start + p.max_ns;
	unsigned long long seq = p.seq_start;
	int reason = 1; // seq cap unless something else ends the loop
	for (; seq < p.seq_cap; ++seq) {
		// every launch completes at least one round, so after it the
		// flags of each warp equal that warp's last sequence
		if (seq != p.seq_start && (seq & 7ull) == 0) {
			int st = 0;
			if (lane == 0)
				st = (*p.stop != 0) ? 1 :
							    (gtimer() > t_end ? 2 : 0);
			st = __shfl_sync(0xffffffffu, st, 0);
			if (st) {
				reason = (st == 1) ? 0 : 2;
				break;
			}
		}
		for (unsigned c = warp; c < p.nchunks; c += nwarps) {
			unsigned long long *pl = payload_ptr(p, c);
			volatile unsigned long long *fl = flag_ptr(p, c);
			const unsigned base = c * 32u * p.wpl;
			if (p.reversed) {
				if (lane == 0)
					*fl = seq;
				__threadfence();
				__syncwarp();
			}
			for (unsigned j = 0; j < p.wpl; ++j) {
				const unsigned idx = j * 32u + lane;
				pl[idx] = (seq << 32) | (unsigned long long)(base + idx);
			}
			__threadfence();
			__syncwarp();
			if (!p.reversed && lane == 0) {
				__threadfence();
				*fl = seq;
			}
			__syncwarp();
		}
	}
	if (lane == 0) {
		p.warp_last[warp] = seq - 1;
		atomicAdd(&p.exit_reason[reason], 1u);
		if (warp == 0) {
			p.span[0] = t_start;
			p.span[1] = gtimer();
		}
	}
}

// ---------------------------------------------------------------- options
struct Opts {
	std::string mode;
	std::vector<std::string> layouts{ "after",    "before",   "sepff",
					  "seppf",    "farafter", "farbefore" };
	std::vector<std::string> writers{ "normal", "reversed" };
	std::vector<size_t> areas{ 1u << 20, 16u << 20 };
	std::vector<unsigned> wpls{ 8, 64 };
	std::vector<size_t> sizes{ 64u << 10, 1u << 20, 16u << 20 };
	std::vector<std::string> bw_writers{ "none", "running" };
	double seconds = 2.0;
	unsigned blocks = 64, threads = 128;
	unsigned long long seq_cap = 1ull << 40;
	double max_kernel_s = 10.0;
	unsigned long long max_copies = 2000000;
	int lat_n = 200, tput_n = 0; // tput_n 0 = by size
	const char *out = "/dev/stdout";
};

static std::vector<std::string> split(const char *s)
{
	std::vector<std::string> v;
	std::string cur;
	for (const char *q = s;; ++q) {
		if (*q == ',' || *q == 0) {
			if (!cur.empty())
				v.push_back(cur);
			cur.clear();
			if (!*q)
				break;
		} else
			cur.push_back(*q);
	}
	return v;
}

static size_t parse_size(const std::string &s)
{
	char *e = nullptr;
	double v = strtod(s.c_str(), &e);
	if (*e == 'K' || *e == 'k')
		v *= 1024;
	else if (*e == 'M' || *e == 'm')
		v *= 1024 * 1024;
	return (size_t)v;
}

static void usage()
{
	fprintf(stderr,
		"usage: copyorder order|bw [--layouts a,b] [--writers normal,reversed]\n"
		"  [--areas 1M,16M] [--wpl 8,64] [--seconds S] [--blocks B] [--threads T]\n"
		"  [--seq-cap N] [--max-kernel-s S] [--max-copies N]\n"
		"  [--sizes 64K,1M,16M] [--bw-writers none,running] [--lat-n N] [--tput-n N]\n"
		"  [--out FILE]\n");
	exit(1);
}

static Opts parse(int argc, char **argv)
{
	Opts o;
	if (argc < 2)
		usage();
	o.mode = argv[1];
	for (int i = 2; i < argc; ++i) {
		std::string a = argv[i];
		auto val = [&]() -> const char * {
			if (i + 1 >= argc)
				usage();
			return argv[++i];
		};
		if (a == "--layouts")
			o.layouts = split(val());
		else if (a == "--writers")
			o.writers = split(val());
		else if (a == "--areas") {
			o.areas.clear();
			for (auto &s : split(val()))
				o.areas.push_back(parse_size(s));
		} else if (a == "--wpl") {
			o.wpls.clear();
			for (auto &s : split(val()))
				o.wpls.push_back((unsigned)atoi(s.c_str()));
		} else if (a == "--sizes") {
			o.sizes.clear();
			for (auto &s : split(val()))
				o.sizes.push_back(parse_size(s));
		} else if (a == "--bw-writers")
			o.bw_writers = split(val());
		else if (a == "--seconds")
			o.seconds = atof(val());
		else if (a == "--blocks")
			o.blocks = (unsigned)atoi(val());
		else if (a == "--threads")
			o.threads = (unsigned)atoi(val());
		else if (a == "--seq-cap")
			o.seq_cap = strtoull(val(), nullptr, 0);
		else if (a == "--max-kernel-s")
			o.max_kernel_s = atof(val());
		else if (a == "--max-copies")
			o.max_copies = strtoull(val(), nullptr, 0);
		else if (a == "--lat-n")
			o.lat_n = atoi(val());
		else if (a == "--tput-n")
			o.tput_n = atoi(val());
		else if (a == "--out")
			o.out = val();
		else
			usage();
	}
	return o;
}

// ---------------------------------------------------------------- state
struct Ctx {
	CUstream s1 = nullptr, s2 = nullptr;
	regbuf_t stopbuf{};
	volatile unsigned *stop_h = nullptr;
	unsigned *stop_d = nullptr;
	unsigned long long *warp_last_d = nullptr;
	unsigned *exit_d = nullptr;
	unsigned long long *span_d = nullptr;
	unsigned nwarps = 0;
};

static void ctx_init(Ctx &c, const Opts &o)
{
	RK(cudaFree(0)); // the runtime makes the primary context current
	CK(cuStreamCreate(&c.s1, CU_STREAM_NON_BLOCKING));
	CK(cuStreamCreate(&c.s2, CU_STREAM_NON_BLOCKING));
	if (regbuf_create(&c.stopbuf, 4096, "stop") != 0)
		exit(2);
	c.stop_h = (volatile unsigned *)c.stopbuf.p;
	CUdeviceptr dp;
	CK(cuMemHostGetDevicePointer(&dp, c.stopbuf.p, 0));
	c.stop_d = (unsigned *)dp;
	c.nwarps = o.blocks * o.threads / 32;
	RK(cudaMalloc(&c.warp_last_d, sizeof(unsigned long long) * c.nwarps));
	RK(cudaMalloc(&c.exit_d, sizeof(unsigned) * 4));
	RK(cudaMalloc(&c.span_d, sizeof(unsigned long long) * 2));
}

struct Writer {
	WP p{};
	unsigned launches = 0;
	unsigned exit_reason[3] = { 0, 0, 0 };
	double kernel_s = 0; // sum over launches, warp 0, from %globaltimer
	double host_launch_t = 0, host_s = 0; // host wall time, launch to collect
	std::vector<unsigned long long> warp_last;
};

static void writer_launch(Ctx &c, const Opts &o, Writer &w,
			  unsigned long long seq_start)
{
	w.p.seq_start = seq_start;
	w.p.stop = c.stop_d;
	w.p.warp_last = c.warp_last_d;
	w.p.exit_reason = c.exit_d;
	w.p.span = c.span_d;
	w.host_launch_t = now_s();
	w.p.seq_cap = seq_start + o.seq_cap; // a cap on rounds per launch
	w.p.max_ns = (unsigned long long)(o.max_kernel_s * 1e9);
	CK(cuMemsetD8Async((CUdeviceptr)c.exit_d, 0, sizeof(unsigned) * 4,
			   c.s1));
	writer<<<o.blocks, o.threads, 0, (cudaStream_t)c.s1>>>(w.p);
	RK(cudaGetLastError());
	w.launches++;
}

// Collects exit reasons and per-warp last sequence after a launch finished.
static unsigned long long writer_collect(Ctx &c, Writer &w)
{
	unsigned er[4];
	unsigned long long sp[2];
	w.warp_last.resize(c.nwarps);
	CK(cuMemcpyDtoHAsync(er, (CUdeviceptr)c.exit_d, sizeof(er), c.s1));
	CK(cuMemcpyDtoHAsync(sp, (CUdeviceptr)c.span_d, sizeof(sp), c.s1));
	CK(cuMemcpyDtoHAsync(w.warp_last.data(), (CUdeviceptr)c.warp_last_d,
			     sizeof(unsigned long long) * c.nwarps, c.s1));
	CK(cuStreamSynchronize(c.s1));
	w.host_s += now_s() - w.host_launch_t;
	w.kernel_s += (double)(sp[1] - sp[0]) * 1e-9;
	for (int i = 0; i < 3; ++i)
		w.exit_reason[i] += er[i];
	unsigned long long mx = 0;
	for (auto v : w.warp_last)
		mx = std::max(mx, v);
	return mx;
}

// ---------------------------------------------------------------- order
struct Obs {
	uint64_t copies = 0, chunk_obs = 0, unflagged = 0, flagged = 0;
	uint64_t viol_chunks = 0, older_words = 0, badpos_words = 0,
		 zero_words = 0;
	uint64_t mixed_chunks = 0, newer_chunks = 0, exact_chunks = 0;
	uint64_t flag_changes = 0, copies_with_change = 0,
		 copies_with_mixed = 0, copies_with_viol = 0;
	uint64_t max_gap = 0;
	double copy_s = 0; // time inside copy + sync
};

// Checks one snapshot. flags_h: per-chunk flag words (copied).
static void check_snapshot(const unsigned long long *const *pl_h,
			   const unsigned long long *flags_h, unsigned nchunks,
			   unsigned wpl, std::vector<unsigned long long> &prev,
			   Obs &ob)
{
	const unsigned wpc = 32u * wpl;
	uint64_t changes = 0, mixed = 0, viol = 0;
	for (unsigned c = 0; c < nchunks; ++c) {
		const unsigned long long f = flags_h[c];
		if (f != prev[c])
			changes++;
		prev[c] = f;
		ob.chunk_obs++;
		if (f == 0) {
			ob.unflagged++;
			continue;
		}
		ob.flagged++;
		const unsigned long long *pl = pl_h[c];
		unsigned long long mn = ~0ull, mxs = 0;
		uint64_t older = 0, badpos = 0, zeros = 0;
		for (unsigned i = 0; i < wpc; ++i) {
			const unsigned long long wv = pl[i];
			const unsigned long long s = wv >> 32;
			if (wv == 0) {
				zeros++;
			} else if ((unsigned)(wv & 0xffffffffu) !=
				   c * wpc + i) {
				badpos++;
			}
			if (s < f)
				older++;
			mn = std::min(mn, s);
			mxs = std::max(mxs, s);
		}
		if (older || badpos) {
			ob.viol_chunks++;
			viol++;
			ob.max_gap = std::max<uint64_t>(ob.max_gap, (uint64_t)(f - mn));
		}
		ob.older_words += older;
		ob.badpos_words += badpos;
		ob.zero_words += zeros;
		if (mn != mxs) {
			ob.mixed_chunks++;
			mixed++;
		} else if (mn == f)
			ob.exact_chunks++;
		if (mn > f)
			ob.newer_chunks++;
	}
	ob.flag_changes += changes;
	ob.copies_with_change += changes ? 1 : 0;
	ob.copies_with_mixed += mixed ? 1 : 0;
	ob.copies_with_viol += viol ? 1 : 0;
}

static void run_order(Ctx &c, const Opts &o, FILE *out)
{
	for (size_t area : o.areas)
		for (unsigned wpl : o.wpls)
			for (auto &ls : o.layouts)
				for (auto &ws : o.writers) {
					int layout = -1;
					for (int k = 0; k < L_N; ++k)
						if (ls == layout_name[k])
							layout = k;
					if (layout < 0) {
						fprintf(stderr, "bad layout %s\n",
							ls.c_str());
						exit(1);
					}
					const int reversed = (ws == "reversed");
					const unsigned wpc = 32u * wpl;
					const unsigned nchunks =
						(unsigned)(area / (wpc * 8));
					const bool sep = layout == L_SEPFF ||
							 layout == L_SEPPF;
					const bool far = layout == L_FARAFTER ||
							 layout == L_FARBEFORE;
					const unsigned stride = wpc + 16;
					const unsigned far_words = (nchunks + 15) / 16 * 16;
					const size_t area_words =
						sep ? (size_t)nchunks * wpc :
						far ? (size_t)nchunks * wpc + far_words :
						      (size_t)nchunks * stride;
					const size_t area_bytes = area_words * 8;
					const size_t flag_bytes =
						sep ? (size_t)nchunks * 8 : 0;
					unsigned long long *area_d = nullptr,
							   *flags_d = nullptr;
					RK(cudaMalloc(&area_d, area_bytes));
					RK(cudaMemset(area_d, 0, area_bytes));
					if (sep) {
						RK(cudaMalloc(&flags_d, flag_bytes));
						RK(cudaMemset(flags_d, 0, flag_bytes));
					}
					RK(cudaDeviceSynchronize());
					regbuf_t hb{}, hf{};
					if (regbuf_create(&hb, area_bytes, "area") != 0)
						exit(2);
					if (sep && regbuf_create(&hf, flag_bytes,
								 "flags") != 0)
						exit(2);
					const unsigned long long *area_h =
						(const unsigned long long *)hb.p;
					std::vector<const unsigned long long *> pl_h(
						nchunks);
					std::vector<unsigned long long> fl_h(nchunks),
						prev(nchunks, 0);
					for (unsigned k = 0; k < nchunks; ++k) {
						if (layout == L_AFTER)
							pl_h[k] = area_h +
								  (size_t)k * stride;
						else if (layout == L_BEFORE)
							pl_h[k] = area_h +
								  (size_t)k * stride +
								  16;
						else if (layout == L_FARBEFORE)
							pl_h[k] = area_h + far_words +
								  (size_t)k * wpc;
						else
							pl_h[k] = area_h +
								  (size_t)k * wpc;
					}
					Writer w;
					w.p.area = area_d;
					w.p.flags = flags_d;
					w.p.layout = layout;
					w.p.reversed = reversed;
					w.p.nchunks = nchunks;
					w.p.wpl = wpl;
					w.p.stride_words = stride;
					w.p.far_words = far_words;
					*c.stop_h = 0;
					__sync_synchronize();
					writer_launch(c, o, w, 1);
					Obs ob;
					unsigned relaunches = 0;
					// the current launch's exit counters were already
					// added (ended by its cap or deadline at time-out,
					// without a relaunch): do not add them twice
					bool collected = false;
					const double t0 = now_s();
					auto copy_once = [&]() {
						const double a = now_s();
						if (layout == L_SEPFF) {
							CK(cuMemcpyDtoHAsync(
								hf.p, (CUdeviceptr)flags_d,
								flag_bytes, c.s2));
							CK(cuMemcpyDtoHAsync(
								hb.p, (CUdeviceptr)area_d,
								area_bytes, c.s2));
						} else if (layout == L_SEPPF) {
							CK(cuMemcpyDtoHAsync(
								hb.p, (CUdeviceptr)area_d,
								area_bytes, c.s2));
							CK(cuMemcpyDtoHAsync(
								hf.p, (CUdeviceptr)flags_d,
								flag_bytes, c.s2));
						} else {
							CK(cuMemcpyDtoHAsync(
								hb.p, (CUdeviceptr)area_d,
								area_bytes, c.s2));
						}
						CK(cuStreamSynchronize(c.s2));
						ob.copy_s += now_s() - a;
						for (unsigned k = 0; k < nchunks; ++k) {
							if (sep)
								fl_h[k] = ((const unsigned long long
										    *)hf.p)[k];
							else if (layout == L_AFTER)
								fl_h[k] = area_h[(size_t)k * stride +
										 wpc];
							else if (layout == L_FARAFTER)
								fl_h[k] = area_h[(size_t)nchunks * wpc +
										 k];
							else if (layout == L_FARBEFORE)
								fl_h[k] = area_h[k];
							else
								fl_h[k] = area_h[(size_t)k *
										 stride];
						}
					};
					while (now_s() - t0 < o.seconds &&
					       ob.copies < o.max_copies) {
						copy_once();
						check_snapshot(pl_h.data(), fl_h.data(),
							       nchunks, wpl, prev, ob);
						ob.copies++;
						if (!collected &&
						    cuStreamQuery(c.s1) == CUDA_SUCCESS) {
							// writer ended early (cap or deadline):
							// continue the sequence in a new launch
							unsigned long long mx =
								writer_collect(c, w);
							collected = true;
							if (now_s() - t0 < o.seconds) {
								relaunches++;
								writer_launch(c, o, w, mx + 1);
								collected = false;
							}
						}
					}
					const double elapsed = now_s() - t0;
					*c.stop_h = 1;
					__sync_synchronize();
					CK(cuStreamSynchronize(c.s1));
					if (!collected)
						writer_collect(c, w);
					// quiescent control: one more copy after the writer
					// stopped; every flag must equal its warp's last
					// sequence and every chunk must be exact.
					Obs q;
					std::vector<unsigned long long> qprev(nchunks, 0);
					copy_once();
					check_snapshot(pl_h.data(), fl_h.data(), nchunks,
						       wpl, qprev, q);
					uint64_t q_flag_mismatch = 0;
					for (unsigned k = 0; k < nchunks; ++k)
						if (fl_h[k] !=
						    w.warp_last[k % c.nwarps])
							q_flag_mismatch++;
					unsigned long long seq_max = 0;
					for (auto v : w.warp_last)
						seq_max = std::max(seq_max, v);
					fprintf(out,
						"{\"test\":\"order\",\"layout\":\"%s\","
						"\"writer\":\"%s\",\"area_payload_bytes\":%zu,"
						"\"copy_bytes\":%zu,\"flag_copy_bytes\":%zu,"
						"\"wpl\":%u,\"chunk_payload_bytes\":%u,"
						"\"nchunks\":%u,\"blocks\":%u,\"threads\":%u,"
						"\"seconds\":%.3f,\"copies\":%" PRIu64
						",\"copy_s\":%.4f,"
						"\"chunk_obs\":%" PRIu64 ",\"unflagged\":%" PRIu64
						",\"flagged\":%" PRIu64 ",\"viol_chunks\":%" PRIu64
						",\"older_words\":%" PRIu64
						",\"badpos_words\":%" PRIu64
						",\"zero_words\":%" PRIu64
						",\"max_gap\":%" PRIu64 ",\"mixed_chunks\":%" PRIu64
						",\"newer_chunks\":%" PRIu64
						",\"exact_chunks\":%" PRIu64
						",\"flag_changes\":%" PRIu64
						",\"copies_with_change\":%" PRIu64
						",\"copies_with_mixed\":%" PRIu64
						",\"copies_with_viol\":%" PRIu64
						",\"writer_launches\":%u,\"relaunches\":%u,"
						"\"exit_stop\":%u,\"exit_seqcap\":%u,"
						"\"exit_deadline\":%u,\"seq_max\":%llu,"
						"\"writer_kernel_s\":%.4f,\"writer_host_s\":%.4f,"
						"\"quiet_viol_chunks\":%" PRIu64
						",\"quiet_mixed_chunks\":%" PRIu64
						",\"quiet_unflagged\":%" PRIu64
						",\"quiet_flag_mismatch\":%" PRIu64 "}\n",
						layout_name[layout], reversed ? "reversed" : "normal",
						(size_t)nchunks * wpc * 8, area_bytes, flag_bytes,
						wpl, wpc * 8, nchunks, o.blocks, o.threads, elapsed,
						ob.copies, ob.copy_s, ob.chunk_obs, ob.unflagged,
						ob.flagged, ob.viol_chunks, ob.older_words,
						ob.badpos_words, ob.zero_words, ob.max_gap,
						ob.mixed_chunks, ob.newer_chunks, ob.exact_chunks,
						ob.flag_changes, ob.copies_with_change,
						ob.copies_with_mixed, ob.copies_with_viol,
						w.launches, relaunches, w.exit_reason[0],
						w.exit_reason[1], w.exit_reason[2], seq_max,
						w.kernel_s, w.host_s,
						q.viol_chunks, q.mixed_chunks, q.unflagged,
						q_flag_mismatch);
					fflush(out);
					regbuf_destroy(&hb);
					if (sep)
						regbuf_destroy(&hf);
					RK(cudaFree(area_d));
					if (flags_d)
						RK(cudaFree(flags_d));
				}
}

// ---------------------------------------------------------------- bw
static double pct(std::vector<double> v, double p)
{
	if (v.empty())
		return -1;
	std::sort(v.begin(), v.end());
	size_t i = (size_t)(p / 100.0 * (double)(v.size() - 1) + 0.5);
	return v[std::min(i, v.size() - 1)];
}

static void run_bw(Ctx &c, const Opts &o, FILE *out)
{
	// All buffers are allocated before the writer starts and freed after it
	// stopped: cuMemFree and cuMemHostUnregister wait for running kernels
	// (seen in the first smoke: the writer then ran to its deadline alone).
	const size_t nsz = o.sizes.size();
	std::vector<CUdeviceptr> src(nsz);
	std::vector<regbuf_t> hb(nsz);
	for (size_t k = 0; k < nsz; ++k) {
		CK(cuMemAlloc(&src[k], o.sizes[k]));
		CK(cuMemsetD8(src[k], 0x5a, o.sizes[k]));
		if (regbuf_create(&hb[k], o.sizes[k], "bw") != 0)
			exit(2);
	}
	CUevent e0, e1;
	CK(cuEventCreate(&e0, CU_EVENT_DEFAULT));
	CK(cuEventCreate(&e1, CU_EVENT_DEFAULT));
	// background writer area: layout before, 16 MiB payload, 2 KiB chunks
	const unsigned bw_wpl = 8, bw_wpc = 256, bw_stride = bw_wpc + 16;
	const unsigned bw_nchunks = (16u << 20) / (bw_wpc * 8);
	unsigned long long *wa = nullptr;
	RK(cudaMalloc(&wa, (size_t)bw_nchunks * bw_stride * 8));
	RK(cudaMemset(wa, 0, (size_t)bw_nchunks * bw_stride * 8));
	RK(cudaDeviceSynchronize());
	for (auto &ws : o.bw_writers) {
		const bool running = (ws == "running");
		Writer w;
		if (running) {
			w.p.area = wa;
			w.p.layout = L_BEFORE;
			w.p.nchunks = bw_nchunks;
			w.p.wpl = bw_wpl;
			w.p.stride_words = bw_stride;
			*c.stop_h = 0;
			__sync_synchronize();
			writer_launch(c, o, w, 1);
		}
		for (size_t k = 0; k < nsz; ++k) {
			const size_t sz = o.sizes[k];
			const bool run_before =
				running && cuStreamQuery(c.s1) == CUDA_ERROR_NOT_READY;
			for (int i = 0; i < 3; ++i)
				CK(cuMemcpyDtoHAsync(hb[k].p, src[k], sz, c.s2));
			CK(cuStreamSynchronize(c.s2));
			std::vector<double> lat, issue, ev;
			for (int i = 0; i < o.lat_n; ++i) {
				const double a = now_s();
				CK(cuEventRecord(e0, c.s2));
				CK(cuMemcpyDtoHAsync(hb[k].p, src[k], sz, c.s2));
				const double b = now_s();
				CK(cuEventRecord(e1, c.s2));
				CK(cuStreamSynchronize(c.s2));
				const double d = now_s();
				float ms = 0;
				CK(cuEventElapsedTime(&ms, e0, e1));
				lat.push_back((d - a) * 1e6);
				issue.push_back((b - a) * 1e6);
				ev.push_back(ms * 1e3);
			}
			int tn = o.tput_n;
			if (tn <= 0)
				tn = sz >= (16u << 20) ? 30 : (sz >= (1u << 20) ? 200 : 1000);
			const double t0 = now_s();
			for (int i = 0; i < tn; ++i)
				CK(cuMemcpyDtoHAsync(hb[k].p, src[k], sz, c.s2));
			CK(cuStreamSynchronize(c.s2));
			const double t1 = now_s();
			// content check of the last copy
			uint64_t bad = 0;
			for (size_t i = 0; i < sz; ++i)
				bad += ((unsigned char *)hb[k].p)[i] != 0x5a;
			const bool run_after =
				running && cuStreamQuery(c.s1) == CUDA_ERROR_NOT_READY;
			fprintf(out,
				"{\"test\":\"bw\",\"writer\":\"%s\",\"bytes\":%zu,"
				"\"lat_n\":%d,\"lat_us_p50\":%.2f,\"lat_us_p90\":%.2f,"
				"\"lat_us_p99\":%.2f,\"lat_us_min\":%.2f,\"lat_us_max\":%.2f,"
				"\"issue_us_p50\":%.2f,\"event_us_p50\":%.2f,"
				"\"event_us_p99\":%.2f,\"tput_n\":%d,\"tput_s\":%.6f,"
				"\"tput_GBps\":%.3f,\"bad_bytes\":%" PRIu64
				",\"writer_running_before\":%d,\"writer_running_after\":%d}\n",
				ws.c_str(), sz, o.lat_n, pct(lat, 50), pct(lat, 90),
				pct(lat, 99), pct(lat, 0), pct(lat, 100), pct(issue, 50),
				pct(ev, 50), pct(ev, 99), tn, t1 - t0,
				(double)sz * tn / (t1 - t0) / 1e9, bad, run_before ? 1 : 0,
				run_after ? 1 : 0);
			fflush(out);
		}
		if (running) {
			*c.stop_h = 1;
			__sync_synchronize();
			CK(cuStreamSynchronize(c.s1));
			writer_collect(c, w);
			fprintf(out,
				"{\"test\":\"bw_writer\",\"exit_stop\":%u,\"exit_seqcap\":%u,"
				"\"exit_deadline\":%u,\"writer_kernel_s\":%.4f,"
				"\"writer_host_s\":%.4f}\n",
				w.exit_reason[0], w.exit_reason[1], w.exit_reason[2],
				w.kernel_s, w.host_s);
			fflush(out);
		}
	}
	cuEventDestroy(e0);
	cuEventDestroy(e1);
	for (size_t k = 0; k < nsz; ++k) {
		regbuf_destroy(&hb[k]);
		CK(cuMemFree(src[k]));
	}
	RK(cudaFree(wa));
}

int main(int argc, char **argv)
{
	Opts o = parse(argc, argv);
	FILE *out = fopen(o.out, "a");
	if (!out) {
		perror(o.out);
		return 1;
	}
	Ctx c;
	ctx_init(c, o);
	if (o.mode == "order")
		run_order(c, o, out);
	else if (o.mode == "bw")
		run_bw(c, o, out);
	else
		usage();
	regbuf_destroy(&c.stopbuf);
	fclose(out);
	return 0;
}
