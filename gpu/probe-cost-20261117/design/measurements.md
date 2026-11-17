# Measurements for the per-thread probe cost design (2026-11-17)

Board: Jetson Orin Nano 8 GB (sm_87, 8 SMs, 1,536 threads/SM, L2 4 MB,
LPDDR5 128-bit, ~102 GB/s peak), GPU 1,020 MHz and CPU 1,728 MHz fixed.
Prototype build: /home/jetson/src/bpftime-proto (branch proto/map-model,
c940870b; the attribution items below ran on this build). Application:
llama.cpp llama-bench, TinyLlama Q4_K_M, `-r 5` with warmup.

## A. The full tools on four kernels (earlier passes, final build)

tg128 (128 generated tokens per run), base 67.96 t/s (1.883 s per run):
- lat (entry stores %globaltimer into thread-local map 1530; exit adds 1 to
  a log2 bucket in sum map 1531): 45.4 t/s, 2.82 s per run, -33.3%.
- per thread of the largest mul_mat_vec_q (3.137e9 threads per run): about
  0.30 ns added; that kernel's own time is about 0.41 ns per thread.

pp512: base 2,514 t/s; lat 2,409 t/s (-4.2%).

## B. Attribution on the real kernel (new, 2 passes a1/a2)

Only the largest mul_mat_vec_q is probed (PT_SET_MMVQ), tg128. Per-thread
cost = (time per run - base time per run) / 3,137,339,392. Count checks:
the sum map holds exactly 15,711,207,424 = 5 runs + warmup token, in both
passes, for count and lat.

| item | what the probes do | t/s (a1 / a2) | added per thread (a1 / a2) |
|---|---|---:|---:|
| base | no agent | 67.87 / 67.59 | 0 |
| empty2 | entry and exit probe functions that return 0 | 65.77 / 65.78 | 0.019 / 0.017 ns |
| timer | each probe calls helper 502 (%globaltimer), result compared, no map | 61.98 / 62.33 | 0.057 / 0.051 ns |
| carryonly | lat without the add: entry stores the timer in 1530, exit reads it back, reads the timer, computes the log2 bucket | 53.48 / 53.62 | 0.162 / 0.157 ns |
| count | empty entry; exit does a sum-map (1531) lookup with constant key through helper 1 and an atomic add 1; no timer | 54.03 / 53.93 | 0.154 / 0.153 ns |
| lat | full tool | 46.01 / 46.06 | 0.286 / 0.282 ns |

Derived (ns per thread): call floor 0.018; timer 0.035; carry + log2 0.105;
lookup + atomic add 0.135; parts sum 0.293 vs lat 0.284.
Budget for at most 5% on tg128 (rule 3): about 0.031 ns per thread in all.

## C. Probe shapes, hand-written CUDA (new, 2 passes q1/q2, 20 rounds each)

/home/jetson/src/bpftime-gpu-work/proto/probe-shape/probe_shape.cu.
ns per thread execution added over the same body without probes (median of
rounds). Bodies: empty, or mem (each thread reads 16 bytes of a 64 MB buffer
and sums them). Launch shapes: anchor 512x512, large 8192x512, mmvq
5632x128 (mul_mat_vec_q's largest launch). SASS checked per kernel: c_* have
2 CALLs, c_carry/c_smlane have STL/LDL, c_param and r_* have none; r_warp has
VOTE+MATCH+POPC.

| shape | empty/anchor | empty/large | empty/mmvq | mem/large | mem/mmvq |
|---|---:|---:|---:|---:|---:|
| base ns/thread | 0.031 | 0.022 | 0.047 | 0.159 | 0.165 |
| c_empty (2 noinline empty calls) | +0.027 | +0.027 | +0.003 | +0.000 | +0.000 |
| c_carry (noinline, start in local memory, timer x2 + clz, no add) | +0.096 | +0.091 | +0.070/0.071 | +0.018 | +0.018/0.019 |
| c_smlane (c_carry + red.add to (smid,lane) shard) | +0.155 | +0.151/0.152 | +0.123/0.125 | +0.068/0.078 | +0.060/0.073 |
| c_param (noinline; entry returns the start, exit takes it as a parameter; + (smid,lane) red) | +0.044 | +0.037 | +0.010 | +0.003 | +0.000 |
| r_carry (inline, start in a register, timer x2 + clz, no add) | +0.009 | +0.009 | -0.001 | +0.000 | +0.000 |
| r_smlane (r_carry + (smid,lane) red) | +0.040/0.049 | +0.037/0.044 | +0.009/0.017 | +0.001 | +0.000 |
| r_warp (r_carry + activemask/match.any combine, leader red of popc) | +0.035 | +0.030 | +0.006 | +0.000 | +0.000 |

(mem/anchor is unstable between passes: base 0.132 vs 0.074, a 4 MB working
set at the L2 size; not used.)

Caveat: the synthetic mem body is a pure stream at ~97 GB/s (0.165 ns per
16 B); the real mul_mat_vec_q runs at ~0.41 ns per thread (~43 GB/s
effective) with more compute, shared-memory reduction and shuffles, so how
much probe work it hides is not known from this.

Correction found today: the earlier merge-cost benchmark's "m_carry"
(0.098 ns, results gpu/map-model-proto-20261116/merge-cost) had its exit
timer read and bucket removed by ptxas (the bucket was compared with 99,
which 63-clz cannot reach); its merge columns are unaffected.

## D. What runs today per thread (SASS of the patched mmvq, sm_87)

See probe-sass-excerpt.txt. Entry: LDG of the probe switch, STL key + generic
LD of it, inline 1530 address, CALL to the timer helper (CS2R), two 32-bit
generic ST of the start. Exit: LDG switch, STL key + generic LD, two 32-bit
generic LD of the start, CALL timer, two more 32-bit generic LD of the start,
~30-instruction shift/compare log2, STL bucket key, CALL helper 1 (generic
lookup: compare chain on map_info type, several LDC, generic LD of the key,
S2UR smid, S2R laneid, shard address), QSPC (isspacep guard) and generic
ATOM.E.ADD.64. Probe functions have a 592-byte local depot (eBPF stack).
