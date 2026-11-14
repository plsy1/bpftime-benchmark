SIZE AT LAUNCH: storage sized from each launch's own grid x block (design only, dev 52126f33; nothing built or run)
Paths: C = /home/jetson/src/bpftime-daily-state/worktrees/4708f4092d1e/candidate. Files are relative to C unless named otherwise. FS = attach/nv_attach_impl/nv_attach_impl_frida_setup.cpp. DT = attach/nv_attach_impl/trampoline/default_trampoline.cu. R = /home/jetson/src/results/gpu. cuda.c is the Neutrino clone named by reader 3. "reader N, item" refers to the facts I was given. [arith] = my arithmetic on those figures.

A. Mechanism
1. The design as worded is blocked. Neutrino allocates a buffer per launch on the host, passes it as an extra kernel argument, syncs, then copies back (cuda.c:557, 566, 591, 620). Here that fails four ways:
   - map_info belongs to the module and is fixed at load (reader 1, A4-A5).
   - 53,848 of 54,272 decode launches are graph replays, which reach no hook (reader 1, A10-A11).
   - The host has about 37.5 us of GPU time per decode launch (15.91 ms / 424), and the gap between kernels is 0.13 us with graphs (reader 2). Folding per-launch per-thread copies at 8 B would move 1.54 GB per prefill and about 19.8 GB/s in decode [arith].
   - An extra kernel argument needs probes that take arguments; today the pass inserts `call fname;` (pass/ptxpass_kprobe_entry/main.cpp:93). Every launch path must also pass it, including unhooked or unnamed ones (cooperative launches, cuLaunchKernelEx, CUDA 13 handles). Otherwise the kernel reads garbage [inferred].
   What survives: Neutrino's size formula, storage that is reused, and in-place reads from the registered segment. There is no host step per launch.
2. Counters, histograms and per-thread results: one copy per thread index, summed over launches as threadhist expects.
   - Sizing: the agent keeps H = the largest threads-per-launch among all geometries it sees before a kernel runs. Sources: direct launches, before it calls the original (FS:95-113, FS:526); launches into capturing streams, which pass FS:111 with grid and block; graph node add/set params (FS:660-686, FS:716). Replays run the captured geometry (D2), so H rises before the first replay.
   - Layout: thread-major [thread][key][value] (1512 is key-major today, nv_gpu_array_host_map.hpp:22-24).
   - Growth: allocate a region for 2H from the segment. No CUDA call is needed, because the segment is registered once at start (runtime/src/bpftime_shm_internal.cpp:1460-1469). Old regions are kept. Then {region, H} is written into a control block in the segment; map_info.extra_buffer points to that block. These are plain host stores, so they are safe during capture. A writer that still holds an old pointer lands in an old region, and the reader sums all regions.
   - Device side: helpers 1/2 load the control block (one more uncached load) and check tid < H. A miss adds to a drop counter and does atomicMax on a `needed` word; the watcher or the next hook then grows the map.
3. Scratch (entry to exit) and record streams: one region per launch, sized on the device, because replays have no host step.
   - First touch (a helper call or the exit accounting below): hash %gridid (inline asm inside the trampoline, no new eBPF helper) into a 64-entry per-module __device__ table of live launches, a device global like DT:122, 127.
   - The thread that wins the CAS sizes the region from gridDim x blockDim (read as at DT:521-561), takes it from a pool with one atomicAdd on the pool's cursor, and publishes it. The other threads spin on the entry until it is published.
   - Two pools, allocated once at attach:
     - scratch: device memory, cuMemAlloc'd by the agent, with no CPU view; the slot is the global thread id inside the launch's region.
     - records: in the segment. Each logical warp gets K records of 8 + value_size bytes (1527's record layout, nv_gpu_ringbuf_map.cpp:11-23) plus a count. The region has a header {gridid, grid, block, globaltimer}.
   - Release: a pass inserts code before every ret/exit, as ptxpass_kretprobe/main.cpp:50-59 does. The warp leader adds popc(activemask) to the launch's entry. The add that reaches the thread total frees the entry and marks the region closed. The next claimant reclaims closed scratch regions; the CPU reader frees record regions after draining them.
   - Full pool or full table: the launch gets no region, and every helper call counts a drop (returns NULL / 2).
4. Files changed: DT and trampoline_ptx.h; a new exit-accounting pass; nv_attach_impl.cpp and FS; in runtime, nv_gpu_array_host_map.*, nv_gpu_ringbuf_map.*, map_handler.cpp, syscall_context.cpp and bpftime_shm.hpp (one growing per-thread type and two new map types).

B. Uses (Orin, llama.cpp's largest launch = 4,194,304 threads = 131,072 warps)
| use | memory bound | loss | GPU cost on the Orin [inferred] | CUDA graph launches | concurrent kernels | CPU view through bpf() |
|---|---|---|---|---|---|---|
| counters / histograms, keyed by thread index (other keys: max_entries x value, as today) | H x value x entries. Growth keeps old regions, so at most 2x: 32 MiB at 8 B, at most 64 MiB [arith] | Exact while a slot has one writer at a time, or with atomic adds. A launch that no hook saw first gives counted misses (drops + needed), then the map grows. With plain `*cnt += 1` (threadhist.bpf.c:21-29), overlapping launches lose counts silently. Decode with graphs has 10,905 overlapping kernel pairs (reader 2); whether their index ranges collide is unknown | The only anchor: 0.6 ns per thread execution for a membar.sys plus an uncached read-modify-write (R/llama-census-20261113/README.md:90-93). The extra control-block load adds 0-1x of that, since it at most repeats the read. Probing every kernel: 192.56M x 0.6 ns = 116 ms on 204.6 ms of GPU time = +57%, up to +113% [arith] | Yes, if the geometry passed a hook at capture time; it is then sized before the first replay. Otherwise the first oversized replay counts misses | Slots are shared by index, as today; exact only with atomic adds | Key k = thread x max_entries + entry, value_size bytes, which passes libbpf's size check (third_party/libbpf/src/libbpf.c:10982-10987). A reserved key returns {H, drops, needed}. Torn reads remain possible (reader 1, item 22) |
| per-thread scratch, entry to exit | threads x value per live launch: 32 MiB at 8 B for the largest launch. One stream has at most 2 live launches (graph overlap <= 672 ns, reader 2), so at most 64 MiB of device pool [inferred] | Exact within each launch. Pool or table full: counted (NULL plus a drop counter) | Device memory is cached by the GPU (RD:214-215). Each probe does a table lookup and a slot access, with no membar.sys, plus one atomic per warp per exit. Likely below the anchor: on the 5090 a host-mapped update cost 9.4x (buffer-survey notes:235). Not measured | Yes, sized on the device. Needs %gridid to be unique per replay (D3) | Yes, separate regions | None: the regions are in device memory. Drop counts through the reserved key |
| event / record streams | warps x (8 + K x (8 + value)) + header per launch. K = 4 with 32 B events (gpu_ext onevalue, notes:138): 131,072 x 168 B = 21 MiB for the largest launch. The pool must hold every undrained launch: about 124 MiB per 10 ms in decode, 47 MiB in prefill [arith]. Per-thread rings sized at launch: 4.19M x (24 + 16 x 1,032) = 64.6 GiB, blocked (7.36 GiB total) | Counted when a warp writes more than K, the pool is full or the table is full. Never silent | Warp-leader records: 6.02M per prefill. At one anchor operation each, +1.8%. At a full warp's 19.2 ns each (if the anchor's 0.6 ns is 32 lanes sharing transactions and a leader write shares none), +56% [arith range]. Plus exit accounting. Per-thread records need K >= 32: 161 MiB for the largest launch, more than the 128 MiB default segment | Yes, sized on the device; %gridid (D3) | Yes: separate regions, and the launch header tags each record | The existing dlsym drain (example kernelretsnoop.c:86-99) walks closed regions in pool order. Drops and pool use come from a lookup on the reserved key; lookup returns ENOTSUP today (nv_gpu_ringbuf_map.cpp:110-137). No epoll (syscall_context.cpp:1150-1157) |

C. Implementation size, about 1,350 lines [inferred]
- DT plus a regenerated trampoline_ptx.h (control-block path, %gridid table, claim/spin/release, the two new helper paths, drop counters): about 350.
- Exit-accounting pass, reusing ptxpass_kretprobe's insertion: about 100.
- Agent (FS, nv_attach_impl.cpp, bpf_attach_ctx_cuda.cpp): about 350.
  - H from the four hook paths.
  - New hooks for cuLaunchKernelEx and cudaLaunchCooperativeKernel; neither is in nv_attach_impl.cpp:488-534.
  - Growth and the kept regions in the segment.
  - The device pool at attach.
  - The watcher polling `needed`.
- Runtime (map_handler.cpp, nv_gpu_*_map.cpp, syscall_context.cpp, bpftime_shm.hpp): new map types, CPU views, the reserved key and draining by region: about 400.
- threadhist and kernelretsnoop changes plus tests: about 150.
- Left to the tool author:
  - choose the map type for each use;
  - choose K, value_size and the two pool budgets (BPFTIME_SHM_MEMORY_MB, device pool);
  - use __sync_fetch_and_add where launches can overlap (relaxed, .gpu scope: compiler_utils.cpp:376-388);
  - read {H, drops} before reading the array;
  - group records by launch header.
  No thread count appears anywhere.

D. Unverified assumptions and step-0 checks, the one that could kill the design first
1. Most large probed launches show their geometry to a hook. Risks: CUDA 13.2 handles go unnamed (nv_attach_impl.cpp:1575-1585); static cudart. softmax's cooperative launch (L/softmax.cu:355-365) runs only when a row has more than 8192 columns, on an nsm x 256 grid of 2,048 threads, so it is harmless.
   Check: log every geometry the hooks see in one pp512 + tg128 run, and compare each kernel's maximum with the nsys sqlite rows. If large launches go unseen, section A2 only grows after counted misses.
2. Replay geometry equals the captured geometry after cudaGraphExecUpdate (L/ggml-cuda.cu:3337, 3341). Partial evidence: the ordered (kernel, grid, block) sequence is the same with graphs on and off (reader 2).
   Check: for each graphId, compare the hooked capture geometry with the nsys kernel rows.
3. %gridid is unique per launch, graph replays included, across all modules of one context. All of section A3 rests on this.
   Check: replay a 3-node graph 100 times and launch on two streams; each kernel stores %gridid to registered memory.
4. A plain host store to the control block reaches kernels already in flight and later replays (open per reader 1, A4).
   Check: a kernel spinning on the word, and a replay issued after the store.
5. The agent can allocate in the segment at run time while the server does too. The server constructs maps there (runtime/src/handler/map_handler.cpp:1396, 1430); the agent doing the same is [inferred].
   Check: the agent allocates 64 MiB while the server creates a map.
6. Costs and progress:
   - the extra control-block load on rms_norm, against the 3.3% anchor;
   - device-pool scratch;
   - a warp-leader record plus exit accounting;
   - whether non-winners spinning in exit and __memcapture probes always make progress, given compute preemption (R/feasibility-20261109/gpu-env/attrs.out.txt:14-23).

E. Where this loses to the other approaches
- Memory follows the largest launch, not the GPU: 32 MiB per 8-B key (64 MiB with growth), and 32 MiB of scratch per live launch. One copy per hardware slot needs 8 or 384 copies for counters, and 12,288 x 8 B = 96 KiB of scratch for the resident threads [arith]. RD requirement 1 (storage never bounded by the grid) is not met.
- On the Orin all CPU-visible memory is the segment, prefaulted and registered whole at start (bpftime_shm_internal.cpp:1414-1469). So sizing at launch changes the layout and the CPU view, not the footprint, and the segment size must still be chosen before start. A fixed budget with counted misses and the needed size in the warning (RD tier 1) gives most of section A2 for far less code.
- Streams: volume depends on records per second times the drain interval, not on the grid. Warps that write nothing still pay for a K-record reservation. A shared byte reservation with counted drops (NVBit/CUPTI, reader 3) bounds memory more tightly. Per-thread records are blocked under any sizing.
- Counters still share slots across concurrent launches and pay one more uncached load per call; correctness under overlap depends on the tool using atomics.
- It gives up Neutrino's per-launch separation and exact per-launch copy-out for counters. Per-launch identity for scratch and streams depends on %gridid, which is unverified.
- Every kernel in a module with these maps pays the exit accounting, one atomic per warp per exit, even when no probe fires. Threads spinning on a launch entry are a new forward-progress risk [inferred].
- It relies on launch hooks, which CUDA 13.2 handles partly defeat, and needs new hooks for cuLaunchKernelEx and cooperative launches.
- On a discrete GPU, in-place reads need a pool the CPU can see. Regions in device memory need IPC, or a copy as 1502 does (nv_gpu_per_thread_array_map.cpp:89-121).