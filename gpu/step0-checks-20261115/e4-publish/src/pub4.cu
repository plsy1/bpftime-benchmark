// pub4: step 0, check 4 (CPU/GPU publication in registered memory).
//
// Test 1 (GPU -> CPU publication). Every resident warp (384 at full
// occupancy) owns a ring of S slots in registered memory. Round r of warp w
// uses slot (r-1) % S:
//   lane 0 waits (capped, volatile loads) until the slot's ack == r - S
//   __syncwarp
//   every lane writes its payload words, v = f(r, w, lane, word)
//   __syncwarp
//   publish (variant):
//     A  lane 0: membar.sys; st.relaxed.sys flag = r     [the protocol]
//     B  every lane membar.sys before the __syncwarp; lane 0: st.relaxed.sys
//                                                         [today's per-lane cost]
//     C  lane 0: st.relaxed.sys flag = r, no fence        [control]
//     D1 lane 0: st.release.sys flag = r
//     D2 lane 0: fence.acq_rel.sys; st.relaxed.sys flag = r
//     E  lane 0 stores the flag BEFORE the payload        [must fail]
//     Cd as C, but lane 0 waits delay_ns (globaltimer) before the flag
//        store, no fence                                  [delay control]
//     Am A with lane 0's membar.sys timed by globaltimer (histograms of the
//        fence's duration, first round of a launch and the rest), which
//        sets Cd's delay                                   [calibration]
// CPU reader threads (second, unregistered mapping of the segment, as the
// loader process has): __atomic_load_n(flag, ACQUIRE) == r, then every
// payload word is read (forward or reverse order) and checked, then
// __atomic_store_n(ack, r, RELEASE).
//
// Test 2 (CPU release -> GPU CAS). M slots, each with a state word
// (round << 2 | FREE/WRITING/FULL/STOP), a winners word and a payload. The CPU
// resets the payload to cpu_pat(r) and release-stores FREE(r); GPU warps
// (lane 0) CAS FREE -> WRITING; the winner counts winners, the warp checks
// that every payload word is cpu_pat(r) (stale reads counted), writes
// gpu_pat(r), __syncwarp, lane 0 membar.sys and stores FULL(r). The CPU checks
// the GPU's payload on FULL. Variants of the GPU acquire:
//     cas      atomicCAS (atom.cas, .gpu scope, relaxed), no fence
//     casfence atomicCAS_system + lane 0 membar.sys
//     casacq   atom.acquire.sys.cas
// Control: --wrong-order: the CPU stores FREE before it resets the payload
// (and waits --wrong-delay-ns in between): stale reads must appear.
//
// Every device wait loop is capped (iteration cap and a globaltimer
// deadline); cap hits and deadline hits are counted. Kernels are launched in
// chunks of bounded length.
#include "../../common/step0_regmem.h"

#include <cuda_runtime.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cinttypes>
#include <cstdarg>
#include <string>
#include <thread>
#include <vector>

#define CUDA_OK(call)                                                          \
	do {                                                                   \
		cudaError_t e_ = (call);                                       \
		if (e_ != cudaSuccess) {                                       \
			fprintf(stderr, "%s:%d: %s -> %s\n", __FILE__,         \
				__LINE__, #call, cudaGetErrorString(e_));      \
			std::exit(11);                                         \
		}                                                              \
	} while (0)

#define FULL_MASK 0xffffffffu
static constexpr int LINE = 128;
static constexpr uint32_t WAIT_CAP = 1u << 26; // ack waits (with nanosleep)
static constexpr uint32_t T2_ITER_CAP = 1u << 30; // test-2 loop iterations

enum Variant { V_A = 0, V_B, V_C, V_D1, V_D2, V_E, V_CD, V_AM };
static const char *VNAMES[] = { "A", "B", "C", "D1", "D2", "E", "Cd", "Am" };
static constexpr int NVARIANTS = 8;
// Am: fence-duration histograms in device memory, FH_BINS each: part 0 the
// first round of a launch, part 1 the other rounds, parts 2 and 3 the same
// for idle warps (--odd-warps-idle); FH_NS per bin, the last bin holds
// everything longer
static constexpr uint32_t FH_BINS = 4096;
static constexpr uint64_t FH_NS = 64; // up to 262 us
static constexpr uint32_t DELAY_CAP = 1u << 16; // Cd: globaltimer polls
enum CasVariant { G_CAS = 0, G_CASFENCE, G_CASACQ };
static const char *GNAMES[] = { "cas", "casfence", "casacq" };

// ---------------------------------------------------------------- device
__device__ __forceinline__ uint64_t gtimer()
{
	uint64_t t;
	asm volatile("mov.u64 %0, %%globaltimer;" : "=l"(t));
	return t;
}
__device__ __forceinline__ void st_u64(uint64_t *p, uint64_t v)
{
	asm volatile("st.global.u64 [%0], %1;" ::"l"(p), "l"(v) : "memory");
}
__device__ __forceinline__ uint64_t ld_u64(const uint64_t *p)
{
	uint64_t v;
	asm volatile("ld.global.u64 %0, [%1];" : "=l"(v) : "l"(p) : "memory");
	return v;
}
__device__ __forceinline__ uint64_t ld_volatile(const uint64_t *p)
{
	uint64_t v;
	asm volatile("ld.volatile.global.u64 %0, [%1];"
		     : "=l"(v)
		     : "l"(p)
		     : "memory");
	return v;
}
__device__ __forceinline__ void st_relaxed_sys(uint64_t *p, uint64_t v)
{
	asm volatile("st.relaxed.sys.global.u64 [%0], %1;" ::"l"(p), "l"(v)
		     : "memory");
}
__device__ __forceinline__ void st_release_sys(uint64_t *p, uint64_t v)
{
	asm volatile("st.release.sys.global.u64 [%0], %1;" ::"l"(p), "l"(v)
		     : "memory");
}
__device__ __forceinline__ void membar_sys()
{
	asm volatile("membar.sys;" ::: "memory");
}
__device__ __forceinline__ void fence_acq_rel_sys()
{
	asm volatile("fence.acq_rel.sys;" ::: "memory");
}
__device__ __forceinline__ uint64_t atom_cas_acq_sys(uint64_t *p, uint64_t c,
						     uint64_t v)
{
	uint64_t r;
	asm volatile("atom.acquire.sys.global.cas.b64 %0, [%1], %2, %3;"
		     : "=l"(r)
		     : "l"(p), "l"(c), "l"(v)
		     : "memory");
	return r;
}

__host__ __device__ __forceinline__ uint64_t t1_value(uint64_t r, uint64_t w,
						      uint64_t lane, uint64_t j)
{
	return (r << 32) | (w << 16) | (lane << 8) | j;
}
// word (lane, j) of a payload: instruction j writes 32 lanes at `stride`
// words apart (stride 1: 256 B per instruction; stride 2: 512 B)
__host__ __device__ __forceinline__ uint64_t t1_word(uint32_t lane, uint32_t j,
						     uint32_t stride)
{
	return ((uint64_t)j * 32 + lane) * stride;
}

struct T1Geom {
	uint32_t nwarps, S, wpl, stride;
	uint64_t slot_bytes;
	uint64_t delay_ns; // Cd: lane 0 waits this long before the flag store
	// --lane0-idle: lane 0 (the publisher) writes no payload word, so its
	// fence has no store of its own to wait for; lanes 1-31 write theirs.
	// A fence that ordered only the issuing lane's accesses would then
	// order nothing of the payload.
	uint32_t lane0_idle;
	// --odd-warps-idle: odd warps write no payload at all (every lane), even
	// warps write as usual. With Am, the fences of the odd warps (no store
	// of their own warp outstanding) are timed in separate histograms under
	// the same load: the fence's cost without stores of its warp to wait for.
	uint32_t odd_idle;
};
static constexpr int STATUS_WORDS = 8;
// per-warp status in device memory: [0] last round published, [1] dead,
// [2] cap hits, [3] deadline hits, [4] Cd: sum of delays (ns) / Am: sum of
// fence durations (ns), [5] Cd: delay cap hits, [6] Am: longest fence (ns)
template <int V>
__global__ void t1_kernel(uint8_t *base, T1Geom g, uint32_t r_begin,
			  uint32_t r_end, uint64_t budget_ns, uint64_t *status,
			  unsigned long long *fhist)
{
	const uint32_t warp = (blockIdx.x * blockDim.x + threadIdx.x) >> 5;
	const uint32_t lane = threadIdx.x & 31;
	if (warp >= g.nwarps)
		return;
	uint64_t *st = status + (uint64_t)warp * STATUS_WORDS;
	if (ld_volatile(st + 1) != 0)
		return;
	const uint64_t deadline = gtimer() + budget_ns;
#pragma unroll 1
	for (uint32_t r = r_begin; r < r_end; r++) {
		uint8_t *slot =
			base + ((uint64_t)warp * g.S + (r - 1) % g.S) *
				       g.slot_bytes;
		uint64_t *flag = (uint64_t *)slot;
		uint64_t *ack = (uint64_t *)(slot + LINE);
		uint64_t *pay = (uint64_t *)(slot + 2 * LINE);
		const uint64_t want = r > g.S ? r - g.S : 0;
		int ok = 1;
		if (lane == 0) {
			uint32_t it = 0;
			int late = 0;
#pragma unroll 1
			for (; it < WAIT_CAP; it++) {
				if (ld_volatile(ack) == want)
					break;
				if ((it & 255) == 255 && gtimer() > deadline) {
					late = 1;
					break;
				}
				__nanosleep(64);
			}
			if (late || it == WAIT_CAP) {
				ok = 0;
				st[1] = 1;
				st[2] += (it == WAIT_CAP);
				st[3] += late;
			}
		}
		ok = __shfl_sync(FULL_MASK, ok, 0);
		if (!ok)
			return; // this warp stops; the round is not published
		__syncwarp(FULL_MASK);
		if (V == V_E) {
			if (lane == 0)
				st_relaxed_sys(flag, r);
			__syncwarp(FULL_MASK);
		}
		if (!(g.lane0_idle && lane == 0) && !(g.odd_idle && (warp & 1))) {
#pragma unroll 1
			for (uint32_t j = 0; j < g.wpl; j++)
				st_u64(pay + t1_word(lane, j, g.stride),
				       t1_value(r, warp, lane, j));
		}
		if (V == V_B)
			membar_sys();
		__syncwarp(FULL_MASK);
		if (lane == 0) {
			if (V == V_A) {
				membar_sys();
				st_relaxed_sys(flag, r);
			} else if (V == V_B || V == V_C) {
				st_relaxed_sys(flag, r);
			} else if (V == V_D1) {
				st_release_sys(flag, r);
			} else if (V == V_D2) {
				fence_acq_rel_sys();
				st_relaxed_sys(flag, r);
			} else if (V == V_CD) {
				// C plus a delay about as long as a membar.sys
				// (run_all.sh: Am's measured median), no fence:
				// does time alone hide the race?
				const uint64_t t0 = gtimer();
				uint64_t t1 = t0;
				uint32_t it = 0;
#pragma unroll 1
				for (; it < DELAY_CAP; it++) {
					t1 = gtimer();
					if (t1 - t0 >= g.delay_ns)
						break;
				}
				st_relaxed_sys(flag, r);
				st[4] += t1 - t0;
				st[5] += it == DELAY_CAP;
			} else if (V == V_AM) {
				// A, with lane 0's fence timed; the histogram
				// update follows the flag store
				const uint64_t t0 = gtimer();
				membar_sys();
				const uint64_t t1 = gtimer();
				st_relaxed_sys(flag, r);
				const uint64_t d = t1 - t0;
				const uint64_t b = d / FH_NS < FH_BINS - 1 ?
							   d / FH_NS :
							   FH_BINS - 1;
				const uint32_t part = (r == r_begin ? 0 : 1) +
						      (g.odd_idle && (warp & 1) ? 2 : 0);
				atomicAdd(fhist + part * FH_BINS + b, 1ull);
				st[4] += d;
				if (d > st[6])
					st[6] = d;
			}
			st[0] = r;
		}
	}
}

struct T2Geom {
	uint32_t M, wpl, stride, nwarps;
	uint64_t slot_bytes;
};
__host__ __device__ __forceinline__ uint64_t cpu_pat(uint64_t r, uint64_t idx)
{
	return (0xCull << 60) | (r << 16) | idx;
}
__host__ __device__ __forceinline__ uint64_t gpu_pat(uint64_t r, uint64_t w,
						     uint64_t idx)
{
	return (0x6ull << 60) | (r << 28) | (w << 16) | idx;
}
enum { S_FREE = 0, S_WRITING = 1, S_FULL = 2, S_STOP = 3 };
// counters in device memory: [0] rounds won, [1] stale words, [2] stale
// rounds, [3] double winners, [4] iteration-cap hits, [5] deadline exits
template <int G>
__global__ void t2_kernel(uint8_t *base, T2Geom g, uint64_t budget_ns,
			  unsigned long long *cnt)
{
	const uint32_t warp = (blockIdx.x * blockDim.x + threadIdx.x) >> 5;
	const uint32_t lane = threadIdx.x & 31;
	if (warp >= g.nwarps)
		return;
	const uint64_t deadline = gtimer() + budget_ns;
	uint32_t si = warp % g.M;
	uint64_t stopped = 0; // bit per slot (M <= 64)
	const uint64_t all = g.M == 64 ? ~0ull : ((1ull << g.M) - 1);
	uint32_t it = 0;
#pragma unroll 1
	for (; it < T2_ITER_CAP; it++) {
		if ((it & 63) == 0 && gtimer() > deadline) {
			if (lane == 0)
				atomicAdd(cnt + 5, 1ull);
			return;
		}
		uint8_t *slot = base + (uint64_t)si * g.slot_bytes;
		uint64_t *state = (uint64_t *)slot;
		unsigned long long *winners =
			(unsigned long long *)(slot + LINE);
		uint64_t *hdr = (uint64_t *)(slot + 2 * LINE);
		uint64_t *pay = (uint64_t *)(slot + 3 * LINE);
		uint64_t s = 0;
		int won = 0, stop = 0;
		if (lane == 0) {
			s = ld_volatile(state);
			const uint32_t tag = s & 3;
			if (tag == S_STOP) {
				stop = 1;
			} else if (tag == S_FREE) {
				uint64_t prev;
				if (G == G_CAS)
					prev = atomicCAS(
						(unsigned long long *)state,
						s, s | S_WRITING);
				else if (G == G_CASFENCE)
					prev = atomicCAS_system(
						(unsigned long long *)state,
						s, s | S_WRITING);
				else
					prev = atom_cas_acq_sys(state, s,
								s | S_WRITING);
				won = prev == s;
			}
		}
		stop = __shfl_sync(FULL_MASK, stop, 0);
		won = __shfl_sync(FULL_MASK, won, 0);
		s = __shfl_sync(FULL_MASK, s, 0);
		if (stop) {
			stopped |= 1ull << si;
			if (stopped == all)
				return;
		}
		if (!won) {
			si = (si + 1) % g.M;
			if (lane == 0)
				__nanosleep(32);
			continue;
		}
		const uint64_t r = s >> 2;
		if (lane == 0) {
			if (atomicAdd_system(winners, 1ull) != 0)
				atomicAdd(cnt + 3, 1ull);
			if (G == G_CASFENCE)
				membar_sys();
		}
		__syncwarp(FULL_MASK);
		uint32_t stale = 0;
#pragma unroll 1
		for (uint32_t j = 0; j < g.wpl; j++) {
			const uint64_t idx = t1_word(lane, j, g.stride);
			stale += ld_u64(pay + idx) != cpu_pat(r, idx);
		}
#pragma unroll 1
		for (uint32_t j = 0; j < g.wpl; j++) {
			const uint64_t idx = t1_word(lane, j, g.stride);
			st_u64(pay + idx, gpu_pat(r, warp, idx));
		}
		if (lane == 0)
			st_u64(hdr, ((uint64_t)warp << 32) | r);
		const uint32_t stale_warp = __reduce_add_sync(FULL_MASK, stale);
		__syncwarp(FULL_MASK);
		if (lane == 0) {
			membar_sys();
			st_relaxed_sys(state, (r << 2) | S_FULL);
			atomicAdd(cnt + 0, 1ull);
			if (stale_warp) {
				atomicAdd(cnt + 1, (unsigned long long)stale_warp);
				atomicAdd(cnt + 2, 1ull);
			}
		}
		si = (si + 1) % g.M;
	}
	if (lane == 0)
		atomicAdd(cnt + 4, 1ull);
}

// ---------------------------------------------------------------- host
static double now_s()
{
	return std::chrono::duration<double>(
		       std::chrono::steady_clock::now().time_since_epoch())
		.count();
}
static inline void cpu_relax()
{
	asm volatile("yield" ::: "memory");
}
static std::string fmt(const char *f, ...)
{
	va_list ap, ap2;
	va_start(ap, f);
	va_copy(ap2, ap);
	const int n = vsnprintf(nullptr, 0, f, ap);
	va_end(ap);
	std::string s(n > 0 ? n : 0, '\0');
	vsnprintf(s.data(), s.size() + 1, f, ap2);
	va_end(ap2);
	return s;
}

struct Opts {
	int test = 1;
	std::string variant = "A";
	uint64_t rounds = 1000000; // total slot-rounds (test 1) / rounds (test 2)
	double budget_s = 30; // whole run
	double chunk_s = 1.0; // target kernel length
	uint64_t chunk_rounds = 0; // test 1: fixed rounds per launch (0: adapt to chunk_s)
	bool stress = false;
	bool lane0_idle = false; // test 1: lane 0 writes no payload word
	bool odd_idle = false; // test 1: odd warps write no payload word
	int S = 4;
	int readers = 4;
	int M = 64;
	int blocks = 48, tpb = 256; // 384 warps
	uint64_t delay_ns = 1500; // variant Cd
	bool wrong_order = false;
	bool poison = false; // test 1: the reader overwrites the payload before the ack
	uint64_t wrong_delay_ns = 50000;
	int log_max = 20;
	int reread_max = 50;
	std::string out;
};

struct ReaderStats {
	uint64_t verified = 0, viol_rounds = 0, bad_words = 0, stale = 0,
		 future = 0, other = 0, healed = 0, persistent = 0,
		 rereads = 0, anomalies = 0;
	int heal_ms_max = 0;
	uint64_t poison_seen = 0, viol_first_round = 0;
	std::vector<std::string> log;
};

static void t1_reader(int id, const Opts &o, const T1Geom &g, uint8_t *view,
		      uint64_t rounds_per_warp, std::atomic<bool> *stop,
		      ReaderStats *rs)
{
	std::vector<uint32_t> mine;
	for (uint32_t w = id; w < g.nwarps; w += o.readers)
		mine.push_back(w);
	std::vector<uint64_t> exp(mine.size(), 1);
	const uint32_t nw = g.wpl * 32;
	std::vector<uint64_t> seen(nw);
	std::vector<uint32_t> badidx;
	uint64_t idle = 0;
	while (!stop->load(std::memory_order_relaxed)) {
		bool progress = false;
		for (size_t i = 0; i < mine.size(); i++) {
			const uint64_t r = exp[i];
			if (r > rounds_per_warp)
				continue;
			const uint32_t w = mine[i];
			uint8_t *slot = view + ((uint64_t)w * g.S +
						(r - 1) % g.S) *
						       g.slot_bytes;
			uint64_t *flag = (uint64_t *)slot;
			uint64_t *ack = (uint64_t *)(slot + LINE);
			volatile uint64_t *pay =
				(volatile uint64_t *)(slot + 2 * LINE);
			const uint64_t f = __atomic_load_n(flag, __ATOMIC_ACQUIRE);
			if (f != r) {
				const uint64_t prev = r > g.S ? r - g.S : 0;
				if (f != prev) {
					rs->anomalies++;
					if ((int)rs->log.size() < o.log_max)
						rs->log.push_back(fmt(
							"anomaly warp %u round %" PRIu64
							" flag %" PRIu64,
							w, r, f));
				}
				continue;
			}
			// read every payload word (reverse order under
			// --stress: last-written instruction first)
			badidx.clear();
			for (uint32_t n = 0; n < nw; n++) {
				const uint32_t q = o.stress ? nw - 1 - n : n;
				const uint32_t j = q / 32, lane = q % 32;
				seen[q] = pay[t1_word(lane, j, g.stride)];
			}
			for (uint32_t q = 0; q < nw; q++) {
				const uint32_t j = q / 32, lane = q % 32;
				if ((g.lane0_idle && lane == 0) ||
				    (g.odd_idle && (w & 1)))
					continue; // never written
				if (seen[q] != t1_value(r, w, lane, j))
					badidx.push_back(q);
			}
			if (!badidx.empty()) {
				rs->viol_rounds++;
				if (o.chunk_rounds && (r - 1) % o.chunk_rounds == 0)
					rs->viol_first_round++;
				rs->bad_words += badidx.size();
				for (uint32_t q : badidx) {
					const uint64_t sr = seen[q] >> 32;
					if ((seen[q] >> 48) == 0xdead)
						rs->poison_seen++; // consumed: stale
					else if (sr < r)
						rs->stale++;
					else if (sr > r)
						rs->future++;
					else
						rs->other++;
				}
				if ((int)rs->log.size() < o.log_max) {
					const uint32_t q = badidx[0];
					rs->log.push_back(fmt(
						"violation warp %u round %" PRIu64
						" slot %" PRIu64 ": %zu bad words, first lane %u word %u seen %016" PRIx64
						" want %016" PRIx64,
						w, r, (r - 1) % g.S,
						badidx.size(), q % 32, q / 32,
						seen[q],
						t1_value(r, w, q % 32, q / 32)));
				}
				// re-read after 1, 2, 4, ... 512 ms (1 s in
				// all): healed = an ordering violation (the
				// words arrived late); never healed = a test
				// bug. Under GPU time-slicing a warp can stop
				// for whole slices between two instructions.
				if (rs->rereads < (uint64_t)o.reread_max) {
					rs->rereads++;
					bool healed = false;
					int waited = 0;
					for (int ms = 1; ms <= 512 && !healed;
					     ms *= 2) {
						std::this_thread::sleep_for(
							std::chrono::milliseconds(
								ms));
						waited += ms;
						healed = true;
						for (uint32_t q : badidx)
							if (pay[t1_word(
								    q % 32, q / 32,
								    g.stride)] !=
							    t1_value(r, w,
								     q % 32,
								     q / 32))
								healed = false;
					}
					if (healed) {
						rs->healed++;
						rs->heal_ms_max = std::max(
							rs->heal_ms_max, waited);
					} else {
						rs->persistent++;
					}
				}
			}
			if (o.poison) // consume: dirty every payload line
				for (uint32_t q = 0; q < nw; q++)
					pay[t1_word(q % 32, q / 32, g.stride)] =
						0xdeadull << 48 | q;
			__atomic_store_n(ack, r, __ATOMIC_RELEASE);
			exp[i] = r + 1;
			rs->verified++;
			progress = true;
		}
		if (!progress) {
			if (++idle > 64) {
				std::this_thread::yield();
				idle = 0;
			} else {
				cpu_relax();
			}
		}
	}
}

struct CpuT2Stats {
	uint64_t full_seen = 0, gpu_viol_rounds = 0, gpu_bad_words = 0,
		 winners_bad = 0, hdr_bad = 0;
	std::vector<std::string> log;
};

static void t2_cpu(int id, const Opts &o, const T2Geom &g, uint8_t *view,
		   uint64_t rounds_per_slot, std::atomic<bool> *stop,
		   std::atomic<int> *slots_done, CpuT2Stats *cs)
{
	std::vector<uint32_t> mine;
	for (uint32_t m = id; m < g.M; m += o.readers)
		mine.push_back(m);
	std::vector<char> done(mine.size(), 0);
	const uint32_t nw = g.wpl * 32;
	uint64_t idle = 0;
	auto reset = [&](uint8_t *slot, uint64_t r) {
		volatile uint64_t *pay = (volatile uint64_t *)(slot + 3 * LINE);
		for (uint32_t q = 0; q < nw; q++) {
			const uint64_t idx = t1_word(q % 32, q / 32, g.stride);
			pay[idx] = cpu_pat(r, idx);
		}
	};
	while (!stop->load(std::memory_order_relaxed)) {
		bool progress = false;
		for (size_t i = 0; i < mine.size(); i++) {
			if (done[i])
				continue;
			uint8_t *slot = view + (uint64_t)mine[i] * g.slot_bytes;
			uint64_t *state = (uint64_t *)slot;
			volatile uint64_t *winners =
				(volatile uint64_t *)(slot + LINE);
			volatile uint64_t *hdr =
				(volatile uint64_t *)(slot + 2 * LINE);
			volatile uint64_t *pay =
				(volatile uint64_t *)(slot + 3 * LINE);
			const uint64_t s = __atomic_load_n(state, __ATOMIC_ACQUIRE);
			if ((s & 3) != S_FULL)
				continue;
			const uint64_t r = s >> 2;
			cs->full_seen++;
			const uint64_t h = *hdr;
			const uint64_t w = h >> 32;
			if ((h & 0xffffffffull) != r) {
				cs->hdr_bad++;
			}
			uint32_t bad = 0;
			for (uint32_t q = 0; q < nw; q++) {
				const uint64_t idx =
					t1_word(q % 32, q / 32, g.stride);
				if (pay[idx] != gpu_pat(r, w, idx))
					bad++;
			}
			if (bad) {
				cs->gpu_viol_rounds++;
				cs->gpu_bad_words += bad;
				if ((int)cs->log.size() < o.log_max)
					cs->log.push_back(fmt(
						"cpu sees bad GPU payload slot %u round %" PRIu64
						": %u words",
						mine[i], r, bad));
			}
			if (*winners != 1) {
				cs->winners_bad++;
				if ((int)cs->log.size() < o.log_max)
					cs->log.push_back(fmt(
						"winners slot %u round %" PRIu64
						" = %" PRIu64,
						mine[i], r, (uint64_t)*winners));
			}
			*winners = 0;
			const uint64_t nr = r + 1;
			if (r >= rounds_per_slot) {
				__atomic_store_n(state, (nr << 2) | S_STOP,
						 __ATOMIC_RELEASE);
				done[i] = 1;
				slots_done->fetch_add(1);
			} else if (!o.wrong_order) {
				reset(slot, nr);
				__atomic_store_n(state, (nr << 2) | S_FREE,
						 __ATOMIC_RELEASE);
			} else {
				// control: FREE before the reset
				__atomic_store_n(state, (nr << 2) | S_FREE,
						 __ATOMIC_RELEASE);
				const double t0 = now_s();
				while ((now_s() - t0) * 1e9 < o.wrong_delay_ns)
					cpu_relax();
				reset(slot, nr);
			}
			progress = true;
		}
		if (!progress) {
			if (++idle > 64) {
				std::this_thread::yield();
				idle = 0;
			} else {
				cpu_relax();
			}
		}
	}
}

static void usage()
{
	fprintf(stderr,
		"pub4 --test 1|2 --variant A|B|C|Cd|D1|D2|E|Am (test 1) or cas|casfence|casacq (test 2)\n"
		"     --rounds N (test 1: total slot-rounds; test 2: total rounds)\n"
		"     --budget-s S --chunk-s S --stress --S slots --readers n --M slots\n"
		"     --wrong-order [--wrong-delay-ns ns] --delay-ns ns (Cd) --blocks n --chunk-rounds n --poison\n"
		"     --lane0-idle (test 1: the publishing lane writes no payload) --odd-warps-idle (odd warps write none)\n"
		"     --out file.json\n");
}

int main(int argc, char **argv)
{
	Opts o;
	for (int i = 1; i < argc; i++) {
		std::string a = argv[i];
		auto val = [&]() -> std::string {
			if (i + 1 >= argc) {
				usage();
				exit(2);
			}
			return argv[++i];
		};
		if (a == "--test")
			o.test = atoi(val().c_str());
		else if (a == "--variant")
			o.variant = val();
		else if (a == "--rounds")
			o.rounds = strtoull(val().c_str(), nullptr, 0);
		else if (a == "--budget-s")
			o.budget_s = atof(val().c_str());
		else if (a == "--chunk-s")
			o.chunk_s = atof(val().c_str());
		else if (a == "--chunk-rounds")
			o.chunk_rounds = strtoull(val().c_str(), nullptr, 0);
		else if (a == "--stress")
			o.stress = true;
		else if (a == "--lane0-idle")
			o.lane0_idle = true;
		else if (a == "--odd-warps-idle")
			o.odd_idle = true;
		else if (a == "--S")
			o.S = atoi(val().c_str());
		else if (a == "--readers")
			o.readers = atoi(val().c_str());
		else if (a == "--M")
			o.M = atoi(val().c_str());
		else if (a == "--poison")
			o.poison = true;
		else if (a == "--blocks")
			o.blocks = atoi(val().c_str());
		else if (a == "--delay-ns")
			o.delay_ns = strtoull(val().c_str(), nullptr, 0);
		else if (a == "--wrong-order")
			o.wrong_order = true;
		else if (a == "--wrong-delay-ns")
			o.wrong_delay_ns = strtoull(val().c_str(), nullptr, 0);
		else if (a == "--out")
			o.out = val();
		else {
			usage();
			return 2;
		}
	}
	const double t_start = now_s();
	CUDA_OK(cudaSetDeviceFlags(cudaDeviceScheduleBlockingSync));
	CUDA_OK(cudaFree(0)); // primary context, as cudart leaves it
	step0_primary_ctx(0);
	const uint32_t nwarps = (uint32_t)(o.blocks * o.tpb / 32);
	const uint32_t wpl = o.stress ? 4 : 1, stride = o.stress ? 2 : 1;
	const uint64_t pay_bytes = (uint64_t)wpl * 32 * stride * 8;
	std::string shm = fmt("step0_e4_%d", (int)getpid());
	step0_regmem m;
	uint64_t slot_bytes, seg;
	if (o.test == 1)
		slot_bytes = 2 * LINE + ((pay_bytes + LINE - 1) / LINE) * LINE;
	else
		slot_bytes = 3 * LINE + ((pay_bytes + LINE - 1) / LINE) * LINE;
	seg = (o.test == 1 ? (uint64_t)nwarps * o.S : (uint64_t)o.M) *
	      slot_bytes;
	seg = ((seg + (1 << 20) - 1) >> 20) << 20;
	if (!step0_regmem_create(m, shm, seg, true))
		return 3;
	memset(m.host2, 0, seg);
	std::string result;
	if (o.test == 1) {
		int v = -1;
		for (int k = 0; k < NVARIANTS; k++)
			if (o.variant == VNAMES[k])
				v = k;
		if (v < 0) {
			usage();
			return 2;
		}
		T1Geom g{ nwarps, (uint32_t)o.S, wpl, stride, slot_bytes,
			  o.delay_ns, o.lane0_idle ? 1u : 0u, o.odd_idle ? 1u : 0u };
		const uint64_t rpw = (o.rounds + nwarps - 1) / nwarps;
		uint64_t *status;
		const size_t status_bytes = (size_t)nwarps * STATUS_WORDS * 8;
		CUDA_OK(cudaMalloc(&status, status_bytes));
		CUDA_OK(cudaMemset(status, 0, status_bytes));
		unsigned long long *fhist;
		const size_t fhist_bytes = 4 * FH_BINS * 8;
		CUDA_OK(cudaMalloc(&fhist, fhist_bytes));
		CUDA_OK(cudaMemset(fhist, 0, fhist_bytes));
		std::atomic<bool> stop{ false };
		std::vector<ReaderStats> rs(o.readers);
		std::vector<std::thread> th;
		for (int i = 0; i < o.readers; i++)
			th.emplace_back(t1_reader, i, std::cref(o), std::cref(g),
					(uint8_t *)m.host2, rpw, &stop, &rs[i]);
		auto kern = v == V_A  ? t1_kernel<V_A> :
			    v == V_B  ? t1_kernel<V_B> :
			    v == V_C  ? t1_kernel<V_C> :
			    v == V_D1 ? t1_kernel<V_D1> :
			    v == V_D2 ? t1_kernel<V_D2> :
			    v == V_E  ? t1_kernel<V_E> :
			    v == V_CD ? t1_kernel<V_CD> :
					t1_kernel<V_AM>;
		uint64_t rb = 1, chunk = o.chunk_rounds ? o.chunk_rounds : 64;
		std::vector<uint64_t> launch_starts;
		int launches = 0;
		const double t_gpu0 = now_s();
		bool budget_hit = false;
		while (rb <= rpw) {
			if (now_s() - t_start > o.budget_s) {
				budget_hit = true;
				break;
			}
			const uint64_t re = std::min<uint64_t>(rb + chunk, rpw + 1);
			const uint64_t rb_old = rb;
			if (launch_starts.size() < 200)
				launch_starts.push_back(rb);
			const double t0 = now_s();
			const uint64_t kb_ns = (uint64_t)(std::max(
				5.0, 4 * o.chunk_s) * 1e9);
			kern<<<o.blocks, o.tpb>>>((uint8_t *)m.dev, g,
						 (uint32_t)rb, (uint32_t)re,
						 kb_ns, status, fhist);
			CUDA_OK(cudaGetLastError());
			CUDA_OK(cudaDeviceSynchronize());
			launches++;
			const double dt = now_s() - t0;
			rb = re;
			// next chunk: aim at chunk_s per launch
			const double per = dt / (double)(re - rb_old);
			uint64_t nc = (uint64_t)(o.chunk_s / std::max(per, 1e-7));
			if (!o.chunk_rounds)
				chunk = std::clamp<uint64_t>(
					nc, 16, std::max<uint64_t>(16, chunk * 4));
			std::vector<uint64_t> sv(nwarps * STATUS_WORDS);
			CUDA_OK(cudaMemcpy(sv.data(), status, status_bytes,
					   cudaMemcpyDeviceToHost));
			bool all_dead = true;
			for (uint32_t w = 0; w < nwarps; w++)
				all_dead &= sv[w * STATUS_WORDS + 1] != 0;
			if (all_dead)
				break;
		}
		const double t_gpu = now_s() - t_gpu0;
		// let the readers catch up (bounded), then stop them
		const double t_wait0 = now_s();
		auto verified = [&]() {
			uint64_t s = 0;
			for (auto &x : rs)
				s += __atomic_load_n(&x.verified, __ATOMIC_RELAXED);
			return s;
		};
		std::vector<uint64_t> sv(nwarps * STATUS_WORDS);
		CUDA_OK(cudaMemcpy(sv.data(), status, status_bytes,
				   cudaMemcpyDeviceToHost));
		uint64_t published = 0, dead = 0, caps = 0, lates = 0,
			 delay_sum = 0, delay_caps = 0, fence_max = 0;
		for (uint32_t w = 0; w < nwarps; w++) {
			const uint64_t *x = &sv[(size_t)w * STATUS_WORDS];
			published += x[0];
			dead += x[1] != 0;
			caps += x[2];
			lates += x[3];
			delay_sum += x[4];
			delay_caps += x[5];
			fence_max = std::max(fence_max, x[6]);
		}
		// Am: fence-duration percentiles (upper bin edges, ns)
		std::string fence_json = "null";
		if (v == V_AM) {
			std::vector<unsigned long long> h(4 * FH_BINS);
			CUDA_OK(cudaMemcpy(h.data(), fhist, fhist_bytes,
					   cudaMemcpyDeviceToHost));
			auto pct = [&](int part) {
				const unsigned long long *x =
					h.data() + part * FH_BINS;
				unsigned long long n = 0;
				for (uint32_t b = 0; b < FH_BINS; b++)
					n += x[b];
				auto at = [&](double q) -> long long {
					if (!n)
						return -1;
					unsigned long long c = 0;
					for (uint32_t b = 0; b < FH_BINS; b++) {
						c += x[b];
						if (c >= q * n)
							return b == FH_BINS - 1 ?
								       -2 :
								       (long long)((b + 1) * FH_NS);
					}
					return -2;
				};
				return fmt("{\"n\": %llu, \"p50\": %lld, \"p90\": %lld, "
					   "\"p99\": %lld, \"overflow\": %llu}",
					   n, at(0.5), at(0.9), at(0.99),
					   x[FH_BINS - 1]);
			};
			fence_json = fmt(
				"{\"bin_ns\": %" PRIu64 ", \"overflow_from_ns\": %" PRIu64
				", \"first_round\": %s, \"other_rounds\": %s, "
				"\"first_round_idle_warps\": %s, \"other_rounds_idle_warps\": %s, "
				"\"mean\": %.1f, \"max\": %" PRIu64 "}",
				FH_NS, (uint64_t)(FH_BINS - 1) * FH_NS,
				pct(0).c_str(), pct(1).c_str(), pct(2).c_str(),
				pct(3).c_str(),
				published ? (double)delay_sum / published : 0.0,
				fence_max);
		}
		while (verified() < published && now_s() - t_wait0 < 5)
			std::this_thread::sleep_for(std::chrono::milliseconds(1));
		stop = true;
		for (auto &t : th)
			t.join();
		ReaderStats tot;
		for (auto &x : rs) {
			tot.verified += x.verified;
			tot.viol_rounds += x.viol_rounds;
			tot.bad_words += x.bad_words;
			tot.stale += x.stale;
			tot.future += x.future;
			tot.other += x.other;
			tot.healed += x.healed;
			tot.persistent += x.persistent;
			tot.rereads += x.rereads;
			tot.anomalies += x.anomalies;
			tot.heal_ms_max = std::max(tot.heal_ms_max, x.heal_ms_max);
			tot.poison_seen += x.poison_seen;
			tot.viol_first_round += x.viol_first_round;
			for (auto &l : x.log)
				if ((int)tot.log.size() < o.log_max)
					tot.log.push_back(l);
		}
		std::string logs = "[";
		for (size_t i = 0; i < tot.log.size(); i++)
			logs += fmt("%s\"%s\"", i ? ", " : "", tot.log[i].c_str());
		logs += "], \"launch_first_rounds\": [";
		for (size_t i = 0; i < launch_starts.size(); i++)
			logs += fmt("%s%" PRIu64, i ? ", " : "", launch_starts[i]);
		logs += "]";
		result = fmt(
			"{\"test\": 1, \"variant\": \"%s\", \"stress\": %d, \"poison\": %d, \"lane0_idle\": %d, \"odd_warps_idle\": %d, \"S\": %d, "
			"\"readers\": %d, \"warps\": %u, \"words_per_lane\": %u, \"stride_words\": %u, "
			"\"slot_bytes\": %" PRIu64 ", \"rounds_requested\": %" PRIu64
			", \"rounds_per_warp\": %" PRIu64 ", \"published\": %" PRIu64
			", \"verified\": %" PRIu64 ", \"violating_rounds\": %" PRIu64
			", \"chunk_rounds\": %" PRIu64 ", \"violations_in_first_round_of_launch\": %" PRIu64
			", \"bad_words\": %" PRIu64 ", \"stale_words\": %" PRIu64
			", \"poison_words\": %" PRIu64 ", \"future_words\": %" PRIu64 ", \"other_words\": %" PRIu64
			", \"rereads\": %" PRIu64 ", \"healed\": %" PRIu64
			", \"persistent\": %" PRIu64 ", \"heal_ms_max\": %d, \"flag_anomalies\": %" PRIu64
			", \"dead_warps\": %" PRIu64 ", \"cap_hits\": %" PRIu64
			", \"deadline_hits\": %" PRIu64 ", \"delay_ns\": %" PRIu64
			", \"mean_delay_ns\": %.1f, \"delay_cap_hits\": %" PRIu64
			", \"fence_ns\": %s"
			", \"blocks\": %d, \"launches\": %d, "
			"\"budget_hit\": %s, \"gpu_wall_s\": %.3f, \"rate_rounds_per_s\": %.1f, "
			"\"log\": %s}",
			VNAMES[v], o.stress, o.poison, (int)o.lane0_idle, (int)o.odd_idle, o.S, o.readers, nwarps, wpl, stride,
			slot_bytes, o.rounds, rpw, published, tot.verified,
			tot.viol_rounds, o.chunk_rounds, tot.viol_first_round, tot.bad_words, tot.stale, tot.poison_seen, tot.future,
			tot.other, tot.rereads, tot.healed, tot.persistent, tot.heal_ms_max,
			tot.anomalies, dead, caps, lates,
			v == V_CD ? o.delay_ns : (uint64_t)0,
			v == V_CD && published ? (double)delay_sum / published : 0.0,
			delay_caps, fence_json.c_str(), o.blocks, launches,
			budget_hit ? "true" : "false", t_gpu,
			tot.verified / std::max(t_gpu, 1e-9), logs.c_str());
		cudaFree(status);
		cudaFree(fhist);
	} else {
		int v = -1;
		for (int k = 0; k < 3; k++)
			if (o.variant == GNAMES[k])
				v = k;
		if (v < 0 || o.M > 64 || o.M < 1) {
			usage();
			return 2;
		}
		T2Geom g{ (uint32_t)o.M, wpl, stride, nwarps, slot_bytes };
		const uint64_t rps = (o.rounds + o.M - 1) / o.M;
		// initial state: payload cpu_pat(1), winners 0, FREE(1)
		for (int s = 0; s < o.M; s++) {
			uint8_t *slot = (uint8_t *)m.host2 + s * slot_bytes;
			uint64_t *pay = (uint64_t *)(slot + 3 * LINE);
			for (uint32_t q = 0; q < wpl * 32; q++) {
				const uint64_t idx =
					t1_word(q % 32, q / 32, stride);
				pay[idx] = cpu_pat(1, idx);
			}
			__atomic_store_n((uint64_t *)slot, (1ull << 2) | S_FREE,
					 __ATOMIC_RELEASE);
		}
		unsigned long long *cnt;
		CUDA_OK(cudaMalloc(&cnt, 8 * 8));
		CUDA_OK(cudaMemset(cnt, 0, 8 * 8));
		std::atomic<bool> stop{ false };
		std::atomic<int> slots_done{ 0 };
		std::vector<CpuT2Stats> cs(o.readers);
		std::vector<std::thread> th;
		for (int i = 0; i < o.readers; i++)
			th.emplace_back(t2_cpu, i, std::cref(o), std::cref(g),
					(uint8_t *)m.host2, rps, &stop,
					&slots_done, &cs[i]);
		auto kern = v == G_CAS	    ? t2_kernel<G_CAS> :
			    v == G_CASFENCE ? t2_kernel<G_CASFENCE> :
					      t2_kernel<G_CASACQ>;
		int launches = 0;
		const double t_gpu0 = now_s();
		bool budget_hit = false;
		while (slots_done.load() < o.M) {
			if (now_s() - t_start > o.budget_s) {
				budget_hit = true;
				break;
			}
			kern<<<o.blocks, o.tpb>>>(
				(uint8_t *)m.dev, g,
				(uint64_t)(o.chunk_s * 1e9), cnt);
			CUDA_OK(cudaGetLastError());
			CUDA_OK(cudaDeviceSynchronize());
			launches++;
		}
		const double t_gpu = now_s() - t_gpu0;
		stop = true;
		for (auto &t : th)
			t.join();
		unsigned long long c[8];
		CUDA_OK(cudaMemcpy(c, cnt, sizeof c, cudaMemcpyDeviceToHost));
		CpuT2Stats tot;
		for (auto &x : cs) {
			tot.full_seen += x.full_seen;
			tot.gpu_viol_rounds += x.gpu_viol_rounds;
			tot.gpu_bad_words += x.gpu_bad_words;
			tot.winners_bad += x.winners_bad;
			tot.hdr_bad += x.hdr_bad;
			for (auto &l : x.log)
				if ((int)tot.log.size() < o.log_max)
					tot.log.push_back(l);
		}
		std::string logs = "[";
		for (size_t i = 0; i < tot.log.size(); i++)
			logs += fmt("%s\"%s\"", i ? ", " : "", tot.log[i].c_str());
		logs += "]";
		result = fmt(
			"{\"test\": 2, \"variant\": \"%s\", \"wrong_order\": %s, "
			"\"wrong_delay_ns\": %" PRIu64 ", \"stress\": %d, \"M\": %d, "
			"\"cpu_threads\": %d, \"warps\": %u, \"words_per_slot\": %u, "
			"\"rounds_requested\": %" PRIu64 ", \"rounds_per_slot\": %" PRIu64
			", \"gpu_rounds_won\": %llu, \"cpu_full_seen\": %" PRIu64
			", \"stale_words\": %llu, \"stale_rounds\": %llu, "
			"\"double_winners_gpu\": %llu, \"winners_bad_cpu\": %" PRIu64
			", \"cpu_bad_gpu_payload_rounds\": %" PRIu64
			", \"cpu_bad_gpu_payload_words\": %" PRIu64 ", \"hdr_bad\": %" PRIu64
			", \"iter_cap_hits\": %llu, \"deadline_exits\": %llu, "
			"\"slots_done\": %d, \"launches\": %d, \"budget_hit\": %s, "
			"\"gpu_wall_s\": %.3f, \"rate_rounds_per_s\": %.1f, \"log\": %s}",
			GNAMES[v], o.wrong_order ? "true" : "false",
			o.wrong_delay_ns, o.stress, o.M, o.readers, nwarps,
			wpl * 32, o.rounds, rps, c[0], tot.full_seen, c[1],
			c[2], c[3], tot.winners_bad, tot.gpu_viol_rounds,
			tot.gpu_bad_words, tot.hdr_bad, c[4], c[5],
			slots_done.load(), launches,
			budget_hit ? "true" : "false", t_gpu,
			tot.full_seen / std::max(t_gpu, 1e-9), logs.c_str());
		cudaFree(cnt);
	}
	step0_regmem_destroy(m);
	printf("%s\n", result.c_str());
	if (!o.out.empty()) {
		FILE *f = fopen(o.out.c_str(), "w");
		if (f) {
			fprintf(f, "%s\n", result.c_str());
			fclose(f);
		}
	}
	return 0;
}
