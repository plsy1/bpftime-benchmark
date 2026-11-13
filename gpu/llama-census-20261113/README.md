# llama.cpp on the Orin: the volume the two directions would see on a real application (2026-11-13)

The advisor asked for the share of direction-1 and direction-2 operations in real applications, not microbenchmarks.
The survey `../real-scenarios-20261113` found:
- No published measurement of that share exists.
- On this Orin, llama.cpp is the real application that can be probed.

This folder does the first stage on llama.cpp:
1. a census of every kernel launch, without bpftime;
2. a smoke test that bpftime dev attaches to llama.cpp and counts every thread;
3. a warm overhead check with three configurations: no agent, an empty probe, and threadhist;
4. a projection of both directions' volume onto the census.

Terms:
- **Direction 1 (D1)**: operations on storage indexed by thread id and sized before the launch. These are maps 1502/1512 (per-thread array) and 1527 (GPU ring buffer, one ring per thread).
- **Direction 2 (D2)**: helper calls that make a synchronous GPU-to-CPU round trip today. These are CPU maps, printk and puts.
- **Thread execution**: one thread of one kernel launch.

## Setup

- **Board:** Orin Nano 8 GB, CUDA 13.2, kernel 6.8.12-1021-tegra-btf. The CPU (cpu5 at 1728000) and GPU (1020000000) clocks were checked before and after every batch (`census/logs/clocks.txt`, `runs/*/meta.txt`). Runs were 22:36-22:46 JST.
- **llama.cpp:** upstream `ggml-org/llama.cpp` 7874988b (master, 2026-11-13), shallow clone.
  - Build:
    ```
    cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON -DGGML_CUDA=ON -DGGML_CUDA_FA=ON -DGGML_CUDA_NO_VMM=ON '-DCMAKE_CUDA_ARCHITECTURES=87-real;87-virtual' -DLLAMA_CURL=OFF
    ```
    These are gpu_ext's flags with the arch changed to sm_87.
  - `libggml-cuda.so` has 149 modules, each with sm_87 PTX and SASS. That is 7,013 kernel entries (`census/libggml-cuda-entries.txt`).
  - The default build (`native` → `87-real`) has no PTX and could not be probed.
  - No application source was changed.
- **Model:** TinyLlama-1.1B-Chat Q4_K_M (TheBloke GGUF, sha256 9fecc3b3…, matches Hugging Face). gpu_ext used the same model.
- **bpftime:** dev 52126f33, `build-cuda` (built 11-12 17:14 from the same sources). Harness `bpftime-gpu-work/run.sh`, one segment per run.
- **Census:** `nsys` 2026.3.1. Each run is `llama-bench --no-warmup -r 1`, so one run is one prefill or one 128-token decode. Each run was done with CUDA graphs off and on (`scripts/census.sh`), and parsed with `scripts/parse_census.py`.

## 1. Census (no bpftime)

| run | launches | thread executions | threads in one launch | GPU kernel time | share in llama.cpp's own kernels |
|---|---:|---:|---|---:|---:|
| one 32-token prefill | 488 | 17,630,720 | 512 to 4,096,000 | 23.8 ms | 100% |
| one 128-token prefill | 488 | 52,209,152 | 512 to 4,096,000 | 54.0 ms | 100% |
| one 512-token prefill | 444 | 192,560,640 | 512 to 4,194,304 | 204.6 ms | 100% |
| 128 decode tokens | 54,272 (424 per token) | 5,037,359,104 (39.4M per token) | 256 to 4,096,000 | 2,036.9 ms (15.9 ms per token) | 100% |

- **Coverage is complete.** With Q4_K_M weights, every kernel is llama.cpp's own: MMQ, mul_mat_vec_q, flash_attn_ext, rope_norm, rms_norm, quantize and binary ops. No cuBLAS kernel ran. So 100% of GPU time is probeable in the PTX build.
- **Graphs do not change the work.** With CUDA graphs on (`*-graph.csv`), the same kernels, launches and thread counts were recorded.
- **Thread counts per launch span four orders of magnitude within one run:**
  - rms_norm: 512 threads.
  - The output projection (mul_mat_vec_q, 32,000-row vocabulary x 128): 4,096,000.
  - RoPE on Q: 4,194,304 at a 512-token prompt. RoPE on K (gpu_ext's target): 524,288, which is 8 times smaller.
- **Decode has many large launches.** Each decode token runs 424 launches and 39.4M thread executions. The matrix-vector kernels have one block of 128 threads per weight row, for example 5,632 x 128 = 720,896.
- **Where the time goes** (per-kernel tables in `census/*.csv`):
  - prefill: mul_mat_q 77% at 512 tokens;
  - decode: mul_mat_vec_q 92%.

## 2. Smoke test: threadhist on rms_norm, under llama-bench

The probe is threadhist: a kretprobe, one 1502/1512 per-thread counter per thread. Its target is `rms_norm_f32<512,1,true,true,false,false>`, the variant this build runs.
- Only the SEC name and the loader's printout changed: the loader sums all slots instead of printing 7 (`probes/threadhist-llama/`). This is a tool port; llama.cpp is unchanged.
- Run with `BPFTIME_MAP_GPU_THREAD_COUNT=262144` and `GGML_CUDA_DISABLE_GRAPHS=1`.

**Result:** the probe counted every thread of every launch, exactly as the census predicts.

| run | slots written | sum of counts | slot 0 | census prediction |
|---|---:|---:|---:|---|
| 32-token prefill, no warm-up (`runs/llama-th-pp32`) | 16,384 | 705,536 | 45 | 16,384 threads max, 705,536 thread executions, 45 launches |
| 32-token prefill, warm-up + 3 (`llama-th-pp32-b`) | 16,384 | 2,822,144 | 180 | 4 x 705,536, 4 x 45 |
| 512-token prefill, warm-up + 3 (`llama-th-pp512`, `-b`) | 262,144 | 45,092,864 | 180 | 4 x 11,273,216, 4 x 45 |

## 3. Warm overhead (llama-bench, warm-up then 3 timed runs, graphs off)

All runs are in `runs/summary.md`.

| configuration | 512-token prefill, tokens/s | 32-token prefill, tokens/s |
|---|---:|---:|
| no agent (two batches) | 2,475.1 / 2,478.4 | 1,288.7 / 1,286.2 |
| agent, empty kretprobe on rms_norm | 2,476.7 (0.0%) | 1,284.8 (-0.2%, within the sd of 15-19) |
| agent, threadhist on rms_norm | 2,394.6 / 2,397.6 (-3.3%) | 1,255.2 (-2.4%) |

- **Patching and the trampoline cost nothing measurable.** The empty probe runs at the no-agent rate.
- **The direction-1 operation costs about 7 ms per 512-token prefill** (213.7 vs 206.7 ms). That is 11.27M per-thread lookups and increments into host-registered memory, about 0.6 ns per thread execution amortized. Since rms_norm itself takes about 4 ms of GPU time, the probe more than doubles that kernel's time.
- **Start-up is a separate cost.** Every agent run took 15.3-16.1 s against 0.6-1.4 s without the agent.
  - llama.cpp loads its CUDA modules on first use. dev processes about 20 of them, extracting PTX with `cuobjdump` and running the passes. Each run made 11 patch requests, and none was found in the cache.
  - With `--no-warmup`, this landed inside the timed prefill: 2.2 tokens/s, 140 times slower than 300 tokens/s.
  - This belongs to neither direction. It was not investigated further.

## 4. Projection of both directions (`projection.md`)

`scripts/projection.py` multiplies the census by the per-firing operation table. The table comes from `per-firing-ops.json`: two independent readings of every probe's source that agree on all GPU programs. Host-call time uses 9.8-15.6 us per call, serialized ([PROTO] in `../real-scenarios-20261113`). It is arithmetic, not measured.

**Direction 1 (per-thread storage), for probes that keep one slot or ring per thread:**
- **One capacity has to cover the largest launch.** For a probe on every kernel that is 4.19M slots. The average launch then writes 10% of them at 512 tokens, 0.9% at 32 tokens and 2.3% in decode.
- **With the default 1,024 slots**, at least 97-99.8% of per-thread records are lost. Lost records are not counted as drops for 1527.
- **kernelretsnoop's per-thread rings** (16 x 1,032 B per thread) would need about 65 GB at 4.19M threads; the segment is 128 MiB. A probe on gpu_ext's single RoPE-K kernel already needs 8.1 GiB at 512 tokens.
- **The counter form fits but slows the kernel.** threadhist's array is 32 MiB at 4.19M slots, so it fits. Its measured cost on one kernel (section 3) extrapolates linearly to about +120 ms per 512-token prefill if every kernel is probed (+58%) [inferred, not measured].

**Direction 2 (host calls):**
- **Tools written for real kernels make none.** That covers gpu_ext's kernelretsnoop (warp leader), launchlate (thread 0) and threadhist, and bpftime's threadhist and kernelretsnoop.
- **CPU-style tools (bpftime's launchlate, threadscheduling, cuda-counter) make 1, 3 or 8 host calls per thread per launch.**
  - On rms_norm alone (2% of GPU time), a 32-token prefill would take 7-11 s with one printk per thread and 55-88 s with cuda-counter, against 24 ms of GPU time.
  - On every kernel, a 512-token prefill would take 31-50 min with launchlate and 4.2-6.7 h with cuda-counter.
  - Combining the calls of a warp into one cuts this by 32 times but leaves 59-96 s per 512-token prefill with one call per thread, every kernel.
- **Even one host call per launch is visible in decode.** It costs 4.4-6.9 ms per 512-token prefill (2.1-3.4% of its GPU time). In decode it costs 4.2-6.6 ms per token, 26-42% of the 15.9 ms GPU time per token, because decode runs 424 small-to-medium launches per token.

## 5. What this says for how far to go

- **Direction 1 shows up in every per-thread scenario on a real application.** The symptoms are the ones gpu_ext reported, here with exact counts:
  - one guessed capacity cannot fit launches from 512 to 4.19M threads;
  - the default loses almost everything;
  - per-thread rings do not fit in memory.

  Fixing sizing and storage, by sizing per launch or keeping per-warp or GPU-local storage, is useful on the real workload.
- **Direction 2 has no volume in tools written for real kernels.** For CPU-style tools on a real model, per-thread host calls cannot be used at any realistic cost, even aggregated per warp.
  - The only regime where a host call per probe is tolerable is per launch, and even that costs a quarter or more of decode time.
  - So direction 2's value on real applications is to make CPU-style tools usable. That means removing per-thread host calls (eliminate or defer, as in the design), not making synchronous calls faster.

## Limits

- One application and one model; prompts of 32, 128 and 512 tokens; decode of 128 tokens; batch 1.
- Census counts come from cold runs (`--no-warmup`); times in the census tables are first-run GPU times.
- Only threadhist on one kernel was measured with bpftime. kernelretsnoop and the CPU-style tools were not run on llama.cpp; their numbers are projections.
- The overhead extrapolation to every kernel assumes the per-thread-op cost on rms_norm carries over to other kernels.
- Host-call costs come from the protocol microbenchmark. Under CUDA graphs, calls would cost more (about 70 us); graphs were off in every bpftime run.

## Files

- `census/`:
  - `<run>-<graphs>.csv`: per kernel launches, threads per launch, thread executions, GPU time;
  - `libggml-cuda-entries.txt`;
  - `logs/`: llama-bench JSON, clocks, configure log.
- `runs/`: the 10 bpftime runs (meta, loader and victim logs) and `summary.md`.
- `probes/`: the threadhist port and the empty control.
- `per-firing-ops.json`: two independent per-firing operation tables and the reconcile verdicts. Made by `scripts/per-firing-op-workflow.js`.
- `projection.md`: made by `scripts/projection.py`.
- `scripts/`: census.sh, parse_census.py, projection.py, runs_summary.py.
- Raw nsys reports and SQLite exports (about 40 MB) stay in `bpftime-gpu-work/llama/census/`; they are not committed.
