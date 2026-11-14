# GPU map model: the design space (2026-11-15)

Code: dev 52126f33 at `/home/jetson/src/bpftime-daily-state/worktrees/4708f4092d1e/candidate`; code paths are relative to it.
R = `/home/jetson/src/results/gpu`; RD = `origin/main:RESEARCH_DIRECTIONS.md`; L = `/home/jetson/src/workloads/llama.cpp/ggml/src/ggml-cuda`.
The four approaches in section 3 are paper designs. Nothing was built or run, and their line counts and GPU costs are
[inferred]. Figures marked (sqlite) come from uncommitted nsys exports in `/home/jetson/src/bpftime-gpu-work/llama/census/*.sqlite`.
PTX ISA quotes: docs.nvidia.com, read 2026-11-15. Record volumes are at uninstrumented speed over GPU kernel time.
pp512 = a 512-token prefill, tg128 = 128 decode tokens (llama-bench runs of TinyLlama Q4_K_M).

## 1. The question

GPU probes keep per-thread data in maps. Each map is sized by a thread count fixed when the server creates it (default 1,024)
and indexed by the thread's linear global id (RD:28-43). llama.cpp on the Orin launches 256 to 4,194,304 threads per
kernel (R/llama-census-20261113/README.md:41-42). With the default, 99.60% of rms_norm's thread executions in pp512 went
uncounted, with no loss count (:76). The question: which storage model bounds memory without knowing the grid, accounts for every loss, works under
CUDA graphs and concurrent kernels, gives the CPU tool a usable `bpf()` view, and costs close to a hand-written buffer?

The survey covers 79 (program, map) uses in 37 GPU programs, each classified by how it uses its map. The programs are
13 on real application kernels, 10 CPU-style, 7 on test kernels and 7 microbenchmarks
(R/callsite-survey-20261113/records.json, `cpu_side` excluded):

| use | uses (on real kernels) | what it needs | size on llama.cpp |
|---|---|---|---|
| counters and histograms read as totals | 27 (6) | exact concurrent adds; 2 uses need the value a fetch-add returns | every real-kernel use fires once per launch, in thread (0,0,0); indexed by thread instead: 4.19M slots, 32 MiB (RD:53-54) |
| entry-to-exit scratch | 4 (0): 1 per thread, 3 per block or launch | a slot that lives from entry to exit | per thread, 8 B: 32 MiB for the largest launch, 96 KiB for the 12,288 resident threads [arith] |
| record streams | 9 (5) | bounded memory, counted drops, timestamps | one record per thread at exit on every kernel: 192.56M per pp512 (README:41), 941M/s, 30-198 GB/s at 32-80 B [arith]; one per warp: 6.02M per pp512, 29.4M/s in prefill and 77.3M/s in decode [arith, sqlite]; per-thread rings: 63-65 GiB (RD:55-56) |
| per-thread results read per thread | 15 (4, the threadhist ports) | the logical thread id as index | as counters indexed by thread; the llama.cpp port only sums its slots (census README:63) |
| CPU-to-GPU data (config, a launch queue) | 16 (6) | already served by 1513 and ARRAY | - |

The other 8 uses are microbenchmark sinks.

Launch shape (sqlite):
- Median threads per launch: 262,144 in pp512, 2,048 in decode.
- Launches above the 12,288 resident threads: 88.3% of pp512 launches (99.94% of thread executions) and 31.4% of
  decode launches (98.33%).

## 2. Hard constraints

**map_info is fixed per module at load.**
- `MapBasicInfo` is 40 B with no spare field and is defined twice (attach/nv_attach_impl/trampoline/default_trampoline.cu:84-92; attach/nv_attach_impl/nv_attach_impl.hpp:48-56).
- `__constant__ map_info[1024]` already takes 40 KiB of constant memory (default_trampoline.cu:101; trampoline_ptx.h:18).
- A map fd >= 1024 gets no map_info (runtime/src/attach/bpf_attach_ctx_cuda.cpp:535-540), which leaves the module unpatched [inferred].
- map_info is written once per module in the cuLibraryLoadData hook (attach/nv_attach_impl/nv_attach_impl.cpp:2366-2385, called at :2457) and never rewritten. The only later write is the probe switch (:2391-2409).
- Every launch, concurrent kernel and graph replay of a module therefore reads the same pointers and sizes. Per-launch or resizable storage needs an indirection the device reads at run time.
- Whether a host store after load reaches running kernels and later replays is untested.

**No host step before each launch.**
- Hooked: cudaLaunchKernel(_ptsz), cuLaunchKernel, graph kernel-node add and set-params, cuLibraryLoadData (nv_attach_impl.cpp:488-534).
- Not hooked: cudaGraphLaunch, cudaGraphExecUpdate, cuLaunchKernelEx, cudaLaunchCooperativeKernel.
- Launches into a capturing stream pass straight through (attach/nv_attach_impl/nv_attach_impl_frida_setup.cpp:111-113).
- CUDA 13 kernel handles are not identified (nv_attach_impl.cpp:1575-1585), and the Orin runs CUDA 13.2 (R/feasibility-20261109/gpu-env/attrs.out.txt:1).
- llama.cpp decode runs 53,848 of 54,272 launches as graph replays (census README:47-49). It updates them with cudaGraphExecUpdate and launches them with cudaGraphLaunch (L/ggml-cuda.cu:3337, 3341, 3386, 5392).
- Probes do run in replays (R/watcher-policy-20261112/README.md:128).
- So sizing must be decided on the device (gridDim and blockDim are readable, default_trampoline.cu:521-561) or fixed before any launch.
- With graphs, 10,905 consecutive kernel pairs per tg128 overlap by up to 672 ns (sqlite). A slot indexed by thread can then hold two launches [inferred].

**Memory placement on the Orin.**
- There is no CUDA IPC (cuIpcGetMemHandle returns 801, R/feasibility-20261109/gpu-env/attach-probe/ipc_test.txt:3). The server therefore creates 1502 as 1512 and 1503 as 1513, in the shared segment (runtime/syscall-server/syscall_context.cpp:568-570; runtime/include/bpftime_shm.hpp:142-154).
- The GPU does not cache registered host memory (RD:214-217). Each 1512/1513/ARRAY lookup or update, and each 1527 record, adds a membar.sys (default_trampoline.cu:255, 262, 271, 322, 333, 351, 479).
- Atomics in registered memory are correct (R/feasibility-20261109/gpu-env/hostreg.atom-dual.txt:7-9):
  - GPU-only adds were exact.
  - CPU adds against GPU adds lost 0.
  - A GPU CAS lock worked.
- Their cost against device memory is unmeasured on the Orin. On an RTX 5090 a host-mapped update cost 9.4x a device-memory one, and a lookup 1.09x (R/buffer-survey-20261111/notes.txt:235).
- Everything the CPU can read comes from one segment fixed at start:
  - 128 MiB by default, at most 10,240 MiB (runtime/include/bpftime_config.hpp:81; runtime/src/bpftime_config.cpp:119-120);
  - prefaulted and registered whole (runtime/src/bpftime_shm_internal.cpp:1414-1469);
  - out of 7.36 GiB that the CPU and GPU share (attrs.out.txt:4).
- Device memory is usable today only for data the CPU never reads (per-module `__device__` globals, default_trampoline.cu:122, 127).
- For data the CPU reads, the agent would have to copy into the segment, as 1502 does on a discrete GPU (runtime/src/bpf_map/gpu/nv_gpu_per_thread_array_map.cpp:89-121). A POSIX-FD VMM export is supported but untried (attrs.out.txt:49-50).

**The CPU-side view.**
- The reader is the loader, a separate process, through intercepted `bpf()`. A lookup copies get_userspace_value_size() bytes (syscall_context.cpp:615-622).
- For 1512 that is value_size x thread count, which libbpf's `bpf_map__lookup_elem` rejects (third_party/libbpf/src/libbpf.c:10982-10987).
- For per-CPU types libbpf requires roundup(value, 8) x possible CPUs, and the Orin has 6 (libbpf.c:10953-10978). Storage per SM (8) or per warp slot therefore cannot pass as a per-CPU map with unmodified libbpf.
- 1527 is read only through a bpftime-only symbol found with dlsym (example/gpu/kernelretsnoop/kernelretsnoop.c:86-99), and:
  - its lookup returns ENOTSUP (runtime/src/bpf_map/gpu/nv_gpu_ringbuf_map.cpp:110-137);
  - epoll covers ordinary RINGBUF and software perf events, not 1527 (syscall_context.cpp:1149-1157);
  - drops reach the tool only as a log line (nv_gpu_ringbuf_map.cpp:56-104).

**Hardware ids and how probes run.**
- Helpers 509-511 return raw %smid, %warpid and %laneid (default_trampoline.cu:563-588). No helper reads %nsmid, %nwarpid or %gridid.
- PTX ISA:
  - %warpid is unique within a CTA "but not across CTAs within a grid".
  - %smid and %warpid "may change during execution" after preemption, and the Orin supports compute preemption (attrs.out.txt:4).
  - %nsmid may exceed the SM count.
  - %gridid is a per-context launch number.
- Observed: one 10-thread block on SM 0 read %warpid values 0-3 across launches (R/fixes-20261109/protocol/runs/san1-ts-legacy/server.log:79-97). No two resident warps have been compared.
- Helpers are `__noinline__` and see only active lanes (attach/nv_attach_impl/pass/ptxpass_kretprobe/main.cpp:50-59; pass/ptxpass_kprobe_memcapture/main.cpp:147):
  - entry probes run in every thread;
  - exit probes run before every ret/exit;
  - `__memcapture` runs before any load or store.
- eBPF has no warp instructions, so lane election or aggregation must be a helper registered at pass/ptxpass_core/src/core.cpp:235-253. An unknown helper ends the application (vm/llvm-jit/src/llvm_jit_context.cpp:907, 938). Helper 506 (membar.sys) is not registered there.
- From eBPF, fetch-add lowers to a relaxed atomicrmw (vm/llvm-jit/src/compiler_utils.cpp:376-388). CMPXCHG, however, returns a plain load taken before the cmpxchg, not the cmpxchg's result (vm/llvm-jit/src/compiler.cpp:1697-1718, re-read for this note). Claims and locks can therefore only live inside helpers.
- Tools write through lookup pointers non-atomically (example/gpu/threadhist/threadhist.bpf.c:21-29), which is safe only while the slot is private.
- Losses today:
  - Slots are per thread index and shared by concurrent launches (default_trampoline.cu:181-189).
  - A 1512 thread outside the thread count prints one message per module and nothing is counted (:195-216).
  - In 1527, a thread past the thread count returns 1 and is not counted. A full ring, a writer collision (per-thread busy flag) or an oversize record counts in `dropped` (:424-459).
  - The server silently halves 1527's thread count until the map fits (runtime/src/handler/map_handler.cpp:1394-1416).

## 3. Four approaches

**A. Per-warp bounded buffers as a library over existing map types (no runtime change).**
An eBPF header plus a loader-side C file. Every map is an ordinary 1513 or ARRAY, created by the server before any launch.
The tool author picks the sizes: S copies by SM, W warp slots per SM, K records per slot.
- Counters: thread (0,0,0) fetch-adds once per launch. Bins that depend on the data are split by `smid % S`, and the loader sums the copies.
- Records: lane 0 picks slot `(smid % S) x W + (warpid % W)`, takes `i = fetch_add(cursor)` and writes only if `i < K`. Drops per slot are exactly `cursor - K`. The ids only spread the writers, so a shared slot loses nothing.
- Reading: once after the run, one whole-value lookup per slot. A variant drains while running, with stamped records and 3-4 fenced lookups per record; its accept rule is unverified.
- Scratch: key `slot x 32 + lane` with a thread-id tag; mismatches are counted.
- Tools must be built with `-mcpu=v3` [inferred].

**B. Storage sized from each launch's own grid x block.**
Neutrino's form is blocked here: the host allocates per launch, passes an extra kernel argument and copies back, but
replays reach no hook and map_info is per module. What survives:
- Thread-indexed counters grow to 2H, where H is the largest geometry any hook has seen (direct launches, capture, graph-node parameters).
  - Old regions are kept in the segment.
  - Helpers read a control block, which costs one more uncached load.
  - Misses are counted, and a `needed` word records the size required.
- Scratch and records get a region per launch, claimed on the device:
  - The first thread hashes `%gridid` into a 64-entry per-module table.
  - It sizes the region from gridDim x blockDim and takes it from a pool with one atomic; the other threads spin until it is published.
  - A new exit pass counts warps out and frees the region.
- The scratch pool is device memory allocated by the agent; the record pool is in the segment.
- New hooks are needed for cuLaunchKernelEx and cooperative launches.

**C. A shared pool of fixed-size chunks claimed per warp call.**
A new map type in the segment: `max_entries` chunks of C records each (C = 1..32), each chunk with a state word (free, writing, full).
- Writing (`bpf_perf_event_output`):
  - The lowest active lane claims `ceil(active lanes / C)` chunks: atomicAdd on a per-SM claim counter, then a CAS from free to writing.
  - It broadcasts the address with `shfl.sync`, and each lane writes at its rank.
  - The leader fences and marks the chunk full.
- When no chunk is free, drops are counted and every lane gets 2.
- The CPU pops full chunks with `BPF_MAP_LOOKUP_AND_DELETE_ELEM` (key size 0; libbpf's size checks pass).
- The grid never sizes it, and no launch hook is involved.
- Counters become `{key, delta}` records; scratch is not served.

**D. Storage indexed by hardware position plus one ring per SM (RD's "tier 2", RD:122-123).**
Sizes come from the device: S = SM count, W = threads per SM / 32 (8 and 48 on the Orin). Larger ids are folded with `mod`.
- Counters: one copy per (SM, warp slot, lane) in the segment, exact because the tool adds with fetch-add. A `bpf()` lookup returns the element-wise u64 sum.
- Scratch: a `.global` array (device memory) appended to each patched module and indexed the same way, with an owner tag (`%gridid`, thread id). At exit a different tag returns NULL and counts a miss.
- Records: one ring per SM in the segment.
  - The lowest active lane claims slots with a CAS loop on the ring's reserve word, and the lanes write.
  - The leader issues one `membar.sys`; per-slot lap words then publish the records.
  - The existing poll drains the rings and reports drops by reason.
- Per-thread results stay on a hand-sized 1512.
- The closest prior use is on this board model: WarpGuard (NVBit-based) on a Jetson Orin Nano wrote per-SM rings in mapped host memory from the lowest active lane and blocked when a ring was full. It reported no ring cost, size or drop count (R/buffer-survey-20261111/notes.txt:116-120).

| approach | counters: loss; memory | scratch: loss; memory | streams: loss; memory | CUDA graph replays | concurrent kernels | CPU view | size [inferred] | tool author still decides |
|---|---|---|---|---|---|---|---|---|
| today (1512, 1527) | past the thread count: NULL, one message, uncounted; 32 MiB at 4.19M threads | 1512 slot per thread; same | ring per thread; past the thread count uncounted, full or collision counted; 64.6 GiB at 4.19M x 16 x 1,032 B | probes run | slots shared by index | 1512: one thread-major blob libbpf rejects; 1527: dlsym poll, drops in a log | - | thread count per map |
| A library | exact with fetch-add; 8 B per per-launch counter, split bins 4 KiB; per-thread index still N x 8 B by hand, overflow counted | tag mismatch counted; 256 KiB (W = 64) | cursor - K counted; lanes 1-31 skipped by design, `__memcapture` gaps silent; 32 MiB = 8 launches of the largest rope_norm | yes, no host step | sums exact; no launch id | ordinary lookups (libbpf accepts); tool sums copies; whole-value copy per slot; polling | ~800-950, 0 in runtime | S, W, K, record size, bins, read point; `-mcpu=v3`; no CMPXCHG or plain += on shared words; fds < 1024 |
| B per launch | grows to 2H, at most 64 MiB at 8 B; counted misses until it grows; exact under overlap only with atomics | per-launch device region, exact; pool or table full counted; at most 64 MiB for 2 live launches [inferred] | K records per warp, counted; 21 MiB at K = 4 x 32 B for the largest launch; ~124 MiB pool per 10 ms of decode | counters: only if the geometry passed a hook at capture; scratch, streams: sized on the device, need a unique `%gridid` | scratch, streams: separate regions; counters shared | key = thread x entries + entry (libbpf accepts), reserved key {H, drops, needed}; dlsym drain | ~1,350 incl. an exit pass, 2 hooks | map type per use, K, two pool budgets, atomics, read {H, drops} first |
| C chunk pool | `{key, delta}` records, exact only when drops = 0; budget-bounded | not served (entry and exit records joined on the CPU: 328 MiB offered per 4.19M launch) | counted per record; fixed budget, e.g. 64 MiB = 51,150 chunks of 32 x 40 B, any grid | yes | yes; records interleave, no launch id | pop one chunk per bpf() call; no ring_buffer__poll, no epoll | ~470 + ~300 tests | budget, C, ids and globaltimer in records, pop rate, sums, joins, sort |
| D hardware position | exact with fetch-add; plain += loses silently if resident warps share a slot; 96 KiB per u64 entry, any grid | moved thread: counted NULL; CTA-local %warpid: collisions, silent torn tag/value [inferred]; 288 KiB per module (2.8 MiB for 10) in device memory | counted by reason; an unpublished slot stalls its ring; 64 MiB = 1.68M 40-B slots | yes; scratch tag needs a unique `%gridid` | counters exact with atomics; rings multi-producer; scratch needs exclusive ids | lookup returns the sum (additive u64 only; libbpf accepts); poll + lost callback + lookup(0) | ~1,300 | map kind per use, atomics, ring slots, ids and globaltimer, merge rings by time, NULL check |

The stream figures use different record sizes (32, 40 and 1,032 B) and different reference launches, so they cannot be
compared as printed. Per 64 MiB of budget, A, C and D each hold about 1.6-2.1M records of 32-40 B [arith]. They differ
in how the budget is split: per hardware slot (A), one shared pool (C), per SM (D), per launch (B).

**GPU cost: one measurement, wide spread (all [inferred]).**
- The only measured cost: threadhist on rms_norm (census README:93).
  - It adds about 7 ms per pp512 for 11.27M per-thread lookups and increments in registered memory.
  - That is about 0.6 ns per thread execution, or 20 ns per warp; rms_norm itself takes about 4 ms.
  - No control run used storage that is not thread-indexed (:94), so no design is shown to be cheaper than today's.
- One record per warp on every kernel of a pp512 (6.02M warps, 204.6 ms of GPU time):
  - B: +1.8% to +56%. The low end assumes 0.6 ns per record (writes share transactions), the high end 19.2 ns.
  - C: 30-60 ns per call, +88% to +176% [arith from its figure].
  - A: 1-4 fenced lookups of 20 ns, +58% to +233% (its own figure, over 206.7 ms of wall time).
  - D: no figure ("same order or more" than 1512).
  - The estimates span about 130x.
- RoPE-K only, the rope kernel gpu_ext probed (524,288 threads and 22 launches per pp512, census README:74; 360,448 warp records): A 3.5-14%, C 5-11%.
- gpu_ext's 5.4-5.6% (notes.txt:139, 233) combined five conditions: one writer per logical-warp slot, no atomics, device memory, serial launches and a geometry known offline. None of A-D has all five on the Orin, so that number is not yet a demonstrated target here.

**Where readers and designers disagree.**
- **Warp slots per SM.** D uses 48 (1,536 / 32, attrs.out.txt:4), A uses 64 as an example, and RD:107 counts 384 warp slots in all. Nobody has read `%nwarpid`. D also adds a lane dimension (12,288 copies), which RD's requirement 1 does not mention.
- **What `%warpid` is.** The PTX text promises uniqueness only within a CTA. Yet a one-warp block read values 0-3, where a CTA-relative index would always be 0 [inferred]. Uniqueness across CTAs on one SM is untested.
  - D's scratch fails if it does not hold (up to 16 blocks per SM, attrs.out.txt:4).
  - A counts mismatches, C uses `%smid` only as a hint, and B does not use it.
- **Writer lane.** A fixes lane 0, as gpu_ext does (gpu_ext 50a51ad4, revision-rq4/onevalue-array-candidate/onevalue-array.bpf.c:39-44), because eBPF cannot read the active mask. C and D elect the lowest active lane inside a helper; `activemask` is already in the trampoline (default_trampoline.cu:30-35).
- **Fences.** C and D publish a whole warp with one leader `membar.sys` after `bar.warp.sync`, and D drops the lookup's fence for counters. Whether one leader fence orders the other lanes' stores for the CPU is unverified. A keeps fenced lookups because no fence helper is registered.
- **Counters.** A argues that once-per-launch adds cover every real-kernel counter use, and the use survey supports this.
  - D builds per-lane copies for counters updated by every thread. Only demos would use them today, plus scenarios that need attach points bpftime lacks (instruction mix, memory-access counts: R/real-scenarios-20261113/README.md:95-96).
  - C turns counters into records.
  - B keeps thread-indexed arrays that grow with the grid, against RD requirement 1.
- **CPU view.** RD requirement 5 asks for per-CPU semantics, which libbpf's per-CPU size check rules out for unmodified libbpf. D returns a sum, A leaves summing to the tool, B uses keys `thread x entries + entry`, and C pops chunks.
- **RD's table of other systems** (RD:84-90) needs five corrections (R/buffer-survey-20261111/notes.txt):
  - NVBit reserves bytes with one fetch-add per push; per-warp records are its mem_trace tool's choice (:273).
  - GT-Pin hashes threads into at most max-hardware-thread buckets (:296).
  - CUPTI pools per-launch driver records and counts drops only when no client buffer is supplied (/usr/local/cuda-13.2/targets/sbsa-linux/include/cupti_activity.h:10751-10760).
  - hip-analyzer overwrites without a device-side count (:88, 95).
  - Neutrino's overflow corrupts neighbouring segments (:3, 10).
- **Agreement.** All four still need RD's tier-1 fixes (RD:120-121: count every loss in 1512 and 1527, no silent shrinking), because none of them replaces 1512 for per-thread results.

## 4. Combinations, and what each approach gives up

**What the facts already decide: eliminations, not a winner.**
- **Host-side allocation per launch:** replays reach no hook, and they carry 99.2% of decode launches (census README:47-49).
- **Per-thread rings at any sizing:** 64.6 GiB at the largest launch, against 7.36 GiB of memory in all (RD:55-56; attrs.out.txt:4).
- **Keeping every per-thread record on every kernel, in any bounded design:** at 32 B the 128 MiB segment fills in 4.5 ms (prefill) or 1.7 ms (decode) [arith]. Only counted drops or blocking remain; NVBit's channel blocks and stalls the GPU (notes.txt:37).
- **Partly decided: one mechanism for every use.**
  - By their own designs, C does not serve scratch and D cannot express per-thread results.
  - A and B nominally serve every use. What counts against them is estimated cost and lane-0 sampling (A) or RD's requirement 1 (B), not a fact.
  - A single mechanism is ruled out only if A and B are rejected on those grounds.

**Splits that make sense.**
- **Counters:** once-per-launch fetch-adds into 1513/ARRAY (A's part, no runtime change) cover what real tools do today. D's summed copies matter only if counters updated by every thread become a target.
- **Scratch:** device memory the CPU never reads, either D's per-module array with an owner tag or B's per-launch pool. No real-kernel tool uses scratch yet, so it can wait.
- **Streams:** C's shared pool or D's per-SM rings, both a fixed budget with counted drops.
  - C gives up order for one shared budget. D keeps order per SM but splits capacity per SM.
  - Either could keep a write position per hardware slot across firings (hip-analyzer's chunk per wavefront, notes.txt:88), but only if the (SM, warp slot) id proves stable.
- **Per-thread results:** a hand-sized 1512 with counted overflow (tier 1), or B's growth limited to that one type.
- **CPU-to-GPU data:** unchanged.
- **Placement is a separate axis for all storage the CPU reads.** The GPU writes to device memory and the agent copies closed units into the segment, as 1502 does on a discrete GPU.
  - The 5090 numbers point to this as the main cost lever (notes.txt:235).
  - C's full chunks are the most natural unit to copy [inferred].
  - It needs allocation in the agent, plus copy ordering against application streams and graphs; both are unknown.
- **A helper that reads `%gridid`** would give records in A, C and D a launch id, and it is B's foundation. One check decides it for all four.

**What each gives up.**
- **A:** device memory, lane election, lane coverage in `__memcapture`, a sound drain, and enforcement of correct use. The tool author carries every size.
- **B:** memory bounded by the hardware (it follows the largest launch), and independence from launch hooks for counters. It adds exit accounting to every kernel of a probed module, and threads that spin on a launch entry.
- **C:** counters and scratch as maps, record order, lookup by key, `ring_buffer__poll` and epoll.
- **D:** per-thread identity; counters whose fetch-add result is used (gpu_ext's launch queue needs one shared word); values that cannot be summed (maxima, timestamps); per-SM distributions once summed. Its scratch rests on id exclusivity, which is undocumented.

## 5. Step 0: on-board checks

Every run on the board needs the user's approval first, plus the usual preparation (clocks, no leftover processes or
shm segments). The checks are ordered by how many approaches each rules in or out; among ties, a check that can rule an
approach out comes first.

1. **One full-occupancy id log** (A, B, C, D).
   - Each warp stores `%smid`, `%warpid`, `%laneid`, `%nsmid`, `%nwarpid`, `%gridid`, ctaid and globaltimer at entry and at exit, into device memory.
   - Runs: block sizes 32-1,024; two streams; a 3-node graph replayed 1,000 times; two modules; a second GPU process forcing time-slicing.
   - Decides:
     - S and W (A, C, D);
     - whether (SM, warp slot) is exclusive among resident warps (D's scratch and plain-add counters stand or fall on it);
     - whether ids change between entry and exit;
     - whether `%gridid` is unique per launch, replays included (B's per-launch regions stand or fall on it; a launch id for A, C, D).
2. **Placement and atomic cost** (A, B, C, D).
   - Registered host memory against cuMemAlloc memory, at 8 x 1,536 threads and at 64 threads.
   - Cases: plain store against `atom.add.u64` per lane; one word hit by 1 to 48 warps per SM with atomicAdd and with CAS; with and without `membar.sys`; today's 1512 lookup-and-increment as the reference.
   - Decides: which reading of the 0.6 ns figure is right; whether any design the CPU can read can reach single-digit percent on the Orin; whether copy-out from device memory is required.
3. **Device memory to the reader** (A, C, D storage; B's record pool).
   - An agent-side cuMemcpyDtoHAsync on its own stream into the segment while llama.cpp runs with graphs (ordering, cost).
   - A POSIX-FD VMM export mapped by a second process.
   - Decides whether storage the CPU reads can live in device memory on the Orin at all.
4. **CPU/GPU publication test in registered memory** (A's drain, C, D).
   - Lanes write a payload, `bar.warp.sync`, one leader `membar.sys`, then a flag. The CPU acquire-reads the flag, then the payload, for 10^8 rounds.
   - Also: a CPU release-store, then a GPU CAS on the same word.
   - Decides: one fence per warp, or one per lane (today's cost); C's state machine; A's accept rule.
5. **eBPF atomics as tools would write them** (A, B, D).
   - Compile-only first: `__sync_fetch_and_add` with a used result at the default `-mcpu` and at v3; read the PTX.
   - Then a run at 12,288 threads: every returned index must appear exactly once.
   - Decides A's record path, and whether "use atomics" is a workable rule for tool authors in B and D.
6. **Drain throughput and segment size** (A, C, D).
   - A pop or lookup loop through intercepted `bpf()` over a pre-filled pool: bytes/s, and the effect on GPU time (CPU and GPU share DRAM).
   - Whether `BPFTIME_SHM_MEMORY_MB=1024` registers and prefaults.
   - Decides the sustainable record rate (decode offers 77.3M warp records/s) and the largest budget.
7. **Warp instructions in a `__noinline__` helper under partial masks** (C, D).
   - `shfl.sync`, `bar.warp.sync` and `activemask` with early returns that depend on data, and inside `__memcapture`.
   - `popc(activemask)` at the exit probe of 4 llama.cpp kernels.
   - Decides whether warp-level reservation is correct, and the records per call (C's chunk size and its waste).
8. **Packaging limits** (B, D; compile and load only).
   - A `.global` appended to a patched llama.cpp module loads, and cuLibraryGetGlobal finds it.
   - Constant memory left next to map_info's 40 KiB.
9. **Launch-hook coverage** (B only; one logged run, then analysis).
   - Log every geometry the hooks see in one pp512 + tg128 run with graphs on, and compare with the nsys rows per kernel and per graph.
   - Decides whether B's counters grow before counted misses, or only after them.

## 6. Open questions for the user and the advisor

1. No bounded design can hold per-thread records on every kernel. Is "counted drops" acceptable there, with one record per warp (0.9-6.2 GB/s offered [arith]) as the design point?
2. Is per-thread identity at unknown scale a requirement? It has 15 uses, 4 of them on real kernels, and the llama.cpp one only sums. If yes, B or a hand-sized 1512 must stay. If no, once-per-launch counters plus C or D cover the rest.
3. RD requirement 1 (RD:107-108) allows storage bounded by the hardware or by an explicit budget. Streams in A, C and D are bounded by a budget, not by the hardware; B is bounded by the grid. Which of these is the contribution?
4. CPU view: standard libbpf calls (no per-CPU semantics possible; sums or pops only), or a bpftime-specific reader API?
5. Platform: the Orin or a discrete GPU (RD:12)? On a discrete GPU, device memory is the default and the cost moves to a copy per lookup (runtime/src/bpf_map/gpu/nv_gpu_shared_array_map.cpp:70-111), so the placement conclusions may flip.
6. RD:131-133 lists what would overturn its focus.
   - A tests "easy to package as a library"; its own list of shortfalls says only partly.
   - The survey's finding that real-kernel counters fire once per launch bears on "counters only", but 5 real stream uses remain.
   - Does the advisor read either condition as met?
7. Map model or attach points first? The heaviest scenarios (instruction mix, address traces, region timing) need attach points bpftime lacks (R/real-scenarios-20261113/README.md:94-99), and they would raise record volume.
8. May the design add helpers (lane election, `%gridid`, warp aggregation), or must it stay within existing helpers plus new map types? The choice between A and C/D hinges on this.
## Files

- `readers/`: the three fact sheets this note is built on (mechanism constraints, what GPU-native tools need, how other systems bound storage).
- `approaches/`: the four approaches as their designers wrote them (A `library`, B `per-launch`, C `chunks`, D `hw-position`).
- `scripts/workflow.js`, `workflow-output.json`: the workflow (3 readers, 4 designers, 1 comparison; nothing built or run).
