// SPDX-License-Identifier: GPL-2.0
/* step0 e7-llama (check 7, llama part): how many lanes run each exit-probe
 * call on four llama.cpp kernels.
 *
 * At every exit-probe call each lane reads the warp's active mask (helper
 * 520, instrumentation build only) and its lane id (511). Only the lowest
 * active lane adds 1 to bin popc(mask) of this kernel's histogram (one
 * fetch-add into a 1513 single-copy array in the shared segment, spread over
 * 8 copies by smid % 8). Non-leader lanes never touch the map unless the mask
 * is inconsistent (anomaly counters). Thread (0,0,0) of block (0,0,0) also
 * counts the launch.
 *
 * Build one object per kernel: -DE7_KIDX=<0..4>, or all four llama programs
 * in one object with -DE7_ALL4. */
#define BPF_NO_GLOBAL_DATA
#include <vmlinux.h>
#include <bpf/bpf_helpers.h>
#include "e7_common.h"

#define BPF_MAP_TYPE_GPU_ARRAY_HOST_MAP 1513

struct {
	__uint(type, BPF_MAP_TYPE_GPU_ARRAY_HOST_MAP);
	__uint(max_entries, E7_ENTRIES);
	__type(key, u32);
	__type(value, u64);
} e7_hist SEC(".maps");

static const u64 (*bpf_get_block_idx)(u64 *x, u64 *y, u64 *z) = (void *)503;
static const u64 (*bpf_get_thread_idx)(u64 *x, u64 *y, u64 *z) = (void *)505;
static const u64 (*bpf_get_sm_id)(void) = (void *)509;
static const u64 (*bpf_get_lane_id)(void) = (void *)511;
static const u64 (*bpf_get_active_mask)(void) = (void *)520; /* step0 build */

static __always_inline void e7_add(u32 key, u64 v)
{
	u64 *p = bpf_map_lookup_elem(&e7_hist, &key);
	if (p)
		__sync_fetch_and_add(p, v); /* result unused: BPF atomic add64 */
}

static __always_inline int e7_exit_probe(u32 k)
{
	u64 m = bpf_get_active_mask() & 0xffffffffULL;
	u64 lane = bpf_get_lane_id() & 31;
	u64 self = 1ULL << lane;
	if (E7_LOWBIT(m) != self) {
		/* not the leader; the two cases below never happen when 520
		 * and 511 are right */
		if (m == 0)
			e7_add(E7_X_KEY(k, E7_X_ANOM_ZERO), 1);
		else if (!(m & self))
			e7_add(E7_X_KEY(k, E7_X_ANOM_SELF), 1);
		return 0;
	}
	u64 c = E7_POPC32(m);
	u64 s = bpf_get_sm_id() % E7_NS;
	e7_add(E7_HIST_KEY(k, s, c), 1);
	if (lane == 0) {
		u64 x = 1, y = 1, z = 1;
		bpf_get_thread_idx(&x, &y, &z);
		if ((x | y | z) == 0) {
			bpf_get_block_idx(&x, &y, &z);
			if ((x | y | z) == 0)
				e7_add(E7_X_KEY(k, E7_X_LAUNCHES), 1);
		}
	}
	return 0;
}

/* Program names must start with cuda__ (runtime/src/attach/bpf_attach_ctx.cpp). */
#define E7_PROGRAM(fn, sec, kidx)                                              \
	SEC(sec) int fn() { return e7_exit_probe(kidx); }

#if defined(E7_ALL4) || (defined(E7_KIDX) && E7_KIDX == E7_K_RMS)
/* rms_norm_f32<512,1,true,true,false,false> (census: 45 launches per pp512) */
E7_PROGRAM(cuda__e7_rms,
	   "kretprobe/_Z12rms_norm_f32ILi512ELi1ELb1ELb1ELb0ELb0EEvPKfPfiiilllfS1_lll5uint3S3_S3_S3_S1_lllS3_S3_S3_S3_f",
	   E7_K_RMS)
#endif
#if defined(E7_ALL4) || (defined(E7_KIDX) && E7_KIDX == E7_K_MMQ)
/* the largest mul_mat_q (40.6% of pp512 GPU time) */
E7_PROGRAM(cuda__e7_mmq,
	   "kretprobe/_Z9mul_mat_qIL9ggml_type12ELi128ELb0EL9ggml_prec30ELb1EEvPKcS3_PKiS5_PK25ggml_cuda_mm_id_tile_specPfS9_PKf11ggml_glu_opfSB_SB_5uint3iiiiiSD_SD_iiiSD_SD_iiiSD_",
	   E7_K_MMQ)
#endif
#if defined(E7_ALL4) || (defined(E7_KIDX) && E7_KIDX == E7_K_MMVQ)
/* the largest mul_mat_vec_q (64.7% of tg128 GPU time) */
E7_PROGRAM(cuda__e7_mmvq,
	   "kretprobe/_Z13mul_mat_vec_qIL9ggml_type12ELi1ELb1ELb0ELb0EEvPKvS2_PKi31ggml_cuda_mm_fusion_args_devicePfj5uint3jjjS7_jjjS7_jjjj",
	   E7_K_MMVQ)
#endif
#if defined(E7_ALL4) || (defined(E7_KIDX) && E7_KIDX == E7_K_ROPEK)
/* rope_norm<true,false,float,half>: RoPE K, gpu_ext's target */
E7_PROGRAM(cuda__e7_ropek,
	   "kretprobe/_Z9rope_normILb1ELb0Ef6__halfEvPKT1_PT2_iiiiiiiiiiiPKifff14rope_corr_dimsfPKfPKlib",
	   E7_K_ROPEK)
#endif
#if !defined(E7_ALL4) && defined(E7_KIDX) && E7_KIDX == E7_K_CTRL
/* controls (all in the control object, all kernels in e7_ctrl):
 * e7ctrl_partial: 48-thread blocks, no branch (masks of 32 and 16 lanes);
 * e7ctrl_diverge: 64-thread blocks, odd lanes leave through their own ret,
 * even lanes wait ~200 us before theirs, so every warp makes two exit-probe
 * calls of 16 lanes (one led by lane 1). A mask that ignored divergence
 * would give one call of 32 per warp and still match the thread total.
 * e7ctrl_merge: the same without the wait (reported, not pass/fail): do two
 * exit groups that reach the probe close together merge inside it? */
E7_PROGRAM(cuda__e7_ctrl, "kretprobe/e7ctrl_partial", E7_K_CTRL)
E7_PROGRAM(cuda__e7_ctrldiv, "kretprobe/e7ctrl_diverge", E7_K_CTRLDIV)
E7_PROGRAM(cuda__e7_ctrlmerge, "kretprobe/e7ctrl_merge", E7_K_CTRLMERGE)
#endif

char LICENSE[] SEC("license") = "GPL";
