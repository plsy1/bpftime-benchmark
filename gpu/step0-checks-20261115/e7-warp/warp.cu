// warp.cu: step-0 check 7, standalone half. A __noinline__ warp-level reservation helper, as the
// shared-pool (C) and per-SM-ring (D) designs would use, called under partial and divergent masks.
//   warp <case|all> <ok|bad_rank|bad_leader0|all> <outdir> [key=value ...]
// cases: early lowlanes nested loops memcap (see README). Ground truth is kept by the CALLER,
// outside the helper: every calling thread writes a unique tag at the returned index, bumps a
// per-index write counter, and counts its own calls. The host then checks
//   total reserved == total calls, every index in [0, total) written exactly once,
//   every (thread, call#) tag present exactly once, no index >= capacity.
#include <cuda.h>
#include <cuda_runtime.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <map>
#include <string>
#include <vector>

#define CK(x)                                                                                      \
	do {                                                                                       \
		cudaError_t e_ = (x);                                                              \
		if (e_ != cudaSuccess) {                                                           \
			fprintf(stderr, "CUDA error %s at line %d: %s\n", cudaGetErrorString(e_), __LINE__, #x); \
			exit(2);                                                                   \
		}                                                                                  \
	} while (0)

#define CKD(x)                                                                                     \
	do {                                                                                       \
		CUresult r_ = (x);                                                                 \
		if (r_ != CUDA_SUCCESS) {                                                          \
			const char *s_ = nullptr;                                                  \
			cuGetErrorString(r_, &s_);                                                 \
			fprintf(stderr, "CU error %d (%s) at line %d: %s\n", (int)r_, s_ ? s_ : "?", __LINE__, #x); \
			exit(2);                                                                   \
		}                                                                                  \
	} while (0)

#define NSITE 4
#define BS 128

// ------------------------------------------------------------------ the helpers under test
// Return value: bits 0-31 index, 32-37 popc(mask), bit 40 "this lane is the lowest active
// lane", bits 48-52 the lowest active lane. The packing fields always come from the true mask;
// only the reservation itself differs between the variants.
__device__ __forceinline__ unsigned lanemask_lt()
{
	unsigned m;
	asm volatile("mov.u32 %0, %%lanemask_lt;" : "=r"(m));
	return m;
}
__device__ __forceinline__ unsigned laneid()
{
	unsigned l;
	asm volatile("mov.u32 %0, %%laneid;" : "=r"(l));
	return l;
}
__device__ __forceinline__ unsigned long long pack(unsigned idx, unsigned n, unsigned lane, unsigned leader)
{
	return (unsigned long long)idx | ((unsigned long long)n << 32) | ((unsigned long long)(lane == leader) << 40) |
	       ((unsigned long long)leader << 48);
}

// The design under test.
extern "C" __device__ __noinline__ unsigned long long reserve_ok(unsigned *cursor)
{
	unsigned mask = __activemask();
	unsigned lane = laneid();
	unsigned leader = __ffs(mask) - 1;
	unsigned n = __popc(mask);
	unsigned rank = __popc(mask & lanemask_lt());
	unsigned base = 0;
	if (lane == leader)
		base = atomicAdd(cursor, n);
	base = __shfl_sync(mask, base, leader);
	__syncwarp(mask);
	return pack(base + rank, n, lane, leader);
}

// Control 1 (no undefined operation): rank from the full mask, i.e. rank = lane. With absent
// lanes this leaves holes and writes past the reserved range or into another warp's range.
extern "C" __device__ __noinline__ unsigned long long reserve_bad_rank(unsigned *cursor)
{
	unsigned mask = __activemask();
	unsigned lane = laneid();
	unsigned leader = __ffs(mask) - 1;
	unsigned n = __popc(mask);
	unsigned rank = __popc(0xffffffffu & lanemask_lt());
	unsigned base = 0;
	if (lane == leader)
		base = atomicAdd(cursor, n);
	base = __shfl_sync(mask, base, leader);
	__syncwarp(mask);
	return pack(base + rank, n, lane, leader);
}

// Control 2: leader fixed to lane 0. The shuffle mask is still the active mask (no hang); when
// lane 0 is absent nobody reserves and the shuffled value is undefined (CUDA docs).
extern "C" __device__ __noinline__ unsigned long long reserve_bad_leader0(unsigned *cursor)
{
	unsigned mask = __activemask();
	unsigned lane = laneid();
	unsigned leader = __ffs(mask) - 1;
	unsigned n = __popc(mask);
	unsigned rank = __popc(mask & lanemask_lt());
	unsigned base = 0;
	if (lane == 0)
		base = atomicAdd(cursor, n);
	base = __shfl_sync(mask, base, 0);
	__syncwarp(mask);
	return pack(base + rank, n, lane, leader);
}

// ------------------------------------------------------------------ caller-side ground truth
struct Ctx {
	unsigned *cursor;
	unsigned long long *tags; // [cap]
	unsigned *wcount;         // [cap]
	unsigned cap;
	unsigned *calls;               // [nthreads]
	unsigned long long *hist;      // [NSITE][33] warp-calls by popc(mask)
	unsigned long long *lead_hist; // [NSITE][32] warp-calls by lowest active lane
	unsigned *oob;
	unsigned char *site_of; // [cap] call site of the lane that got this index (0xff = unwritten)
	unsigned char *resv_n;  // [cap] popc(mask) stored by the leader at its index (= the reservation
	                        // base for the correct helper); 0 elsewhere
	const unsigned *in; // [iters][nthreads]
	unsigned *data;     // memcap scratch
	unsigned ndata;
	unsigned iters;
	unsigned nthreads;
};

template <int V> __device__ __forceinline__ unsigned long long reserve(unsigned *c)
{
	if constexpr (V == 0)
		return reserve_ok(c);
	else if constexpr (V == 1)
		return reserve_bad_rank(c);
	else
		return reserve_bad_leader0(c);
}

template <int V> __device__ __forceinline__ void rec_call(const Ctx &c, unsigned gtid, unsigned &ncall, int site)
{
	unsigned long long r = reserve<V>(c.cursor);
	unsigned idx = (unsigned)r;
	unsigned n = (unsigned)(r >> 32) & 0x3f;
	unsigned is_leader = (unsigned)(r >> 40) & 1;
	unsigned leader = (unsigned)(r >> 48) & 0x1f;
	if (idx < c.cap) {
		c.tags[idx] = ((unsigned long long)gtid << 32) | ncall;
		// caller-side bookkeeping of which call sites share one reservation (helper unchanged)
		c.site_of[idx] = (unsigned char)site;
		if (is_leader)
			c.resv_n[idx] = (unsigned char)n;
		atomicAdd(&c.wcount[idx], 1u);
	} else {
		atomicAdd(c.oob, 1u);
	}
	if (is_leader) {
		atomicAdd(&c.hist[site * 33 + n], 1ull);
		atomicAdd(&c.lead_hist[site * 32 + leader], 1ull);
	}
	ncall++;
}

// 1. early: data-dependent early returns (about 1 in 16 per iteration), calls on ~3 in 4.
template <int V> __global__ void __launch_bounds__(BS, 12) k_early(Ctx c)
{
	unsigned gtid = blockIdx.x * blockDim.x + threadIdx.x;
	unsigned ncall = 0;
	for (unsigned it = 0; it < c.iters; it++) {
		unsigned v = c.in[it * c.nthreads + gtid];
		if ((v & 15) == 0) {
			c.calls[gtid] = ncall;
			return;
		}
		if (v & 0x30)
			rec_call<V>(c, gtid, ncall, 0);
	}
	c.calls[gtid] = ncall;
}

// 2. lowlanes: lane 0 and the lowest 1..31 lanes (a per-warp, data-chosen cut) return before
// any call; more lanes return later; two call sites.
template <int V> __global__ void __launch_bounds__(BS, 12) k_lowlanes(Ctx c)
{
	unsigned gtid = blockIdx.x * blockDim.x + threadIdx.x;
	unsigned lane = threadIdx.x & 31;
	unsigned ncall = 0;
	unsigned cut = 1 + c.in[gtid & ~31u] % 31; // same word for the whole warp
	if (lane < cut) {
		c.calls[gtid] = 0;
		return;
	}
	for (unsigned it = 0; it < c.iters; it++) {
		unsigned v = c.in[it * c.nthreads + gtid];
		if ((v & 31) == 0) {
			c.calls[gtid] = ncall;
			return;
		}
		if (v & 0x40)
			rec_call<V>(c, gtid, ncall, 0);
		else if (v & 0x80)
			rec_call<V>(c, gtid, ncall, 1);
	}
	c.calls[gtid] = ncall;
}

// 3. nested: nested divergent branches with the helper at different call sites.
template <int V> __global__ void __launch_bounds__(BS, 12) k_nested(Ctx c)
{
	unsigned gtid = blockIdx.x * blockDim.x + threadIdx.x;
	unsigned ncall = 0;
	for (unsigned it = 0; it < c.iters; it++) {
		unsigned v = c.in[it * c.nthreads + gtid];
		if ((v & 63) == 0) {
			c.calls[gtid] = ncall;
			return;
		}
		if (v & 0x100) {
			if (v & 0x200) {
				rec_call<V>(c, gtid, ncall, 0);
			} else {
				if (v & 0x400)
					rec_call<V>(c, gtid, ncall, 1);
			}
		} else if (v & 0x800) {
			rec_call<V>(c, gtid, ncall, 2);
		}
	}
	c.calls[gtid] = ncall;
}

// 4. loops: calls inside a loop whose trip count depends on the lane and the data, with a
// data-dependent break.
template <int V> __global__ void __launch_bounds__(BS, 12) k_loops(Ctx c)
{
	unsigned gtid = blockIdx.x * blockDim.x + threadIdx.x;
	unsigned lane = threadIdx.x & 31;
	unsigned ncall = 0;
	for (unsigned it = 0; it < c.iters; it++) {
		unsigned v = c.in[it * c.nthreads + gtid];
		if ((v & 63) == 0) {
			c.calls[gtid] = ncall;
			return;
		}
		unsigned trip = (v >> 8) % 6 + (lane & 3); // 0..8
		for (unsigned j = 0; j < trip; j++) {
			rec_call<V>(c, gtid, ncall, 0);
			if (((v >> (16 + j)) & 3) == 0)
				break;
		}
	}
	c.calls[gtid] = ncall;
}

// 5. memcap: a call immediately before each global load and store of a data-dependent
// gather/scatter (bpftime's __memcapture inserts a call before every ld/st).
template <int V> __global__ void __launch_bounds__(BS, 12) k_memcap(Ctx c)
{
	unsigned gtid = blockIdx.x * blockDim.x + threadIdx.x;
	unsigned ncall = 0;
	for (unsigned it = 0; it < c.iters; it++) {
		unsigned v = c.in[it * c.nthreads + gtid];
		unsigned a = v % c.ndata;
		rec_call<V>(c, gtid, ncall, 0);
		unsigned x = c.data[a]; // load
		if (x & 1) {
			rec_call<V>(c, gtid, ncall, 1);
			c.data[(a + 1) % c.ndata] = x + v; // store under divergence
		}
		unsigned trip = (x >> 1) & 3;
		for (unsigned j = 0; j < trip; j++) {
			unsigned b = (a + 7 * j + 1) % c.ndata;
			rec_call<V>(c, gtid, ncall, 2);
			x += c.data[b]; // load in a loop
		}
		if ((x & 15) == 0) {
			c.calls[gtid] = ncall;
			return;
		}
		rec_call<V>(c, gtid, ncall, 3);
		c.data[a] = x; // store
	}
	c.calls[gtid] = ncall;
}

// ------------------------------------------------------------------ host
static std::map<std::string, std::string> g_kv;
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
static uint64_t splitmix(uint64_t &x)
{
	uint64_t z = (x += 0x9e3779b97f4a7c15ull);
	z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ull;
	z = (z ^ (z >> 27)) * 0x94d049bb133111ebull;
	return z ^ (z >> 31);
}

typedef void (*KFn)(Ctx);
struct CaseDef {
	const char *name;
	unsigned iters;
	unsigned max_per_iter; // upper bound of calls per thread per iteration
	KFn fn[3];
};
#define KSET(k) {k<0>, k<1>, k<2>}
static CaseDef g_cases[] = {
	{"early", 64, 1, KSET(k_early)},  {"lowlanes", 64, 1, KSET(k_lowlanes)}, {"nested", 64, 1, KSET(k_nested)},
	{"loops", 32, 8, KSET(k_loops)}, {"memcap", 32, 6, KSET(k_memcap)},
};
static const char *g_vnames[3] = {"ok", "bad_rank", "bad_leader0"};

struct Totals {
	unsigned long long calls = 0, reserved = 0, count_mismatch = 0, missing = 0, dbl = 0, oob = 0, bad_tag = 0,
			   lost_call = 0, dup_tag = 0, hist_mismatch = 0, launches = 0;
	std::vector<unsigned long long> hist = std::vector<unsigned long long>(NSITE * 33, 0);
	std::vector<unsigned long long> lead = std::vector<unsigned long long>(NSITE * 32, 0);
	std::vector<unsigned long long> site_hist = std::vector<unsigned long long>(NSITE + 1, 0);
};

int main(int argc, char **argv)
{
	if (argc < 4) {
		fprintf(stderr, "usage: %s <case|all> <ok|bad_rank|bad_leader0|all> <outdir> [key=value ...]\n", argv[0]);
		return 1;
	}
	std::string want_case = argv[1], want_var = argv[2], out = argv[3];
	for (int i = 4; i < argc; i++) {
		const char *eq = strchr(argv[i], '=');
		if (!eq) {
			fprintf(stderr, "bad option %s\n", argv[i]);
			return 1;
		}
		g_kv[std::string(argv[i], eq - argv[i])] = eq + 1;
	}
	mkdir(out.c_str(), 0775);
	unsigned long long min_calls = (unsigned long long)opti("min_calls", 1200000);
	unsigned long long ctl_min_calls = (unsigned long long)opti("ctl_min_calls", 100000);
	int max_launches = (int)opti("max_launches", 60);
	uint64_t seed0 = (uint64_t)opti("seed", 20261115);
	long long iters_override = opti("iters", 0);
	cudaDeviceProp prop;
	CK(cudaGetDeviceProperties(&prop, 0));
	// ptx=<file>: load the kernels from this PTX with cuModuleLoadData (driver JIT, as bpftime
	// loads patched modules) and launch them with cuLaunchKernel, instead of the nvcc-built ones
	std::string ptx_path = g_kv.count("ptx") ? g_kv["ptx"] : "";
	CUmodule jmod = nullptr;
	if (!ptx_path.empty()) {
		CK(cudaFree(0));
		FILE *pf = fopen(ptx_path.c_str(), "rb");
		if (!pf) {
			perror(ptx_path.c_str());
			return 2;
		}
		std::vector<char> ptx;
		char buf[65536];
		size_t k;
		while ((k = fread(buf, 1, sizeof(buf), pf)) > 0)
			ptx.insert(ptx.end(), buf, buf + k);
		fclose(pf);
		ptx.push_back(0);
		CKD(cuModuleLoadData(&jmod, ptx.data()));
		printf("kernels from %s (driver JIT)\n", ptx_path.c_str());
	}
	auto jit_fn = [&](const char *name, int v) {
		char mangled[128];
		snprintf(mangled, sizeof(mangled), "_Z%zu%sILi%dEEv3Ctx", strlen(name) + 2, (std::string("k_") + name).c_str(), v);
		CUfunction f;
		CKD(cuModuleGetFunction(&f, jmod, mangled));
		return f;
	};

	// largest grid and capacity over all cases
	int occ_min = 1 << 30;
	for (auto &cd : g_cases)
		for (int v = 0; v < 3; v++) {
			int occ = 0;
			if (jmod)
				CKD(cuOccupancyMaxActiveBlocksPerMultiprocessor(&occ, jit_fn(cd.name, v), BS, 0));
			else
				CK(cudaOccupancyMaxActiveBlocksPerMultiprocessor(&occ, cd.fn[v], BS, 0));
			if (occ < occ_min)
				occ_min = occ;
		}
	int blocks_full = (int)opti("blocks", 0);
	unsigned max_threads = (unsigned)((blocks_full ? blocks_full : 16 * prop.multiProcessorCount) * BS);
	unsigned max_cap = 0;
	for (auto &cd : g_cases) {
		unsigned it = iters_override ? (unsigned)iters_override : cd.iters;
		if (max_threads * it * cd.max_per_iter > max_cap)
			max_cap = max_threads * it * cd.max_per_iter;
	}
	max_cap += 64;
	Ctx c;
	memset(&c, 0, sizeof(c));
	CK(cudaMalloc(&c.cursor, 4));
	CK(cudaMalloc(&c.tags, sizeof(unsigned long long) * max_cap));
	CK(cudaMalloc(&c.wcount, 4ull * max_cap));
	CK(cudaMalloc(&c.calls, 4ull * max_threads));
	CK(cudaMalloc(&c.hist, sizeof(unsigned long long) * NSITE * 33));
	CK(cudaMalloc(&c.lead_hist, sizeof(unsigned long long) * NSITE * 32));
	CK(cudaMalloc(&c.oob, 4));
	CK(cudaMalloc(&c.site_of, max_cap));
	CK(cudaMalloc(&c.resv_n, max_cap));
	unsigned *d_in;
	size_t max_in = (size_t)max_threads * 64;
	if (iters_override > 64)
		max_in = (size_t)max_threads * iters_override;
	CK(cudaMalloc(&d_in, 4 * max_in));
	c.ndata = 65536;
	CK(cudaMalloc(&c.data, 4ull * c.ndata));
	c.in = d_in;

	FILE *fr = fopen((out + "/results.csv").c_str(), "w");
	FILE *fh = fopen((out + "/hist.csv").c_str(), "w");
	FILE *fd = fopen((out + "/leader.csv").c_str(), "w");
	FILE *fm = fopen((out + "/meta.json").c_str(), "w");
	if (!fr || !fh || !fd || !fm) {
		perror("output");
		return 2;
	}
	fprintf(fm, "{\"pid\": %d, \"device\": \"%s\", \"sms\": %d, \"bs\": %d, \"min_calls\": %llu, \"ctl_min_calls\": %llu, "
		    "\"seed\": %llu, \"capacity\": %u, \"ptx\": \"%s\"}\n",
		(int)getpid(), prop.name, prop.multiProcessorCount, BS, min_calls, ctl_min_calls,
		(unsigned long long)seed0, max_cap, ptx_path.c_str());
	fclose(fm);
	fputs("case,variant,launch,seed,blocks,threads,occ_blocks_per_sm,iters,calls,reserved,count_mismatch,missing,"
	      "double,oob,bad_tag,lost_call,dup_tag,hist_mismatch,violations,warp_calls,partial_warp_calls,"
	      "lane0_absent_warp_calls,reservations,mixed_site_reservations,resv_tiling_errors,ms\n",
	      fr);
	fputs("case,variant,site,popc,warp_calls\n", fh);
	fputs("case,variant,site,leader_lane,warp_calls\n", fd);
	FILE *fs = fopen((out + "/sites.csv").c_str(), "w");
	if (!fs) {
		perror("sites.csv");
		return 2;
	}
	fputs("case,variant,distinct_sites,reservations\n", fs);

	std::vector<unsigned> h_in(max_in), h_calls(max_threads), h_wc, h_data(c.ndata);
	std::vector<unsigned long long> h_tags;
	std::vector<unsigned char> h_site, h_rn;
	int ret = 0;
	for (auto &cd : g_cases) {
		if (want_case != "all" && want_case != cd.name)
			continue;
		for (int v = 0; v < 3; v++) {
			if (want_var != "all" && want_var != g_vnames[v])
				continue;
			int occ = 0;
			CUfunction jf = nullptr;
			if (jmod) {
				jf = jit_fn(cd.name, v);
				CKD(cuOccupancyMaxActiveBlocksPerMultiprocessor(&occ, jf, BS, 0));
			} else {
				CK(cudaOccupancyMaxActiveBlocksPerMultiprocessor(&occ, cd.fn[v], BS, 0));
			}
			int blocks = blocks_full ? blocks_full : occ * prop.multiProcessorCount;
			unsigned nth = (unsigned)blocks * BS;
			unsigned iters = iters_override ? (unsigned)iters_override : cd.iters;
			unsigned cap = nth * iters * cd.max_per_iter + 64;
			if (cap > max_cap)
				cap = max_cap;
			c.cap = cap;
			c.iters = iters;
			c.nthreads = nth;
			unsigned long long target = v == 0 ? min_calls : ctl_min_calls;
			Totals T;
			for (int L = 0; L < max_launches && T.calls < target; L++) {
				// case index, not the name's first letter ("lowlanes" and "loops" share it)
				uint64_t seed = seed0 * 1000003ull + (uint64_t)(&cd - g_cases) * 7919ull + v * 104729ull + L;
				uint64_t st = seed;
				for (size_t i = 0; i < (size_t)iters * nth; i++)
					h_in[i] = (unsigned)splitmix(st);
				for (unsigned i = 0; i < c.ndata; i++)
					h_data[i] = (unsigned)splitmix(st);
				CK(cudaMemcpy(d_in, h_in.data(), 4ull * iters * nth, cudaMemcpyHostToDevice));
				CK(cudaMemcpy(c.data, h_data.data(), 4ull * c.ndata, cudaMemcpyHostToDevice));
				CK(cudaMemset(c.cursor, 0, 4));
				CK(cudaMemset(c.oob, 0, 4));
				CK(cudaMemset(c.wcount, 0, 4ull * cap));
				CK(cudaMemset(c.site_of, 0xff, cap));
				CK(cudaMemset(c.resv_n, 0, cap));
				CK(cudaMemset(c.calls, 0xff, 4ull * nth)); // a thread that never stores its count shows up
				CK(cudaMemset(c.hist, 0, sizeof(unsigned long long) * NSITE * 33));
				CK(cudaMemset(c.lead_hist, 0, sizeof(unsigned long long) * NSITE * 32));
				CK(cudaDeviceSynchronize());
				double t0 = now_s();
				if (jf) {
					void *args[] = {&c};
					CKD(cuLaunchKernel(jf, blocks, 1, 1, BS, 1, 1, 0, 0, args, nullptr));
				} else {
					cd.fn[v]<<<blocks, BS>>>(c);
					CK(cudaGetLastError());
				}
				CK(cudaDeviceSynchronize());
				double t1 = now_s();
				unsigned total = 0, oob = 0;
				CK(cudaMemcpy(&total, c.cursor, 4, cudaMemcpyDeviceToHost));
				CK(cudaMemcpy(&oob, c.oob, 4, cudaMemcpyDeviceToHost));
				CK(cudaMemcpy(h_calls.data(), c.calls, 4ull * nth, cudaMemcpyDeviceToHost));
				unsigned long long hist[NSITE * 33], lead[NSITE * 32];
				CK(cudaMemcpy(hist, c.hist, sizeof(hist), cudaMemcpyDeviceToHost));
				CK(cudaMemcpy(lead, c.lead_hist, sizeof(lead), cudaMemcpyDeviceToHost));
				unsigned span = total < cap ? total : cap;
				h_wc.resize(span);
				h_tags.resize(span);
				if (span) {
					CK(cudaMemcpy(h_wc.data(), c.wcount, 4ull * span, cudaMemcpyDeviceToHost));
					CK(cudaMemcpy(h_tags.data(), c.tags, 8ull * span, cudaMemcpyDeviceToHost));
				}
				h_site.resize(span);
				h_rn.resize(span);
				if (span) {
					CK(cudaMemcpy(h_site.data(), c.site_of, span, cudaMemcpyDeviceToHost));
					CK(cudaMemcpy(h_rn.data(), c.resv_n, span, cudaMemcpyDeviceToHost));
				}
				// reservations of the correct helper: [base, base + n) from the leader's entry.
				// They must tile [0, total); each is classified by the number of distinct call
				// sites among its lanes. Only meaningful for the correct helper (V == 0).
				unsigned long long resv = 0, mixed = 0, tiling = 0;
				unsigned long long site_hist[NSITE + 1] = {0};
				if (v == 0) {
					unsigned i = 0;
					while (i < span) {
						unsigned rn = h_rn[i];
						if (rn == 0) {
							tiling++;
							i++;
							continue;
						}
						if ((unsigned long long)i + rn > span) {
							tiling++;
							break;
						}
						unsigned smask = 0;
						for (unsigned k = i; k < i + rn; k++) {
							if (k > i && h_rn[k])
								tiling++;
							if (h_site[k] >= NSITE)
								tiling++;
							else
								smask |= 1u << h_site[k];
						}
						int ds = __builtin_popcount(smask);
						site_hist[ds]++;
						resv++;
						if (ds > 1)
							mixed++;
						i += rn;
					}
					if (span != total)
						tiling++;
				}
				// --- checks
				unsigned long long calls = 0, missing = 0, dbl = 0, bad_tag = 0, lost = 0, dup = 0,
						   unset = 0;
				std::vector<unsigned long long> off(nth + 1, 0);
				for (unsigned t = 0; t < nth; t++) {
					unsigned k = h_calls[t];
					if (k == 0xffffffffu) {
						unset++;
						k = 0;
					}
					off[t + 1] = off[t] + k;
				}
				calls = off[nth];
				std::vector<unsigned char> seen(calls, 0);
				// writes beyond the cap cannot be checked here; they are counted in oob
				for (unsigned i = 0; i < span; i++) {
					if (h_wc[i] == 0) {
						missing++;
						continue;
					}
					if (h_wc[i] > 1)
						dbl += h_wc[i] - 1;
					unsigned long long tg = h_tags[i];
					unsigned t = (unsigned)(tg >> 32), k = (unsigned)tg;
					if (t >= nth || h_calls[t] == 0xffffffffu || k >= h_calls[t]) {
						bad_tag++;
						continue;
					}
					unsigned char &s = seen[off[t] + k];
					if (s < 255)
						s++;
				}
				// with a double write only the last tag survives, so lost calls are expected
				// alongside doubles; both count as violations
				for (unsigned long long i = 0; i < calls; i++) {
					if (seen[i] == 0)
						lost++;
					else if (seen[i] > 1)
						dup += seen[i] - 1;
				}
				unsigned long long cm = total > calls ? total - calls : calls - total;
				unsigned long long hsum = 0, wcalls = 0, partial = 0, l0abs = 0;
				for (int s = 0; s < NSITE; s++) {
					for (int p = 0; p <= 32; p++) {
						hsum += (unsigned long long)p * hist[s * 33 + p];
						wcalls += hist[s * 33 + p];
						if (p < 32)
							partial += hist[s * 33 + p];
						T.hist[s * 33 + p] += hist[s * 33 + p];
					}
					for (int l = 0; l < 32; l++) {
						if (l > 0)
							l0abs += lead[s * 32 + l];
						T.lead[s * 32 + l] += lead[s * 32 + l];
					}
				}
				unsigned long long hm = hsum != calls ? 1 : 0;
				unsigned long long viol = cm + missing + dbl + oob + bad_tag + lost + dup + hm + unset;
				fprintf(fr, "%s,%s,%d,%llu,%d,%u,%d,%u,%llu,%u,%llu,%llu,%llu,%u,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%llu,%.3f\n",
					cd.name, g_vnames[v], L, (unsigned long long)seed, blocks, nth, occ, iters, calls, total,
					cm, missing, dbl, oob, bad_tag, lost, dup, hm, viol, wcalls, partial, l0abs, resv, mixed,
					tiling, (t1 - t0) * 1e3);
				for (int k = 0; k <= NSITE; k++)
					T.site_hist[k] += site_hist[k];
				fflush(fr);
				T.calls += calls;
				T.reserved += total;
				T.count_mismatch += cm;
				T.missing += missing;
				T.dbl += dbl;
				T.oob += oob;
				T.bad_tag += bad_tag;
				T.lost_call += lost + unset;
				T.dup_tag += dup;
				T.hist_mismatch += hm;
				T.launches++;
			}
			unsigned long long viol = T.count_mismatch + T.missing + T.dbl + T.oob + T.bad_tag + T.lost_call +
						  T.dup_tag + T.hist_mismatch;
			for (int s = 0; s < NSITE; s++) {
				for (int p = 0; p <= 32; p++)
					if (T.hist[s * 33 + p])
						fprintf(fh, "%s,%s,%d,%d,%llu\n", cd.name, g_vnames[v], s, p, T.hist[s * 33 + p]);
				for (int l = 0; l < 32; l++)
					if (T.lead[s * 32 + l])
						fprintf(fd, "%s,%s,%d,%d,%llu\n", cd.name, g_vnames[v], s, l, T.lead[s * 32 + l]);
			}
			if (v == 0)
				for (int k = 1; k <= NSITE; k++)
					if (T.site_hist[k])
						fprintf(fs, "%s,%s,%d,%llu\n", cd.name, g_vnames[v], k, T.site_hist[k]);
			fflush(fh);
			fflush(fd);
			fflush(fs);
			printf("%-9s %-11s launches=%llu calls=%llu violations=%llu (count %llu, missing %llu, double %llu, "
			       "oob %llu, bad_tag %llu, lost %llu, dup %llu, hist %llu) blocks=%d occ=%d\n",
			       cd.name, g_vnames[v], T.launches, T.calls, viol, T.count_mismatch, T.missing, T.dbl, T.oob,
			       T.bad_tag, T.lost_call, T.dup_tag, T.hist_mismatch, blocks, occ);
			fflush(stdout);
		}
	}
	fclose(fr);
	fclose(fh);
	fclose(fd);
	fclose(fs);
	return ret;
}
