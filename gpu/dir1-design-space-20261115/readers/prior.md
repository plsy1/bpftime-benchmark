How existing systems bound GPU instrumentation storage (read-only, 2026-11-15)
Roots: notes = /home/jetson/src/results/gpu/buffer-survey-20261111/notes.txt. Its lines are cut at a fixed width, so a fact past the cut is in the same item of workflow-results.json in that directory. Its checker lines 197-297 override earlier lines. RD = git -C /home/jetson/src/bpftime show origin/main:RESEARCH_DIRECTIONS.md. GX = /tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/survey-src/gpu_ext (HEAD 50a51ad4, read with git show HEAD:path); rq4 = GX/workloads/llama.cpp/observability_overhead/revision-rq4. cuda.c = /tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/repo-neutrino/neutrino/neutrino/src/cuda.c. channel.hpp and nvbit.h = /tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/nvbit/nvbit_release_aarch64/core/{utils/channel.hpp,nvbit.h}. cupti_activity.h = /usr/local/cuda-13.2/targets/sbsa-linux/include/cupti_activity.h. ringbuf.c = /home/jetson/src/l4t-r39.2.1/Linux_for_Tegra/source/kernel/kernel-noble/kernel/bpf/ringbuf.c.
Metrics differ between systems: prefill loss (token/s), kernel-only slowdown and whole-run slowdown. The platform is given each time. [inferred] means my own reading, not stated in a source.

1. gpu_ext per-warp "onevalue" buffer (rq4/onevalue-array-candidate)
- Layout and slot: one value of map type 1503 (cuMemAlloc device memory). It holds 16,384 per-warp u64 append counters plus 16,384 x 44 events of 32 B, 23,199,768 B in all, frozen by _Static_assert (onevalue_array_value.h:18-19,28-44). Only lane index 0 writes: (linear_thread & 31)==0 (onevalue-array.bpf.c:41), a fixed lane with no active-mask election. The slot is the logical warp position, block_x*warps_per_block+warp (:43-44), and only 1-D grids are accepted (:52-53). The writer reads the counter, stores the event and sets counter+1, with no atomic (:57-70). The counter is never reset, so capacity = warp positions x launches in the run (44 rope_norm launches, README.md:24-27).
- When full: more than 44 events in a warp go to total_overflow; a warp past 16,384 or a nonzero y/z goes to total_out_of_range. Both are counted, but with plain += (bpf.c:54,60), so they can lose counts under concurrency [inferred]. Neither was triggered in the measured runs (rq4/results-onevalue-array-bootstrap-575-20260907/README.md:28-29).
- Host read: once, after the target exits, as one bpf_map_lookup_elem of the whole value by a separate collector process that owns the CUDA context (README.md:77-79,102-104). It takes 10.38 ms (5 runs) or 10.60 ms (10 runs), outside prefill timing (results README:30-31,92-94).
- Cost (RTX 5090, llama.cpp pp512): prefill loss 5.57% / 5.40% (notes:139). NVBit writing the same per-warp device array: 9.56% vs 5.31% (notes:233). The host-mapped per-thread ring lost 90.7% and allocated 892 MiB for 22 MiB (notes:137,140). That ring was measured in another campaign, and placement, layout, publication and drain changed together (notes:143). Placement alone (one 32-thread block): an update costs 3.85 us per launch in device memory vs 36.2 us host-mapped, 9.4x (notes:235).
- Gives up: it is sized from one offline geometry and is "for this pp512 geometry and serial target launches, not an unbounded or concurrent-kernel streaming collector" (results README:98-100). It is not drained while running.
- To package in bpftime [inferred]:
  - capacity tied to warps x launches, or a per-launch reset;
  - a writer lane chosen by the runtime;
  - atomic loss counters;
  - a bpf() view other than one 22 MiB value.
  1503 needs a CUDA IPC handle (rq4/device-array-runtime-notes.md:37-39). On the Orin, IPC fails (801), so dev creates 1503 as 1513 in registered host memory that the GPU does not cache (RD:209-217). That is the placement that cost 9.4x on the 5090.
- Related, not hand-written: GX/workloads/cross-layer-raw-map uses bpftime's own 1527 (4 records per thread) plus 1502, on the authors' bpftime tree bpftime-table1-575 (raw_map.bpf.c:26-38). gpu_ext's audit says that tree counts drops by reason: full, out-of-range, bad-size, other (audit.md:14-22; not read in source). All 2,560 overflow drops were accounted for (README.md:93-97). No latency or bandwidth was measured (README.md:84-89).

2. gpu_ext full-record device buffer (rq4/full-record-device-buffer)
- Layout and slot: 32 values of 1503 (gpu_ext calls them "banks"). Each holds 16,384 thread slots x 256 records x 80 B plus a u64 counter per slot: 335,675,408 B per value, about 10 GiB in total (full_record_device_buffer_value.h:42-45,63-80,151-152). The slot is the global linear thread id (bpf.c:66-73), and bank = id/16384. Every thread writes, with a plain counter (one writer per slot) (bpf.c:86-96,139). It is split into banks because a 1.25 GiB value was truncated in BTF (README.md:23-31) and the value size is a signed int (full-record-device-buffer-plan.md:109-112).
- When full: threads past 524,288 go to total_out_of_range in bank 0; records past 256 per slot go to total_overflow. Both use plain += (bpf.c:76-84,93-95). Both were zero in every run (rq4/results-full-record-device-buffer-20260908.md:45-47).
- Host read: after the client exits, 32 whole-value lookups copy 10,741,613,056 B in a median 1.49 s, outside prefill timing (results:37-40).
- Cost (RTX 5090 pp512, 23,068,672 records): prefill loss 39.1% vs 99.1% for the host-mapped per-thread ring (results:8-14). The ring arm had automatic warp execution on and this arm had it off (results:28-35). A field-major layout lost 27.6%; adding warp-contiguous slot order lost 12.9% (rq4/results-full-record-soa-warp-20260909.md:10-14). Footprint did not shrink: 10.7 GB vs an 11.8 GB ring segment for 1.85 GB of payload (notes:237).
- Gives up: memory = grid x records per thread. The slot order is tuned to one geometry (README.md:139-148). It is read after the run only.
- To package [inferred]: this is the per-thread ring moved to device memory with a hand-set thread count. Packaging needs values over 512 MiB or built-in banking, device memory the loader process can read (IPC again), and a bulk drain. It shows that placement and layout matter, not how to bound memory.

3. Neutrino (OSDI '25; local clone; numbers via notes)
- Layout and slot: a buffer per launch, grid x block x cap per thread or grid x warps x cap per warp (cuda.c:524-544). Each thread or warp owns a fixed segment, with no atomics (notes:3). cap is a constant or "dynamic": the whole kernel first runs with a counting probe and the maximum over threads is used (cuda.c:495-520). The code says this breaks kernels that modify their inputs in place (cuda.c:487-489).
- When full: there is no bounds check. An overrun writes into the neighbour's segment or past the buffer: silent corruption, not a counted drop (notes:3,10).
- Host read: after each launch: cuMemcpyDtoH, fwrite to a file, cuMemFree (cuda.c:619-621,655-660).
- Cost (A100 per the artifact appendix, notes:198): kernel-only slowdown 1.04x on average for light probes, 7.12x for the per-access memory trace (notes:7). Exposed latency on Conv2D: per-thread maps 5.45x/5.32x, of which copy-back, write and free take about 4.0x kernel time; per-warp 1.36x, of which about 0.3x (notes:9,203).
  - Footprint is computed by formula, not measured (notes:199): for Llama3-8B BS256, 64 MB at 8 B per thread and 5.50 GB for the memory trace (notes:4).
  - The counting run was never timed (notes:10).
- Gives up: memory and copy-back grow with the grid, each launch waits for its copy-back, and overflow corrupts.
- To package [inferred]: bpftime's agent sees each launch, but maps are created by the syscall server before any launch, in another process (RD:36-38). A per-launch map needs allocation and copy-out in the agent at launch time, plus a defined lifetime for the bpf() reader. Per-thread copy-out is the part Neutrino found dominant.

4. NVBit channel (v1.8.1 aarch64 release, local)
- Layout and slot: one byte buffer, 1 MiB of mapped pinned host memory by default, or device memory with USE_ASYNC_STREAM (channel.hpp:283-290; notes:36). Whichever lane calls push reserves bytes with fetch_add on head_bytes, copies its packet and adds to tail_bytes (channel.hpp:96-137). Per-warp 288 B records (the first active lane, after shuffles) and one channel per context are how the mem_trace tool uses it, not properties of the core (notes:31,273).
- When full: it blocks and never drops. The producer that crosses the end waits for earlier writers, rings a host-visible doorbell and spins until the host clears it. Producers that arrive meanwhile spin (channel.hpp:118-127,196-205). Warps that do not push keep running (notes:273). USE_ASYNC_STREAM can deadlock (notes:37).
- Host read: during the run, a host thread drains each announced prefix (channel.hpp:55-75).
- Cost: no measurement isolates the channel (notes:38). Whole tool, RTX 5090 pp512: per-thread channel records lost 99.62% of prefill (notes:140); NVBit writing gpu_ext's per-warp device array lost 9.56% (notes:233). Granularity changed between the two as well. Footprint 1 MiB plus a 1 MiB host receive buffer (notes:36).
- Gives up: GPU progress depends on the host thread; it is loss-free only by stalling.
- To package [inferred]: blocking breaks the eBPF helper contract (bpf ringbuf returns NULL instead) and deadlocks if the reader waits on the GPU. bpftime's existing GPU-to-host path is already serialized (RD:163-167). The reusable piece is the non-blocking part: byte reservation, then refuse and count when the round is sealed. Its default placement is the memory the Orin's GPU does not cache (RD:214-217).

5. hip-analyzer chunk allocator (AMD MI210; unpublished thesis, via notes)
- Layout and slot: one buffer per stream (or per CU, chosen from HW_ID) of 2^20 sub-buffers x 4 KiB = 4 GiB per stream; this is a stated configuration, not a measurement (notes:88,262). A wavefront takes a whole chunk with one atomicAdd on a registry counter, then writes events at an offset held in a scalar register (notes:88). A 4 KiB chunk holds at most 170 events (notes:262).
- When full: the chunk index wraps modulo the count with no device-side check, so old chunks are overwritten. The host infers overwrites from the index delta at kernel end. No overwrite rate is reported (notes:88,95,245).
- Host read: at kernel boundaries (a CDNA2 assumption, notes:250).
- Cost (kernel-only, HeCBench, traces discarded): median 1.33x, vs 1.96x for one global atomic per event and 1.50x for exactly pre-sized per-wavefront buffers (notes:89). Worst kernel 7.78x vs 32.08x, and the means cover different kernel subsets (notes:261). Below 256 waves an event costs about 115 vs 290 ticks, and the gain levels off at 4 KiB chunks (notes:91-92,242). Fewer events help the chunk schemes little, because each wavefront pays for a chunk anyway (notes:94).
- Gives up: memory (half-empty chunks were not studied, notes:95); overwrites are not counted on the device; no drain in the middle of a kernel.
- To package [inferred]: the chunk cursor lives in a register across events. An eBPF probe keeps nothing between invocations except map memory, so the runtime would need per-warp (or per-hardware-slot) cursors in memory, retirement of partial chunks at kernel exit, and overwrite/drop counters.

6. KPerfIR / Proton [inferred throughout: read only via documentation summaries, notes:111-115,252]
- Layout and slot: per-warp-group slices of a shared-memory buffer; offsets are fixed at compile time, with no atomics.
- When full: the buffer is circular and the oldest records are overwritten (the default), or it flushes to global memory. Losses are not counted.
- Host read: copied to global scratch memory at kernel end and read after the kernel.
- Cost: about 8% on average and at most 15% of kernel latency (H100, MI300). Shared memory is typically 1-4 KB (a statement, not a measurement).
- Gives up: completeness (only the latest records survive); it needs the compiler to reserve shared memory.
- To package: bpftime rewrites the PTX of already-compiled kernels, and reserving shared memory there changes occupancy. "Overwrite oldest" could be a map flag.

7. GT-Pin (Intel; API docs via notes)
- Layout and slot: a buffer mapped to both CPU and GPU.
  - Profile array: threads are hashed by thread ID into a power-of-two number of buckets in [1, max hardware threads], each bucket a non-interleaved chunk, "to minimize contention" (notes:46,296).
  - Trace: maxTraceSize bytes, maxThreads (default 0x1000) concurrent writers.
- When full: the trace is truncated, IsTruncated() returns true and the generated code sets flag f0.0 (notes:296). Profile-array overflow is not documented.
- Host read: after the kernel or at sync points (notes:46).
- Cost: no number isolates the buffer. Whole-run 2-10x while collecting a profile (notes:47,275). HPCToolkit goes from 1.81x to 2.23x when GT-Pin block counts are added (Intel P580; notes:49,282). Footprint is not published (notes:53).
- Gives up: thread identity inside a bucket; Intel only.
- To package [inferred]: this is the closest to per-CPU map semantics, with buckets bounded by hardware threads. It needs a stable bucket id (%smid and %warpid can change after preemption, RD:140-141), or a hash with contention inside each bucket.

8. CUPTI activity buffers (CUDA 13.2 headers on this board)
- Layout: records written by the driver, one per kernel, memcpy or memset, not per thread. Device buffers of 3,200,000 B (about 100,000 records), up to 250 per context, reused once consumed: "memory footprint does not scale with the kernel count" (cupti_activity.h:9871-9879,9895,9942). That is up to about 800 MB per context [computed]. Buffers are in device memory by default since CUDA 12.0u1 (:9898-9899). The client hands CUPTI host buffers by callback.
- When full: small device buffers risk dropping timestamps (:9888-9890). Records with no client buffer (and CDP records) are dropped and counted, and the count resets when read (:10751-10760).
- Host read: during the run, through the buffer-completed callback; a periodic flush from another thread, and a forced flush at exit (:11066-11075). PC sampling, for comparison: each SM samples into a 512 MB hardware buffer and droppedSamples is counted (notes:55,280); it serializes kernels (notes:292).
- Cost: NVIDIA publishes no overhead or drop rates (notes:58).
- Gives up: per-thread data; it is bounded only because its volume is per launch.
- To package [inferred]: the consumer side (pooled fixed buffers, reuse, a drop count reset on read, periodic and forced flush) fits a ringbuf-like map. It says nothing about per-thread volume.

9. Linux bpf ringbuf (CPU; local L4T 6.8 tree)
- Layout and slot: one multi-producer buffer whose size is fixed at creation. Reserve takes a spinlock (trylock in NMI) and checks that producer - consumer stays at most size-1 (ringbuf.c:405-452).
- When full: reserve returns NULL (:449-452). This tree's ringbuf.c keeps no drop counter (no "drop" or "lost" in the file), so programs count drops themselves.
- Host read: during the run; the consumer mmaps the data and writes consumer_pos (:269-272).
- Cost (commit c97099b0 benchmark; platform not stated in notes): 12.05 M records/s with one producer (per-CPU perf buffer 0.93). Throughput falls from 10.9 M/s at one producer to about 2.0 M/s at 20-52 producers. With the consumer on the producer's CPU, 3.05 M/s was delivered and 3.5-3.9 M/s dropped (notes:183-186). No footprint numbers (notes:187).
- Gives up: completeness under overload, and throughput with many producers.
- To package [inferred]: bpftime's 1527 is this API split into one ring per thread index, which is why its memory grows with the grid (RD:31-34). One shared reservation point for the Orin's 12,288 resident threads (RD:41-42) is far beyond the 52 producers measured.

What none of them handles
- **Integrated GPU:** none of the nine measured buffer cost on one.
  - WarpGuard (NVBit-based, Jetson Orin Nano) ran per-SM rings in pinned mapped host memory: the lowest active lane writes, and a full ring is flushed to the CPU. Ring cost and size were never given. Beyond 1M records per launch it slowed down, a cause shared with its verifier (notes:116-120,248,256).
  - gpu_ext's device-memory results need 1503 plus CUDA IPC, which the Orin lacks (RD:209-219).
- **Unknown scale:** NVBit, CUPTI, hip-analyzer, KPerfIR and the bpf ringbuf cope by blocking, dropping or overwriting. None keeps memory bounded with no loss and no blocking without knowing the grid, and none gives exact per-thread counts at unknown scale. Neutrino sizes per launch; gpu_ext sizes from offline runs (a pp32 preflight lost 450,560 events, RD:71-73).
- **Concurrent kernels:** gpu_ext's buffers assume serial launches. NVBit's mem_trace uses one channel per context. bpftime's slots belong to a thread index that concurrent kernels share (RD:43).
- **CUDA graphs:** not addressed in what was read.
  - gpu_ext ran with graphs off (rq4/onevalue-array-candidate/build-and-measurement.md:56).
  - NVBit has a graph-node launch callback (nvbit.h:310-314).
  - A grep of the whole Neutrino clone finds no cuGraphLaunch hook [inferred: graph-launched kernels get no buffer].
- **A reader in another process:** the non-bpftime tools read in the instrumented process [inferred]. gpu_ext crosses processes with CUDA IPC (device-array-runtime-notes.md:37-39), which the Orin lacks. bpftime's reader is the loader (RD:218-219).
- **A standard map API:** only gpu_ext reads a GPU buffer through bpf_map_lookup_elem, as one whole value after the run. None drains device memory during a run through a map API.
- **Loss counts under concurrency:** gpu_ext's counters are non-atomic, Neutrino has none, and hip-analyzer infers losses afterwards. Only CUPTI and the gpu_ext authors' ring (four reasons, per their audit) count them.

Differences from RD's table (RD:84-90)
- NVBit: reservation is a byte fetch_add per push call; per-warp records and per-context channels are mem_trace's choices.
- GT-Pin: threads are hashed into at most max-hardware-thread buckets, not one bucket per hardware thread.
- CUPTI: buffers hold per-launch driver records, pooled and reused; drops are counted when the client has supplied no buffer.
- hip-analyzer: when full it overwrites without a device-side count, and the host infers losses at kernel end.
- Neutrino: overflow corrupts neighbouring data, unchecked.

Disclosure: one `git grep` on the blobless gpu_ext clone fetched blobs and triggered auto-gc before I stopped it. HEAD is still 50a51ad4, and no tracked file changed (git status clean).