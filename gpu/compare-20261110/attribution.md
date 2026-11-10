# Which dev change causes each official-vs-dev difference

Every difference between official 4708f409 and dev c115b33d in this benchmark, with the dev commit
(SHA on c115b33d's history) and the part of it that causes it. From a read-only review: one
reader per difference group (source of both trees, `git log -S/-G/-L` over 4708f409..c115b33d,
these runs' logs, the fix round's measurements and mutation tests), each checked by a code-lens
and a data-lens skeptic, then a completeness pass. No new runs.

Strength:

- **proven**: a measurement or mutation isolates the change, or log lines in these runs show the
  mechanism on both sides;
- **strong**: only that commit touches the code path, no isolating measurement;
- **inferred**: candidates named, not separated;
- **unknown**: the difference is real but no candidate is supported.

## Settled

| difference | cause | strength | evidence |
|---|---|---|---|
| CPU map cases 33.7 ms vs 0.32-0.37 ms; CUDA Counter 337 ms vs 3.5 ms | **518f05b5**, the watcher loop only: official sleeps 1 ms after every request (official `bpf_attach_ctx_cuda.cpp:275-276`), so each host call costs about 1.05 ms; dev claims requests by CAS, answers by release store and keeps polling 200 us after a request (dev `:184-257`). The other parts of 518f05b5 (request written under the lock, GPU-memory module lock) do not matter at one warp. | proven | 518f05b5's mutation putting the 1 ms sleep back: 2048 calls in 2161 ms = 1.055 ms/call (`fixes-20261109/protocol/mutations/`). Official here: (33,713-39.5)/32 = 1,052 us per call; CUDA Counter (337,373-39.4)/320 = 1,054 us. Call counts identical on both builds at debug level (E: 128 per micro case = 4 launches x 32 threads; Counter 352 + 3 x 320). Same 33.4-36.1 ms in upstream's 5090/P40 tables. |
| Map types 1502/1503 fail on official (D micro cases; A Thread Histogram, Memory Trace), work on dev | **3823cac5**: the syscall server reads `CU_DEVICE_ATTRIBUTE_INTEGRATED` and creates 1502/1503 as the host-memory types 1512/1513 (dev `syscall_context.cpp:240-250, 566-580`). Official always exports device memory through `cuIpcGetMemHandle`, which returns 801 on the Orin. | proven | D/A logs: `Unable to open CUDA IPC handle ...: 801`, `failed to create: -EINVAL`, `Failed to load and verify BPF skeleton`. C log: `type 1502 exports device memory through CUDA IPC, which this integrated GPU lacks; creating type 1512 in host memory`. With 1512/1513 asked for directly (gate G) official counts correctly. |
| Default nvcc 13 victim (`__cudaLaunchKernel`): official runs it uninstrumented (D Counter 46 us, no calls; Retsnoop no events), dev instruments it (C: calls=97, events=128) | **811b841f**, load-time substitution: dev hooks `cuLibraryLoadData` and loads the patched image in place of the module cudart loads, so every launch runs patched code. Official patches too (`Recorded pass` in D) but reaches the patched kernel only by naming each launch on the late path, and a CUDA 13 kernel handle cannot be named (upstream #646). | strong | Code blame: every cited line is 811b841f. Isolating run not done: dev with `BPFTIME_CUDA_LOAD_SUBSTITUTION=0` should fail like D. |
| Kernel Retsnoop events: official 32 of 128, dev 128 | **518f05b5**: (1) host `drain_data` now passes every record up to tail per poll; official takes one record per thread per poll and stops at a record being written (official `nv_gpu_ringbuf_map.cpp:61-78`); (2) the kernelretsnoop loader polls every 10 ms instead of every 1 s. The runner kills the loader right after the victim exits. | proven (commit); parts not separated | Only 518f05b5 touches the ring map and that loader. All 32 official events carry one timestamp (the warm-up launch); dev's 128 form 4 launches. No drop on the GPU on either build (4 records per 16-entry ring). |
| Per-GPU-thread array medium/large/xlarge: official writes past its 1024-thread map (skipped), dev runs them | **518f05b5**, GPU array bounds: threads or keys outside the map get NULL / -E2BIG, one warning per module. Official returns the out-of-range address and prints a warning per thread. | proven | Dev logs 1 `outside map` warning per case; official source `default_trampoline.cu:189-198`. The dev numbers cover only the first 1024 threads' map work. |
| Attach time (not in the benchmark's numbers; the runner waits a fixed 10 s): official about 1.4 s, dev about 0.15 s | **811b841f**: the default path patches fatbins as cudart loads them, without the late bootstrap's host symbol cache (555,942 symbols) and repeated fatbin scans. | strong | Log spans from shm registration to the last pass line in every case of every round. |

Log-only differences (no effect on numbers): `Not a GPU map!` and `VM factory ... already
registered` warnings official-only (fa499bcf demotes them); memtrace's patched-image line has an
empty probe list on dev (fa499bcf); `bpf_printk` through spdlog on dev (47735d88); `Using SM arch
from BPFTIME_SM_ARCH` official-only (2349d977 stops the setenv); shared memory 50 MB vs 128 MB
(99845759); missing-key deletes report -1 on dev, 0 on official (b6391cd6); the watcher's debug
line format (518f05b5).

## Not settled

Single-run gaps under about 5 us cannot be attributed at all: the no-agent baselines, which run
no bpftime code, already spread 35.1-40.4 us across sets.

| difference | candidates | strength | what would settle it |
|---|---|---|---|
| GPU Ringbuf small_64x16 / small / medium: dev +16% / +19% / +12% | The micro loader never polls, so after 16 launches every ring is full on both builds and nearly every timed call takes the drop path. 518f05b5 made that path heavier on dev (busy-word CAS, `membar.gl`, volatile loads, drop counter) where official does two loads and returns. 5c5312d7's per-call gate adds to dev. | inferred | Dev with official's drop path (518f05b5 device part reverted on the full branch only), and a polling probe copy so the ring never fills. |
| Kernel Retsnoop time: official 61.7 us, dev 46.0 us | None supported. 636501ff (stack frame 16,464 -> 592 B) is contradicted: the same frame change shows no gain in other cases. | unknown | More timed iterations (vec_add `32 1000 32 1`), then the launch-path ablation below. |
| Entry+Exit small_64x16 +15% / small +9%, Memtrace tiny +6% (dev slower) | Not a per-launch cost: the per-round medians overlap or match. 12 of 99 official per-round medians sit at 25-28 us (none of dev's), so official sometimes runs in a faster mode. Candidates: official's late-path launch (811b841f replaced it); or official occasionally running the original, uninstrumented kernel, which would be a correctness gap, not speed. | unknown | Dev with `BPFTIME_CUDA_LOAD_SUBSTITUTION=0` (late path, launches redirected like official), 10+ rounds in random order. |
| Large/xlarge light probes, dev faster: Entry+Exit large -15%, Memtrace large -6%, GPU Array lookup xlarge -3%, Empty/Entry xlarge -1-2% | 811b841f's launch path: official, per launch, looks up the host symbol, launches the patched function through a second hook and records two CUDA events; dev launches through cudart unchanged. 636501ff's smaller GPU stack frame (16 KB -> 592 B per thread) for the cases that pass a stack address (Memtrace, array lookup). Kernels are otherwise identical (ptxas: same registers). | inferred | Launch-path ablation as above; dev rebuilt with the frame forced back to 32 slots (one line in `local-call-return-block.patch`). |
| Per-GPU-thread array tiny -16% / small_64x16 -11%; GPU Array update small_64x16 -17% / small -10% (dev faster) | 636501ff (frame), 518f05b5 (bounds checks: extra work for dev), 813c3ecb (regenerated helper PTX, 24-30 -> 32-36 registers, no occupancy change), 811b841f (launch path). A lookup-only case with the same frame change shows no gain at these sizes, so the frame alone does not explain it. | unknown | The frame and launch-path ablations; dev without the 1512/1513 bounds checks. |
| GPU Array update large +13% (dev slower) but xlarge -14% (dev faster) | Same candidates. 100k-1M threads write one value in host memory with two `membar.sys` each, a contention-bound case; the sign flip rules out a simple per-thread cost. | unknown | The same ablations, 5+ rounds, and the final counter value (lost updates). |
| On dev, lookups cost more than updates (array lookup 367 vs update 324 us) | Dev-internal, not an official-vs-dev difference. 2349d977's extra `cuMemHostGetDevicePointer` per returned pointer is one candidate; the hash-lookup gap has none. | unknown | Time that driver call; a hash lookup with key 0 present. |
| Global timer, Empty/Entry/Exit small and medium, other overlapping rows | Inside the 3-round spread. | none claimed | — |

## GPU commits with no measured effect on these runs

- 49251fff (cmake): configure time only.
- 12b6ee8f (unpatchable fatbin keeps its module): every fatbin patched; 0 unpatched launches.
- 9ccda7e8 (server exit crash): exit codes were not recorded by the runner.
- c36bc849 (test), c115b33d (no-GPU agent): not on this path.
- f9b823fa (bpf-to-bpf frame step): no probe here has local calls.
- 5c5312d7 (probes off at the last detach): adds a per-call gate on dev, a cost not a gain; size not measured.

## Ablations that would settle the open rows

Each on dev, 10+ interleaved rounds against official, cases as listed above:

1. `BPFTIME_CUDA_LOAD_SUBSTITUTION=0`: the late path, no rebuild (811b841f's launch path).
2. GPU stack frame forced back to 32 slots (636501ff), rebuild.
3. Probe gate removed (5c5312d7), rebuild.
4. Official's ringbuf drop path, and the 1512/1513 bounds checks removed (518f05b5 device
   parts), trampoline PTX regenerated, rebuild.
