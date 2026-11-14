Design: storage indexed by hardware position (SM, resident warp slot, lane), plus one record ring per SM.
Paths: code = dev 52126f33 at /home/jetson/src/bpftime-daily-state/worktrees/4708f4092d1e/candidate, T = attach/nv_attach_impl/trampoline/default_trampoline.cu, R = /home/jetson/src/results/gpu, README = R/llama-census-20261113/README.md, RD = origin/main:RESEARCH_DIRECTIONS.md. "(r1)/(r2)/(r3)" = a reader's fact I did not reopen. [inferred] = my reasoning.

A. Mechanism
- Sizes come from the device, never the grid: the syscall server (it already opens a CUDA context for 1527, runtime/src/handler/map_handler.cpp:1374-1387) reads S = SM count and W = max threads per SM / 32 (Orin 8 and 48, R/feasibility-20261109/gpu-env/attrs.out.txt:4). An env override can raise them; %nsmid/%nwarpid are readable only on the device, so the device folds its ids (s = %smid mod S, w = %warpid mod W). A larger id then shares a slot instead of missing; that %smid < 8 and %warpid < 48 is [inferred].
- Preemption: %smid/%warpid "may change" and are documented unique only within a CTA (r1 item 14). So counters stay exact through atomics, the ring uses the SM id only to spread contention, and scratch detects a move with an owner tag and counts it.
- Counters/histograms: the server allocates them at map creation in the segment (registered host memory, as 1512), layout [key][s][w][lane]. That is one copy per lane of each resident warp slot, the GPU form of one copy per CPU. Lookup returns the lane's copy; adjacent lanes are adjacent, so a warp's writes coalesce.
  - Exactness comes from the tool adding with __sync_fetch_and_add (relaxed atomicrmw, vm/llvm-jit/src/compiler_utils.cpp:376-388; atom.add.u64 per r1 item 24). The copies only spread the atomics.
  - bpf() lookup of key k returns the element-wise u64 sum over S x W x 32 copies (value_size bytes; value_size % 8 == 0 is checked at creation). An update from the CPU writes copy 0 and zeroes the others.
- Scratch, entry to exit: the agent appends a .global array to each patched module's PTX at patch time, sized S x W x 32 x {16-B owner tag, value}; the driver allocates it at load, as today's __device__ globals (T:122, 127).
  - One array per module suffices: a kernel's entry and exit probes are in the same module. The CPU never reads it, so device memory needs no IPC.
  - Owner tag = (%gridid, linear global thread id), read inside the helper. Update (entry) writes the value, then the tag; lookup (exit) returns the entry if the tag is the caller's, else NULL and a counted miss.
  - This fits host_map_test unchanged: update at entry (example/gpu/host_map_test/host_map_test.bpf.c:95), lookup with a NULL check at exit (:127).
- Records: the server allocates R = S rings in the segment, and a warp writes to ring smid mod R. Each ring is a 64-B header {reserve, head, drops by reason} plus N slots of 8 + value_size bytes. A slot's 8-B word holds the size and the lap number: how many times the ring had wrapped when the slot was claimed, plus one.
- Helper 25 (perf event output): (1) the __activemask() lanes elect the lowest lane; (2) it reads head and claims n = popc(mask) slots with a CAS loop on reserve (a failed CAS means another warp claimed); (3) if the ring is full it adds n to the full-drop count and every lane gets 2; (4) it broadcasts the base with shfl, each lane writes record base + rank, bar.warp.sync, the leader issues one membar.sys, then each lane writes its lap word. This replaces the per-thread busy flag, which drops on any collision (T:454-459).
- Drain: the existing bpftime-only poll (example/gpu/kernelretsnoop/kernelretsnoop.c:86-99) walks R rings instead of thread_count headers (runtime/src/bpf_map/gpu/nv_gpu_ringbuf_map.cpp:56-104). It reads a slot only when its lap word matches, then releases head.
  - Drop counts go to a lost-records callback on the poll and to lookup of key 0, which is ENOTSUP today (:110-115).
  - A claimed slot that is not yet published stalls its ring, with no skip (a skip would let a late writer land under a newer claim). The reader reports a stall it notices.
- No host action per launch; map_info stays as written at module load. MapBasicInfo has no spare field (T:84-92): max_thread_count carries S and W packed (32 bits each) or R. extra_buffer in a per-module copy of map_info carries the scratch array's address (cuLibraryGetGlobal).
- Files:
  - T: helpers 1, 2, 3 and 25 get type branches, no new helper ids, so attach/nv_attach_impl/pass/ptxpass_core/src/core.cpp:235-253 is unchanged; trampoline_ptx.h is regenerated.
  - nv_attach_impl.cpp: write_trampoline_globals (2366-2385) and the late path in nv_attach_fatbin_record.cpp (283-292, r1 item 4) handle the scratch array and per-module map_info.
  - runtime/include/bpftime_shm.hpp: three map type numbers.
  - runtime/src/bpf_map/gpu/: new storage classes.
  - map_handler.cpp: creation, get_userspace_value_size, slot count.
  - syscall_context.cpp: poll with lost-records callback.

B. The three uses
| use | memory bound (formula; Orin, largest launch 4.19M threads) | loss | GPU cost on the Orin [inferred] | CUDA graph launches | concurrent kernels | CPU view through bpf() |
|---|---|---|---|---|---|---|
| counters / histograms | S x W x 32 x value_size x max_entries. 12,288 copies [8 x 48 from attributes; larger ids fold] = 96 KiB per u64 entry, for any grid; 1512 needs 32 MiB at 4.19M (RD:53-54) | Exact with atomic adds. Plain `*cnt += 1` (example/gpu/threadhist/threadhist.bpf.c:21-29) loses counts silently when two resident warps share (s, w): fold, preemption, or a CTA-local %warpid. Never out of range | One uncached atomic per lane per update, plus the lookup's membar.sys, which can be dropped because only bpf() update writes from the CPU. 1512's plain write costs 0.6 ns per thread execution (README:93) and +58% if on every kernel (:95). Expect the same order or more: an atomic in uncached memory is unmeasured, and device memory (9.4x cheaper on the 5090, r3) cannot be read by another process here | yes: nothing per launch | yes: shared copies stay exact with atomics | lookup(k) returns the sum, value_size bytes, so unmodified libbpf works. Only for additive u64 words: maxima, minima and timestamps cannot be summed; a per-copy view would need its own size, as 1512 today |
| per-thread scratch, entry to exit | S x W x 32 x (16 + value_size x max_entries) per patched module, in device memory: 12,288 x 24 B = 288 KiB for one u64; 2.8 MiB for llama.cpp's 10 modules; grid-independent | Exact while the thread keeps its (smid, warpid). A thread moved between entry and exit gets NULL, counted. Ids folded past S or W, or a CTA-local %warpid, make resident warps share an entry: counted NULL at best, and two warps writing one entry at once can leave A's tag with B's value, which is silent [inferred]. If %warpid is CTA-local, up to 16 blocks per SM (attrs.out.txt:4) collide and this row fails | One 24-B store at entry; one 24-B load and compare at exit, in GPU-cached memory. Well below the host-memory paths, unmeasured | yes if %gridid differs per replay (unverified); otherwise the tag is weaker but entry always rewrites it | yes if (smid, warpid) is exclusive among resident warps of different kernels (unverified) | none needed. The miss count goes to the log and the map's stats |
| event / record streams | R x (64 + N x (8 + value_size)), a budget set at creation. With kernelretsnoop's 1,032-B slot (r2): 8 x 8,192 = 64.5 MiB, against 64.5 GiB for 16 per-thread records x 4.19M threads. At 40-B slots, 64 MiB holds 1.68M records | Counted drops by reason (full, too large) reach the tool; none silent. A writer that never publishes (killed kernel) stalls its ring. Per 10-ms poll, decode offers 773K warp-leader records (29.5 MiB at 40 B) and prefill 294K. Per-thread records on every kernel offer 24.7M per poll (0.92 GiB), so nearly all are counted drops. At 1,032-B slots, 65,536 slots fill in 0.85 ms in decode and 2.2 ms in prefill (rates from r2 sec. 2) | Per warp call: one uncached head load, at least one uncached CAS (up to 48 warps per SM on one word), coalesced payload stores, and one membar.sys. Today each lane pays a CAS, membar.gl, membar.sys and atomicExch (T:454-482), so a full warp does 32x fewer fences. The CAS cost under contention is unknown | yes | yes: multi-producer reservation | The existing poll symbol, now over R rings, with a lost-records callback and lookup(0) for drop counts. Order is per ring only; a global order means merging by globaltimer |

C. Implementation size (rough) and the tool author's remaining work
- Device side:
  - T: about 250 lines (slot index and fold, three type branches, tag check, ring reservation, publication);
  - nv_attach_impl.cpp plus PTX assembly: about 80 (append the scratch .global, write map_info per module).
- Runtime:
  - runtime/src/bpf_map/gpu/: about 400 (summed copies about 120, rings with drain and stats about 220, scratch stub about 40);
  - map_handler.cpp: about 120;
  - bpftime_shm.hpp: about 10;
  - syscall_context.cpp: about 30.
- Tests: about 250; ports of threadhist and kernelretsnoop: about 150. About 1,300 lines in all.
- Still by hand:
  - choose the map kind per use;
  - add with __sync_fetch_and_add;
  - pick ring slots N from the expected rate x poll interval and keep value_size small;
  - put ids and globaltimer in each record and merge the rings;
  - check NULL at exit.
  Per-logical-thread outputs stay on 1512, sized by hand.

D. Unverified assumptions, with the step-0 check for each
1. %smid < 8, %warpid < 48 and both contiguous. Check: a kernel at full occupancy (8 x 1,536 threads, block sizes 32-1,024) logs %nsmid, %nwarpid, %smid and %warpid per warp into device memory.
2. %warpid is a hardware slot, not CTA-local. A one-warp block read 0-3 across launches (R/fixes-20261109/protocol/runs/san1-ts-legacy/server.log:79-82, 88-97), but no two resident warps were ever compared. Check: every warp logs (smid, warpid, gridid, ctaid, globaltimer at start and end) across two streams. Expect no time-overlapping pair to share (smid, warpid).
3. Ids do not change between entry and exit. Check: the same log under a second GPU process that forces time-slicing (computePreemptionSupported=1, attrs.out.txt:14). Compare the ids at entry and at exit.
4. %gridid is unique per launch across modules and graph replays. Check: log it in a captured graph replayed 1,000 times plus two modules.
5. Cost. Check, on registered host vs device memory: per-lane plain add vs atom.add.u64; a 64-bit CAS on one word by 1 to 48 warps per SM; with and without membar.sys.
6. Publishing other lanes' stores after bar.warp.sync plus one leader membar.sys. Check: a litmus run where the CPU acquire-reads the lap word, then the payload, for 10^8 rounds, counting stale payloads. If it fails, each lane needs its own membar.sys, today's cost.
7. A .global array appended to a patched llama.cpp module loads and cuLibraryGetGlobal finds it.
8. End to end: threadhist with an atomic add on rms_norm must sum to 11,273,216 per prefill (README:76). Compare its cost against 1512's 3.3% (README:90).

E. Where it loses
- Per-logical-thread results (15 class-(c) uses, r2: threadhist as written, thread_mapping, cross-layer sequence numbers) cannot be expressed: a copy holds every thread that ever ran there. Only the llama.cpp port, which sums, fits. Per-launch storage or a hand-sized 1512 still wins here.
- Uses that need the value a fetch-add returns (gpu_ext launchlate device_state, r2) need one shared counter, as 1513 with atomics, not sharded copies.
- Cost is probably not lower than today's 1512 for counters: one uncached operation per lane, now atomic. The 5.4-5.6% hand-written per-warp buffer had one writer per slot, no atomics and device memory (r3). The rings add a cross-warp atomic in uncached memory. Device memory, the real cost lever, is closed to CPU-read storage on the Orin without IPC (r1 item 21).
- Bounded rings cannot carry per-thread streams on every kernel: about 1 GiB per poll is offered and nearly all of it is dropped, counted. NVBit-style blocking would not drop, but it stalls the GPU.
- Records are ordered only within one ring; global order needs a timestamp merge. A claimed but unpublished slot stalls its ring until the writer publishes.
- Scratch and plain-add counters depend on undocumented behaviour, namely that (smid, warpid) is exclusive among resident warps. If step 0 refutes it, the scratch row fails. A per-thread .local variable allocated by the PTX pass, if the probe ABI can pass its address [inferred], would not depend on it.
- Summing on read gives no per-SM or per-warp distribution. Tools that want one key by %smid themselves, as threadscheduling does.

Disclosure: this report was drafted in /tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/hwpos/report.txt (scratchpad only). The arithmetic was done with python3 -I one-liners. No project file was edited, and nothing ran on the GPU.