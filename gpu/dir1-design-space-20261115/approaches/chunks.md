CHUNK ALLOCATION: a bounded pool of fixed-size chunks. In each helper call, a warp's lowest active lane claims chunks with one system-scope atomic for the whole call. The CPU pops full chunks, and drops are counted when the pool is empty.
Paths: C = /home/jetson/src/bpftime-daily-state/worktrees/4708f4092d1e/candidate (dev 52126f33), R = /home/jetson/src/results/gpu, RD = origin/main:RESEARCH_DIRECTIONS.md. M n = item n of the brief's mechanism list (reader 1); S2 = reader 2's survey. File names in A, B, D and E are under C (full list in C).

A. Mechanism
- Type: a new GPU map type (e.g. 1537, i.e. 1527 + 10 by the host-memory convention, bpftime_shm.hpp:68,128-132).
  bpf(BPF_MAP_CREATE): key_size 0; value_size = chunk bytes = 32 + R x stride; max_entries = chunk
  count; map_extra = largest payload (bpftime_shm.hpp:38); stride = roundup(8 + payload, 8); R = 1..32.
- Storage: the syscall server creates it once in the segment, as for 1527 (map_handler.cpp:1374-1420).
  The device address is per pid, as in nv_gpu_ringbuf_map.cpp:25-54. Layout: a header (drop counters, S claim counters),
  one u32 state per chunk (FREE/WRITING/FULL), then the chunks. Each chunk is a 32-B header {count, smid, globaltimer}
  followed by R records {u64 size; payload}, 1527's format (gpu_helper_protocol.h:86-100). The grid never sizes it.
- MapBasicInfo keeps its 40 B (M1): max_entries = chunks, value_size = chunk bytes, and max_thread_count
  holds stride and R (get_gpu_map_max_thread_count, map_handler.cpp:1625-1665).
- Writer: bpf_perf_event_output (helper 25), branching on map_type (default_trampoline.cu:433-437).
  mask = activemask (:30-35); leader = lowest lane; rank = popc(mask below the lane); n = popc(mask).
  The leader claims ceil(n/R) chunks. SM s (= %smid mod S) takes chunks s, s+S, s+2S, ... from its
  counter (atomicAdd_system), then does atomicCAS_system FREE->WRITING there or on the next k = 4 chunks.
  shfl.sync broadcasts the address (legal, M27); lanes write at their rank; bar.warp.sync (:36-39); the
  leader writes the header, issues membar.sys and stores FULL. A wrong or moved %smid costs a failed CAS, not
  correctness (M14). Nothing is kept after the call: no partial chunk outlives a kernel, and no launch hook is needed.
- Full: no FREE chunk after k + 1 tries -> atomicAdd_system(drops, n records); every lane returns 2, as
  1527 does (:424-428). A payload larger than map_extra is counted separately and returns 3.
- CPU: BPF_MAP_LOOKUP_AND_DELETE_ELEM with no key (syscall_context.cpp:650-667, then a new case beside QUEUE's
  pop at map_handler.cpp:1829-1831) pops one FULL chunk. It scans the states round-robin, acquire-loads, copies, writes
  the drop totals into the copy's header and release-stores FREE; ENOENT if none is full. LOOKUP peeks; UPDATE and
  DELETE return ENOTSUP. libbpf's size checks pass (key 0, value = chunk; libbpf.c:10933-10987). The
  bpftime-only dlsym poll (kernelretsnoop.c:86-99) gets a per-record branch, so a 1527 tool switches by changing the
  map type. A chunk left WRITING by a killed writer is freed only when no agent is attached (a timeout would race a preempted warp).

B. Per use (budget example: 64 MiB of chunks; 32-B payload -> stride 40 B, R=32 chunk 1,312 B, R=1 chunk 72 B)

| use | memory bound (formula; llama.cpp's 4.19M-thread launch) | loss | GPU cost on the Orin [inferred] | CUDA graph launches | concurrent kernels | CPU view through bpf() |
|---|---|---|---|---|---|---|
| counters / histograms | Not a counter store: each update becomes a {key, delta} record (16 B, stride 24). Bound = max_entries x value_size, e.g. 64 MiB at any grid. One launch offers 100 MiB (every thread, R=32) or 7 MiB (one lane per warp, R=1; eBPF cannot aggregate itself, M28) | counted drops; totals exact only when the drop total is 0, otherwise a lower bound with a known missing count | worse than an in-place add: threadhist's add is 0.62 ns/thread (R/llama-census-20261113/README.md:93); a record adds 2 system atomics per warp call (>= 1.5x) | yes | yes | pop chunks, sum on the CPU; no lookup by key. Fallback: an array with atomics (RD:120-121, tier 1) |
| per-thread scratch, entry to exit | NOT SERVED: no lookup by thread, and nothing is kept between firings. Fallback: an entry record and an exit record per thread, joined on the CPU: 328 MiB offered per 4.19M launch (2 x 131,072 chunks of 1,312 B) against the fixed budget | counted; an unmatched entry or exit is visible | 2 stream records per thread (about 2x the row below) | yes | the join is ambiguous across concurrent launches: no launch id (no %gridid helper, M13) | pop + CPU join |
| event / record streams | max_entries x value_size + 4 B x max_entries + header, fixed: 64 MiB = 51,150 chunks at 256 or 4.19M threads alike. The 4.19M launch offers 164 MiB per-thread (131,072 x 1,312 B; 61% dropped if nothing pops during it) or 9 MiB one-per-warp (R=1) | counted per record (pool full / too large); every popped chunk carries the totals | per warp call: 2 system atomics + membar.sys + coalesced stores in uncached host memory. I estimate 1.5-3x threadhist's ~20 ns per warp (0.62 x 32), i.e. 30-60 ns. Warp leader on RoPE-K only (360,448 calls per prefill, S2): 11-22 ms, about 5-11% of 204.6 ms. Per thread on every kernel: >= +90%, and 128 MiB fills in 4.5 ms (S2) against a 10 ms poll, so most records drop | yes: the pool address is in map_info from module load (M4); replays need no hook, so the gaps in M8-M10 do not matter | yes: claims are per call with no per-launch state; records interleave and carry no launch id | LOOKUP_AND_DELETE pops one chunk (QUEUE-like), LOOKUP peeks; per-record callback through the bpftime-only poll; no epoll (M20) |

C. Implementation size
- C/attach/nv_attach_impl/trampoline/default_trampoline.cu: helper-25 branch ~100 lines; trampoline_ptx.h regenerated.
- .../trampoline/gpu_helper_protocol.h: layout + static_asserts ~35.
- New C/runtime/src/bpf_map/gpu/nv_gpu_chunk_pool_map.{hpp,cpp} ~250: layout, per-pid device address, pop/peek, per-record drain, drop totals.
- C/runtime/src/handler/map_handler.cpp ~60, in the switches at :135, :441, :773, :1374, :1475, :1625, :1666, :1797.
- C/runtime/include/bpftime_shm.hpp (enum) and C/runtime/src/bpftime_shm.cpp:806 (poll dispatch) ~20.
- Total ~470 lines, plus ~300 of tests: CPU threads as writers on the same layout, a CUDA stress test on the board, a kernelretsnoop variant.
- Launch hooks, ptxpass and the JIT are untouched.
- The tool author must still:
  - choose the budget (max_entries) and an R that matches how many lanes emit per call (32 per-thread, 1 warp-leader);
  - put identity in every record (block, thread, globaltimer; there is no launch id);
  - pop faster than budget / offered bytes per second, and check the drops;
  - sort across chunks by globaltimer when order matters;
  - do sums and entry/exit joins on the CPU;
  - enlarge the segment (128 MiB default, bpftime_config.hpp:81; BPFTIME_SHM_MEMORY_MB up to 10240, bpftime_config.cpp:119-120).

D. Unverified assumptions, and the step-0 check for each
1. A CPU release-store of FREE is seen by a later GPU atomicCAS_system, and payload stores + membar.sys + the FULL store
   reach the CPU's acquire-load in order. Tested so far: GPU-only adds, CPU vs GPU adds on one word (lost 0), and a GPU-only
   CAS lock (R/feasibility-20261109/gpu-env/hostreg.atom-dual.txt:6-8). CPU stores against GPU CAS have not been tested.
   Check: 8 x 1,536 threads emit tagged records while a CPU thread pops. Each tag must appear once, checksums must match, and delivered + dropped must equal offered, over >= 1e8 records.
2. shfl.sync and bar.warp.sync inside a __noinline__ trampoline helper under partial masks (divergent exits, __memcapture,
   M29); the trampoline has no shfl today (M25). Check: a test kernel with data-dependent early returns; compare the per-lane records.
3. The per-call cost of system atomics + membar.sys in uncached registered memory against cuMemAlloc memory; not measured on the Orin
   (M19). Check: a full-occupancy microbenchmark comparing the pool in the segment, the same code in device memory, and 1527; then RoPE-K-only
   and every-kernel probes on llama.cpp pp512. All of B's cost column depends on this.
4. Claim-counter contention: about 48 resident warps per SM on one host-memory word [inferred]. Known only: 2,048,000 GPU
   atomicAdd_system on one word took 67.3 ms beside CPU adds, about 30M/s (hostreg.atom-dual.txt:7; hostreg.cu:438-441, 472-496)
   [arith]. Spinning on a host word turned a 10 us host call into 300 us (default_trampoline.cu:118-121). Check: S = 1, 8 and 64,
   and per-module device-memory counters (the CPU never reads them, M7).
5. %nsmid, to size S (only a hint). Check: read it under a full grid.
6. The number of active lanes at helper 25 on real kernels, which sets R and the waste. Check: record popc(activemask) on 4 llama.cpp kernels via 1512.
7. Pop throughput through intercepted bpf() in the loader, which sets the drop rate. Check: a pop loop over a pre-filled pool, in bytes/s.
8. Whether a 1 GiB segment registers and prefaults (bpftime_shm_internal.cpp:1414-1469). Check: start the agent with BPFTIME_SHM_MEMORY_MB=1024.

E. Where it loses
- To hip-analyzer itself: hip-analyzer spreads one claim over a wavefront's successive events, keeping its write position in a register. Here a claim covers only the lanes of
  one call, because a probe keeps nothing between firings. For __memcapture (many events per warp), a write position kept per hardware slot would spread claims better,
  but it needs state between firings and a (smid, warpid) that can be trusted (M14, M16).
- To hand-written per-warp slots: gpu_ext's onevalue buffer (plain stores, no atomics, device memory; 5.4-5.6% on a 5090, R/buffer-survey-20261111/notes.txt:139)
  is cheaper when the geometry is known and launches are serial. The pool pays 2 system atomics per call in uncached host memory,
  the placement that cost 9.4x in a 5090 update microbenchmark (notes.txt:235).
- To per-SM or per-warp-slot rings (RD:122-123): rings keep time order per slot and need no claim step. The pool has no global order (sort
  by globaltimer). It shares the budget across modules and processes, but across SMs only within the k probes.
- To counters: an array with warp-aggregated atomics is exact, fixed-size and needs no CPU work. Here sums are exact only with zero
  drops, and every update costs record bytes.
- Per-thread scratch is not served at all.
- A fixed R wastes space when fewer lanes emit: a warp-leader record in an R = 32 chunk uses 1/32 of it. R is the tool author's guess.
- To standard consumers: libbpf's ring_buffer__add requires BPF_MAP_TYPE_RINGBUF and a byte ring mapped twice
  (C/third_party/libbpf/src/ringbuf.c:96-99, 136-150), and epoll is wired only for RINGBUF (syscall_context.cpp:1150-1157).
  So there is no ring_buffer__poll and no epoll; popping takes one bpf() call per chunk.
- Under overload (per-thread records on every kernel, 30-198 GB/s offered, S2), most records drop, and each failed call still pays k + 1
  atomics. NVBit's blocking channel loses nothing but stalls the GPU.
- A chunk left WRITING by a killed writer stays unusable until no agent is attached.