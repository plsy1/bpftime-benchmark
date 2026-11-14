Design: no runtime change. Bounded per-warp buffers as a library over existing map types (dev 52126f33)
Paths: code relative to /home/jetson/src/bpftime-daily-state/worktrees/4708f4092d1e/candidate; tr = attach/nv_attach_impl/trampoline/default_trampoline.cu; R = /home/jetson/src/results/gpu. [inferred] = not read directly or not run.

The case for it: all four real-kernel (a) uses run once per launch in thread (0,0,0), threadhist's llama.cpp port only sums its slots, and kernelretsnoop's useful form is one record per warp (reader 2). All three fit the existing maps if writes use fetch-add and the drops are counted. The 16 CPU-to-GPU uses already work on 1513 and ARRAY. A runtime map model is then needed only for the gaps listed in E.

A. Mechanism
1. What ships: one eBPF header and one loader-side C file. Every map is an ordinary BTF map definition: 1503, which is created as 1513 in the segment on the Orin (syscall_context.cpp:570), or ARRAY. The syscall server allocates the maps at load time, before any launch, sized by constants in the tool. Nothing in runtime/, attach/ or vm/ changes. 1504 adds nothing: it needs a kernel BPF map id and is registered host memory too (array_map_kernel_gpu.cpp:46-60, 287-291).
2. Atomics are fetch-add only. eBPF ADD|FETCH lowers to an atomicrmw and returns its result (vm/llvm-jit/src/compiler_utils.cpp:372-397). CMPXCHG puts a plain load taken before the cmpxchg into r0, not the cmpxchg result (vm/llvm-jit/src/compiler.cpp:1697-1718), so it cannot claim anything. GPU-only adds in registered memory were exact (R/feasibility-20261109/gpu-env/hostreg.atom-dual.txt:6-8).
3. Counters: a 1513 of u64 bins.
   - Per-launch counts: thread (0,0,0) adds once. "Threads run" is gridDim x blockDim from helpers 504 and 508 (tr:520-529, 552-561), which is all the llama.cpp threadhist port needs.
   - Data-dependent per-thread bins: each thread adds into bin[k][smid % S] (helper 509), and the loader sums the S copies.
4. Writer slot = (smid % S) x W + (warpid % W), from helpers 509/510 (tr:563-588, registered at core.cpp:251-252). The ids only spread the writers. Every reservation is a fetch-add, so a slot shared by two warps (through folding or preemption) loses nothing. Variant: gpu_ext's logical warp index (onevalue-array.bpf.c:43-44), whose bound grows with grid/32.
5. Leader = lane 0, from helper 511 or linear thread & 31 as in onevalue-array.bpf.c:39-42 [that the two agree is inferred]. eBPF has no activemask, shuffle or match, and no helper exposes them, so a record carries lane 0's values only.
6. Records read after the run: a record map (1513), key = slot, value = {u64 cursor; K records of R bytes}. Lane 0 looks up its slot (membar.sys inside, tr:259-263), takes i = fetch_add(cursor, 1), and writes record i only if i < K. One fenced lookup per record.
7. When full: an index >= K is not written, and drops per slot = cursor - K, exactly, kept in the map. Lanes 1-31 are not recorded, by design.
8. CPU read: once the kernels have finished (process exit, or a point the tool knows is quiet), the loader makes one bpf_map_lookup_elem per slot, which copies the whole value (syscall_context.cpp:615-622; libbpf accepts it because value_sz equals value_size). It keeps min(cursor, K) records, sums the drops, and resets the slot with bpf_map_update_elem (a memcpy, nv_gpu_shared_array_host_map.cpp:50-66).
9. Records drained while running (a variant; its soundness is not verified):
   - A header map (1513) holds {cursor, done}. A position map (1513) holds the consumed index and is written only by the loader. Each record is stamped with its index.
   - Lane 0 takes i = fetch_add(cursor) and reads the consumed index (fenced lookup). If i - consumed >= K it drops. Otherwise it writes the record at i % K, does a fenced lookup, writes stamp = i, does another fenced lookup, then fetch_add(done).
   - That is 3-4 fenced lookups per record. Helper 506 (membar.sys, tr:540-545) is not registered for PTX (core.cpp:242-253), so a second lookup is the only fence available.
   - The loader accepts a slot only if done == cursor before its copy and cursor is unchanged after it. An index below cursor with no matching stamp is a counted drop. It then writes the position map. Nothing is ever reset.
10. Scratch from entry to exit: a 1513 with key = slot x 32 + lane and value = {tag, value}.
    - At entry the probe writes the value, then tag = logical global thread id + 1.
    - At exit it recomputes the key. It uses the value only if the tag matches, then clears the tag. On a mismatch (warp moved, or ids shared) it fetch-adds a mismatch counter.
11. Build with -mcpu=v3, because fetch-add with a used result needs it. The examples compile with plain -target bpf (example/gpu/threadhist/Makefile:115), and the board has clang-15/16 [default v1: inferred].

B. The three uses on the Orin (unit cost: threadhist costs 7 ms over 352,288 warps, about 20 ns per fenced lookup per warp, R/llama-census-20261113/README.md:90-93; all costs below are [inferred] from it)

Memory bound (formula, and value at the 4.19M-thread launch):
- **Counters, histograms:** B x S x 8 B; 64 bins x 8 copies = 4 KiB at any grid, and 8 B per per-launch counter. threadhist read per thread stays N x 8 B = 32 MiB at 4,194,304, sized by hand.
- **Per-thread scratch:** S x W x 32 x 16 B = 8 x 64 x 32 x 16 = 256 KiB at any grid. Logical-id key: 64 MiB.
- **Record streams:** S x W x (8 + K x R).
  - Read after the run (K = 2,048, R = 32): 32.0 MiB, holding 1,048,576 records. The 4.19M launch is rope_norm, 16,384 x 256 threads = 131,072 warps (pp512-nograph.sqlite), so this holds 8 of its launches. Its 22 launches per pp512 overflow by 1,835,008 records, counted.
  - Drained: 512 x (16 + 256 x 40) B = 5.0 MiB.
  - Logical-warp key: 131,072 x (8 + 44 x 32) B = 177 MiB, above the 128 MiB segment.

Loss behaviour:
- **Counters:** exact (atomics). Per-thread overflow is counted.
- **Scratch:** counted (tag mismatch). Silent only if two resident warps share (smid, warpid) and race inside one probe.
- **Streams:** counted (cursor - K, or missing stamps). Silent: lanes 1-31 by design, and every lane in __memcapture when lane 0 is inactive.

GPU cost on the Orin [inferred]:
- **Counters:**
  - Per launch: one fenced lookup and one atomic, 444 per prefill, under 1 ms.
  - Per-thread bins: at least 20 ns per warp plus 32 atomics, unmeasured. That is at least 3.3% for one rms_norm-sized kernel and at least +58% on every kernel.
- **Scratch:** 2 fenced lookups per warp plus coalesced 16 B per lane, about 40 ns per warp. That is +117% on every kernel (6.02M warps, pp512).
- **Streams:** 20 ns x (1 to 4 fenced lookups) plus a fetch-add per record.
  - RoPE-K pp512 (360,448 records): 7-29 ms = 3.5-14% of 206.7 ms.
  - Every kernel, pp512: 120-480 ms (+58% to +233%).
  - Decode: 25-98 ms per token against 15.9 ms of GPU time.

Works with CUDA graph launches?
- All three: yes. No host action is needed. map_info is fixed at module load and shared by all launches (nv_attach_impl.cpp:2366-2385). Probes run in replays (R/watcher-policy-20261112/README.md:89, 128).

Works with concurrent kernels?
- **Counters:** yes, and sums stay exact, but there is no per-launch split.
- **Scratch:** yes if (smid, warpid) is unique among resident warps; otherwise the mismatches are counted.
- **Streams:** storage yes (fetch-add; graph replays overlap by up to 672 ns, which is harmless). Records carry no launch id, because no helper reads %gridid.

CPU-side view through bpf():
- **Counters:** a lookup per bin, value_size as declared, so libbpf works. The tool sums the S copies. Not per-CPU semantics.
- **Scratch:** not read. The mismatch counter is a bin.
- **Streams:**
  - A whole-value copy per slot per read: 64 KiB after the run, 10 KiB drained.
  - No zero-copy: bpftime_map_lookup_elem is not exported (runtime/syscall-server/syscall-server.version:2).
  - No epoll, so the drain needs a polling thread.

C. Implementation size (rough, by hand)
- example/gpu/lib/gpu_bounded.bpf.h, about 200 lines: helper declarations (502-505, 508-511), map macros, per-launch add, add into split bins, scratch put/take, the two record paths.
- example/gpu/lib/gpu_bounded_reader.{h,c}, about 300 lines: sum the copies, read slots after the run, the drain thread with its accept rule, drop totals, reset.
- Ports: threadhist (per-launch sum), kernelretsnoop (per-warp records), an entry-to-exit timing example: about 100 lines each.
- A test kernel and checks (fetch-add indices unique, drop arithmetic, tag mismatches): about 150 lines.
- Total about 800-950 lines. runtime/, attach/ and vm/: 0 lines.

What the tool author still does by hand:
- Choose S, W, K, R and the bins, the read point or the drain period, and the record layout. Write SEC names per kernel.
- Accept lane-0 sampling, the lack of a launch id, and incomplete coverage in __memcapture.
- Size per-thread identity (threadhist read per thread) as N by hand. The library only counts the overflow.
- Never use CMPXCHG, or plain += on shared words. gpu_ext's total_overflow += 1 (onevalue-array.bpf.c:54, 60) and its plain cursor (:57-70) race once launches overlap, and reader 2 counted 10,905 overlapping kernel pairs in decode with graphs.
- Build with -mcpu=v3.
- Keep map fds below 1024. The library adds 2-4 maps, and an fd >= 1024 leaves the module unpatched (bpf_attach_ctx_cuda.cpp:535-540).

D. Assumptions not yet verified, and the step-0 check for each (nothing was run for this design)
1. **About 20 ns per fenced lookup per warp:** that this also holds for a lane-0 fetch-add plus store, and for small grids (decode median 2,048 threads), where latency is not hidden.
   - Check: a vec_add-style kernel at 8 x 1,536 threads and at 64 threads, with four probe bodies (empty, threadhist, record read after the run, drained record), measuring GPU time per warp.
   - Then RoPE-K pp512 under llama-bench, as in census section 3.
2. **Atomic cost:** 32 lanes into one bin, against bins split per lane, against per-launch only, in registered memory. Same harness.
3. **fetch-add returns the atomicrmw result on the GPU:** the only atom.add witnessed has no used result (R/dir2-design-20261113/read/cuda-counter-patched-20261110.sm_75.ptx:1965). Check the kept PTX of a probe that uses the result, and check that a 12,288-thread probe gets every index exactly once.
4. **Ranges of %smid and %warpid on a full grid, and per-slot cursor skew on RoPE-K pp512:** these set S and W (if %nwarpid is 48, W = 64 wastes 25%) and the drops on hot slots.
5. **Scratch mismatches:** 0 on a llama.cpp prefill run alone. Then count them with a second GPU process time-slicing (computePreemptionSupported=1).
6. **A lookup works as a fence:** the drain never accepts a record whose checksum fails, with 1 us kernels in a loop.
7. **Accept rule:** it needs done to be read before cursor, and one bpf() copy of {cursor, done} does not order the two reads on the CPU [inferred]. Count records stamped after their slot was accepted; split the header into two maps if needed.
8. **Cost of whole-value copies:** 512 lookups x 10 KiB at 100 Hz, and their effect on GPU time (the CPU and GPU share DRAM).
9. **Graph replays:** per-launch counts equal the nsys launch counts in tg128 with graphs on.
10. **-mcpu=v3 is needed:** compile a __sync_fetch_and_add whose result is used, with clang-15 defaults.

E. Where it loses to the other approaches
1. **Placement and cost.** On the Orin every byte is registered host memory that the GPU does not cache, and every lookup adds a membar.sys (tr:255-271). No map type gives eBPF device memory on the Orin.
   - gpu_ext's 5.4-5.6% was device memory: on the RTX 5090 a host-mapped update cost 9.4x a device-memory one (buffer-survey notes:235).
   - The estimated +58% to +233% for streams on every kernel is a ceiling only a runtime can move, by allocating device memory in the agent and copying out.
2. **No warp aggregation.** Without activemask, shuffle or match, per-thread counters pay one atomic per lane and records sample lane 0. Combining lanes, or electing the lowest active lane, needs a new helper, which is a runtime change.
3. **__memcapture:** lane 0 may be inactive, which leaves silent gaps.
4. **The drain while running is the weak part:** 3-4 fences per record, an unverified accept rule, whole-value copies and polling.
   - Without it the store fills: 8 launches of the largest rope_norm.
   - Decode, at 1.23M records per token, needs it.
5. **Per-thread identity at unknown scale** (15 (c) uses, including threadhist read per thread): memory is still N x 8 B, sized by hand. The library only makes the loss visible.
6. **No launch id:** concurrent launches of one kernel cannot be told apart.
7. **Fixed capacity per slot:** skew drops records on hot slots while others stay empty. A shared pool with chunk reservation (hip-analyzer style) adapts.
8. **Nothing enforces correct use.** 1512 and 1527 still lose silently at default sizes, so the tier-1 fixes are needed either way. The CMPXCHG lowering is a bug the library avoids rather than fixes.
9. **On a discrete GPU:** 1503 is device memory (cheap writes), but every bpf() lookup copies device to host per key (runtime/src/bpf_map/gpu/nv_gpu_shared_array_map.cpp:70-111).