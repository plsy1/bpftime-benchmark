# Research directions

Directions chosen for the work after the official-vs-dev GPU comparison (2026-11-10/11). Each entry
says what the problem is, the evidence, where upstream stands, the known designs elsewhere, and the
open questions. An entry is a direction, not a plan: the plan is made when the direction is
started. Newest last.

## 1. GPU maps sized by thread index: per-thread arrays and the GPU ringbuf

Decided 2026-11-11. Real-application evidence and the discussion of 11-14: "Real applications"
at the end.

### The problem

Two bpftime GPU map types keep one copy per GPU thread *index*:

| type | number | memory |
|---|---|---|
| `BPF_MAP_TYPE_PERGPUTD_ARRAY_MAP` / `_HOST_MAP` | 1502 / 1512 | value_size x max_entries x thread_count |
| `BPF_MAP_TYPE_GPU_RINGBUF_MAP` | 1527 | (header + max_entries x (8 + value_size)) x thread_count |

The slot of a thread is its linear global id in the launch (`getGlobalThreadId`,
`attach/nv_attach_impl/trampoline/default_trampoline.cu`). The other GPU maps (1501 hash, 1503/1513
shared array, 1504 array shared with a bpf map) are sized by entries only.

- **thread_count is fixed when the map is created.** The syscall server creates the map when the
  tool loads the program, before any kernel is launched and in another process, so it cannot know
  the grid. The default is 1024 (`bpf_map_attr::gpu_thread_count`, `runtime/include/bpftime_shm.hpp`);
  the only override is the env `BPFTIME_MAP_GPU_THREAD_COUNT` on the syscall server. Official reads it
  for 1502 and 1527, not for 1512 (the map type used on the Orin); dev 3823cac5 makes 1512 read it.
- **Threads past thread_count lose data.** Ringbuf: they return without writing, without a drop
  count or a warning (official and dev). Per-thread array: official returns an address past the map
  (writes into whatever follows in the shared-memory segment) and printfs once per thread; dev
  518f05b5 returns NULL / -E2BIG and warns once.
- **Memory grows with the grid, not with the GPU.** The micro benchmark's ring is 408 B per thread:
  about 400 MB at 1M threads, more than the default segment (official 50 MB, dev 128 MB); ringbuf
  creation then halves thread_count until it fits ("Clamped GPU ringbuf thread count"). The Orin Nano
  holds only 12,288 threads at a time (8 SMs x 1536, measured with `cudaGetDeviceProperties`).
- **Slots belong to a thread index, not to a running thread.** Two kernels running at the same time
  share slots (dev's ringbuf takes a busy word, 518f05b5, for this case).
- **The benchmark is affected.** In `gpu/compare-20261110` no thread count was set, so GPU Ringbuf
  large/xlarge have only the first 1024 threads writing; the xlarge number (+50-55 us) is likely
  mostly about 1M threads taking the `tid >= max_thread_count` return (inferred, not measured). Per-GPU-thread array medium and
  larger cannot be compared at all (official writes past the map).

### Evidence from upstream's own work

- CI needs a different thread count per example ([#490](https://github.com/eunomia-bpf/bpftime/pull/490)):
  threadhist 524288, kernel_trace 4096, kernelretsnoop 10 "to avoid bad_alloc", default 8192.
- gpu_ext revision notes (llama.cpp, RTX 5090,
  [device-array-runtime-notes.md](https://github.com/eunomia-bpf/gpu_ext/blob/main/workloads/llama.cpp/observability_overhead/revision-rq4/device-array-runtime-notes.md)):
  kernelretsnoop's payload is 22 MiB, but the ring allocates 524,288 thread slots, about 892 MiB. A
  single shared GPU array used as an event buffer cost 5.573% paired overhead; a collector still on
  the per-thread ring, 90.812%.

### Where upstream stands (checked 2026-11-11)

- **master:** no change to these paths between 4708f409 and 39cc1fce. The current behaviour came in
  as stopgaps: default 1000 ([#423](https://github.com/eunomia-bpf/bpftime/pull/423), whose
  description already says the server cannot know the thread count), 1024x1024 plus the env and the
  per-thread warning ([#501](https://github.com/eunomia-bpf/bpftime/pull/501)), back to 1024
  ([#515](https://github.com/eunomia-bpf/bpftime/pull/515)), the ringbuf silent return and halving
  ([#490](https://github.com/eunomia-bpf/bpftime/pull/490)), 1512 without the env
  ([#522](https://github.com/eunomia-bpf/bpftime/pull/522)).
- **issues:** [#530](https://github.com/eunomia-bpf/bpftime/issues/530) (threadhist counts only 1024
  threads) was answered with the env and "we will improve it", then closed;
  [#552](https://github.com/eunomia-bpf/bpftime/issues/552) (2026-03) lists the
  `exceeds max_thread_count` warning as pre-existing.
- **unmerged branches** `official/revision/*` (2026-09, no PR): thread-count validation and
  write-back to attr (201bb2dc, af759303), a ringbuf that counts every drop by reason (052513a5),
  opt-in warp-leader-only output (89a12447). The per-thread-index design is unchanged.
- **direction in the authors' papers:** gpu_ext ([arXiv 2512.12615](https://arxiv.org/abs/2512.12615))
  and LPC 2025: run eBPF once per warp, and hierarchical maps (GPU-local shards merged periodically).
  Not in bpftime master.
- Nothing upstream sizes these maps by the launch grid or by resident threads.

### Designs elsewhere

| system | buffer | when full |
|---|---|---|
| NVBit | one channel per context (mem_trace 1 MB), atomic reservation, one record per warp | GPU waits for the host |
| Neutrino (OSDI '25) | allocated per launch from the grid (per thread or per warp), passed as a kernel parameter | no bounds check |
| Xtrace (arXiv 2609.28769) | per launch, one slot per thread; per warp when registers are short | — |
| compute-sanitizer MemoryTracker sample | one global array, atomic index | drop, countable |
| CUPTI activity | per-context device buffers | drop, counted |
| GT-Pin | buckets <= hardware threads, hashed by hardware thread id | — |
| SASSI | no per-thread state; aggregated on the GPU | — |
| bpf ringbuf | one ring for all CPUs, reservation under a lock | reservation fails, drop |

PTX documents `%warpid` and `%smid` as able to change after preemption; Neutrino computes a virtual
warp index from `%ctaid`/`%tid` instead.

### What other systems measured (survey 2026-11-11)

A read-only survey of published numbers on these designs, each number checked against its source
by a second reader (raw notes: results branch `gpu/buffer-survey-20261111/`). Event buffers (the
ringbuf side) have some measurements; per-thread state maps (the 1502/1512 side) almost none.

**Event buffers**

- **The only head-to-head with the instrumentation held fixed:** hip-analyzer (Darche,
  Dagenais; Darche's 2025 PhD thesis ch. 5, submitted to ACM TOPC 2024, unpublished), AMD MI210,
  313 HeCBench kernels, every basic block traced. Kernel slowdown mean / median:
  | design | slowdown |
  |---|---|
  | one global buffer, an atomic per event | 3.73x / 1.96x |
  | one buffer per CU, an atomic per event | 2.47x / 1.60x |
  | global chunk allocator (a wavefront takes 4 KiB chunks) | 1.79x / 1.33x |
  | chunk allocator per CU | 1.77x / 1.32x |
  | exact per-wavefront preallocation from a counting pass (like sizing per launch) | 2.07x / 1.50x (4x padding: 2.18x / 1.58x) |

  Per event (microbenchmark, clock ticks): up to 256 concurrent waves chunk 90-115, exact
  preallocation 80, atomics 290; at high concurrency the global atomic stays at 495-750, the
  others settle at 310-385. Chunks stop helping beyond 4 KiB (32 B chunks: up to 5000 ticks under
  contention). Cost: the chunk schemes reserved 4 GiB of device memory per stream; some exact
  preallocation runs could not complete because of trace size.
- **Closest to bpftime:** gpu_ext revision notes (authors' repo `workloads/llama.cpp/
  observability_overhead/revision-rq4`, unpublished; RTX 5090, llama.cpp prefill,
  kernelretsnoop; campaigns not interleaved with each other):
  - bpftime's per-thread ring in host-mapped memory (524,288 thread slots): 892 MiB for 22 MiB
    of events (40x), 90.7% throughput loss. One GPU-memory array with a slot per warp: 22.1 MiB,
    5.4-5.6%. NVBit with the same per-warp device array: 9.6% (gpu_ext 5.3%).
  - Draining the ring continuously or only at the end: 90.7% vs 90.8%. The cost is the producer
    side, not the drain.
  - Placement alone (operation-matched microbenchmark): an update to a device-memory array
    (1503) 3.85 us per launch, to a host-mapped one (1513) 36.2 us (9.4x). On the Orin every map
    is host-mapped and uncached by the GPU.
  - Full per-thread records (23M x 80 B): ring 99.1% loss, GPU-memory arena 39.1%; layout alone
    takes the arena to 27.6% (SoA) and 12.9% (warp-contiguous SoA).
- **Neutrino** (OSDI '25, per-launch sizing): for per-thread maps on large grids the copy-back and
  write after each launch dominates (Conv2D: 5.3-5.45x of the kernel in all, about 4x of it
  collection; per-warp 1.36x, about 0.3x). Its memory figures are computed from the formula
  (`--memusage`), not measured; a memory trace needs up to 7 GB (12.6x the model's own memory on
  ResNet). The two-pass counting cost is not measured, works only for kernels that do not modify
  their inputs, and writes are not bounds-checked.
- **Xtrace** (arXiv, per-launch per-thread slots): 0.9-2.8% overhead vs Neutrino 11.8-75.6%,
  flat over a 64x larger launch; attributed to less compiler interference, not to the buffer; no
  memory numbers.
- **Blocking when full is expensive:** GVProf (SC '20, 72 MB queue per stream, kernel paused when
  full) median 31x / mean 236x without sampling; BARRACUDA 58.9x vs iGUARD's in-GPU 3.9x (SOSP
  '21), BARRACUDA reserving half the GPU memory and failing beyond 8 GB of application data.
- **NVBit** (1 MiB shared channel, blocks when full): the channel itself was never measured.
- **CPU analogue:** the bpf ringbuf (one ring for all CPUs) drops from 10.9 M records/s with one
  producer to about 2 M/s with 20 or more (kernel commit c97099b0).

**Per-thread state maps**

- gpu_ext revision: threadhist's per-thread array has 1,048,576 slots (8 MiB) and 22,528 of them
  used (2.1%; correctness run, not timed). Whole-tool overhead 3% vs NVBit 87% (P40, paper Table
  2), 2.97% vs 10.35% (5090 re-run): end to end, the map not isolated.
- Upstream bpftime micro: a per-thread array update adds 3.3-3.7 us per launch over the entry
  probe on the 5090 (32 to 10,240 threads), 13.8-15.1 us on the P40; larger grids skip the write
  past 1024 threads, so their rows are not usable. Host-side access costs the same as a plain GPU
  array (0.99-1.01x).
- Neutrino's thread-level counter maps: 16x the memory of its warp-level maps by construction;
  but they are allocated and copied back per launch, not kept across launches like bpftime's.
- GT-Pin's buckets per hardware thread are documented as reducing contention; no measurement of
  the bucket count was published. SASSI aggregates on the device: the handler body (atomics
  included) is under 20% of its overhead, an upper bound.

**Not measured by anyone:** buffer designs on an integrated GPU (WarpGuard ran NVBit on an Orin
Nano but gave no buffer numbers); slots by hardware position; per-launch sizing vs chunk
allocation vs per-warp aggregation under the same instrumentation on NVIDIA; per-thread state
kept across launches at large grids; real memory footprints (mostly computed). A search aimed at
per-thread state (privatization plus reduction, per-thread counters on GPUs) has not been done.

### Approaches to evaluate

1. **Size at launch.** The agent already hooks every launch and sees grid x block; allocate (or
   grow) the per-thread storage for that launch, as Neutrino and Xtrace do.
2. **Shared buffer with reservation, aggregated per warp.** One ring (or one per SM) with an atomic
   reservation by a warp leader; memory fixed, independent of the grid.
3. **Slots by hardware position** (SM x warp slot x lane, or buckets as in GT-Pin): 12,288 slots on
   the Orin. Fits accumulation and entry-to-exit scratch, not per-logical-thread results; needs
   `%warpid`/`%smid` to stay put between entry and exit.
4. **Cheaper fixes on the current design:** warn on the host at launch when grid x block exceeds a
   map's thread count; count every drop; make 1512 read the env and readback use the effective
   count (dev already has the last two through 3823cac5).

### Open questions

- Which real workloads need per-logical-thread results, and which only need accumulation or event
  streaming? That decides between approaches 1 and 2/3.
- Cost on the Orin of an atomic reservation in host-registered memory (uncached for the GPU) against
  per-thread rings.
- How concurrent kernels and CUDA graphs should share or separate these buffers.
- What the host-side `bpf()` view of a per-thread map should be when its size changes per launch.

### Noticed while reading, not verified

- `BPF_MAP_TYPE_GPU_HASH_MAP` (1501) creates `next_prime(max_entries)` buckets but a value array of
  `max_entries` entries (`nv_gpu_shared_hash_map.cpp:17-22`), so keys hashing to the last buckets
  may fail to store or read (official and dev alike).

## 2. GPU-to-CPU host calls

Decided 2026-11-11. Larger than direction 1, so it is split into three layers: layer 1 is
engineering and comes first, layer 2 is the research core, layer 3 waits for direction 1.
Measured on a real application (11-13): "Real applications" at the end.

### What goes through the host

A GPU probe that calls one of these helpers asks the CPU to run it and waits for the answer:

| helper | why the host |
|---|---|
| lookup / update / delete on an ordinary (CPU) map | the map is CPU code |
| every operation on `BPF_MAP_TYPE_GPU_HASH_MAP` (1501) | the trampoline has no device path for it (official neither) |
| `bpf_trace_printk` (6), `puts` (501) | printed by the host |
| `bpf_get_current_pid_tgid` (14) | answered by the host |

GPU maps (1502/1512, 1503/1513, 1504) and the GPU ringbuf (1527) are accessed directly.

### How it works (dev c115b33d)

- **One request slot per process:** `CommSharedMem` (`trampoline/gpu_helper_protocol.h:75-84`) in
  the shared segment, which is registered with CUDA (host memory, not cached by the GPU): flags,
  request id, map id, a request union (key and value buffers of 1 MiB each) and a response union.
- **GPU side** (`host_call`, `trampoline/default_trampoline.cu:124-160`): the 32 lanes of a warp
  take turns (no intra-warp spinning before sm_70); a lane queues on a per-module lock in GPU
  memory, takes `occupy_flag` in the slot, writes the request byte by byte, `membar.sys`, sets
  `flag1`, spins on `flag2`, reads the response; four `membar.sys` per call.
- **Host side** (`runtime/src/attach/bpf_attach_ctx_cuda.cpp:184-257`): one watcher thread takes a
  request by CAS on `flag1`, serves it (`serve_cuda_request`, `:116-178`), stores `flag2`
  (release), keeps polling for 200 us after a request, then sleeps 50 us doubling to 1 ms. A hit
  lookup also translates the value's address for the GPU with two driver calls
  (`gpu_address_of_map_value`).
- **Official** sleeps 1 ms after every request: 1.05 ms per call on the Orin, the 5090 and the P40
  alike. Dev's watcher (518f05b5) brought it to about 10 us.

### Numbers

| what | value | source |
|---|---|---|
| dev, per call, CPU map cases (32 calls per launch, one warp) | 8.7-10.2 us (cases 319-367 us vs 39.7 us baseline) | `results/gpu/compare-20261110/summary.md` |
| dev, CUDA Counter (10 calls per thread, 320 per launch) | 3,486 us, 87x the baseline | same |
| official, per call | about 1,052 us | same, `attribution.md` |
| GPU-CPU round-trip floor on the Orin | 4.78-4.90 us mean (max 23.1) | `results/gpu/feasibility-20261109/gpu-env/hostreg.hsloop-atomdev.txt` |
| dev, 1 thread / 1 / 8 / 32 warps calling | 15.6 / 10.4 / 9.9 / 9.8 us per call | `results/gpu/fixes-20261109/protocol/README.md` |
| host-memory lock only, 8 / 32 warps | 74 / 300 us per call | same |

Calls are fully serialized: time grows with threads x calls, without bound (1M threads with one
call each would be about 10 s per launch). The micro cases are artificial: one warp, one key, an
empty hash map (lookups miss), a watcher that never goes idle. Array lookup costs more than update
because that probe also increments through the returned pointer and a hit pays the two driver
calls.

### Layer 1: remove calls that need no host (first, engineering)

- **Ordinary ARRAY maps accessed by the GPU directly.** Their values (`array_map_impl::data`) are
  in the segment, and the whole segment is registered with CUDA (`bpftime_shm_internal.cpp:1463`),
  so lookup/update can take the 1513 path. Estimate: CPU array cases about 42-48 us per launch
  (7-8x). Semantics: no host lock any more, so values over 8 bytes can tear for concurrent CPU
  readers (as bpf's ARRAY).
- **`bpf_get_current_pid_tgid` as a per-module constant.** Today the low half is the watcher
  thread's tid, meaningless for the caller; what a GPU thread should get there must be decided.
  Estimate: about -20% on CUDA Counter.
- **Bounded watcher wake-up.** After idle a call may wait up to 1 ms of backoff; unmeasured. First
  add a sparse-caller case, then e.g. keep the watcher hot while a probed launch with host-call
  helpers runs.
- **Several CUDA processes on one segment.** Each agent zeroes `CommSharedMem` at start and any
  process's watcher may serve any request (known, not fixed).
- **Lookup address translation** by offset from the segment's device base instead of two driver
  calls per hit (estimate about 1.3 us per lookup; time the calls first).

Layer 1 also gives layer 2 a clean baseline: without it, protocol measurements include calls that
should not exist.

**Done (2026-11-11):**

- Ordinary ARRAY maps on the GPU (dev 1b3d9c99) and `bpf_get_current_pid_tgid` as a per-module
  constant, tgid in both halves (aa6eb973): CPU Array map update 320 -> 51 us, lookup 371 -> 49 us,
  CUDA Counter 3,487 -> 2,929 us; the other 20 cases of the 24-case subset unchanged (results
  `gpu/hostcall-l1-20261111`, `gpu/baseline-sub-20261111`).
- Watcher wake-up (dev 46f5a7cb): measured first (`gpu/hostwake-20261111`: 60-950 us per call
  once calls are sparser than the 200 us polling window), then the watcher waits with aarch64 WFE
  on the request flag while a probed kernel may run (`gpu/wfe-20261111`: a GPU write wakes the
  core in about 2 us, at the power of a sleeping core): 13.5-16.6 us at every gap
  (`gpu/hostwake-fix-20261111`). That held the watcher's core for as long as any probed kernel
  ran; under full CPU load the load lost 13% of its throughput. Replaced 2026-11-12 (dev
  52126f33) by measured rules (`gpu/watcher-policy-20261112`, set AC3): a poll window doubling
  from 10 to 50 us after a request, closed only by two long gaps in a row, then short deferral
  sleeps, 100 us sleeps while a noted launch may run, a park on the launch note otherwise, and a
  latch for launches no hook notes (graphs). A sparse call takes about 70 us, CPU 0.02-0.13 of a
  core instead of 1.0, under full load the load keeps 0.988 of its speed (0.863), graph launches
  70 us (534). WFE stays opt-in (`BPFTIME_CUDA_WATCHER_WFE=1`). Weak case left: a duty-cycled
  load (30% of calls over 1 ms).

Still open in layer 1: several CUDA processes on one segment, lookup address translation.

**The waiting rules beyond this board (checked 2026-11-12, discussed 2026-11-13).** Without
`jetson_clocks` (frequency scaling, idle states; `gpu/watcher-policy-20261112/dvfs`) the
comparisons that chose AC3 held. Its behaviour depends on which step the gaps between requests
fall into and on how busy the CPU is. The gaps are set by the GPU program (where probes sit, how
often kernels launch, how many lanes call), by the round trip (within a burst about 10 us per
call on the Orin, so the 50 us window cap leaves about 5x headroom) and by the watcher itself (a
gap is measured from the end of a service to the detection of the next request, so a sleeping
watcher measures longer gaps). On a discrete GPU the round trip over PCIe may push gaps within a
burst past 50 us; calls would then fall to the sleeping steps (untested, no such machine).
Possible later: measure true gaps from a GPU timestamp (`%globaltimer`) written into each request,
after calibrating GPU against CPU time once, and derive the fixed values (window cap, 20 us x 8,
100 us) from the measured distribution. On the Orin that can at best match the hand-tuned values;
it is worth doing only with a second platform to show it.

### Layer 2: the synchronous protocol on an integrated GPU (research core)

Question: on an IO-coherent integrated GPU with a ~4.9 us round trip, what request protocol gets a
host call close to the floor and stops the cost growing with every thread, while keeping per-lane
arguments, ordering and BPF update flags correct?

- **Warp batching:** one request per warp for lanes with the same (operation, map); uniform cases
  need only ballot/shuffle (works on sm_61), different keys need `match.any.sync` (sm_70+).
  Estimate: CPU map cases 324 -> 50-60 us, CUDA Counter 3,486 -> 150-250 us.
- **Several slots** (e.g. one per SM) with the arbiter in GPU memory (a lock in host memory
  collapses under contention: 300 us at 32 warps); throughput then bound by how fast the host
  serves.
- **Per-call cost:** fewer `membar.sys`, 8-byte copies; at most 10 -> 6-7 us (the floor).
- **Toolchain:** the trampoline PTX is generated for sm_61 (`trampoline_ptx.h:7`; probe PTX is
  retargeted to the device). sm_70+ instructions need a second variant, keeping sm_61 for the P40.

Limit: no synchronous design removes the growth at large grids. Perfect warp batching still needs
about 31k round trips for 1M threads, about 160 ms per launch. At that scale operations have to
move to the GPU (layer 3).

Evaluation needs cases the benchmark lacks: sparse callers, different keys per lane, populated hash
maps, many warps and SMs, several processes.

### Layer 3: synchronous exact vs asynchronous approximate (after direction 1)

gpu_ext ([arXiv 2512.12615](https://arxiv.org/abs/2512.12615)) answers host-call cost with GPU-local
map shards flushed asynchronously (approximate, eventually consistent) and warp-leader execution.
Its Fig. 12(b) "CPU map access via PCIe is 6000x slower" matches upstream's 5090 table (33,450 us
vs 5.2 us baseline), which is the 1 ms watcher sleep, not PCIe. On an integrated GPU the round trip
is ~5 us, and some operations need exact, immediately visible results (`BPF_NOEXIST`/`BPF_EXIST`
updates, deletes, hash inserts, values the CPU reads at once). Which operations belong on which
side has not been measured anywhere. This layer meets direction 1: moving operations to the GPU
means GPU maps, whose sizing is direction 1.

### Where upstream stands (checked 2026-11-11)

- master: nothing on the watcher, `CommSharedMem` or batching after 4708f409.
- [#426](https://github.com/eunomia-bpf/bpftime/issues/426) (open, GPU helper performance) was
  answered with "use the GPU maps"; [#503](https://github.com/eunomia-bpf/bpftime/pull/503) (open)
  fixes `CommSharedMem` visibility on Orin/Thor (`cudaHostAlloc`, volatile loads). No issue or PR on batching or several slots.

### Prior work to check before claiming novelty

- GENESYS (ISCA '18): GPU-issued system calls coalesced on an AMD integrated GPU.
- LLVM libc GPU RPC: warp-wide ports and several slots.
- NVBit's device-to-host channel (one buffer, a host receiver thread).

### Framing for a paper (discussion 2026-11-13)

Layer 2 as planned above (warp batching, several slots, an arbiter in GPU memory) is close to
GENESYS and LLVM libc's GPU RPC; done well, it still reads as known ideas applied to eBPF.

What could be new: eBPF programs can be analyzed when they are loaded, so the runtime knows, for
each call site, which helper it calls, whether the result is used, whether the operation commutes,
and whether the lanes of a warp share the key. It can then pick, per call site, the cheapest
execution that keeps BPF semantics:

| call site | execution | example |
|---|---|---|
| the data can live on the GPU | no host call (eliminate) | ordinary ARRAY maps, pid (layer 1, done) |
| result unused, operation commutes | buffer on the GPU, apply on the host in batches, final state exact (defer) | counters and histograms, `map[key] += 1` |
| lanes of a warp on the same key | one request for the warp (aggregate) | many threads updating one counter |
| the result is needed at once | synchronous round trip, with the host-side waiting rules (layer 1) | a lookup whose value is used; `BPF_NOEXIST`/`BPF_EXIST`, deletes |

Thesis: most eBPF helper calls on a GPU do not need a synchronous per-thread round trip, and
because eBPF programs are analyzable the runtime can choose, per call site, to eliminate,
aggregate, defer or synchronize without changing what the program observes. This spans layers 2
and 3 and differs from gpu_ext, which applies warp-leader execution and asynchronous, approximate
map shards to whole maps rather than choosing per call site while keeping results exact.

- First check, done 2026-11-13 (`results/gpu/callsite-survey-20261113`): every helper call site of
  bpftime's 22 GPU example and benchmark programs and of gpu_ext's 15 device-side tools, classified
  and then checked against the sources by a second agent. bpftime: 168 call sites run on the GPU,
  109 already without a host call, 59 host calls today: 20 eliminate (maps only GPU probes touch,
  such as entry/exit timestamps), 39 defer (21 printk; counters, histograms and per-thread records
  the CPU reads but the probe does not need back), **0 sync**; 51 of the 59 share their target
  across a warp. gpu_ext: 80 call sites, **no host call at all**: its authors rewrote the tools by
  hand with GPU map types, warp-leader writes and banked buffers. Also found (from the code): the
  counter idiom (lookup, atomic add, else `BPF_NOEXIST` insert) loses increments in its synchronous
  form when the lanes of the first warps all miss; a deferred exact add would not. So the framing
  holds on these programs, and the contribution becomes doing automatically, with exact results,
  what gpu_ext did by hand. Limits: examples and research tools (authors avoid synchronous
  patterns), static counts; GPU ports of CPU tools would be the next test.
- Evaluation needs a discrete GPU (over PCIe each avoided round trip is worth more), real
  workloads (LLM inference and the like), and comparisons with official bpftime, gpu_ext, NVBit
  and Neutrino; check what upstream (gpu_ext) is doing first to avoid overlap.
- Judgement at the time: layer 1 alone is workshop material; a top venue only if the per-call-site
  choice works and the survey shows most call sites can skip the round trip.

## Real applications: what the two directions are worth (2026-11-13/14)

The advisor's question (11-13): the GPU numbers so far came from microbenchmarks; find mainstream
scenarios and measure what share the operations of the two directions take in real applications,
then decide how far to take each direction.

Terms:
- **Census:** every kernel launch of an application counted with Nsight Systems, without bpftime.
- **Per-firing operation table:** for each tool, the map and helper operations one probe firing
  executes, read from the sources twice independently.
- **Projected:** census times table, not measured.

### Mainstream scenarios (survey, `results/gpu/real-scenarios-20261113`)

- **No measured shares.** None of the published work read for the survey measures these shares.
- **Direction-1 operations come from what a scenario needs:** per-thread or per-warp state and
  records. Examples are kernel and warp timing, per-thread histograms, address traces and
  intra-kernel region timing.
- **Direction-2 operations come from how a tool is written.**
  - Among the tools whose data path was read, only tools written the CPU way (HASH maps, printk per
    thread) make a host call per event. The one exception is hang debugging (CUTracer, SM90+).
  - The tools already run on real kernels make none in their current sources: bpftime's four
    examples on llama.cpp, PyTorch, FAISS and CUTLASS, and gpu_ext's 15 device-side tools.
- **The top-ranked scenarios first need attach points bpftime lacks:** any instruction, and the
  effective address of an access. They are ranked by the number of tools implementing them, not by
  usage, which nobody reports. They are correctness checking (Compute Sanitizer), instruction mix,
  address traces and intra-kernel region timing; address traces and region timing then also need
  direction 1.
- **On the Orin, only the application's own kernels built with PTX can be probed in practice.**
  - cuBLAS, cuDNN and cuRAND carry PTX only for sm_120/121.
  - A few entries in cuBLASLt, cuSPARSE, cuSOLVER and TensorRT would pass the PTX selection; they
    are unchecked.
  - dev does not hook Triton's `cuModuleLoadData`.

### llama.cpp on the Orin (`results/gpu/llama-census-20261113`)

Setup: llama.cpp 7874988b built with sm_87 PTX, TinyLlama-1.1B Q4_K_M, batch 1, dev 52126f33. CUDA
graphs were off in every bpftime run.

- **Every kernel is llama.cpp's own and carries sm_87 PTX.**
  - One 512-token prefill: 444 launches, 192.6M thread executions, 204.6 ms of GPU time.
  - Decode: 424 launches and 39.4M thread executions per token.
  - Threads per launch range from 512 to 4,194,304 within the 512-token prefill, and from 256 to
    4,096,000 within decode.
  - One kernel at different call sites spans 125 times: mul_mat_vec_q runs at 32,768 to 4,096,000
    threads.
- **Probes counted every thread exactly on four kernels:**
  - rms_norm;
  - the largest mul_mat_q (40.5% of prefill GPU time);
  - the largest mul_mat_vec_q (62.7% of decode);
  - the RoPE kernel applied to the keys (gpu_ext's target).
- **Steady-state overhead is small** (warm runs, one kernel):
  - Once patched, rms_norm with an empty probe runs at the no-agent rate.
  - threadhist's probe body on rms_norm costs 3.3% of a 512-token prefill.
- **Start-up is a separate cost, outside both directions.** Agent start-up added 14.4-14.8 s.
  - dev extracted and patched the 10 modules llama.cpp loads lazily, and every request missed the
    patch cache.
  - Most of the time is likely this; it was not timed per step.
- **Direction 1, measured.** With the default 1,024 slots, 99.6% of rms_norm's thread executions at
  512 tokens were not counted. There was one GPU warning and no count of the loss.
- **Direction 2, measured.** A HASH lookup per thread on rms_norm (1.5% of that prefill's GPU time)
  made a 32-token prefill take 7.63 s instead of 24.9 ms, 307 times slower.
  - Each call took 10.8 µs, and the calls were served one after another, about 93,000 a second.
  - Counts were exact, because the loader inserted the key before attaching, so every lookup hit.
    The usual insert-on-miss counter loses counts through synchronous calls.
  - The projection had said 6.9-11.0 s.
- **Projected:**
  - CPU-style tools on every kernel would take 31 min to 6.7 h per 512-token prefill.
  - One host call per launch would cost 26-42% of decode GPU time.
  - Per-thread rings in kernelretsnoop's layout would need about 63-65 GiB at 4.1-4.2M threads.

### Direction 2: what the measurement says

- **Where the 10.8 µs per call goes.** About 4.9 µs is the bare GPU-CPU round trip on this board:
  the ping-pong in `results/gpu/feasibility-20261109`, with a busy-polling host.
  - About 6 µs is the protocol on top: the per-module lock and `occupy_flag`, the request written
    into uncached host memory, four `membar.sys` per call (the ping-pong had fences too), and the
    host's lookup plus `cuPointerGetAttribute` (0.21 µs).
  - This split is inferred from separate measurements, not timed per step. `globaltimer` stamps
    per step would split it.
- **The watcher's waiting rules were probably not the cost.** The 10.8 µs matches the dense-burst
  microbenchmark (9.8-10.4 µs), against about 69 µs for a sparse call. This is inferred: the run
  kept no watcher statistics.
- **Serialization dominates.** There is one request slot per process, so the total is calls times
  the cost of one call.
  - Even at the bare round trip, the 705,536 calls would take about 3.5 s, 140 times the prefill.
  - So a cheaper synchronous call is worth at most about 2 times for per-thread calls.
  - Per-thread calls have to be eliminated or deferred, as the direction-2 design does
    (`results/gpu/dir2-design-20261113`).
  - The protocol's cost still matters for calls made once per launch.
- **Relation to gpu_ext.** The aim is the same as gpu_ext's: no host call per thread. gpu_ext reaches
  it by rewriting tools by hand; here the runtime would do it for unmodified tools.

### Direction 1: discussion (2026-11-14)

- **Counters can be sized large, but the bound is unknown when the map is created.** A per-thread
  counter is 8 B per thread: 32 MiB covers every kernel at 512 tokens.
  - The application's configuration sets the bound: llama.cpp's ubatch (512 tokens by default;
    prompts are cut into ubatches), model sizes, and the batch.
  - The vocabulary is one such size: 32,000 rows x 128 threads here. A 128,256-word vocabulary would
    give about 16.4M threads if the kernel keeps its shape [inferred].
  - The only moment the bound is known is the launch (grid x block).
  - A loss is nearly silent.
- **Per-thread records cannot be sized large.** 16 records of 1,032 B plus a 24-B header per thread
  is 64.6 GiB at 4.19M threads.
  - When a 1527 map does not fit, the server halves its thread count until it fits, with a warning.
  - Every poll scans every thread's header.
- **What grows with what.** Thread ids restart at 0 at every launch.
  - Storage keyed by thread id grows with the size of one launch, not with run time.
  - What grows with run time is the number of records. Decode at about 57 tokens a second is about
    2.25 billion thread executions a second: about 70M warps and 24,000 launches.
- **LLM inference.** The largest launch is fixed by the model and the configuration.
  - But prefill and decode differ by up to 512 times for one kernel: rms_norm runs 262,144 threads
    in a 512-token prefill and 512 in decode.
  - Our judgement, not a survey result: most questions asked of inference need per-launch,
    per-block, per-warp or per-SM data rather than per-thread data. Examples are time per kernel or
    per layer, launch latency, SM balance and tail effects. Per-thread uses exist: per-thread
    histograms, and Neutrino's per-thread byte and tensor-op counts.
- **How other tools handle it** (survey; KPerfIR, Proton, IKET and Triton-distributed were read only
  through documentation summaries; re-check before citing):
  - **Neutrino** sizes its maps at every launch from grid x block. Records per thread are capped by
    hand or by a counting launch, and every probed launch becomes synchronous.
  - **NVBit** (nvbit-tutorial) streams records through a fixed channel. When it is full, the writing
    warps stall until the host drains it. Counters are aggregated with atomics.
  - **KPerfIR** keeps a fixed circular buffer per warp group and overwrites the oldest records.
    **Proton** drops older counters when a segment is full.
  - **IKET** runs a sizing pass first; **Triton-distributed** leaves the size to the user.
  - **gpu_ext** both reuses bpftime's per-thread maps with hand-set capacities (threadhist with
    1,048,576 slots, of which 2.1% were used; an 892 MiB per-thread ring) and rewrites tools by hand
    to avoid them.

  Apart from gpu_ext, none of these keys storage by global thread id with a capacity fixed in
  advance. bpftime inherited eBPF's fixed maps created at load. On the CPU, per-CPU maps are sized by
  the hardware; on the GPU, per-thread maps are sized by the data.
- **A new map model (an idea, not designed).** Key storage by hardware position, as per-CPU maps do.
  - Positions on the Orin: per SM (8), per resident warp slot (384), per resident thread slot
    (12,288). User space sums the slots, as with per-CPU maps.
  - Records go to a ring per SM, with drops counted, as with per-CPU perf buffers.
  - Allocation per launch only where a value per logical thread is really needed.
  - CPU-written tools that use `PERCPU_ARRAY`, `PERCPU_HASH`, `RINGBUF` or `PERF_EVENT_ARRAY` might
    need fewer changes if bpftime mapped those types onto these on the GPU. Caveats:
    - user space would have to combine the GPU slots into libbpf's one-value-per-CPU layout;
    - none of bpftime's GPU examples uses these types today;
    - probes still need their kernel names set.
  - This complements direction 2. Direction 1 decides where the data lives and how it leaves the
    GPU; direction 2 decides how read-modify-write idioms avoid the host.
- **Why we judge direction 1 harder in kind.**
  - Direction 2 has a fixed target, the helpers' own semantics; its difficulty is getting a
    concurrent protocol right.
  - Direction 1 has to choose what to give up: per-thread semantics, simplicity (per-launch
    lifecycles, graph launches) or completeness (overwrite or drop).
  - It can be done in tiers:
    1. count and report every loss, put the needed size into the warning, no silent shrinking;
    2. hardware-position arrays and per-SM rings;
    3. per-launch per-thread storage, only if a scenario needs it.

### Conclusions and open decisions

- **What breaks on llama.cpp on the Orin** (one application, one model, CUDA graphs off):
  - per-thread host calls (measured: 307 times slower on one kernel with 1.5% of the GPU time);
  - per-thread rings (projected: tens of GiB);
  - per-thread counters with the default capacity (measured: 99.6% lost).
- **What works:** counters with the capacity set by hand, and hand-rewritten tools. So existing
  eBPF tools written the CPU way are not usable on such applications without directions 1 and 2.
- **Directions 1 and 2 are not sufficient.** Start-up time, missing attach points, and the coverage of
  vendor libraries and Triton remain.
- **Open, with the advisor:**
  - Which comes first: design direction 1, implement the direction-2 design's first step (slice 1:
    20 of the 59 host-call sites), or both at once.
  - How far direction 1 goes (the tiers above).
