/* Shared by the prototype's eBPF tools (proto/map-model). */
#ifndef PT_COMMON_H
#define PT_COMMON_H
#define BPF_NO_GLOBAL_DATA
#include <vmlinux.h>
#include <bpf/bpf_helpers.h>

/* today's types (Orin creates 1502 as 1512, 1503 as 1513) */
#define MT_PERTHREAD 1502
#define MT_GPU_ARRAY 1503
/* the prototype's */
#define MT_THREAD_LOCAL 1530
#define MT_SUM 1531
#define MT_DEV_PERTHREAD 1532
/* the device hash map (stage one, 2026-11-28): u32 keys, u64 values,
 * NOEXIST inserts, 64-bit adds through lookups */
#define MT_DEV_HASH 1533

static const u64 (*bpf_get_globaltimer)(void) = (void *)502;
static const u64 (*bpf_get_block_idx)(u64 *x, u64 *y, u64 *z) = (void *)503;
static const u64 (*bpf_get_block_dim)(u64 *x, u64 *y, u64 *z) = (void *)504;
static const u64 (*bpf_get_thread_idx)(u64 *x, u64 *y, u64 *z) = (void *)505;

static __always_inline u32 log2_u64(u64 v)
{
	u32 r = 0;
	if (v >> 32) { v >>= 32; r += 32; }
	if (v >> 16) { v >>= 16; r += 16; }
	if (v >> 8) { v >>= 8; r += 8; }
	if (v >> 4) { v >>= 4; r += 4; }
	if (v >> 2) { v >>= 2; r += 2; }
	if (v >> 1) { r += 1; }
	return r;
}

/* the linear thread index of a one-dimensional grid */
static __always_inline u64 lin_tid(void)
{
	u64 bx, by, bz, dx, dy, dz, tx, ty, tz;
	bpf_get_block_idx(&bx, &by, &bz);
	bpf_get_block_dim(&dx, &dy, &dz);
	bpf_get_thread_idx(&tx, &ty, &tz);
	return bx * dx + tx;
}

/* kernels: k index, section name suffix */
#define K_RMS "_Z12rms_norm_f32ILi512ELi1ELb1ELb1ELb0ELb0EEvPKfPfiiilllfS1_lll5uint3S3_S3_S3_S1_lllS3_S3_S3_S3_f"
#define K_MMQ "_Z9mul_mat_qIL9ggml_type12ELi128ELb0EL9ggml_prec30ELb1EEvPKcS3_PKiS5_PK25ggml_cuda_mm_id_tile_specPfS9_PKf11ggml_glu_opfSB_SB_5uint3iiiiiSD_SD_iiiSD_SD_iiiSD_"
#define K_MMVQ "_Z13mul_mat_vec_qIL9ggml_type12ELi1ELb1ELb0ELb0EEvPKvS2_PKi31ggml_cuda_mm_fusion_args_devicePfj5uint3jjjS7_jjjS7_jjjj"
#define K_ROPEK "_Z9rope_normILb1ELb0Ef6__halfEvPKT1_PT2_iiiiiiiiiiiPKifff14rope_corr_dimsfPKfPKlib"
#define NK 4

/* PT_PAIRS(X): X(fn_suffix, kidx, kernel) for the build's kernel set */
#if defined(PT_SET_EXITFAST)
#define PT_PAIRS(X) X(kslow, 0, "pt_kslow")
#elif defined(PT_SET_TEST)
#define PT_PAIRS(X) X(kdiv, 0, "pt_kdiv") X(kflat, 1, "pt_kflat")
#elif defined(PT_SET_LLAMA4)
#define PT_PAIRS(X) X(rms, 0, K_RMS) X(mmq, 1, K_MMQ) X(mmvq, 2, K_MMVQ) X(ropek, 3, K_ROPEK)
#elif defined(PT_SET_RMS)
#define PT_PAIRS(X) X(rms, 0, K_RMS)
#elif defined(PT_SET_MMQ)
#define PT_PAIRS(X) X(mmq, 1, K_MMQ)
#elif defined(PT_SET_MMVQ)
#define PT_PAIRS(X) X(mmvq, 2, K_MMVQ)
#elif defined(PT_SET_ROPEK)
#define PT_PAIRS(X) X(ropek, 3, K_ROPEK)
#else
#error "PT_SET_*"
#endif
#endif
