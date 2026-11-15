// kernels_b.cu: the application kernels of part (b). Compiled to PTX
// (nvcc -ptx); the dev kprobe and kretprobe passes then insert the entry
// probe (kernel start) and the exit probe (before every ret/exit), and the
// routing passes the carry's address to both. Nothing here knows about
// probes: ground truth is kept by the probes themselves (rec map).
//
//   kdiv   divergence (step0/e7-warp patterns): absent low lanes, data-
//          dependent early returns, nested divergent branches, lane-dependent
//          loops, calls to a __noinline__ function with a 2-KiB frame between
//          entry and exit (it overwrites where the probe and helper frames
//          were), and a lane-dependent spin before the end. nvcc merges
//          kdiv's returns into one ret, so the exit probe has one call site
//          here; kmr (hand-written PTX) has seven.
//   kspin  every thread spins spin_ns (cap on iterations, counted) between
//          entry and exit, sampling %globaltimer; lane 0 of each warp logs
//          gaps > gap_ns (time-slicing by another process, as step0/e1-ids).
//          Also used, under other names, for the two-stream case.
#include <stdint.h>

static __device__ __forceinline__ uint64_t gt()
{
	uint64_t t;
	asm volatile("mov.u64 %0, %%globaltimer;" : "=l"(t));
	return t;
}
static __device__ __forceinline__ uint32_t smid()
{
	uint32_t v;
	asm volatile("mov.u32 %0, %%smid;" : "=r"(v));
	return v;
}
static __device__ __forceinline__ uint32_t warpid()
{
	uint32_t v;
	asm volatile("mov.u32 %0, %%warpid;" : "=r"(v));
	return v;
}

// A call with its own frame between entry and exit: a 2-KiB array indexed
// by data (so it lives in local memory; not volatile: nvcc 13.2 would emit
// ld.volatile.local, which needs PTX ISA 9.1, above the trampoline's 8.1
// that heads the module). ptxas (whole-module compile, no recursion) places
// every callee's frame inside the kernel's own frame, all callees at the same
// offsets above the 16-B carry (SASS: carry at R1+0, the probe's eBPF stack
// and this array from R1+0x10; kernel frame 2,064 B = 16 + 2,048): this
// array overwrites the bytes where the probe functions' and helpers' frames
// were, so storage kept in such a frame is destroyed here (variant frame);
// the kernel-scope carry must not be.
extern "C" __device__ __noinline__ uint64_t c1_clobber(uint32_t v)
{
	uint64_t buf[256];
#pragma unroll 1
	for (uint32_t i = 0; i < 256; i++)
		buf[(i * 5u + v) & 255] = (uint64_t)v * 0x9E3779B97F4A7C15ull + i;
	uint64_t s = 0;
#pragma unroll 1
	for (uint32_t i = 0; i < 256; i += 7)
		s += buf[(v + i) & 255];
	return s;
}

#define SPIN_CAP (1u << 22)

// in: (iters + 2) * nthreads words (word 0..nthreads-1: per-warp cuts)
extern "C" __global__ void kdiv(const uint32_t *in, uint64_t *kout, uint32_t iters,
				uint32_t nthreads, uint64_t spin_ns, uint32_t *caps)
{
	const uint32_t gtid = blockIdx.x * blockDim.x + threadIdx.x;
	const uint32_t lane = threadIdx.x & 31;
	const uint32_t wbase = gtid & ~31u;
	uint64_t acc = 0;
	// absent low lanes: lanes below a per-warp cut leave before anything
	const uint32_t cut = in[wbase < nthreads ? wbase : 0] % 33;
	if (lane < cut && (in[nthreads + (wbase < nthreads ? wbase : 0)] & 1)) {
		kout[gtid] = 1;
		return;
	}
	for (uint32_t it = 0; it < iters; it++) {
		const uint32_t v = in[(it + 2) * nthreads + gtid];
		if ((v & 15) == 0) { // data-dependent early return
			kout[gtid] = acc | (1ull << 62);
			return;
		}
		if (v & 0x100) { // nested divergent branches; the call sites are
			if ((v & 0x600) == 0x600) // taken in about 1 of 8 iterations
				acc += c1_clobber(v);
			else
				acc += v;
		} else if ((v & 0x1800) == 0x1800) {
			acc += c1_clobber(v ^ 0x55u);
		}
		const uint32_t trip = (v >> 8) % 6 + (lane & 3);
		for (uint32_t j = 0; j < trip; j++) {
			acc += (uint64_t)j * v;
			if (((v >> (16 + j)) & 3) == 0)
				break;
		}
	}
	if (spin_ns) { // lane-dependent spin: lanes reach the exit apart
		const uint64_t t0 = gt(), d = spin_ns * (lane + 1) / 32;
		uint32_t n = 0;
		for (; n < SPIN_CAP; n++) {
			if (gt() - t0 >= d)
				break;
			__nanosleep(500);
		}
		if (n >= SPIN_CAP)
			atomicAdd(caps, 1u);
	}
	kout[gtid] = acc;
}

// Per-warp record of kspin (WREC x u64): t0, t1, gaps, max gap, iterations,
// capped, (smid << 32 | warpid) at start and at end, injected stall start and
// end. Gap log: per warp, logcap (prev, now) pairs written by lane 0; gaps
// beyond logcap are counted, not logged.
// Detector control (step0/e1-ids case e): once, at spin_ns / 2, the whole
// warp stalls itself for inj_ns (sleeping, timer not sampled), which the next
// sample must see as a gap; the host counts it apart from preemption.
#define WREC 10
#define KSPIN_BODY                                                                       \
	const uint32_t lane = threadIdx.x & 31;                                          \
	const uint32_t gw = (blockIdx.x * blockDim.x + threadIdx.x) >> 5;               \
	const uint64_t t0 = gt();                                                        \
	const uint32_t sm0 = smid(), w0 = warpid();                                      \
	uint64_t prev = t0, maxg = 0, gaps = 0, now = t0, inj0 = 0, inj1 = 0;            \
	uint32_t n = 0;                                                                  \
	for (; n < SPIN_CAP; n++) {                                                      \
		now = gt();                                                              \
		if (now > prev && now - prev > gap_ns) {                                 \
			if (lane == 0 && gaps < logcap) {                                \
				glog[((uint64_t)gw * logcap + gaps) * 2] = prev;         \
				glog[((uint64_t)gw * logcap + gaps) * 2 + 1] = now;      \
			}                                                                \
			gaps++;                                                          \
			if (now - prev > maxg)                                           \
				maxg = now - prev;                                       \
		}                                                                        \
		prev = now;                                                              \
		if (now - t0 >= spin_ns)                                                 \
			break;                                                           \
		if (inj_ns && inj0 == 0 && now - t0 >= spin_ns / 2) {                     \
			inj0 = now;                                                      \
			uint32_t k = 0;                                                  \
			while (gt() - inj0 < inj_ns && k < SPIN_CAP) {                   \
				__nanosleep(1000);                                       \
				k++;                                                     \
			}                                                                \
			inj1 = gt();                                                     \
			if (k >= SPIN_CAP)                                               \
				n = SPIN_CAP;                                            \
		}                                                                        \
		__nanosleep(1000);                                                       \
	}                                                                                \
	if (lane == 0) {                                                                 \
		uint64_t *r = wrec + (uint64_t)gw * WREC;                                \
		r[0] = t0;                                                               \
		r[1] = now;                                                              \
		r[2] = gaps;                                                             \
		r[3] = maxg;                                                             \
		r[4] = n;                                                                \
		r[5] = n >= SPIN_CAP;                                                    \
		r[6] = ((uint64_t)sm0 << 32) | w0;                                       \
		r[7] = ((uint64_t)smid() << 32) | warpid();                              \
		r[8] = inj0;                                                             \
		r[9] = inj1;                                                             \
	}                                                                                \
	__syncwarp();

extern "C" __global__ void kspin(uint64_t *wrec, uint64_t *glog, uint64_t spin_ns,
				 uint64_t gap_ns, uint32_t logcap, uint64_t inj_ns)
{
	KSPIN_BODY
}
// The same body under two more names: the two-stream case (one kernel per
// stream: each has its own probe functions and maps).
extern "C" __global__ void ks2a(uint64_t *wrec, uint64_t *glog, uint64_t spin_ns,
				uint64_t gap_ns, uint32_t logcap, uint64_t inj_ns)
{
	KSPIN_BODY
}
extern "C" __global__ void ks2b(uint64_t *wrec, uint64_t *glog, uint64_t spin_ns,
				uint64_t gap_ns, uint32_t logcap, uint64_t inj_ns)
{
	KSPIN_BODY
}
