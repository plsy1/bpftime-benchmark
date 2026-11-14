# Research directions

The research directions chosen after the official-vs-dev GPU comparison (2026-11-10/11): the
current conclusions and the evidence for them. How they were reached is in `TIMELINE.md`, the
results branch, and the earlier full versions of this file ("Earlier versions" at the end).

## Current state (2026-11-15)

- **Candidate focus, to be confirmed with the advisor:** a GPU map model (direction 1, widened).
- **Direction 2** (GPU-to-CPU host calls): its engineering layer is in dev, two items still open;
  the research scheme (keep CPU maps, make host calls cheaper or defer them) is dropped.
- **Open:** the advisor's view; a discrete GPU for the evaluation; the evidence steps below.
- `TIMELINE.md`'s 11-14 entry ("both directions are needed") predates this decision, which was
  first recorded in fce97abb.

Terms:
- **Host call:** a GPU probe's helper call that the CPU serves while the GPU thread waits.
- **CPU-style tool:** an eBPF tool written as for the CPU: CPU map types (HASH and the like),
  printk, entry/exit state keyed by thread.
- **GPU-native tool:** a tool written for GPU probes with bpftime's GPU map types (15xx).
- **GPU map model:** what storage a GPU-side map has (how much, indexed by what, where in memory),
  what happens when it is full, and what the CPU-side reader sees.

## Direction 1: a GPU map model

### The problem

Two bpftime GPU map types keep one copy per GPU thread index, the thread's linear global id in the
launch:

| type | number | memory |
|---|---|---|
| per-thread array (`PERGPUTD_ARRAY_MAP` / `_HOST_MAP`) | 1502 / 1512 | value_size x max_entries x thread_count |
| GPU ringbuf (`GPU_RINGBUF_MAP`) | 1527 | (header + max_entries x (8 + value_size)) x thread_count |

- **thread_count is fixed when the map is created**, by the syscall server, before any launch and
  in another process. Default 1,024; the only override is `BPFTIME_MAP_GPU_THREAD_COUNT` on the
  server (official ignores it for 1512, the Orin's type; dev 3823cac5 reads it).
- **Threads past it lose data.** The ring returns silently. The per-thread array writes past the
  map on official; dev returns NULL / -E2BIG with one warning, and counts nothing.
- **Memory grows with the grid, not with the GPU.** The Orin holds 12,288 threads at a time (8 SMs
  x 1,536). When a ring does not fit the segment, the server halves its thread count.
- **Slots belong to a thread index, not to a running thread:** concurrent kernels share them.

### Evidence

Ours, llama.cpp on the Orin (`results/gpu/llama-census-20261113`; TinyLlama Q4_K_M, graphs off):
- Threads per launch: 512 to 4,194,304 in a 512-token prefill, 256 to 4,096,000 in decode; one
  kernel spans 125x across call sites. The application's configuration (ubatch, model sizes,
  batch) sets the bound; only the launch knows it.
- Default 1,024 slots: 99.6% of rms_norm's thread executions at 512 tokens went uncounted, with one
  GPU warning line and no count.
- Sized by hand, counts are exact: threadhist on rms_norm cost 3.3% of the prefill (262,144 slots,
  2 MiB). Every kernel would need 4.19M slots (32 MiB), at about +58% probe cost [inferred].
- Per-thread records in kernelretsnoop's layout would need about 63-65 GiB at 4.1-4.2M threads
  [projected].

gpu_ext's revision notes (RTX 5090, llama.cpp 512-token prefill; not in its paper except the 5.57%;
`results/gpu/buffer-survey-20261111`, `results/gpu/overlap-survey-20261113`):
- kernelretsnoop on bpftime's per-thread ring: about 91% throughput lost (99% with full records);
  892 MiB allocated for 22 MiB of records. With full per-thread records a hand-written GPU arena
  still cost 39.1% (12.9% with a warp-contiguous layout); the 5% below is for per-warp records.
- A hand-written GPU-memory buffer with a slot per warp: 22.1 MiB, 5.4-5.6% (the paper's 5.57%
  against NVBit). Separate campaigns; placement, layout, publication and drain timing changed
  together. Draining the ring continuously or at the end made no difference: the cost is on the
  producer side.
- Placement alone (a 5090 microbenchmark, one 32-thread block): an update to a device-memory array
  3.85 us per launch, to a host-mapped one 36.2 us (9.4x); lookups differ by 1.09x. On the Orin
  every map is host-mapped, and the gap to device memory was not measured ("GPU map types on an
  integrated GPU" below).
- Capacities were set from offline thread counts (524,288 at 512 tokens); an earlier capped 8,192
  run lost records, and a 32-token preflight dropped 450,560 events. The final runs lost nothing:
  the point is that capacity had to be tuned by hand.

Upstream bpftime: only stopgaps (#423, #501, #515, #490, #522); CI sets a different thread count
per example (#490); #530 (threadhist counts 1,024 threads) closed with the env. Thread-count checks
and drop counts exist only on unmerged `official/revision/*` branches. No commit touched the GPU
maps between 2026-09-01 and 745443f6 (2026-11-13).

### How other systems bound their storage

| system | storage | when full |
|---|---|---|
| Neutrino (OSDI '25) | per launch, sized from the grid, copied back after each launch | no bounds check |
| NVBit | one channel per context, warp-level reservation | writers wait for the host |
| KPerfIR / Proton | fixed buffer per warp group / per segment | overwrite / drop |
| hip-analyzer (MI210) | 4 KiB chunks per wavefront (kernel slowdown median 1.33x vs 1.96x for a global atomic) | - |
| GT-Pin | buckets per hardware thread | - |
| CUPTI activity | per-context device buffers | drop, counted |
| gpu_ext | bpftime's maps sized by hand, one hand-written per-warp buffer | maps as bpftime; the buffer counts drops |

KPerfIR and Proton were read only through documentation summaries, and hip-analyzer's numbers are
from an unpublished thesis: re-check before citing.

- Per-launch copy-back dominates on large grids (Neutrino Conv2D with per-thread maps: 5.3-5.45x of
  the kernel in all, about 4x of it collection; per warp 1.36x). Blocking when full looks expensive
  (GVProf median 31x whole-run without sampling; the buffer's share not isolated).
- gpu_ext's paper describes per-warp/per-SM shards merged with relaxed consistency, but they are not
  in its repository (its own 2026-09-04 audit; their code was not run), and the paper never sizes a
  thread-indexed map.
- No GPU eBPF map type is sized by the launch or by the hardware (only hand-written uses). The new
  part is a model that does so within eBPF map semantics. The 11-11 survey found no buffer
  measurements on an integrated GPU.

### Requirements (our proposal)

1. Storage bounded by the hardware (per SM, per warp slot: 8 and 384 on the Orin) or by an explicit
   budget, never by the grid.
2. No silent loss: exact counters (aggregate in the warp, write per hardware slot, sum on read), or
   drops counted and shown to the reader.
3. Bounded record streams: a ring per SM or warp slot with warp-level reservation, drained while
   running.
4. GPU cost close to the hand-written buffers (about 5%) without hand-writing.
5. A defined CPU-side view through the standard `bpf()` API, ideally per-CPU map semantics.
6. Hardware only as cost parameters (uncached registered host memory on the Orin, device memory
   and copy-back on a discrete GPU).

### Focus (our judgement)

- **Tier 1, engineering:** count and report every loss, put the needed size into the warning, no
  silent shrinking. This fixes the counter case; it is not research.
- **Tier 2, the research focus:** hardware-position storage and per-SM rings, for record streams
  and unknown scale.
- **Tier 3:** per-launch per-thread storage, only if a scenario needs it.
- **Not in focus:** hash (arbitrary-key) maps on the GPU. 1501 is a host call on every operation,
  but no GPU-native tool is known to need one; gpu_ext's tools use fixed-bin arrays.
- **Ceiling:** entry and exit probes give a thread at most one or two records per launch;
  `__memcapture` fires at every load and store but gives no address. Scenarios that need new attach
  points (correctness checking, instruction mix, region timing, address traces) would raise record
  volume and the weight of this direction.
- **Would overturn it:** hand-written bounded buffers turn out easy to package as a library without
  runtime support; GPU-native tools turn out to need counters only; the bpftime/gpu_ext team builds
  hierarchical maps first.

### Open questions

- Which storage GPU-native tools need: counters, per-thread state, or record streams.
- The cost on the Orin of a warp-level reservation in host-registered memory against per-thread
  rings.
- Concurrent kernels and CUDA graphs sharing buffers; `%warpid` and `%smid` can change after
  preemption (PTX), which hardware-position slots must allow for.
- The `bpf()` view of a map whose storage follows the hardware.
- Prior work on per-thread state on GPUs (privatization plus reduction, per-thread counters) has
  not been searched yet; it bears on requirement 2.
- Noticed, not verified: 1501 makes `next_prime(max_entries)` buckets but a value array of
  `max_entries`, so keys in the last buckets may fail.

### Next steps (proposed; decision with the advisor)

1. Tell the advisor: direction 2's research scheme dropped, a GPU map model as the candidate focus
   (the earlier email leaned to direction 2 first).
2. A map-usage survey of GPU-native tools (gpu_ext, Neutrino, NVBit tools, KPerfIR/Proton).
3. Reproduce per-thread ring vs bounded buffer on llama.cpp on the Orin: overhead, memory, loss.
4. Then the tier-2 design, and a discrete-GPU platform.

## Direction 2: GPU-to-CPU host calls (research scheme dropped 2026-11-14)

**What goes to the host.** The trampoline (dev 52126f33, `default_trampoline.cu`) has the only six
`host_call()` sites: lookup and update on any map that is not 1502, 1503/1504, 1512, 1513 or an
ordinary ARRAY with a device address; delete on any map but such an ARRAY; `bpf_trace_printk`;
`puts`; and `bpf_get_current_pid_tgid` in a module the agent did not write.

**How a call works and what it costs.** One request slot per process; a lane queues on a per-module
lock, takes the slot, writes the request, waits for the watcher thread; lanes take turns; four
`membar.sys` per call. On llama.cpp a call took 10.8 us (about 4.9 us bare round trip plus about
6 us protocol [inferred]), served one after another: a HASH lookup per thread on rms_norm made a
32-token prefill take 7.63 s instead of 24.9 ms (307x).

**Done, in dev:**
- ordinary ARRAY maps on the GPU (1b3d9c99) and pid as a per-module constant (aa6eb973): CPU Array
  update 320 -> 51 us, lookup 371 -> 49 us, CUDA Counter 3,487 -> 2,929 us per launch
  (`results/gpu/hostcall-l1-20261111`);
- the watcher's waiting rules (52126f33, `results/gpu/watcher-policy-20261112`): a sparse call
  about 70 us (14 us with WFE, which held a whole core), 0.02-0.13 of a core, graph launches 70 us;
  WFE waiting is opt-in (`BPFTIME_CUDA_WATCHER_WFE=1`).

Still open: several CUDA processes on one segment (each agent zeroes the request slot, and any
process's watcher may serve any request: a correctness bug); lookup address translation (two driver
calls per hit).

**Why the research scheme was dropped.** Host calls come from CPU-type maps, printk/puts and deletes.
CPU-style tools use them (all 59 surveyed sites are in bpftime's own GPU examples,
`results/gpu/callsite-survey-20261113`); the tools already run on real kernels make none (gpu_ext's
15; bpftime's four on llama.cpp, PyTorch, FAISS, CUTLASS); and no CPU tool runs on the GPU
unchanged (below). A faster or deferred host call (layer 2's protocol, the per-call-site design in
`results/gpu/dir2-design-20261113`, never built) would serve only CPU-style tools, and no tool run
on real kernels is one. What remains, arbitrary-key maps on the GPU, moved to direction 1.

## Background facts

### No CPU eBPF tool runs on the GPU unchanged (read from code, not run)

- **Attach points:** a CUDA kernel's entry or exit (`kprobe/`/`kretprobe/` plus the mangled name,
  program names starting with `cuda__`), or `kprobe/__memcapture` before every PTX load and store
  (the PTX line's text, no address). No register or argument context.
- **Helpers:** the PTX path registers 1, 2, 3, 6, 14, 25 (GPU ringbuf only) and the 5xx GPU helpers
  (`ptxpass_core/src/core.cpp:235-253`). Any other helper fails PTX generation, and
  `generate_ptx`'s `ExitOnError` then ends the application
  (`vm/llvm-jit/src/llvm_jit_context.cpp:907, 938`).
- **What CPU tools use** (57 libbpf-tools, `.bpf.c` files naming each): `bpf_ktime_get_ns` 36,
  `bpf_get_current_comm` 35, `bpf_probe_read*` 25, `bpf_perf_event_output` 22, `bpf_get_stackid` 7;
  HASH maps 48. pid is one constant for every GPU thread, there is no comm or stack, and per-thread
  entry/exit keys become millions of keys.

What carries over is the programming model (eBPF, libbpf, standard map APIs, common idioms).

### GPU map types on an integrated GPU

- Without CUDA IPC (`cuIpcGetMemHandle` returns 801 on the Orin) dev creates 1502 as 1512, 1503 as
  1513 and 1501 as an ordinary HASH (3823cac5, `bpftime_shm.hpp:137-154`); 1504 and 1527 are
  unchanged. On official, creating 1501/1502/1503 fails on the Orin.
- 1501 keeps its keys and index in the shared segment and its values in GPU memory; the trampoline
  has no device path for it, so every GPU operation is a host call on any GPU.
- The Orin's GPU caches device memory but not pinned or registered host memory (CUDA for Tegra,
  Table 1; registered memory is cached only with full coherency, Thor onward). The 1512/1513 values
  and the segment are registered host memory, and the 1512, 1513 and ordinary-ARRAY paths add a
  `membar.sys` per lookup and update.
  Device memory is out because the CPU tool in another process must read the map (no CUDA IPC; the
  shareable-handle export was not tried).

### gpu_ext (`results/gpu/overlap-survey-20261113`, read again 2026-11-14)

- **What it is:** "eBPF extension in GPU driver": struct_ops hooks in a fork of NVIDIA's open kernel
  modules (575.57.08) for UVM eviction and prefetch and for scheduling; 83 host-side eBPF programs
  (policies, tests, tracers; the paper evaluates 59 policies) acting through a few kfuncs; the
  device side is bpftime's CUDA attach. Its central case is memory
  oversubscription on a discrete GPU; the Orin's GPU runs on `nvgpu`, so the driver part does not
  apply here.
- **Paper:** OSDI '26 rejected; ASPLOS '27 accepted as "gpu_ext: Safe and Programmable OS-Level GPU
  Resource Management with eBPF" (arXiv 2512.12615 is the OSDI-era text). The OSDI version named
  three challenges: a safe driver policy interface (C1), eBPF semantics against SIMT (C2), and
  host-device shared state (C3). The accepted version is framed around AI-generated policies and an
  asynchronous execution model.
- **Built:** C1 (the driver hooks, kfuncs and policies; its headline results, up to 1.76x over
  application-tuned baselines); the SIMT verifier (in bpftime only on unmerged PR #624). **Not in
  the public repository:** warp-leader aggregation, hierarchical maps (the camera-ready describes
  them as implemented; their code was not run, the authors' local trees not checked).
- **Device-side tools** (`results/gpu/callsite-survey-20261113`): 15 programs, 43 map call sites,
  all GPU types except two ordinary-ARRAY sites in a microbenchmark arm; no HASH; every key a
  bounded index.
- **Activity:** last commit 50a51ad (2026-09-24).

### Real applications (`results/gpu/real-scenarios-20261113`, `results/gpu/llama-census-20261113`)

- None of the published work we read measures what share of real workloads these operations take.
- The top-ranked GPU instrumentation scenarios (ranked by the number of tools, not by usage:
  correctness checking, instruction mix, address traces, region timing) first need attach points
  bpftime lacks: any instruction, the effective address.
- On the Orin only an application's own kernels built with PTX can be probed: cuBLAS, cuDNN and
  cuRAND carry PTX only for sm_120/121, and Triton's module loads are not hooked.
- llama.cpp: a 512-token prefill is 444 launches and 192.6M thread executions; decode is 424
  launches and 39.4M thread executions per token. An empty probe costs nothing once patched.
- Agent start-up added 14.4-14.8 s (likely the 10 lazily loaded modules patched again in every
  run, missing the patch cache; not timed per step), outside both directions.

## Earlier versions

The full record up to 2026-11-14, with the surveys' details, direction 2's layers 2 and 3, the
2026-11-13 paper framing and the watcher measurements, is in
[fce97abb](https://github.com/plsy1/bpftime-benchmark/blob/fce97abb/RESEARCH_DIRECTIONS.md). Raw
results are on the results branch under `gpu/`.
