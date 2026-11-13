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

- **Every kernel is llama.cpp's own and carries sm_87 PTX.** With Q4_K_M weights the kernels are MMQ, mul_mat_vec_q, flash_attn_ext, rope_norm, rms_norm, quantize and binary ops. No cuBLAS kernel ran.
  - Probes were verified with exact counts (section 2) on rms_norm, the largest mul_mat_q (40.5% of 512-token prefill GPU time), the largest mul_mat_vec_q (62.7% of decode GPU time) and RoPE K.
  - The other kernels were not probed.
- **CUDA graphs change neither the kernels nor the thread counts.**
  - With graphs on (`*-graph.csv`, `GGML_CUDA_DISABLE_GRAPHS` unset), decode ran 53,848 of its 54,272 launches inside graphs. Prefill used none.
  - Kernels, launches and thread executions are identical to the runs with graphs off. Decode GPU time was 1,963.8 ms against 2,036.9 ms.
  - A first graph pass set `GGML_CUDA_DISABLE_GRAPHS=0`. llama.cpp only tests whether the variable is set (`ggml-cuda/graph.cuh:66`), so that pass also ran without graphs and was redone (`scripts/census.sh`, `batch2.sh`).
- **Thread counts per launch span four orders of magnitude within one run:**
  - rms_norm: 512 threads.
  - The output projection (mul_mat_vec_q, 32,000-row vocabulary x 128): 4,096,000.
  - RoPE on Q: 4,194,304 at a 512-token prompt. RoPE on K (gpu_ext's target): 524,288, which is 8 times smaller.
- **Decode has many large launches.** Each decode token runs 424 launches and 39.4M thread executions. The matrix-vector kernels have one block of 128 threads per weight row, for example 5,632 x 128 = 720,896.
- **Where the time goes** (per-kernel tables in `census/*.csv`):
  - prefill: mul_mat_q 77% at 512 tokens;
  - decode: mul_mat_vec_q 92%.

## 2. Smoke test and count checks: threadhist under llama-bench

The probe is threadhist: a kretprobe, one 1502/1512 per-thread counter per thread. Its target is `rms_norm_f32<512,1,true,true,false,false>`, the variant this build runs.
- Only the SEC name and the loader's printout changed: the loader sums all slots instead of printing 7 (`probes/threadhist-llama/`). This is a tool port; llama.cpp is unchanged.
- Run with `BPFTIME_MAP_GPU_THREAD_COUNT=262144` and `GGML_CUDA_DISABLE_GRAPHS=1`.

**Result:** the probe counted every thread of every launch, exactly as the census predicts.

| run | slots written | sum of counts | slot 0 | census prediction |
|---|---:|---:|---:|---|
| 32-token prefill, no warm-up (`runs/llama-th-pp32`) | 16,384 | 705,536 | 45 | 16,384 threads max, 705,536 thread executions, 45 launches |
| 32-token prefill, warm-up + 3 (`llama-th-pp32-b`) | 16,384 | 2,822,144 | 180 | 4 x 705,536, 4 x 45 |
| 512-token prefill, warm-up + 3 (`llama-th-pp512`, `-b`) | 262,144 | 45,092,864 | 180 | 4 x 11,273,216, 4 x 45 |
| largest mul_mat_q, 512-token prefill, no warm-up (`llama-th-mmq-pp512`) | 90,112 | 1,892,352 | 21 | 90,112, 1,892,352, 21 |
| RoPE K (gpu_ext's target), 512-token prefill (`llama-th-ropek-pp512`) | 524,288 | 11,534,336 | 22 | 524,288, 11,534,336, 22 |
| largest mul_mat_vec_q, 128-token decode (`llama-th-mmvq-tg128`) | 720,896 | 3,137,339,392 | 7,040 | 720,896, 3,137,339,392, 7,040 |
| rms_norm, 512-token prefill, **default 1,024 slots** (`llama-th-default-pp512`) | 1,024 | 45,056 | 45 | 11,273,216 thread executions: 99.60% not counted |

- Each probe was a copy of the port with only the SEC name changed (`probes/th-*`). Every probed launch was counted in full.
- With the default thread count, every thread at index 1,024 or above was skipped. The application printed one line from the GPU, as designed (`bpftime: GPU thread 2336, key 0 is outside map 3 (1024 threads, 1 entries): lookups return NULL, updates fail. Set BPFTIME_MAP_GPU_THREAD_COUNT ...`). Neither log said how many were lost.
- The tokens/s of the runs without warm-up include the agent's start-up (below), so they are not throughput figures.

## 3. Warm overhead (llama-bench, warm-up then 3 timed runs, graphs off)

All runs are in `runs/summary.md`.

| configuration | 512-token prefill, tokens/s | 32-token prefill, tokens/s |
|---|---:|---:|
| no agent (two batches) | 2,475.1 / 2,478.4 | 1,288.7 / 1,286.2 |
| agent, empty kretprobe on rms_norm | 2,476.7 (0.0%) | 1,284.8 (-0.2%, within the sd of 15-19) |
| agent, threadhist on rms_norm | 2,394.6 / 2,397.6 (-3.3%) | 1,255.2 (-2.4%) |

- **Once patched, the trampoline costs nothing measurable.** rms_norm with an empty probe runs at the no-agent rate (warm runs). The patching itself is part of the start-up cost below.
- **threadhist's probe body costs about 7 ms per 512-token prefill** (213.7 vs 206.7 ms). The body is 11.27M lookups of a per-thread slot (with a `membar.sys`) and increments into host-registered memory, about 0.6 ns per thread execution amortized. Since rms_norm itself takes about 4 ms of GPU time, the probe more than doubles that kernel's time.
  - No control does the same work on storage that is not thread-indexed. So this is the cost of the probe body, not a cost of direction 1 as such.
  - Extrapolated linearly to a probe on every kernel, it would be about +120 ms per 512-token prefill (+58%) [inferred, not measured; it assumes the per-thread cost is the same in other kernels].
- **Start-up is a separate cost.** Every agent run took 15.3-16.1 s against 0.6-1.4 s without the agent.
  - llama.cpp loads its CUDA modules on first use. dev processed 10 of them per run (9 in the decode run), extracting PTX with `cuobjdump` and running the passes. Each run made 10 patch requests and one PTX compile, and none was found in the cache. Most of the start-up is likely this; it was not timed per step.
  - With `--no-warmup`, this landed inside the timed prefill: 2.2 tokens/s, 140 times slower than 300 tokens/s.
  - This belongs to neither direction. It was not investigated further.

## 4. Projection of both directions (`projection.md`)

`scripts/projection.py` multiplies the census by the per-firing operation table. The table comes from `per-firing-ops.json`: two independent readings of every probe's source that agree on all GPU programs. Host-call time uses 9.8-15.6 us per call, serialized ([PROTO] in `../real-scenarios-20261113`). It is arithmetic, not measured.

**Direction 1 (per-thread storage), for probes that keep one slot or ring per thread:**
- **One capacity has to cover the largest launch.** For a probe on every kernel that is 4.19M slots. The average launch then writes 10% of them at 512 tokens, 0.9% at 32 tokens and 2.3% in decode.
- **With the default 1,024 slots**, at least 97-99.8% of per-thread records are lost. Measured on rms_norm at 512 tokens (section 2): 99.60% not counted, with one warning line from the GPU and no count of the loss. For 1527, lost records are not counted as drops either.
- **kernelretsnoop's per-thread rings** (16 x 1,032 B per thread) would need 63-65 GiB at 4.1-4.2M threads; the segment is 128 MiB. A probe on gpu_ext's single RoPE-K kernel already needs 8.1 GiB at 512 tokens.
- **The counter form fits.** threadhist's array is 32 MiB at 4.19M slots.

**Direction 2 (host calls; one case measured, section 6):**
- **Tools written for real kernels make none.** That covers gpu_ext's kernelretsnoop (warp leader), launchlate (thread 0) and threadhist, and bpftime's threadhist and kernelretsnoop.
- **CPU-style tools (bpftime's launchlate, threadscheduling, cuda-counter) make 1, 3 or 8 host calls per thread per launch.**
  - On rms_norm alone (2% of GPU time), a 32-token prefill would take 7-11 s with one printk per thread and 55-88 s with cuda-counter, against 24 ms of GPU time.
  - On every kernel, a 512-token prefill would take 31-50 min with launchlate and 4.2-6.7 h with cuda-counter.
  - Combining the calls of a warp into one cuts this by 32 times but leaves 59-96 s per 512-token prefill with one call per thread, every kernel.
- **Even one host call per launch is visible in decode.** It costs 4.4-6.9 ms per 512-token prefill (2.1-3.4% of its GPU time). In decode it costs 4.2-6.6 ms per token, 26-42% of the 15.9 ms GPU time per token, because decode runs 424 small-to-medium launches per token.

## 5. Observations for deciding how far to go

- **Direction 1 shows up in every per-thread scenario on a real application.** The symptoms are the ones gpu_ext reported, here with exact counts:
  - one guessed capacity cannot fit launches from 512 to 4.19M threads;
  - the default loses almost everything;
  - per-thread rings do not fit in memory.

  Sizing per launch, or keeping per-warp or GPU-local storage, would address these on the real workload.
- **Direction 2 has no volume in tools written for real kernels.** For CPU-style tools on a real model, per-thread host calls cannot be used at any realistic cost, even aggregated per warp.
  - The only regime where a host call per probe is tolerable is per launch, and even that costs a quarter or more of decode time.
  - So on real applications, direction 2 matters only if CPU-style tools are to be usable. That would mean removing per-thread host calls (eliminate or defer, as in the design) rather than making synchronous calls faster. Whether that is worth doing is open.

## 6. Measured: one host call per thread on a real kernel

The probe is a CPU-style counter on rms_norm (`probes/th-hostcall`): one `bpf_map_lookup_elem` on an ordinary HASH map per GPU thread, then an atomic add through the returned pointer.
- On dev, every lookup is a synchronous host call.
- The loader inserts key 0 before attaching, so every lookup hits.
- The run was a 32-token prefill: one warm-up, then one timed run, with graphs off (`runs/llama-hostcall-pp32`, 23:02 JST, clocks checked).

| | measured | projected (section 4, one host call per thread on rms_norm) |
|---|---:|---:|
| count after warm-up + timed run | 1,411,072 = 2 x 705,536, every thread counted | 705,536 per prefill |
| timed prefill | **7.63 s** (4.2 tokens/s) | 6.9-11.0 s |
| per host call | 10.8 us (7.63 s / 705,536); 10.7 us from the loader's once-a-second count (median of 14 busy seconds, 84-98k calls/s) | 9.8-15.6 us |
| against the same prefill without the agent | 24.9 ms (1,287 tokens/s): **307 times slower** | 291-463 times the GPU time |

- **The host-call cost holds on a real kernel.** The microbenchmark's cost carries over to llama.cpp at 10.8 us per call, and the calls ran one after another at about 93,000 per second.
- **No increment was lost.** The pointer the host call returns is usable for an atomic add from every lane.
- **The probed kernel is small.** rms_norm is 1.5% of this prefill's GPU time and runs 45 of its 488 launches. A per-thread host call on it alone turned a 25 ms prefill into 7.6 s.

## Limits

- One application and one model; prompts of 32, 128 and 512 tokens; decode of 128 tokens; batch 1.
- Census counts come from cold runs (`--no-warmup`); times in the census tables are first-run GPU times.
- Measured with bpftime: threadhist on four kernels and one CPU-style host-call counter on rms_norm (section 6). kernelretsnoop and the other CPU-style tools were not run on llama.cpp; their numbers are projections. The host-call projection matched the one measured case (7.63 s against 6.9-11.0 s).
- The overhead extrapolation to every kernel assumes the per-thread-op cost on rms_norm carries over to other kernels.
- Host-call costs come from the protocol microbenchmark. Under CUDA graphs, calls would cost more (about 70 us); graphs were off in every bpftime run.
- Per-thread host-call counts are from `per-firing-ops.json`: 8 for cuda-counter, 3 for threadscheduling. They supersede the "about 10" and "5" in `../real-scenarios-20261113` (see item 16 there).

## Files

- `census/`:
  - `<run>-<graphs>.csv`: per kernel launches, threads per launch, thread executions, GPU time;
  - `libggml-cuda-entries.txt`;
  - `logs/`: llama-bench JSON, clocks, configure log.
- `runs/`: the 15 bpftime runs (meta, loader and victim logs) and `summary.md`.
- `probes/`: the threadhist port, its copies on other kernels (`th-*`), the empty control and the host-call counter (`th-hostcall`).
- `per-firing-ops.json`: two independent per-firing operation tables and the reconcile verdicts. Made by `scripts/per-firing-op-workflow.js`.
- `projection.md`: made by `scripts/projection.py`.
- `scripts/`: census.sh, batch2.sh (graph census and count checks), parse_census.py, projection.py, runs_summary.py.
- Raw nsys reports and SQLite exports (about 40 MB) stay in `bpftime-gpu-work/llama/census/`; they are not committed.
