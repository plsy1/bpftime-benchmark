// ids.cu: step-0 check 1, one full-occupancy id log.
//   ids <case> <outdir> [key=value ...]
// cases:
//   a  co-resident launches at block sizes 32..1024 (+ an oversubscribed control launch)
//   b  two grids on two streams, one shared arrival counter
//   c  %gridid over 1,000 plain launches, 1,000 launches on two streams, a 3-node graph
//      replayed 1,000 times, and a graph updated with cudaGraphExecUpdate between replays
//   d  the same recording kernel in three modules (runtime API, PTX via cuModuleLoadData,
//      cubin via cuLibraryLoadData), launches interleaved on one stream
//   e  long kernel: per-warp gap and id-change detector over %globaltimer (run alone as the
//      control, or while ./hog runs in a second process)
// Every device-side wait has an iteration cap and a %globaltimer cap; cap hits are counted.
#include <cuda.h>
#include <cuda_runtime.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <map>
#include <string>
#include <vector>
#include "rec.cuh"

static void die_rt(cudaError_t e, const char *what, int line)
{
	fprintf(stderr, "CUDA error %d (%s) at line %d: %s\n", (int)e, cudaGetErrorString(e), line, what);
	exit(2);
}
#define CK(x)                                                                                      \
	do {                                                                                       \
		cudaError_t e_ = (x);                                                              \
		if (e_ != cudaSuccess)                                                             \
			die_rt(e_, #x, __LINE__);                                                  \
	} while (0)
#define CKD(x)                                                                                     \
	do {                                                                                       \
		CUresult r_ = (x);                                                                 \
		if (r_ != CUDA_SUCCESS) {                                                          \
			const char *s_ = nullptr;                                                  \
			cuGetErrorString(r_, &s_);                                                 \
			fprintf(stderr, "CU error %d (%s) at line %d: %s\n", (int)r_, s_ ? s_ : "?", \
				__LINE__, #x);                                                     \
			exit(2);                                                                   \
		}                                                                                  \
	} while (0)

// ------------------------------------------------------------------ options
static std::map<std::string, std::string> g_kv;
static double optd(const char *k, double d)
{
	auto it = g_kv.find(k);
	return it == g_kv.end() ? d : atof(it->second.c_str());
}
static long long opti(const char *k, long long d)
{
	auto it = g_kv.find(k);
	return it == g_kv.end() ? d : atoll(it->second.c_str());
}
static double now_s()
{
	timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return ts.tv_sec + ts.tv_nsec * 1e-9;
}

// ------------------------------------------------------------------ device code
// Capped wait until *ctr >= total. Returns iterations; sets flags bit0 (iteration cap) or bit1
// (time cap). Only lane 0 of a warp calls it.
__device__ __forceinline__ uint32_t spin_until(const unsigned *ctr, unsigned total, uint64_t cap_iters,
						uint64_t cap_ns, uint32_t &flags)
{
	uint64_t t0 = rd_gtime();
	uint64_t it = 0;
	while (true) {
		unsigned v = *(volatile const unsigned *)ctr;
		if (v >= total)
			break;
		if (++it >= cap_iters) {
			flags |= 1;
			break;
		}
		if (rd_gtime() - t0 > cap_ns) {
			flags |= 2;
			break;
		}
	}
	return it > 0xffffffffull ? 0xffffffffu : (uint32_t)it;
}

// Cases a and b: entry record, arrival, capped wait for every warp of the launch (or of both
// launches in case b), exit record. rec[2*w] = entry, rec[2*w+1] = exit.
__global__ void k_barrier(IdRec *rec, unsigned *arrive, unsigned total, uint32_t tag, uint32_t warp_base,
			  uint64_t cap_iters, uint64_t cap_ns, unsigned *caphits)
{
	uint32_t tid = threadIdx.x;
	uint32_t wpb = (blockDim.x + 31) / 32;
	uint32_t w = warp_base + blockIdx.x * wpb + (tid >> 5);
	if ((tid & 31) == 0) {
		IdRec e;
		read_ids(e);
		e.tag = tag;
		e.kind = 0;
		e.warp_lin = w;
		e.aux = 0;
		e.flags = 0;
		rec[2 * w] = e;
		__threadfence();
		atomicAdd(arrive, 1u);
		uint32_t fl = 0;
		uint32_t it = spin_until(arrive, total, cap_iters, cap_ns, fl);
		IdRec x;
		read_ids(x);
		x.tag = tag;
		x.kind = 1;
		x.warp_lin = w;
		x.aux = it;
		x.flags = fl;
		rec[2 * w + 1] = x;
		if (fl)
			atomicAdd(caphits, 1u);
	}
	__syncwarp();
}

// Cases c and d: one record per warp, no waiting.
__global__ void k_plain(IdRec *rec, uint32_t tag, uint32_t kind)
{
	record_plain(rec, tag, kind);
}

// Case c graph nodes: block 0 lane 0 takes a replay number from a device counter and records.
__global__ void k_node(IdRec *rec, unsigned *ctr, uint32_t node, uint32_t cap, uint32_t kind, unsigned *over)
{
	if (threadIdx.x == 0 && blockIdx.x == 0) {
		uint32_t r = atomicAdd(&ctr[node], 1u);
		if (r < cap) {
			IdRec x;
			read_ids(x);
			x.tag = node;
			x.kind = kind;
			x.warp_lin = 0;
			x.aux = r;
			x.flags = 0;
			rec[node * cap + r] = x;
		} else {
			atomicAdd(over, 1u);
		}
	}
}

// Case e: long kernel with a per-warp gap / id-change detector.
struct WarpSum {
	uint64_t iters, t_loop0, t_loop1, max_dt, max_dt_inj, min_nz_dt; // max_dt excludes the injected stall
	uint32_t gaps, inj_detected, inj_done, changes_gap, changes_nogap, neg_dt;
	uint32_t events, ev_overflow, samples, smp_overflow, warp;
	uint32_t flags; // bit0 loop iteration cap, bit1 entry barrier cap, bit2 exit barrier cap, bit3 stall cap
};
struct EvRec {
	uint64_t t_prev, t_now;
	uint32_t warp, kind; // kind bit0 gap (dt > gap_ns), bit1 id change, bit2 injected gap
	uint32_t smid0, warpid0, smid1, warpid1;
};
struct Sample {
	uint64_t t;
	uint32_t warp, smid, warpid, pad;
};
struct LongP {
	IdRec *rec;
	WarpSum *sum;
	EvRec *ev;
	Sample *smp;
	unsigned *arrive; // [0] entry barrier, [1] exit barrier
	unsigned *caphits;
	unsigned total;
	uint32_t evcap, smpcap, sleep_ns;
	uint64_t dur_ns, gap_ns, period_ns, inj_ns, loop_cap_iters, bar_cap_iters, bar_cap_ns;
};

// 128 threads x 12 blocks per SM = 1,536 threads: the register budget for full occupancy
__global__ void __launch_bounds__(128, 12) k_long(LongP p)
{
	uint32_t tid = threadIdx.x;
	uint32_t wpb = (blockDim.x + 31) / 32;
	uint32_t w = blockIdx.x * wpb + (tid >> 5);
	if ((tid & 31) == 0) {
		WarpSum s;
		memset(&s, 0, sizeof(s));
		s.warp = w;
		s.min_nz_dt = ~0ull;
		IdRec r;
		read_ids(r);
		r.tag = 0;
		r.kind = 0;
		r.warp_lin = w;
		r.aux = 0;
		r.flags = 0;
		p.rec[3 * w] = r;
		__threadfence();
		atomicAdd(&p.arrive[0], 1u);
		uint32_t fl = 0;
		uint32_t it0 = spin_until(&p.arrive[0], p.total, p.bar_cap_iters, p.bar_cap_ns, fl);
		if (fl)
			s.flags |= 2;
		read_ids(r);
		r.kind = 1;
		r.aux = it0;
		r.flags = fl;
		p.rec[3 * w + 1] = r;

		uint64_t t0 = rd_gtime();
		uint64_t prev_t = t0, last_smp = t0;
		uint32_t ps = rd_smid(), pw = rd_warpid();
		uint32_t inj_pending = 0;
		EvRec *ev = p.ev + (uint64_t)w * p.evcap;
		Sample *sm = p.smp + (uint64_t)w * p.smpcap;
		uint64_t it = 0;
		for (; it < p.loop_cap_iters; ++it) {
			uint64_t now = rd_gtime();
			uint32_t cs = rd_smid(), cw = rd_warpid();
			bool gap = false;
			if (now < prev_t) {
				s.neg_dt++;
			} else {
				uint64_t dt = now - prev_t;
				if (inj_pending) {
					if (dt > s.max_dt_inj)
						s.max_dt_inj = dt;
				} else if (dt > s.max_dt) {
					s.max_dt = dt;
				}
				if (dt && dt < s.min_nz_dt)
					s.min_nz_dt = dt;
				gap = dt > p.gap_ns;
			}
			bool ch = (cs != ps) || (cw != pw);
			if (gap || ch) {
				uint32_t kind = (gap ? 1u : 0u) | (ch ? 2u : 0u) | ((gap && inj_pending) ? 4u : 0u);
				if (gap) {
					if (inj_pending)
						s.inj_detected++;
					else
						s.gaps++;
				}
				if (ch) {
					if (gap)
						s.changes_gap++;
					else
						s.changes_nogap++;
				}
				if (s.events < p.evcap) {
					EvRec e;
					e.t_prev = prev_t;
					e.t_now = now;
					e.warp = w;
					e.kind = kind;
					e.smid0 = ps;
					e.warpid0 = pw;
					e.smid1 = cs;
					e.warpid1 = cw;
					ev[s.events++] = e;
				} else {
					s.ev_overflow++;
				}
			}
			if (gap)
				inj_pending = 0;
			prev_t = now;
			ps = cs;
			pw = cw;
			if (now - last_smp >= p.period_ns) {
				if (s.samples < p.smpcap) {
					Sample q;
					q.t = now;
					q.warp = w;
					q.smid = cs;
					q.warpid = cw;
					q.pad = 0;
					sm[s.samples++] = q;
				} else {
					s.smp_overflow++;
				}
				last_smp = now;
			}
			if (now - t0 >= p.dur_ns)
				break;
			if (!s.inj_done && now - t0 >= p.dur_ns / 2) {
				// positive control for the detector: a self-inflicted stall of inj_ns that the
				// next iteration must classify as a gap (kind bit2)
				// it sleeps instead of spinning, so it does not starve the other warps
				uint64_t ti = 0;
				while (rd_gtime() - now < p.inj_ns && ti < (1ull << 20)) {
					ti++;
					__nanosleep(1000);
				}
				if (ti >= (1ull << 20))
					s.flags |= 8; // stall loop hit its iteration cap
				s.inj_done = 1;
				inj_pending = 1;
			}
			// every warp yields each iteration, so that warp scheduling (oldest slot first)
			// cannot starve warps and fake gaps; one iteration then takes about sleep_ns
			if (p.sleep_ns)
				__nanosleep(p.sleep_ns);
		}
		if (it >= p.loop_cap_iters)
			s.flags |= 1;
		s.iters = it;
		s.t_loop0 = t0;
		s.t_loop1 = prev_t;
		__threadfence();
		atomicAdd(&p.arrive[1], 1u);
		uint32_t fl2 = 0;
		uint32_t it1 = spin_until(&p.arrive[1], p.total, p.bar_cap_iters, p.bar_cap_ns, fl2);
		if (fl2)
			s.flags |= 4;
		read_ids(r);
		r.kind = 2;
		r.aux = it1;
		r.flags = fl2;
		p.rec[3 * w + 2] = r;
		p.sum[w] = s;
		if (s.flags)
			atomicAdd(p.caphits, 1u);
	}
	__syncwarp();
}

// Case e, prio option: a same-process kernel on a high-priority stream ("intruder"). Block 0
// records when it ran, so the host can tell whether it ran inside the long kernel (preemption
// within the context) or only after it.
__global__ void k_intr(uint64_t dur_ns, uint64_t *ts)
{
	if ((threadIdx.x & 31) == 0) {
		uint64_t t0 = rd_gtime(), it = 0;
		while (rd_gtime() - t0 < dur_ns && it < (1ull << 22)) {
			it++;
			__nanosleep(1000);
		}
		if (it >= (1ull << 22))
			atomicAdd((unsigned long long *)&ts[3], 1ull); // warps that hit the iteration cap
		if (blockIdx.x == 0 && threadIdx.x == 0) {
			ts[0] = t0;
			ts[1] = rd_gtime();
			ts[2] = rd_smid();
		}
	}
	__syncwarp();
}

// ------------------------------------------------------------------ host helpers
static std::string g_out;
static FILE *open_out(const std::string &name)
{
	std::string p = g_out + "/" + name;
	FILE *f = fopen(p.c_str(), "w");
	if (!f) {
		perror(p.c_str());
		exit(2);
	}
	return f;
}
static const char *REC_HDR =
	"case,launch,kind,tag,warp_lin,warp_in_cta,smid,warpid,laneid,nsmid,nwarpid,ctax,ctay,ctaz,"
	"gridid,gridid32,gtime,aux,flags\n";
static void write_rec(FILE *f, const char *cs, long launch, const IdRec &r)
{
	fprintf(f, "%s,%ld,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%llu,%llu,%llu,%u,%u\n", cs, launch, r.kind,
		r.tag, r.warp_lin, r.warp_in_cta, r.smid, r.warpid, r.laneid, r.nsmid, r.nwarpid, r.ctax,
		r.ctay, r.ctaz, (unsigned long long)r.gridid, (unsigned long long)r.gridid32,
		(unsigned long long)r.gtime, r.aux, r.flags);
}
static int g_sms = 0;
static void dev_info(FILE *meta)
{
	cudaDeviceProp p;
	CK(cudaGetDeviceProperties(&p, 0));
	g_sms = p.multiProcessorCount;
	int drv = 0, rt = 0;
	cudaDriverGetVersion(&drv);
	cudaRuntimeGetVersion(&rt);
	fprintf(meta, "\"device\": {\"name\": \"%s\", \"sms\": %d, \"max_threads_per_sm\": %d, "
		      "\"max_blocks_per_sm\": %d, \"driver\": %d, \"runtime\": %d, \"preemption\": %d},\n",
		p.name, p.multiProcessorCount, p.maxThreadsPerMultiProcessor, p.maxBlocksPerMultiProcessor, drv, rt,
		p.computePreemptionSupported);
}

// ------------------------------------------------------------------ case a
static void case_a()
{
	int reps = (int)opti("reps", 3);
	uint64_t cap_ns = (uint64_t)(optd("cap_ms", 500) * 1e6);
	uint64_t cap_iters = (uint64_t)opti("cap_iters", 1ll << 23);
	const int sizes[] = {32, 48, 64, 96, 128, 256, 512, 1024};
	FILE *fr = open_out("a_records.csv");
	FILE *fl = open_out("a_launches.csv");
	fputs(REC_HDR, fr);
	fputs("case,launch,rep,bs,blocks,warps,occ_blocks_per_sm,control,caphits,wall_ms\n", fl);
	unsigned *d_arrive, *d_cap;
	CK(cudaMalloc(&d_arrive, 4));
	CK(cudaMalloc(&d_cap, 4));
	long launch = 0;
	struct L {
		int bs, mult, control;
	};
	std::vector<L> plan;
	for (int r = 0; r < reps; r++)
		for (int bs : sizes)
			plan.push_back({bs, 1, 0});
	// control: twice the resident capacity at 128 threads; the barrier must time out (cap hits)
	plan.push_back({128, 2, 1});
	int rep_ctr[2048] = {0};
	for (auto &l : plan) {
		int occ = 0;
		CK(cudaOccupancyMaxActiveBlocksPerMultiprocessor(&occ, k_barrier, l.bs, 0));
		int blocks = occ * g_sms * l.mult;
		int wpb = (l.bs + 31) / 32;
		unsigned warps = (unsigned)blocks * wpb;
		IdRec *d_rec;
		CK(cudaMalloc(&d_rec, sizeof(IdRec) * 2 * warps));
		CK(cudaMemset(d_rec, 0xff, sizeof(IdRec) * 2 * warps));
		CK(cudaMemset(d_arrive, 0, 4));
		CK(cudaMemset(d_cap, 0, 4));
		CK(cudaDeviceSynchronize());
		double t0 = now_s();
		k_barrier<<<blocks, l.bs>>>(d_rec, d_arrive, warps, (uint32_t)launch, 0, cap_iters, cap_ns, d_cap);
		CK(cudaGetLastError());
		CK(cudaDeviceSynchronize());
		double t1 = now_s();
		std::vector<IdRec> h(2 * warps);
		unsigned caphits = 0;
		CK(cudaMemcpy(h.data(), d_rec, sizeof(IdRec) * 2 * warps, cudaMemcpyDeviceToHost));
		CK(cudaMemcpy(&caphits, d_cap, 4, cudaMemcpyDeviceToHost));
		CK(cudaFree(d_rec));
		for (auto &r : h)
			write_rec(fr, l.control ? "a_over" : "a", launch, r);
		int rep = rep_ctr[l.bs + 1024 * l.control]++;
		fprintf(fl, "%s,%ld,%d,%d,%d,%u,%d,%d,%u,%.3f\n", l.control ? "a_over" : "a", launch, rep, l.bs,
			blocks, warps, occ, l.control, caphits, (t1 - t0) * 1e3);
		printf("a: launch %ld bs=%d blocks=%d warps=%u occ=%d control=%d caphits=%u wall=%.1f ms\n", launch,
		       l.bs, blocks, warps, occ, l.control, caphits, (t1 - t0) * 1e3);
		launch++;
	}
	fclose(fr);
	fclose(fl);
	CK(cudaFree(d_arrive));
	CK(cudaFree(d_cap));
}

// ------------------------------------------------------------------ case b
static void case_b()
{
	int reps = (int)opti("reps", 3);
	uint64_t cap_ns = (uint64_t)(optd("cap_ms", 500) * 1e6);
	uint64_t cap_iters = (uint64_t)opti("cap_iters", 1ll << 23);
	const int sizes[] = {128, 256, 96};
	FILE *fr = open_out("b_records.csv");
	FILE *fl = open_out("b_launches.csv");
	fputs(REC_HDR, fr);
	fputs("case,launch,rep,bs,blocks_k1,blocks_k2,warps,occ_blocks_per_sm,caphits,wall_ms\n", fl);
	cudaStream_t s1, s2;
	CK(cudaStreamCreateWithFlags(&s1, cudaStreamNonBlocking));
	CK(cudaStreamCreateWithFlags(&s2, cudaStreamNonBlocking));
	unsigned *d_arrive, *d_cap;
	CK(cudaMalloc(&d_arrive, 4));
	CK(cudaMalloc(&d_cap, 4));
	long launch = 0;
	for (int r = 0; r < reps; r++) {
		for (int bs : sizes) {
			int occ = 0;
			CK(cudaOccupancyMaxActiveBlocksPerMultiprocessor(&occ, k_barrier, bs, 0));
			int cap_blocks = occ * g_sms;
			int b1 = cap_blocks / 2, b2 = cap_blocks - b1;
			int wpb = (bs + 31) / 32;
			unsigned w1 = (unsigned)b1 * wpb, w2 = (unsigned)b2 * wpb, warps = w1 + w2;
			IdRec *d_rec;
			CK(cudaMalloc(&d_rec, sizeof(IdRec) * 2 * warps));
			CK(cudaMemset(d_rec, 0xff, sizeof(IdRec) * 2 * warps));
			CK(cudaMemset(d_arrive, 0, 4));
			CK(cudaMemset(d_cap, 0, 4));
			CK(cudaDeviceSynchronize());
			double t0 = now_s();
			// tag: 2*launch for K1, 2*launch+1 for K2; K2's warps follow K1's in rec
			k_barrier<<<b1, bs, 0, s1>>>(d_rec, d_arrive, warps, (uint32_t)(2 * launch), 0, cap_iters, cap_ns,
						     d_cap);
			CK(cudaGetLastError());
			k_barrier<<<b2, bs, 0, s2>>>(d_rec, d_arrive, warps, (uint32_t)(2 * launch + 1), w1, cap_iters,
						     cap_ns, d_cap);
			CK(cudaGetLastError());
			CK(cudaDeviceSynchronize());
			double t1 = now_s();
			std::vector<IdRec> h(2 * warps);
			unsigned caphits = 0;
			CK(cudaMemcpy(h.data(), d_rec, sizeof(IdRec) * 2 * warps, cudaMemcpyDeviceToHost));
			CK(cudaMemcpy(&caphits, d_cap, 4, cudaMemcpyDeviceToHost));
			CK(cudaFree(d_rec));
			for (auto &x : h)
				write_rec(fr, "b", launch, x);
			fprintf(fl, "b,%ld,%d,%d,%d,%d,%u,%d,%u,%.3f\n", launch, r, bs, b1, b2, warps, occ, caphits,
				(t1 - t0) * 1e3);
			printf("b: launch %ld bs=%d k1=%d k2=%d blocks warps=%u caphits=%u wall=%.1f ms\n", launch, bs,
			       b1, b2, warps, caphits, (t1 - t0) * 1e3);
			launch++;
		}
	}
	fclose(fr);
	fclose(fl);
	CK(cudaStreamDestroy(s1));
	CK(cudaStreamDestroy(s2));
}

// ------------------------------------------------------------------ case c
static void case_c()
{
	int n = (int)opti("n", 1000);
	const int G = 2, B = 64, WPL = G * B / 32; // 4 warps per plain launch
	FILE *fr = open_out("c_records.csv");
	FILE *fm = open_out("c_meta.csv");
	fputs(REC_HDR, fr);
	fputs("part,key,value\n", fm);
	cudaStream_t s1, s2;
	CK(cudaStreamCreateWithFlags(&s1, cudaStreamNonBlocking));
	CK(cudaStreamCreateWithFlags(&s2, cudaStreamNonBlocking));
	IdRec *d_rec;
	size_t nrec = (size_t)n * WPL;
	CK(cudaMalloc(&d_rec, sizeof(IdRec) * nrec * 2));
	std::vector<IdRec> h(nrec * 2);

	// c1: n plain launches on one stream (tag = issue index)
	// cudaMemset goes to the legacy default stream, which does not order against the
	// non-blocking streams s1/s2: synchronize before the launches, or a late fill could
	// erase records
	CK(cudaMemset(d_rec, 0xff, sizeof(IdRec) * nrec));
	CK(cudaDeviceSynchronize());
	for (int i = 0; i < n; i++) {
		k_plain<<<G, B, 0, s1>>>(d_rec + (size_t)i * WPL, (uint32_t)i, 10);
		CK(cudaGetLastError());
	}
	CK(cudaDeviceSynchronize());
	CK(cudaMemcpy(h.data(), d_rec, sizeof(IdRec) * nrec, cudaMemcpyDeviceToHost));
	for (size_t i = 0; i < nrec; i++)
		write_rec(fr, "c1", (long)(i / WPL), h[i]);

	// c2: n launches alternating between two streams (kind 20 = s1, 21 = s2)
	CK(cudaMemset(d_rec, 0xff, sizeof(IdRec) * nrec));
	CK(cudaDeviceSynchronize());
	for (int i = 0; i < n; i++) {
		cudaStream_t s = (i & 1) ? s2 : s1;
		k_plain<<<G, B, 0, s>>>(d_rec + (size_t)i * WPL, (uint32_t)i, 20 + (i & 1));
		CK(cudaGetLastError());
	}
	CK(cudaDeviceSynchronize());
	CK(cudaMemcpy(h.data(), d_rec, sizeof(IdRec) * nrec, cudaMemcpyDeviceToHost));
	for (size_t i = 0; i < nrec; i++)
		write_rec(fr, "c2", (long)(i / WPL), h[i]);

	// c3: a 3-node graph (stream capture, a chain) replayed n times
	unsigned *d_ctr, *d_over;
	CK(cudaMalloc(&d_ctr, 4 * 8));
	CK(cudaMalloc(&d_over, 4));
	uint32_t cap = (uint32_t)n + 16;
	IdRec *d_g;
	CK(cudaMalloc(&d_g, sizeof(IdRec) * cap * 6));
	CK(cudaMemset(d_g, 0xff, sizeof(IdRec) * cap * 6));
	CK(cudaMemset(d_ctr, 0, 32));
	CK(cudaMemset(d_over, 0, 4));
	CK(cudaDeviceSynchronize());
	auto capture = [&](IdRec *base, unsigned *ctr, uint32_t kind) {
		cudaGraph_t g;
		CK(cudaStreamBeginCapture(s1, cudaStreamCaptureModeGlobal));
		for (uint32_t node = 0; node < 3; node++)
			k_node<<<1, 32, 0, s1>>>(base, ctr, node, cap, kind, d_over);
		CK(cudaStreamEndCapture(s1, &g));
		return g;
	};
	cudaGraph_t g1 = capture(d_g, d_ctr, 30);
	cudaGraphExec_t ge;
	CK(cudaGraphInstantiate(&ge, g1, 0));
	for (int i = 0; i < n; i++)
		CK(cudaGraphLaunch(ge, s1));
	CK(cudaStreamSynchronize(s1));
	// c4: a second instance, n/2 replays, cudaGraphExecUpdate to a re-captured graph with other
	// arguments (as llama.cpp does), n/2 more replays
	cudaGraph_t g2a = capture(d_g + 3 * cap, d_ctr + 4, 40);
	cudaGraphExec_t ge2;
	CK(cudaGraphInstantiate(&ge2, g2a, 0));
	for (int i = 0; i < n / 2; i++)
		CK(cudaGraphLaunch(ge2, s1));
	CK(cudaStreamSynchronize(s1));
	// the update graph writes to the same region and counters but with kind 41
	cudaGraph_t g2b = capture(d_g + 3 * cap, d_ctr + 4, 41);
	cudaGraphExecUpdateResultInfo info;
	cudaError_t ue = cudaGraphExecUpdate(ge2, g2b, &info);
	int update_ok = (ue == cudaSuccess);
	if (!update_ok) {
		fprintf(stderr, "cudaGraphExecUpdate failed: %s (result %d)\n", cudaGetErrorString(ue), (int)info.result);
		cudaGetLastError();
	}
	for (int i = 0; i < n - n / 2; i++)
		CK(cudaGraphLaunch(ge2, s1));
	CK(cudaStreamSynchronize(s1));
	CK(cudaDeviceSynchronize());
	std::vector<IdRec> hg(cap * 6);
	unsigned ctr[8], over = 0;
	CK(cudaMemcpy(hg.data(), d_g, sizeof(IdRec) * cap * 6, cudaMemcpyDeviceToHost));
	CK(cudaMemcpy(ctr, d_ctr, 32, cudaMemcpyDeviceToHost));
	CK(cudaMemcpy(&over, d_over, 4, cudaMemcpyDeviceToHost));
	for (uint32_t node = 0; node < 3; node++)
		for (uint32_t r = 0; r < cap; r++) {
			const IdRec &x = hg[node * cap + r];
			if (x.tag != 0xffffffffu)
				write_rec(fr, "c3", (long)r, x);
			const IdRec &y = hg[3 * cap + node * cap + r];
			if (y.tag != 0xffffffffu)
				write_rec(fr, "c4", (long)r, y);
		}
	fprintf(fm, "c3,replays,%d\nc4,replays,%d\nc4,update_ok,%d\n", n, n, update_ok);
	for (int i = 0; i < 3; i++)
		fprintf(fm, "c3,ctr_node%d,%u\nc4,ctr_node%d,%u\n", i, ctr[i], i, ctr[4 + i]);
	fprintf(fm, "c,overflow,%u\nc1,launches,%d\nc2,launches,%d\n", over, n, n);
	printf("c: c1 %d launches, c2 %d launches, c3 ctr=%u/%u/%u, c4 ctr=%u/%u/%u update_ok=%d over=%u\n", n, n,
	       ctr[0], ctr[1], ctr[2], ctr[4], ctr[5], ctr[6], update_ok, over);
	fclose(fr);
	fclose(fm);
	CK(cudaGraphExecDestroy(ge));
	CK(cudaGraphExecDestroy(ge2));
	CK(cudaGraphDestroy(g1));
	CK(cudaGraphDestroy(g2a));
	CK(cudaGraphDestroy(g2b));
	CK(cudaFree(d_rec));
	CK(cudaFree(d_g));
}

// ------------------------------------------------------------------ case d
static std::string exe_dir()
{
	char buf[PATH_MAX];
	ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
	if (n <= 0)
		return ".";
	buf[n] = 0;
	std::string s(buf);
	return s.substr(0, s.rfind('/'));
}
static std::vector<char> read_file(const std::string &p, bool nul)
{
	FILE *f = fopen(p.c_str(), "rb");
	if (!f) {
		perror(p.c_str());
		exit(2);
	}
	std::vector<char> v;
	char b[65536];
	size_t k;
	while ((k = fread(b, 1, sizeof(b), f)) > 0)
		v.insert(v.end(), b, b + k);
	fclose(f);
	if (nul)
		v.push_back(0);
	return v;
}
static void case_d()
{
	int n = (int)opti("n", 300);
	const int G = 2, B = 64, WPL = G * B / 32;
	CK(cudaFree(0)); // create the primary context through the runtime
	CUcontext ctx;
	CKD(cuCtxGetCurrent(&ctx));
	std::string dir = exe_dir();
	std::vector<char> ptx = read_file(dir + "/idmod.ptx", true);
	std::vector<char> cubin = read_file(dir + "/idmod.cubin", false);
	CUmodule mod;
	CKD(cuModuleLoadData(&mod, ptx.data())); // driver JIT of the PTX
	CUfunction f_jit;
	CKD(cuModuleGetFunction(&f_jit, mod, "idmod_plain"));
	CUlibrary lib;
	CKD(cuLibraryLoadData(&lib, cubin.data(), nullptr, nullptr, 0, nullptr, nullptr, 0));
	CUkernel kern;
	CKD(cuLibraryGetKernel(&kern, lib, "idmod_plain"));
	CUfunction f_lib;
	CKD(cuKernelGetFunction(&f_lib, kern));
	cudaStream_t s;
	CK(cudaStreamCreateWithFlags(&s, cudaStreamNonBlocking));
	size_t nl = (size_t)n * 3, nrec = nl * WPL;
	IdRec *d_rec;
	CK(cudaMalloc(&d_rec, sizeof(IdRec) * nrec));
	CK(cudaMemset(d_rec, 0xff, sizeof(IdRec) * nrec));
	CK(cudaDeviceSynchronize());
	for (int i = 0; i < n; i++) {
		for (int m = 0; m < 3; m++) {
			uint32_t tag = (uint32_t)(3 * i + m);
			IdRec *base = d_rec + (size_t)tag * WPL;
			uint32_t kind = 50 + m; // 50 runtime API, 51 PTX JIT module, 52 cubin library
			if (m == 0) {
				k_plain<<<G, B, 0, s>>>(base, tag, kind);
				CK(cudaGetLastError());
			} else {
				void *args[] = {&base, &tag, &kind};
				CKD(cuLaunchKernel(m == 1 ? f_jit : f_lib, G, 1, 1, B, 1, 1, 0, (CUstream)s, args,
						   nullptr));
			}
		}
	}
	CK(cudaStreamSynchronize(s));
	std::vector<IdRec> h(nrec);
	CK(cudaMemcpy(h.data(), d_rec, sizeof(IdRec) * nrec, cudaMemcpyDeviceToHost));
	FILE *fr = open_out("d_records.csv");
	fputs(REC_HDR, fr);
	for (size_t i = 0; i < nrec; i++)
		write_rec(fr, "d", (long)(i / WPL), h[i]);
	fclose(fr);
	printf("d: %d x 3 launches (runtime, PTX JIT module, cubin library)\n", n);
	CK(cudaFree(d_rec));
	CKD(cuModuleUnload(mod));
	CKD(cuLibraryUnload(lib));
	CK(cudaStreamDestroy(s));
}

// ------------------------------------------------------------------ case e
static void case_e()
{
	int bs = (int)opti("bs", 128);
	if (bs > 128) {
		fprintf(stderr, "case e: bs must be <= 128 (launch bounds)\n");
		exit(1);
	}
	int reps = (int)opti("reps", 2);
	LongP p;
	memset(&p, 0, sizeof(p));
	p.dur_ns = (uint64_t)(optd("dur_ms", 300) * 1e6);
	p.gap_ns = (uint64_t)(optd("gap_us", 50) * 1e3);
	p.period_ns = (uint64_t)(optd("period_us", 1000) * 1e3);
	p.inj_ns = (uint64_t)(optd("inj_us", 200) * 1e3);
	p.loop_cap_iters = (uint64_t)opti("loop_cap_iters", 1ll << 24);
	p.sleep_ns = (uint32_t)opti("sleep_ns", 1000);
	p.bar_cap_iters = (uint64_t)opti("cap_iters", 1ll << 23);
	p.bar_cap_ns = (uint64_t)(optd("cap_ms", 2000) * 1e6);
	p.evcap = (uint32_t)opti("evcap", 1024);
	p.smpcap = (uint32_t)opti("smpcap", 2048);
	int occ = 0;
	CK(cudaOccupancyMaxActiveBlocksPerMultiprocessor(&occ, k_long, bs, 0));
	int blocks = (int)opti("blocks", occ * g_sms);
	unsigned warps = (unsigned)blocks * ((bs + 31) / 32);
	p.total = warps;
	CK(cudaMalloc(&p.rec, sizeof(IdRec) * 3 * warps));
	CK(cudaMalloc(&p.sum, sizeof(WarpSum) * warps));
	CK(cudaMalloc(&p.ev, sizeof(EvRec) * (size_t)p.evcap * warps));
	CK(cudaMalloc(&p.smp, sizeof(Sample) * (size_t)p.smpcap * warps));
	CK(cudaMalloc(&p.arrive, 8));
	CK(cudaMalloc(&p.caphits, 4));
	// prio=N: N intruder kernels on a high-priority stream while the long kernel runs on a
	// low-priority one (same context); 0 = off
	int prio = (int)opti("prio", 0);
	int intr_blocks = (int)opti("intr_blocks", g_sms);
	uint64_t intr_ns = (uint64_t)(optd("intr_ms", 5) * 1e6);
	int intr_gap_us = (int)(optd("intr_gap_ms", 20) * 1e3);
	cudaStream_t s_lo = 0, s_hi = 0;
	uint64_t *d_its = nullptr;
	FILE *fi = nullptr;
	if (prio > 0) {
		int least = 0, greatest = 0;
		CK(cudaDeviceGetStreamPriorityRange(&least, &greatest));
		CK(cudaStreamCreateWithPriority(&s_lo, cudaStreamNonBlocking, least));
		CK(cudaStreamCreateWithPriority(&s_hi, cudaStreamNonBlocking, greatest));
		CK(cudaMalloc(&d_its, sizeof(uint64_t) * 4 * prio));
		fi = open_out("e_intruders.csv");
		fprintf(fi, "launch,i,t0,t1,smid,capped_warps,blocks,prio_least,prio_greatest,host_issue_ms,long_dur_ms\n");
		printf("e: prio mode, %d intruders of %d blocks x %.1f ms, priorities %d (long) / %d (intruders)\n", prio,
		       intr_blocks, intr_ns / 1e6, least, greatest);
	}
	FILE *fr = open_out("e_records.csv");
	FILE *fs = open_out("e_warps.csv");
	FILE *fe = open_out("e_events.csv");
	FILE *fp = open_out("e_samples.csv");
	FILE *fl = open_out("e_launches.csv");
	fputs(REC_HDR, fr);
	fputs("launch,warp,iters,t_loop0,t_loop1,max_dt,max_dt_inj,min_nz_dt,gaps,inj_detected,inj_done,changes_gap,"
	      "changes_nogap,neg_dt,events,ev_overflow,samples,smp_overflow,flags\n",
	      fs);
	fputs("launch,warp,kind,t_prev,t_now,smid0,warpid0,smid1,warpid1\n", fe);
	fputs("launch,warp,t,smid,warpid\n", fp);
	fputs("launch,bs,blocks,warps,occ_blocks_per_sm,dur_ns,gap_ns,period_ns,inj_ns,sleep_ns,loop_cap_iters,"
	      "bar_cap_ns,evcap,smpcap,caphits,wall_ms,host_t0,host_t1\n",
	      fl);
	for (int L = 0; L < reps; L++) {
		CK(cudaMemset(p.rec, 0xff, sizeof(IdRec) * 3 * warps));
		CK(cudaMemset(p.sum, 0, sizeof(WarpSum) * warps));
		CK(cudaMemset(p.arrive, 0, 8));
		CK(cudaMemset(p.caphits, 0, 4));
		// the intruder slots are cleared here, before the long kernel: a memset issued
		// after it (legacy stream, no order against s_lo/s_hi) could queue behind the
		// full-occupancy kernel or land after an intruder wrote its timestamps
		if (prio > 0)
			CK(cudaMemset(d_its, 0, sizeof(uint64_t) * 4 * prio));
		CK(cudaDeviceSynchronize());
		std::vector<double> issue_ms(prio > 0 ? prio : 0);
		double t0 = now_s();
		k_long<<<blocks, bs, 0, s_lo>>>(p);
		CK(cudaGetLastError());
		if (prio > 0) {
			for (int i = 0; i < prio; i++) {
				usleep(intr_gap_us);
				issue_ms[i] = (now_s() - t0) * 1e3; // host time of the launch call, from k_long's launch
				k_intr<<<intr_blocks, 128, 0, s_hi>>>(intr_ns, d_its + 4 * i);
				CK(cudaGetLastError());
			}
		}
		CK(cudaDeviceSynchronize());
		double t1 = now_s();
		if (prio > 0) {
			std::vector<uint64_t> its(4 * prio);
			CK(cudaMemcpy(its.data(), d_its, sizeof(uint64_t) * 4 * prio, cudaMemcpyDeviceToHost));
			int least = 0, greatest = 0;
			CK(cudaDeviceGetStreamPriorityRange(&least, &greatest));
			for (int i = 0; i < prio; i++)
				fprintf(fi, "%d,%d,%llu,%llu,%llu,%llu,%d,%d,%d,%.3f,%.3f\n", L, i, (unsigned long long)its[4 * i],
					(unsigned long long)its[4 * i + 1], (unsigned long long)its[4 * i + 2],
					(unsigned long long)its[4 * i + 3], intr_blocks, least, greatest, issue_ms[i],
					p.dur_ns / 1e6);
		}
		std::vector<IdRec> hr(3 * warps);
		std::vector<WarpSum> hs(warps);
		std::vector<EvRec> he((size_t)p.evcap * warps);
		std::vector<Sample> hp((size_t)p.smpcap * warps);
		unsigned caphits = 0;
		CK(cudaMemcpy(hr.data(), p.rec, sizeof(IdRec) * hr.size(), cudaMemcpyDeviceToHost));
		CK(cudaMemcpy(hs.data(), p.sum, sizeof(WarpSum) * hs.size(), cudaMemcpyDeviceToHost));
		CK(cudaMemcpy(he.data(), p.ev, sizeof(EvRec) * he.size(), cudaMemcpyDeviceToHost));
		CK(cudaMemcpy(hp.data(), p.smp, sizeof(Sample) * hp.size(), cudaMemcpyDeviceToHost));
		CK(cudaMemcpy(&caphits, p.caphits, 4, cudaMemcpyDeviceToHost));
		for (auto &r : hr)
			write_rec(fr, "e", L, r);
		uint64_t tot_gaps = 0, tot_ch = 0, tot_inj = 0;
		for (unsigned w = 0; w < warps; w++) {
			const WarpSum &s = hs[w];
			fprintf(fs, "%d,%u,%llu,%llu,%llu,%llu,%llu,%llu,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n", L, w,
				(unsigned long long)s.iters, (unsigned long long)s.t_loop0,
				(unsigned long long)s.t_loop1, (unsigned long long)s.max_dt,
				(unsigned long long)s.max_dt_inj,
				(unsigned long long)s.min_nz_dt, s.gaps, s.inj_detected, s.inj_done, s.changes_gap,
				s.changes_nogap, s.neg_dt, s.events, s.ev_overflow, s.samples, s.smp_overflow, s.flags);
			tot_gaps += s.gaps;
			tot_ch += s.changes_gap + s.changes_nogap;
			tot_inj += s.inj_detected;
			uint32_t ne = s.events < p.evcap ? s.events : p.evcap;
			for (uint32_t i = 0; i < ne; i++) {
				const EvRec &e = he[(size_t)w * p.evcap + i];
				fprintf(fe, "%d,%u,%u,%llu,%llu,%u,%u,%u,%u\n", L, e.warp, e.kind,
					(unsigned long long)e.t_prev, (unsigned long long)e.t_now, e.smid0, e.warpid0,
					e.smid1, e.warpid1);
			}
			uint32_t np = s.samples < p.smpcap ? s.samples : p.smpcap;
			for (uint32_t i = 0; i < np; i++) {
				const Sample &q = hp[(size_t)w * p.smpcap + i];
				fprintf(fp, "%d,%u,%llu,%u,%u\n", L, q.warp, (unsigned long long)q.t, q.smid, q.warpid);
			}
		}
		fprintf(fl, "%d,%d,%d,%u,%d,%llu,%llu,%llu,%llu,%u,%llu,%llu,%u,%u,%u,%.3f,%.6f,%.6f\n", L, bs,
			blocks, warps, occ, (unsigned long long)p.dur_ns, (unsigned long long)p.gap_ns,
			(unsigned long long)p.period_ns, (unsigned long long)p.inj_ns, p.sleep_ns,
			(unsigned long long)p.loop_cap_iters, (unsigned long long)p.bar_cap_ns, p.evcap, p.smpcap,
			caphits, (t1 - t0) * 1e3, t0, t1);
		printf("e: launch %d blocks=%d warps=%u wall=%.1f ms warp-gaps=%llu id-changes=%llu "
		       "injected-detected=%llu caphits=%u\n",
		       L, blocks, warps, (t1 - t0) * 1e3, (unsigned long long)tot_gaps, (unsigned long long)tot_ch,
		       (unsigned long long)tot_inj, caphits);
		fflush(stdout);
	}
	fclose(fr);
	fclose(fs);
	fclose(fe);
	fclose(fp);
	fclose(fl);
	if (fi)
		fclose(fi);
}

int main(int argc, char **argv)
{
	if (argc < 3) {
		fprintf(stderr, "usage: %s <a|b|c|d|e> <outdir> [key=value ...]\n", argv[0]);
		return 1;
	}
	std::string cs = argv[1];
	g_out = argv[2];
	mkdir(g_out.c_str(), 0775);
	for (int i = 3; i < argc; i++) {
		const char *eq = strchr(argv[i], '=');
		if (!eq) {
			fprintf(stderr, "bad option %s\n", argv[i]);
			return 1;
		}
		g_kv[std::string(argv[i], eq - argv[i])] = eq + 1;
	}
	FILE *meta = open_out(cs + "_meta.json");
	fprintf(meta, "{\n\"case\": \"%s\",\n\"pid\": %d,\n", cs.c_str(), (int)getpid());
	dev_info(meta);
	fprintf(meta, "\"options\": {");
	bool first = true;
	for (auto &kv : g_kv) {
		fprintf(meta, "%s\"%s\": \"%s\"", first ? "" : ", ", kv.first.c_str(), kv.second.c_str());
		first = false;
	}
	fprintf(meta, "}\n}\n");
	fclose(meta);
	if (cs == "a")
		case_a();
	else if (cs == "b")
		case_b();
	else if (cs == "c")
		case_c();
	else if (cs == "d")
		case_d();
	else if (cs == "e")
		case_e();
	else {
		fprintf(stderr, "unknown case %s\n", cs.c_str());
		return 1;
	}
	return 0;
}
