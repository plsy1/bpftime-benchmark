// rec.cuh: id record shared by ids.cu (runtime API) and idmod.cu (second module, driver API).
// Every special register is read with inline PTX; build/*.ptx and build/*.sass show what survives.
#pragma once
#include <stdint.h>

// One record = one warp at one point (entry, exit, ...). 80 bytes, plain POD.
struct IdRec {
	uint64_t gridid;   // mov.u64 %gridid (PTX ISA: .u64 since PTX 3.0)
	uint64_t gridid32; // mov.u32 %gridid, zero-extended (to compare with the low half)
	uint64_t gtime;    // mov.u64 %globaltimer (ns)
	uint32_t tag;      // case/launch tag set by the host
	uint32_t kind;     // 0 entry, 1 exit (after the barrier), 2 graph node, ...
	uint32_t smid, warpid, laneid, nsmid, nwarpid;
	uint32_t ctax, ctay, ctaz;
	uint32_t warp_in_cta; // linear tid / 32
	uint32_t warp_lin;    // linear warp index within the launch
	uint32_t aux;         // spin iterations, replay counter, ...
	uint32_t flags;       // bit0 iteration cap hit, bit1 time cap hit
};

#ifdef __CUDACC__
__device__ __forceinline__ uint64_t rd_gtime()
{
	uint64_t t;
	asm volatile("mov.u64 %0, %%globaltimer;" : "=l"(t));
	return t;
}
__device__ __forceinline__ uint32_t rd_smid()
{
	uint32_t v;
	asm volatile("mov.u32 %0, %%smid;" : "=r"(v));
	return v;
}
__device__ __forceinline__ uint32_t rd_warpid()
{
	uint32_t v;
	asm volatile("mov.u32 %0, %%warpid;" : "=r"(v));
	return v;
}
__device__ __forceinline__ uint32_t rd_laneid()
{
	uint32_t v;
	asm volatile("mov.u32 %0, %%laneid;" : "=r"(v));
	return v;
}

// Fill every hardware field of r (the caller sets tag/kind/aux/flags/warp_lin).
__device__ __forceinline__ void read_ids(IdRec &r)
{
	uint32_t g32;
	asm volatile("mov.u64 %0, %%gridid;" : "=l"(r.gridid));
	asm volatile("mov.u32 %0, %%gridid;" : "=r"(g32));
	r.gridid32 = g32;
	asm volatile("mov.u32 %0, %%smid;" : "=r"(r.smid));
	asm volatile("mov.u32 %0, %%warpid;" : "=r"(r.warpid));
	asm volatile("mov.u32 %0, %%laneid;" : "=r"(r.laneid));
	asm volatile("mov.u32 %0, %%nsmid;" : "=r"(r.nsmid));
	asm volatile("mov.u32 %0, %%nwarpid;" : "=r"(r.nwarpid));
	asm volatile("mov.u32 %0, %%ctaid.x;" : "=r"(r.ctax));
	asm volatile("mov.u32 %0, %%ctaid.y;" : "=r"(r.ctay));
	asm volatile("mov.u32 %0, %%ctaid.z;" : "=r"(r.ctaz));
	asm volatile("mov.u64 %0, %%globaltimer;" : "=l"(r.gtime));
	uint32_t tid = threadIdx.x + blockDim.x * (threadIdx.y + blockDim.y * threadIdx.z);
	r.warp_in_cta = tid >> 5;
}

// The plain recording kernel body used by cases c and d: lane 0 of every warp writes one
// record at rec[warp_lin]. rec must hold gridDim * warps_per_block records.
__device__ __forceinline__ void record_plain(IdRec *rec, uint32_t tag, uint32_t kind)
{
	uint32_t tid = threadIdx.x;
	uint32_t wpb = (blockDim.x + 31) / 32;
	uint32_t w = blockIdx.x * wpb + (tid >> 5);
	if ((tid & 31) == 0) {
		IdRec r;
		read_ids(r);
		r.tag = tag;
		r.kind = kind;
		r.warp_lin = w;
		r.aux = 0;
		r.flags = 0;
		rec[w] = r;
	}
}
#endif
