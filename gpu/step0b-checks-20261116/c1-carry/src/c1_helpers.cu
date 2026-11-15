// c1_helpers.cu: the lookup wrapper for the new map types of check C1.
// Compiled with nvcc -rdc=true -ptx and appended to a patched module after
// the trampoline (whose map_info and _bpf_helper_ext_0001 it uses). The
// routing (tools/c1lib.py) sends a probe function's helper-1 calls on maps of
// these types here, with eBPF r5 (param 4, unused by every lookup) set to the
// carry's generic address.
#include <stdint.h>

struct MapBasicInfo { // trampoline/default_trampoline.cu (40 B)
	bool enabled;
	int key_size;
	int value_size;
	int max_entries;
	int map_type;
	void *extra_buffer;
	uint64_t max_thread_count;
};
extern __constant__ MapBasicInfo map_info[1024];
extern "C" __device__ uint64_t _bpf_helper_ext_0001(uint64_t, uint64_t, uint64_t, uint64_t,
						    uint64_t);

#define MT_CARRY 1540   // the thread's own carry (kernel-scope .local)
#define MT_FRAME 1541   // UNSOUND on purpose: this function's own frame
#define MT_SLOTTAG 1542 // (SM, warp slot, lane) slot + owner tag (fallback design)
#define MT_SLOT 1543    // control: the same slot WITHOUT a tag
#define MT_SHARED 1544  // wrong on purpose: one slot for every thread
#define MT_LANE 1545    // wrong on purpose: a slot per lane id only

static __device__ __forceinline__ uint32_t sreg_smid()
{
	uint32_t v;
	asm volatile("mov.u32 %0, %%smid;" : "=r"(v));
	return v;
}
static __device__ __forceinline__ uint32_t sreg_warpid()
{
	uint32_t v;
	asm volatile("mov.u32 %0, %%warpid;" : "=r"(v));
	return v;
}
static __device__ __forceinline__ uint32_t sreg_nwarpid()
{
	uint32_t v;
	asm("mov.u32 %0, %%nwarpid;" : "=r"(v));
	return v;
}
static __device__ __forceinline__ uint32_t sreg_laneid()
{
	uint32_t v;
	asm volatile("mov.u32 %0, %%laneid;" : "=r"(v));
	return v;
}

// The owner tag of the slot design: the thread's linear id in its grid + 1
// (0 never names an owner). No launch id: %gridid repeats across graph
// replays (step 0, check 1), so a tag cannot tell two launches apart.
static __device__ __forceinline__ uint64_t owner_tag()
{
	const uint64_t bx = blockIdx.x, by = blockIdx.y, bz = blockIdx.z;
	const uint64_t gx = gridDim.x, gy = gridDim.y;
	const uint64_t dx = blockDim.x, dy = blockDim.y, dz = blockDim.z;
	const uint64_t block = (bz * gy + by) * gx + bx;
	const uint64_t thr = ((uint64_t)threadIdx.z * dy + threadIdx.y) * dx + threadIdx.x;
	return block * (dx * dy * dz) + thr + 1;
}

// Arguments as eBPF passes them: r1 = map fd, r2 = key pointer, r3 = op
// (the probes call helper 1 with a third argument: 0 at entry, 1 at exit;
// every existing lookup ignores r3), r5 = the carry's generic address (set
// by the routing; eBPF's r5 is unused by every lookup).
static __device__ __forceinline__ uint64_t hw_slot()
{
	return ((uint64_t)sreg_smid() * sreg_nwarpid() + sreg_warpid()) * 32 + sreg_laneid();
}
extern "C" __noinline__ __device__ uint64_t _bpf_helper_ext_0001_c1(uint64_t map, uint64_t key,
								     uint64_t op, uint64_t b,
								     uint64_t carry)
{
	const MapBasicInfo &mi = map_info[map];
	const uint32_t vs = (uint32_t)mi.value_size, ne = (uint32_t)mi.max_entries;
	if (mi.map_type == MT_CARRY) {
		// the thread's own carry (kernel-scope .local, address from the kernel)
		const uint32_t k = *(const uint32_t *)(uintptr_t)key;
		if (carry == 0 || k >= ne)
			return 0;
		return carry + (uint64_t)k * vs;
	}
	if (mi.map_type == MT_SLOTTAG) {
		// op 0: claim at entry (write the owner tag), op 1: check at exit
		// (NULL unless this thread's tag is there: the counted fallback)
		const uint64_t s = hw_slot();
		if (s >= ne)
			return 0;
		uint64_t *slot = (uint64_t *)((char *)mi.extra_buffer + s * vs);
		const uint64_t tag = owner_tag();
		if (op == 0) {
			slot[0] = tag;
			return (uint64_t)(uintptr_t)(slot + 1);
		}
		return slot[0] == tag ? (uint64_t)(uintptr_t)(slot + 1) : 0;
	}
	if (mi.map_type == MT_SLOT || mi.map_type == MT_SHARED || mi.map_type == MT_LANE) {
		const uint64_t s = mi.map_type == MT_SLOT ? hw_slot() :
				   mi.map_type == MT_LANE ? (uint64_t)sreg_laneid() : 0;
		if (s >= ne)
			return 0;
		return (uint64_t)(uintptr_t)((char *)mi.extra_buffer + s * vs);
	}
#ifndef C1_NO_FRAME // part (b) only: its 64-B frame would weigh on parts (c) and (d)
	if (mi.map_type == MT_FRAME) {
		// UNSOUND on purpose: a .local in this function's own frame, whose
		// lifetime ends when it returns (PTX ISA 5.1.5: function-scope .local
		// is allocated on the stack). ptxas accepts it.
		uint64_t p;
		asm volatile("{\n\t.local .align 16 .b8 c1_frame_buf[64];\n\t"
			     "cvta.local.u64 %0, c1_frame_buf;\n\t}"
			     : "=l"(p));
		return p;
	}
#endif
	return _bpf_helper_ext_0001(map, key, op, b, carry);
}
