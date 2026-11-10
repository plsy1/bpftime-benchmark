# Which dev change causes each official-vs-dev difference

Every difference between official 4708f409 and dev c115b33d in this benchmark, with the dev commit
(SHA on c115b33d's history) and the part of it that causes it. From a read-only review: one
reader per difference group (source of both trees, `git log -S/-G/-L` over 4708f409..c115b33d,
these runs' logs, the fix round's measurements and mutation tests), each checked by a code-lens
and a data-lens skeptic, then a completeness pass. The open rows were then measured with
ablations (section "Ablations": each candidate change undone on dev, 10 interleaved rounds
against official and dev; [`../ablate-20261110`](../ablate-20261110/)).

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
| Default nvcc 13 victim (`__cudaLaunchKernel`): official runs it uninstrumented (D Counter 46 us, no calls; Retsnoop no events), dev instruments it (C: calls=97, events=128) | **811b841f**, load-time substitution: dev hooks `cuLibraryLoadData` and loads the patched image in place of the module cudart loads, so every launch runs patched code. Official patches too (`Recorded pass` in D) but reaches the patched kernel only by naming each launch on the late path, and a CUDA 13 kernel handle cannot be named (upstream #646). | strong | Code blame: every cited line is 811b841f. Isolating run not done: dev with `BPFTIME_CUDA_LOAD_SUBSTITUTION=0` on the default victim should fail like D. The ablation's L set runs that mode on the legacy victim only, so it does not test the CUDA 13 handle. |
| Kernel Retsnoop events: official 32 of 128, dev 128 | **518f05b5**: (1) host `drain_data` now passes every record up to tail per poll; official takes one record per thread per poll and stops at a record being written (official `nv_gpu_ringbuf_map.cpp:61-78`); (2) the kernelretsnoop loader polls every 10 ms instead of every 1 s. The runner kills the loader right after the victim exits. | proven (commit); host side | Only 518f05b5 touches the ring map and that loader. The ablation's T set (518f05b5's ringbuf device code undone) still delivers 128 events in all 10 rounds, so the cause is the host side (`drain_data`, the loader's poll); the two host parts are not separated. All 32 official events carry one timestamp (the warm-up launch); dev's 128 form 4 launches. No drop on the GPU on either build (4 records per 16-entry ring). |
| Per-GPU-thread array medium/large/xlarge: official writes past its 1024-thread map (skipped), dev runs them | **518f05b5**, GPU array bounds: threads or keys outside the map get NULL / -E2BIG, one warning per module. Official returns the out-of-range address and prints a warning per thread. | proven | Dev logs 1 `outside map` warning per case; official source `default_trampoline.cu:189-198`. The dev numbers cover only the first 1024 threads' map work. |
| Attach time (not in the benchmark's numbers; the runner waits a fixed 10 s): official about 1.4 s, dev about 0.15 s | **811b841f**: the default path patches fatbins as cudart loads them, without the late bootstrap's host symbol cache (555,942 symbols) and repeated fatbin scans. | strong | Log spans from shm registration to the last pass line in every case of every round. |

Log-only differences (no effect on numbers): `Not a GPU map!` and `VM factory ... already
registered` warnings official-only (fa499bcf demotes them); memtrace's patched-image line has an
empty probe list on dev (fa499bcf); `bpf_printk` through spdlog on dev (47735d88); `Using SM arch
from BPFTIME_SM_ARCH` official-only (2349d977 stops the setenv); shared memory 50 MB vs 128 MB
(99845759); missing-key deletes report -1 on dev, 0 on official (b6391cd6); the watcher's debug
line format (518f05b5).

## Ablations

Each candidate undone on its own on top of dev, then 10 rounds of official (O), dev (D) and the
four ablations, interleaved with the order rotated per round
([`../ablate-20261110`](../ablate-20261110/), tables in its summary.md):

- **L**: dev with `BPFTIME_CUDA_LOAD_SUBSTITUTION=0`: the late path, launches redirected by name
  as on official (811b841f's launch path undone, no rebuild);
- **F**: GPU stack frame back to 32 slots (636501ff undone);
- **G**: no probe gate (5c5312d7 undone);
- **T**: official's 1512/1513 lookup/update device code, and the ringbuf's full check before
  dev's busy-word CAS (518f05b5's device parts undone; host side unchanged).

Numbers below are medians over the 10 rounds of the benchmark's number (the per-run mean), O vs
D, then the ablation's shift against D with its Mann-Whitney p (10 vs 10 rounds). 25 cases x 5
comparisons are tested, so about one p below 0.01 is expected by chance; the claims marked
proven rest on p < 0.001 and a shift that moves dev to official or past it.

| difference (10 rounds: O vs D) | cause | strength | evidence |
|---|---|---|---|
| GPU Ringbuf small_64x16 / small / medium, dev slower: 38.4 / 38.2 / 37.7 vs 45.7 / 42.0 / 44.2 us (+19% / +10% / +17%) | **518f05b5**, the GPU ringbuf write path: on a full ring (the micro loader never polls, so nearly every timed call finds it full) dev takes the busy word, fences and counts the drop; official checks for a full ring first and returns. | proven | T: -11.9 / -6.2 / -7.8 us (p < 0.001), to 33.8 / 35.7 / 36.4, below official by about the launch-path saving below. L, F, G do not lower it (L raises it by 9-13 us). |
| Light probes at large / xlarge, dev faster: Entry+Exit large 45.2 vs 38.6, Empty large 44.7 vs 37.9, Empty xlarge 177.1 vs 172.3, Entry xlarge 172.6 vs 170.5, Memtrace large 177.6 vs 172.9 us | **811b841f**, the launch path: dev launches the patched module through cudart unchanged; official redirects every launch to the patched kernel by name. | proven | L: +8.7 to +13.6 us on each (p < 0.001), past official. F, G, T: no shift above 2 us with p < 0.05. |
| Small map cases, dev faster: Per-GPU-thread array tiny / small_64x16 38.9 / 39.4 vs 35.9 / 36.2, GPU Array update small_64x16 / small 40.8 / 40.8 vs 35.5 / 36.2 us | **811b841f**, the launch path, as above. 518f05b5's bounds checks (extra work on dev) cost nothing measurable here. | proven | L: +10.0 to +12.8 us (p < 0.001). T within -1.4 to +0.3 us (p >= 0.10), F and G within 2.1 us (p >= 0.16). |
| Kernel Retsnoop time, dev faster: 60.3 vs 49.1 us | **811b841f**, the launch path; **636501ff** (one stack frame) adds a cost in one launch per run, not per launch. | proven (L), likely (F) | L: +10.7 us (p < 0.001), on the fastest and the slowest of the 3 timed launches alike. F: +6.7 us on the means (p < 0.001), +2.3 on the round medians (p 0.026); per round, F's slowest launch is 71.1 us against D's 59.0 (medians over rounds), its fastest 45.9 against 43.5: one launch per run pays about 12 us more with 16 KB frames (official, also 16 KB: slowest 66-82 us), likely local memory set up for the larger frame. The compare round's "contradicted" rested on the micro cases, which time thousands of launches. T, G: no increase. |
| GPU Array map lookup xlarge, dev faster: 457.7 vs 444.9 us (-3%) | **811b841f**, the launch path; the gate's removal moves it as much. | inferred | L: +10.5 us (p 0.010); G: +11.6 us (p 0.007), although G only removes a load; mechanism not known (code layout, scheduling). |
| GPU Array map update large, dev slower: 221.1 vs 246.9 us (+12%) | **518f05b5**'s 1513 update device code (bounds check, E2BIG path, fence form), and **5c5312d7**'s gate. 100k threads update one value in host memory: a contention-bound case. | proven (T), likely (G) | T: -15.3 us (p < 0.001, 59% of the gap); G: -11.1 us (p 0.005, 43%). Not tested together. L, F: -2 to -3 us. |
| GPU Array map update xlarge, dev faster: 1,682 vs 1,509 us (-10%) | Not one of the four. | unknown | No ablation brings dev near official: G +50 us (p 0.011), T -63 us (p 0.006, away from official), L -13, F -33 (p >= 0.10). Untested candidates: 813c3ecb (helper PTX regenerated, more registers), host-memory registration of the map (2349d977). |
| Entry+Exit small_64x16 / small, Memtrace tiny (the compare round's +15% / +9% / +6%) | No difference: the 3-round gaps were noise. | settled | 10 rounds: 34.9 vs 33.5, 31.3 vs 32.8, 32.2 vs 33.2 us, p 0.97 / 0.45 / 1.0. |

What the ablations also show:

- **636501ff (one GPU stack frame)** changes no light-probe or map case's time (largest:
  Memtrace large +2.0 us, p 0.054); the one micro shift is GPU Ringbuf small, +2.1 us (p 0.014;
  small_64x16 and medium -1.5 / -0.9, p >= 0.19), away from official, not explained. It saves
  local memory (592 instead of 16,464 bytes a thread); time only in Retsnoop's one launch.
- **5c5312d7 (probe gate)** costs nothing measurable on the light probes (G within 3 us,
  p >= 0.16); it moves only the contention-bound array cases, in both directions.
- **Dev's late path costs more per launch than official's**: L is 5-9 us slower than O on 11
  of the 14 light-probe and small-map cases (Entry+Exit large 50.5 vs 45.2, Empty large 51.5 vs
  44.7, Per-GPU-thread array tiny 45.9 vs 38.9 us; 1.8 us on Entry+Exit small_64x16, within
  2.3 us of official on GPU Array lookup xlarge and Kernel Retsnoop). The late path is not dev's default; it runs when the driver was
  initialized before the agent (an injected agent, CUDA in a static constructor). Not attributed:
  per redirected launch dev calls `dladdr`, then its host symbol resolver, and builds the name
  strings (`resolve_host_function_symbol`, `nv_attach_impl.cpp:1709`); official's cost was not
  broken down.
- Every dev set (D, L, F, G, T): 128 Retsnoop events in all 10 rounds; official 32.

## Not settled

Single-run gaps under about 5 us cannot be attributed at all: the no-agent baselines, which run
no bpftime code, already spread 35.1-40.4 us across sets.

| difference | candidates | strength | what would settle it |
|---|---|---|---|
| GPU Array map update xlarge (above) | 813c3ecb, 2349d977, or an interaction of 518f05b5's device code with the gate | unknown | Those reverted on dev; T and G together; the final counter value (lost updates). |
| Dev's late path 5-9 us per launch slower than official's (above) | Per-launch name resolution (`dladdr`, the resolver, strings) | unknown | Time the redirect's parts; cache the stub-to-function lookup and rerun L. |
| On dev, lookups cost more than updates (array lookup 367 vs update 324 us) | Dev-internal, not an official-vs-dev difference. 2349d977's extra `cuMemHostGetDevicePointer` per returned pointer is one candidate; the hash-lookup gap has none. | unknown | Time that driver call; a hash lookup with key 0 present. |
| Global timer, Empty/Entry/Exit small and medium, GPU Array map lookup medium, other overlapping rows of the compare round | Inside the 3-round spread; not in the ablation set. | none claimed | — |

## GPU commits with no measured effect on these runs

- 49251fff (cmake): configure time only.
- 12b6ee8f (unpatchable fatbin keeps its module): every fatbin patched; 0 unpatched launches.
- 9ccda7e8 (server exit crash): exit codes were not recorded by the runner.
- c36bc849 (test), c115b33d (no-GPU agent): not on this path.
- f9b823fa (bpf-to-bpf frame step): no probe here has local calls.
