// SPDX-License-Identifier: (LGPL-2.1 OR BSD-2-Clause)
// step0 check 5: eBPF atomics as tools would write them, on a GPU kretprobe.
// One source, three programs (one per build, selected with -DE5_MODE_<X>):
//   FADD  idx = __sync_fetch_and_add(counter, 1)            (the rule under test)
//   PLAIN idx = *counter; *counter = idx + 1                 (negative control: must lose)
//   CAS   do { old = *counter; } while (cas(counter, old, old + 1) != old); idx = old
//         (with an iteration cap; the JIT's CMPXCHG returns a plain load, compiler.cpp:1697-1718)
// Every firing writes a tag unique per (launch, thread) into out[idx].
#define BPF_NO_GLOBAL_DATA
#include <vmlinux.h>
#include <bpf/bpf_helpers.h>

// The server creates 1503 as 1513 (host memory, registered) on this board.
#define BPF_MAP_TYPE_GPU_ARRAY_MAP 1503

#define E5_MAX_THREADS 12288 /* threads per launch the seen[] map covers */
#define E5_OUT_ENTRIES 131072 /* >= 12,288 x 10 calls */
#define E5_TAG_SHIFT 20 /* tag = (launch << 20) + gid + 1 */
#define E5_CAS_CAP 1024 /* device-side retry cap; hits are counted */

// ctl[] words (shared with e5_loader.c)
#define CTL_COUNTER 0
#define CTL_OVERFLOW 1 /* idx >= E5_OUT_ENTRIES */
#define CTL_CAS_CAPHITS 2 /* CAS loop gave up after E5_CAS_CAP tries */
#define CTL_CAS_RETRIES 3 /* sum over firings of failed CAS tries */
#define CTL_BAD_GID 4 /* gid >= E5_MAX_THREADS */
#define CTL_NULL 5 /* a lookup returned NULL */
#define CTL_N 8

struct {
	__uint(type, BPF_MAP_TYPE_GPU_ARRAY_MAP);
	__uint(max_entries, CTL_N);
	__type(key, u32);
	__type(value, u64);
} ctl SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_GPU_ARRAY_MAP);
	__uint(max_entries, E5_OUT_ENTRIES);
	__type(key, u32);
	__type(value, u64);
} out SEC(".maps");

// seen[gid]: firings of thread gid so far; only thread gid touches it and the
// victim's launches are serialized, so a plain increment is exact here.
struct {
	__uint(type, BPF_MAP_TYPE_GPU_ARRAY_MAP);
	__uint(max_entries, E5_MAX_THREADS);
	__type(key, u32);
	__type(value, u64);
} seen SEC(".maps");

static const u64 (*bpf_get_block_idx)(u64 *x, u64 *y, u64 *z) = (void *)503;
static const u64 (*bpf_get_block_dim)(u64 *x, u64 *y, u64 *z) = (void *)504;
static const u64 (*bpf_get_thread_idx)(u64 *x, u64 *y, u64 *z) = (void *)505;

static __always_inline void ctl_add(u32 k, u64 v)
{
	u64 *w = bpf_map_lookup_elem(&ctl, &k);
	if (w)
		__sync_fetch_and_add(w, v); /* result unused: plain atomic add */
}

// bpftime runs a program on the GPU only when its name starts with cuda__
SEC("kretprobe/e5step0_atomics_kernel")
int cuda__e5_probe(void *ctx)
{
	u64 tx, ty, tz, bx, by, bz, dx, dy, dz;
	bpf_get_thread_idx(&tx, &ty, &tz);
	bpf_get_block_idx(&bx, &by, &bz);
	bpf_get_block_dim(&dx, &dy, &dz);
	u64 gid = bx * dx + tx;
	if (gid >= E5_MAX_THREADS) {
		ctl_add(CTL_BAD_GID, 1);
		return 0;
	}
	u32 k0 = CTL_COUNTER;
	u64 *ctr = bpf_map_lookup_elem(&ctl, &k0);
	u32 g = (u32)gid;
	u64 *lc = bpf_map_lookup_elem(&seen, &g);
	if (!ctr || !lc) {
		ctl_add(CTL_NULL, 1);
		return 0;
	}
	u64 launch = *lc;
	*lc = launch + 1;
	u64 tag = (launch << E5_TAG_SHIFT) + gid + 1;
	u64 idx;
#if defined(E5_MODE_FADD)
	idx = __sync_fetch_and_add(ctr, 1);
#elif defined(E5_MODE_PLAIN)
	idx = *ctr;
	*ctr = idx + 1;
#elif defined(E5_MODE_CAS)
	u64 old = 0;
	u32 i;
	int ok = 0;
	for (i = 0; i < E5_CAS_CAP; i++) {
		old = *ctr;
		if (__sync_val_compare_and_swap(ctr, old, old + 1) == old) {
			ok = 1;
			break;
		}
	}
	if (i)
		ctl_add(CTL_CAS_RETRIES, i);
	if (!ok) {
		ctl_add(CTL_CAS_CAPHITS, 1);
		return 0;
	}
	idx = old;
#else
#error "define E5_MODE_FADD, E5_MODE_PLAIN or E5_MODE_CAS"
#endif
	if (idx >= E5_OUT_ENTRIES) {
		ctl_add(CTL_OVERFLOW, 1);
		return 0;
	}
	u32 k = (u32)idx;
	u64 *slot = bpf_map_lookup_elem(&out, &k);
	if (!slot) {
		ctl_add(CTL_NULL, 1);
		return 0;
	}
	*slot = tag;
	return 0;
}

char LICENSE[] SEC("license") = "GPL";
