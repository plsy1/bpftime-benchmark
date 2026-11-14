# What GPU-native tools need from storage (read-only survey, 2026-11-15)

I did not edit any results file, run anything on the GPU, or build anything. The derived numbers come from two scratch scripts, both run with python3 -I:
- /tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/s/classify.py
- /tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/s/census_dist.py

## 1. Every GPU map use, by what it is used for

**Source and unit.** The source is the verified records in /home/jetson/src/results/gpu/callsite-survey-20261113/records.json, excluding `cpu_side`. That gives 37 programs, 79 (program, map) uses and 130 map call sites. Each use gets one class, by how it is used, not by map type.

**Program groups.** These are the survey's file list (`summary.txt:1-50`), grouped:
- **real (13):** run on real application kernels.
  - bpftime: the threadhist ports to FAISS, llama.cpp and PyTorch, and cutlass_launch_counter.
  - gpu_ext revision-rq4 on llama.cpp: kernelretsnoop x3, threadhist, full-record-device-buffer, onevalue-array and launchlate x3. onevalue-array's file targets vectorAdd; its layout was measured on llama.cpp (`buffer-survey-20261111/notes.txt:138-139`).
- **cpu-style (10):** make host calls today. cuda-counter, rocm-counter, cudagraph, host_map_test, gpu_shared_map, atomizer, launchlate x2, threadscheduling x2.
- **test-kernel (7):** other GPU-native tools on vectorAdd-type kernels.
  - bpftime: threadhist, threadhist-gpu-kernel-shared-map, kernelretsnoop, kernel_trace, mem_trace.
  - gpu_ext: bpftime-device-smoke, cross-layer-raw-map.
- **bench (7):** microbenchmarks or not instrumentation. bpftime `benchmark/gpu/micro` x2 and directly_run_on_gpu; gpu_ext fig15 x3 and trampoline-scaling.

| class | uses: real / cpu-style / test-kernel / bench = total | sites | examples (records.json file:line) | needs per-logical-thread identity? |
|---|---|---:|---|---|
| (a) counters and histograms, CPU reads totals | 4 / 15 / 2 / 4 = 25 | 52 | cutlass_call_count; launchlate time_histogram x3; HASH call_count and total_time_ns; sm/warp_histogram; mem_trace counter | No. Keys are 0, the pid, the SM or warp id, tid%10, or a latency bin |
| (a) where the probe uses the value it gets back | 2 / 0 / 0 / 0 = 2 | 4 | gpu_ext launchlate device_state: the fetch-add result picks the FIFO slot (575-06:212, 575-07:215) | No. Needs an atomic fetch-add that returns a value (an ordering), not only a sum |
| (b) per-thread scratch, entry to exit | 0 / 1 / 0 / 0 = 1 | 2 | host_map_test thread_timestamp, 1512 (host_map_test.bpf.c:95, 127) | Yes, within one launch |
| (b) same idea, keyed per block or per launch | 0 / 3 / 0 / 0 = 3 | 9 | start_ts: cuda-counter and rocm-counter per block (cuda_probe.bpf.c:44, 66, 70); cudagraph by thread 0 (53, 75, 78) | Block id, or none |
| (c) per-thread results the CPU reads per thread | 4 / 3 / 3 / 5 = 15 | 19 | threadhist x4 on real kernels; threadscheduling thread_mapping x2; host_map_test perthread_counter; device-smoke; cross-layer aggregate; directly_run vec_C/gemm_C; trampoline-scaling x2 | Yes. The index is the logical thread id, summed over launches |
| (d) event and record streams | 5 / 0 / 3 / 1 = 9 | 10 | gpu_ext kernelretsnoop x3, full-record arena, onevalue arena; bpftime kernelretsnoop, kernel_trace; raw_records | No for meaning: the record carries the ids and the globaltimer |
| (e) CPU-written values read by the GPU | 2 / 4 / 0 / 6 = 12 | 12 | launchlate clock_calibration x2; atomizer partition x2; bpftime launchlate last_uprobe_time x2; directly_run inputs x6 | No |
| fits none: a CPU-to-GPU queue, one entry per launch | 4 / 0 / 0 / 0 = 4 | 5 | gpu_ext launchlate launch_times x3 and queue_state. The CPU uprobe enqueues, GPU thread 0 dequeues (575-01:156-165); 575-07 writes the GPU time back (:224) | No. Needs matching in launch order |
| fits none: map-operation microbenchmark sinks | 0 / 0 / 0 / 8 = 8 | 17 | fig15 map_probe x4, uniform x3, warp x1 (keys: lane id, 0, %warpid) | n/a |

Total: 21 / 26 / 8 / 24 = 79 uses and 130 sites. Not maps: 21 printk sites, 20 of them in cpu-style programs. They are an output stream, and every one is a host call today (`callsite-survey README:49`).

**How the CPU actually reads the per-thread counters**
- **threadhist as written is (c):** the loader prints every slot.
  - bpftime: `example/gpu/threadhist/threadhist.c:46-48`.
  - gpu_ext s0: `threadhist.c:67-75` prints each thread and the total.
- **The llama.cpp port only sums the slots** (`llama-census-20261113/README.md:63`), so that consumer needs (a).
- **device-smoke** reads the per-thread counts but prints totals and the distribution (`gpu_ext workloads/bpftime-device-smoke/probe.c:44-63`).
- **cross-layer** uses its per-thread count as the record's sequence number (records.json, raw_map.bpf.c:52). That is per-thread state kept across launches.

**What the real-kernel tools do**
- Every (a) use runs once per launch, in thread (0,0,0) (records.json frequency `per_launch`).
- Per-thread volume comes only from threadhist (c) and kernelretsnoop (d).
- No real-kernel or test-kernel tool keeps per-thread entry-to-exit state; class (b) occurs once, in a demo. Kernel timing on real kernels uses exit timestamps only.

**Where the (d) stores put records, and what happens out of range**
- **bpftime 1527:** one ring per logical thread, 16 x 1,032 B (`projection.md:51-58`). Output past the capacity returns 1 and is not counted in `dropped` (`real-scenarios README:268`).
- **gpu_ext full-record:** the logical thread id maps to a bank and slot: 32 banks x 16,384 slots x 256 records x 80 B, sized for RoPE-K at pp512 (524,288 slots). Out-of-range and overflow are counted (`full-record-device-buffer.bpf.c:69-95`; `notes.txt:145`).
- **gpu_ext onevalue (the 5.4-5.6% buffer):** one slot per logical warp (block x warps_per_block + warp, `onevalue-array.bpf.c:39-44`), written by lane 0 only, 44 events per slot. Out-of-range and overflow are counted (:52-62).
  - 22.1 MiB = 16,384 warp slots x (8 + 44 x 32 B) [arithmetic from notes.txt:138].
- **Both hand-written bounded buffers are indexed by logical thread or warp id**, with one writer per slot and no atomics, sized from one known launch geometry. Neither is indexed by hardware position.

## 2. llama.cpp on the Orin (TinyLlama Q4_K_M, nsys, no bpftime)

**Sources**
- Per-kernel CSVs: `/home/jetson/src/results/gpu/llama-census-20261113/census/{pp512,tg128}-{nograph,graph}.csv`. They hold launches and min/max threads per kernel only.
- Per-launch numbers come from the uncommitted raw exports `/home/jetson/src/bpftime-gpu-work/llama/census/<run>.sqlite`, opened read-only with `select gridX*gridY*gridZ*blockX*blockY*blockZ, graphId, streamId, start, end from CUPTI_ACTIVITY_KIND_KERNEL`.
- Their launch and thread-execution totals match the CSVs and README:37-42.

| | 512-token prefill | decode, per token (128-token run) |
|---|---|---|
| launches | 444 | 424 (54,272 / 128) |
| thread executions | 192,560,640 | 39,354,368 |
| warp executions | 6,017,520 | 1,229,824 |
| threads per launch: min / p10 / median / p90 / max | 512 / 2,048 / 262,144 / 720,896 / 4,194,304 | 256 / 512 / 2,048 / 262,144 / 4,096,000 |
| mean threads per launch | 433,695 | 92,817 |
| launches above 12,288 threads | 392 of 444 (88.3%), holding 99.94% of thread executions | 17,024 of 54,272 (31.4%), holding 98.33% |
| launches at or below 1,024 threads | 3 | 14,336 (26.4%) |
| GPU kernel time | 204.6 ms | 15.91 ms (2,036.9 / 128) |

For contrast, a 32-token prefill has a median of 16,384 threads per launch, and 283 of 488 launches (58.0%) are above 12,288 (sqlite).

**CUDA graphs change the timing, not the launch pattern**
- The ordered (kernel, grid, block) sequence is identical with graphs on and off in all four runs (sqlite). CSV columns 1-7 are identical after sorting.
- Prefill puts 0 launches into graphs either way. Decode puts 53,848 of 54,272 into graphs (graphId; README:48).
- Every run uses one stream (15).
- With graphs, decode:
  - GPU time drops from 2,036.9 to 1,963.8 ms;
  - the median gap between kernels drops from 1.57 to 0.13 us;
  - 10,905 consecutive kernel pairs overlap by up to 672 ns (median 320 ns), against 0 without graphs.
- These are back-to-back launches whose ends and starts overlap slightly, not independent concurrent kernels [inferred]. Storage indexed by thread id could hold two launches at once for up to about 0.7 us [inferred].

**Records a probe would produce at uninstrumented speed** [arithmetic; the denominator is GPU kernel time; GB = 10^9 B]

| probe | per prefill | per decode token | per s, prefill | per s, decode | 32 B: prefill / decode | 80 B: prefill / decode |
|---|---:|---:|---:|---:|---:|---:|
| per-thread exit, every kernel | 192.56M | 39.35M | 941M | 2.47G | 30.1 / 79.1 GB/s | 75.3 / 197.8 GB/s |
| warp leader, every kernel | 6.02M | 1.23M | 29.4M | 77.3M | 0.94 / 2.47 GB/s | 2.35 / 6.18 GB/s |
| one record per launch | 444 | 424 | 2.2K | 26.6K | negligible | negligible |
| per-thread exit, RoPE-K only (gpu_ext's target) | 11.53M | 22,528 [K = the 1,024-thread rope row, inferred from README:144] | 56.4M | 1.4M | 1.80 / 0.05 GB/s | 4.51 / 0.11 GB/s |
| warp leader, RoPE-K only | 360,448 | 704 | 1.76M | 44K | 0.06 GB/s | 0.14 GB/s |

- **Volume.** A per-thread probe on every kernel writes 6.16 GB (32 B) or 15.4 GB (80 B) per prefill, and 1.26 or 3.15 GB per decode token. At 32 B it would fill the 128 MiB segment (`projection.md:55-58`) in 4.5 ms during prefill and 1.7 ms during decode.
- **Other denominators:**
  - Warm wall time, pp512 at 2,475.1 tok/s (`runs/summary.md:5`), gives 931M records/s.
  - Cold decode at 57.3 tok/s (`census/logs/tg128-nograph.log` avg_ts) gives 2.26G records/s. There is no warm decode baseline.
- **gpu_ext's figures are for RoPE-K only** (RTX 5090, about 38,000 tok/s): 91% for the per-thread ring, 39.1% and 12.9% for the full-record buffer, 5.4-5.6% for the per-warp buffer (`notes.txt:139, 141, 152, 189-190`).
  - Its 720,896 warp records are 2 passes x 360,448. Its full records are 23,068,672 x 80 B = 1.85 GB = 2 x 11,534,336 (`notes.txt:137-147`).
  - A per-thread probe on every kernel sees 16.7x RoPE-K's volume per prefill [arithmetic].
- **Measured on the Orin:**
  - threadhist on rms_norm costs 3.3%, about 7 ms or about 0.6 ns per thread execution (README:90-93).
  - With the default 1,024 slots, 99.60% of rms_norm's thread executions were not counted (README:76).

## 3. Which scenarios use each class, and what bpftime lacks

Sources: `/home/jetson/src/results/gpu/real-scenarios-20261113/README.md:92-103`. Tier A = shipped by NVIDIA; B = several independent tools used on real DL kernels; C = mostly bpftime or gpu_ext examples. Ranking is by number of tools, and the order is [inferred] (README:85-90).

| scenario (tier) | classes it would use | bpftime attach point |
|---|---|---|
| Correctness checking (A) | (e) allocation table copied to the GPU [inferred]; rare (d) error reports | Lacks a per-instruction attach point and the effective address (README:79, 94) |
| Instruction mix (A) | (a) opcode- or PC-keyed counters, per instruction per warp: the highest frequency of all | Lacks a per-instruction attach point (README:95) |
| Memory-access counts (A) | (a) shared counter (mem_trace), or (c) per-thread accumulator (Neutrino: 8 B per thread) | Partial: the memcapture regex misses forms and gives no address (README:78-81, 96) |
| Region timing inside a kernel (B) | (d) per-warp buffers, tens to hundreds of records per warp per launch, capacity guessed | Lacks an attach point at a chosen instruction (README:97) |
| Kernel, block and warp timing (B) | (d) per-thread or per-warp exit records; (b) only for entry-exit pairs [inferred, README:98]; (a) for sums | Has it |
| Memory address traces (B) | (d), one record per access per thread, growing as threads x accesses | Lacks the effective address (README:99) |
| Device printing (C) | (d)-type output; every printk is a host call today | Has it (README:100) |
| Work distribution (C) | (c) a threadhist slot per thread, thread_mapping; (a) SM and warp histograms sized by the hardware | Has it (README:101) |
| Launch latency (C) | (e) and the CPU-to-GPU per-launch queue, plus an (a) histogram | Has it (README:102) |
| Device-side policy (C) | (e) CPU-written partition config | Partly (README:103) |

**What this says a map model must cover** [inferred from sections 1-3]
- **Totals** (27 of 79 uses) and **CPU-to-GPU data** (16 uses) do not depend on thread count. What they need is exact concurrent adds, and in 2 uses an add that returns its value.
- **Per-logical-thread identity** is needed by 15 (c) uses and 1 (b) use.
  - On real kernels this is only threadhist (4 uses), and its llama.cpp consumer only sums the slots.
  - As a histogram over thread index it has as many bins as the largest launch: 4.19M slots, 32 MiB (`projection.md:51-58`).
- **Streams** (9 uses) do not need slots indexed by thread for their meaning. They need bounded storage, counted drops, and timestamp order.
  - The volumes are the every-kernel rows above, 30-198 GB/s offered at one record per thread.
  - The heavy scenarios (instruction mix, address traces, region timing) first need attach points bpftime lacks.