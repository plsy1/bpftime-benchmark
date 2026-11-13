# Fused call sites and merge-on-read overlays: per-call-site execution of GPU eBPF helpers (performance and code generation first)

All code citations are dev `52126f33` (`dev/uprobe-official-4708f4092d1e`, worktree
`/home/jetson/src/bpftime-daily-state/worktrees/4708f4092d1e/candidate`); llvmbpf line numbers are
the clean submodule `vm/llvm-jit` at 87bac39a (before the parent's patch overlay). "[inferred]" marks
a claim derived from code or reasoning but not run; "[estimate]" marks a projected number; every
measured number names its results directory under `/home/jetson/src/results/gpu/`.

## Overview

**Problem.** A GPU probe's helper call on a CPU map (HASH, 1501 on the Orin, printk, every
non-ARRAY delete) is a synchronous host call: one request slot per shared segment, lanes of a warp
served one at a time, about 9-12 us per call back to back on the Orin, 14-70 us for an isolated
call, and the whole GPU serialized on that slot (`default_trampoline.cu:129-175`). Cost grows as
threads x calls. The same protocol also loses counts: the counter idiom's first-touch inserts
collide and a plain read-modify-write through a shared pointer collapses to one update per warp.

**Design in one paragraph.** A load-time analyzer in the agent classifies every helper call site
and recognises multi-instruction idioms as single operations (add-or-create, add-if-exists,
last-writer-wins store, tombstone, read-only read, printk record). Each admitted idiom is rewritten
*in the bytecode* into one call of a private fused helper id that carries a site id in a free
argument, padded with no-ops so jump offsets do not move; the decision therefore survives O3,
memcapture's pc shift and the missing pc in IR. Code generation then has two tiers: tier 0 is a
generic fused helper in the trampoline (warp grouping with `match.any.sync`, lane sums with
`popc` or `redux.sync`, one posted `red.add` per group into a per-map table); tier 1 is an IR pass
added to llvmbpf's `generate_ptx` through the patch overlay that inlines the fused call with the
site's constants (delta, key kind, pre-resolved slot address). The effects land in a per-map
**overlay**: a table of pending GPU effects in the registered segment (Orin) or in device memory
(discrete GPU). An overlay word is written by the GPU or by the CPU, never by both, so no CPU-GPU
atomic is needed. Every CPU reader (the loader's syscall server, CPU probes in the agent, the
watcher) **merges** base map and overlay on each access, so a host read is current when it
happens, not at kernel completion. Anything the analyzer cannot prove keeps today's synchronous
call.

**Headline projections** (detailed in section P, grounded in measured numbers). "Today" is the
latest measured dev build, **aa6eb973** (set N of baseline-sub-20261111 and hostcall-l1-20261111).
52126f33 adds only the watcher changes 46f5a7cb and 52126f33 (AC3 policy, WFE opt-in), whose
measured cost in 32-call bursts is 11.8 us per call without load and 15.7 under full load
(watcher-policy-20261112/v3/README.md). The host-call-bound numbers at 52126f33 are therefore
expected within about 10-30% of these, and R10's baseline must be re-measured at 52126f33.

| case (Orin) | today (aa6eb973, measured) | projected | basis |
|---|---:|---:|---|
| CPU Hash map update / lookup / delete, 32 threads | 327.5 / 341.1 / 317.7 us per launch | about 49-52 us | CPU Array cases fell 320-371 -> 49-51 us when their host calls went (hostcall-l1-20261111) |
| CUDA Counter, 32 threads | 2,928.9 us per launch, `calls=97` | about 60-90 us, `calls=128` | 8 host calls per thread -> 0; 97 = 4 launches x 32 - 31 first-touch losses |
| same probes at 100,352 threads (large) | about 0.9 s per launch per site [estimate: 9 us x threads] | about 35-210 us per site | measured GPU-local cost of one uncached line at that geometry: 1513 lookup 34.6 us, update 211 us (baseline-sub-20261111) |

The CUDA Counter count is the exactness headline: the loader prints `calls=97` in every measured
run (baseline-sub-20261111/README.md "Checks"), which is 4 launches x 32 threads minus the 31
first-touch increments that today's protocol drops; the design's reference semantics demands 128.

## P. Where cycles go today, and what the design removes

### Measured costs (Orin Nano, jetson_clocks, GPU 1,020 MHz)

| quantity | value | source |
|---|---|---|
| host call, back to back (32-lane burst) | 9.2 us (WFE) to 12.4 us (AC); AC3, the rule 52126f33 ships, 11.8 us without load and 15.7 us under full load | watcher-policy-20261112/summary.md cell c06; v3/README.md cells c05/c06 |
| host call, CPU hash micro (32 threads x 1 call, aa6eb973) | (327.5 - 39.5) / 32 = 9.0 us | hostcall-l1-20261111/README.md |
| host call, CUDA Counter (32 threads x 8 calls, aa6eb973) | (2,928.9 - 39.5) / 256 = 11.3 us | baseline-sub-20261111/summary.md |
| isolated host call | 13.9 us (WFE) / 63-70 us (sleeping watcher, M100/A/AC; v3 found AC3 equal to AC in c02 within noise) median | watcher-policy-20261112/README.md (c02), v3/README.md item 5 |
| isolated host call under full CPU load | median about 1 ms with the pre-46f5a7cb watcher; AC 1.04% of calls > 1 ms | wfe-load-20261111/README.md; watcher-policy-20261112 (c01) |
| official's host call | about 1 ms (33.7 ms for 32 calls) | compare-20261110/README.md |
| probed launch, no map work (32 threads) | about +7-8 us over baseline | empty probe tiny 34.2 vs 27.4 us; "about 8 us remain over the baseline" (hostcall-l1) |
| GPU-local ARRAY op incl. membar.sys, 32 threads | lookup 49.2 us, update 51.0 us per launch (baseline 39.5): about 3-5 us per site above the probe | baseline-sub-20261111 |
| GPU-local map op, 100,352 threads, one uncached line | lookup +34.6 us, lookup+byte-wise update +211 us per launch | GPU Array map lookup/update (large) 65.9 / 242.4 vs 31.3 |
| same, 1,000,448 threads | +1,314 us (1,474 vs 159.8) | xlarge-20261110 (D) and baseline-sub |

Not measured: the split of the ~10 us host call into lock wait, byte-wise request copy over
uncached memory, the four `membar.sys`, watcher detection and service. Step 0 of R10 measures it
with `%globaltimer` stamps inside `host_call`.

### Cost structure

Today, for a launch of T threads with sites s reached by E_s lanes:
`t_launch ~ t_base + t_probe + sum_s E_s * c_rt`, with c_rt about 9-12 us, and the sum is
serialized across the whole GPU (one `CommSharedMem` per segment, `bpftime_shm_internal.cpp:1072-1079`;
lanes in turn, `default_trampoline.cu:143-171`). At 1,000,448 threads a cuda-counter-like probe with
8 host-call sites would take about 1,000,448 x 8 x 11.3 us = 90 s per launch [estimate, linear
extrapolation].

With the design: `t_launch ~ t_base + t_probe + sum_s g(W_s, K_s)`, where W_s is the number of warps
reaching s and K_s the distinct targets per warp; each group costs one or two posted reductions
plus at most one or two dependent uncached loads. g is bounded above by the measured GPU-local
costs in the table (35-211 us at 100k threads, 1.3 ms at 1M threads on one hot line), because a
fused site issues at most 2 posted reductions per group where the measured 1513 update issued 8
byte stores, 8 byte loads and 2 `membar.sys` per lane.

### Projected per-case numbers (all [estimate]; slice measurements in R10)

| case | today (aa6eb973, measured) | projected | reasoning |
|---|---:|---:|---|
| CPU Hash map update (minimal) | 327.5 | 49-52 | one LWW group store per warp; same floor as CPU Array update 51.0 |
| CPU Hash map lookup (minimal) | 341.1 | 48-50 | GPU-confined read; analog CPU Array lookup 49.2 |
| CPU Hash map delete (minimal) | 317.7 | 48-50 | deferred tombstone, pre-resolved slot (literal key 0) |
| CUDA Counter (minimal) | 2,928.9 | 60-90 | 8 fused sites x 3-5 us (ARRAY analog) on the probe path; printk drained off the critical path |
| hash counter micro, new mode (minimal / small / large) | about 330 us / 9.2 ms / 0.9 s [estimate from 9 us per call] | 49-52 us / 35-45 us / 70-250 us | 1 / 32 / 3,136 warps, 2 posted reductions per warp |
| GPU Array map update (xlarge), today GPU-local but racy | 1,474 | below 1,474 | lookup + `*v + 1` + update(ANY) recognized as add on an array: one `red.add` per warp instead of 16 byte operations per lane; also exact (today collapses) |

Speedups: 6-7x for one-call micro cases at 32 threads (the floor is the probed launch), 30-50x
for CUDA Counter at 32 threads, and 3-4 orders of magnitude once T is in the hundred thousands,
because the cost moves from threads x c_rt (serial) to warps x posted reductions (parallel).
printk-heavy probes at large T become bound by the CPU's line formatting in the drain (about 1-2 us
per line [inferred]) instead of by c_rt; they gain about 5-10x, not orders of magnitude.

## R1. Reference semantics

**Choice: SIS, serial idiom semantics.** It is per-operation sequential consistency in which the
unit of atomicity is the recognized idiom instead of the single helper call, and in which host reads
are linearized at the moment they read the overlay.

- **Units.** One unit is each of: a recognized idiom instance (shapes A, B and C of R4 become
  `add_or_create(map, k, d, init)`; the H lookup-and-increment becomes `add_if_exists(map, k, d)`);
  every other helper call; every `BPF_ATOMIC` instruction; every loader `bpf()` map command; every
  CPU-probe helper call. Plain loads and stores through value pointers outside recognized idioms
  are not units; they keep Linux's racy word-level semantics.
- **Exact.** An execution is exact if a total order of all units exists that is consistent with
  every thread's program order (GPU thread, CPU thread, loader thread), such that every value
  returned to any party, and the host-visible state at every observation point below, equal those
  of executing the units sequentially in that order with Linux's per-helper semantics (flags,
  EEXIST, ENOENT, E2BIG; `kernel/bpf/hashtab.c:1084-1113` in the L4T 6.8.12 tree).
- **What "exact" is measured against.** That serial order, not today's bpftime. Today's
  CUDA Counter `calls=97` is not exact under SIS (the order cannot lose increments of a unit); 128
  is the only exact answer. It is legal under Linux's own guarantee (S1, per-helper atomicity),
  which SIS refines.
- **Deferred units.** A deferred unit's place in the order can be later than the moment its helper
  returned on the GPU, at the point where its posted reduction or store is globally performed. The
  probe cannot observe the difference: the analyzer admits deferral only for results that are not
  used and only when the same program execution does not read the same map again before the effect
  is published (read-your-writes; R5 condition 4).
- **Observation points where host-visible state is guaranteed current.**
  - O1, every loader `BPF_MAP_LOOKUP_ELEM`, `GET_NEXT_KEY`, `LOOKUP_AND_DELETE_ELEM`, `UPDATE_ELEM`,
    `DELETE_ELEM`. These run in the loader process on the segment (`syscall_context.cpp:600-679`); with
    merge-on-read they include every GPU effect globally performed before the read of the overlay.
  - O2, every CPU-probe map helper call in the agent, and every host request the watcher serves.
  - O3, the application's completion of a kernel (stream or device synchronize): every effect of
    that kernel is included in any read that follows it causally. Effects are posted writes to
    mapped memory, which CUDA makes visible after synchronization [inferred: the standard rule for
    mapped pinned memory, not tested on this board].
  - O4, printk: a record is emitted no later than the watcher's next wake after it is published.
    The watcher sleeps 100 us while a noted probed launch may run and otherwise parks with a 1 ms
    backstop (`cuda_watcher_policy.hpp:26-27`). Records are also emitted at the next drain-first
    host request, at detach, and at normal process exit.
  - Not covered: loader reads through an ARRAY mmap (plain memory, no interception,
    `syscall_context.cpp:1036-1048`). ARRAY maps never get an overlay, so this read path stays
    as current as today.
- **Staleness bound.** O1 and O2: none beyond the store-propagation latency of the GPU's posted
  writes. printk: one watcher period plus drain time. No snapshot across keys (as Linux).

**Per idiom against Linux** (shapes as in BRIEF.md and R4):

| shape | Linux guarantees | SIS here | GPU value-returning reads see |
|---|---|---|---|
| A counter (lookup, NOEXIST insert, atomic add) | first-touch increments may be lost (`hashtab.c:1084-1089`) | **more**: every execution counted | - |
| B plain RMW + EXIST self-write, else NOEXIST | lost updates (plain race, element replacement) | **more**: every delta summed | - |
| C leader lookup + update(ANY) | race between concurrent launches | **more**: exact add | - |
| D entry update(ANY) / exit lookup + delete | last writer wins; reads through a pointer after delete see the old or a reused element | **same** (last writer by the serial order; the exit read is a snapshot at the lookup) | the latest LWW store in the order, or the merged host value when the map is CPU-dirty |
| E per-thread record update(ANY) | last writer per key; which new keys fit a full map follows execution order | **same**, except which keys fit follows the order of slot claims (a legal serial order; admission rule in R3) and can be conservative (over the bound the insert goes to the exact synchronous path) | - |
| F leader publish update(ANY) | last writer | **same** | - |
| G printk | per-CPU order, timestamp merge across CPUs, lines lost on overrun | **same** order class (per GPU thread program order, globaltimer merge per drain batch); **more** on loss (backpressure, no drop); **less** on real-time interleaving with host output (GPU lines appear up to one drain period later) | - |
| H benchmark hash ops | per helper | **same** | lookup returns NULL (nothing inserts key 0) |

Plain RMWs through a pointer that are not recognized (none of the 59 surveyed host-call sites) keep
the synchronous path and Linux's semantics.

## R2. What differs from gpu_ext and upstream automatic warp execution

| | gpu_ext (paper, ASPLOS '27 camera-ready) | upstream `official/revision/automatic-warp-execution` | this design |
|---|---|---|---|
| unit of decision | tool, rewritten by hand | whole program, yes/no, opt-in | each call site; idioms as one site |
| consistency | relaxed, eventual: shards merged into snapshots at kernel completion (arXiv 2512.12615v2 sections 4.4.3, 5.3) | claims observational equivalence; leader-only `*v = *v + 1` counts once per converged group | SIS (R1): exact against a serial order |
| when the host sees effects | after a merge at kernel completion by a daemon; never for kernels that do not complete | immediately (no deferral) | at every read: the reader merges base and overlay itself |
| how lanes are combined | warp leader executes | lowest active lane executes the handler | `match.any.sync` groups lanes by key at run time; the group's count is `popc(group)` and its delta `redux.sync` over the group, never "one per election" |
| merge rules | not stated (no sum vs last-writer rule, no host-write rule) | none (no deferral) | per operation algebra: add with create-if-absent (sum, count, constant correction), LWW with a version handshake, tombstone, conditional ops evaluated GPU-confined or synchronously |
| maps the host writes | no rule | rejected | CPU writes go to the base map and record offsets; GPU reads check a CPU-write sequence word |
| atomics, printk, CPU maps | tools avoid them | program rejected | supported per site |
| implemented | not implemented (gpu_ext's own audit, overlap-survey-20261113) | unmerged branch | design |

Concretely, cuda-counter has 11 host-call sites (callsite-survey records). Auto-warp admits the
program for none of them: it rejects any atomic (pc 28 of the entry probe), printk and CPU-map
updates (overlap-survey README). This design fuses all 11, and a program in which one site cannot be
proven still gets every other site fused.

Exact rather than eventual has four causes. Grouping counts lanes. A read merges at read time,
with no snapshot point. The merge rules follow the algebra of each operation and never drop or
approximate. Neither side writes the other's words, so a merge never races a writer it could undo.

## R3. "Only GPU probes touch this map" is decided at run time: overlays and merge-on-read

### The single-writer rule

Every word of the protocol is written by exactly one side: GPU-written words only by GPU threads
(GPU atomics among GPU threads), CPU-written words only by CPU parties (under the base map's stripe
lock, `bpftime_hash_map.hpp:715-745`). Plain aligned 64-bit reads across sides are allowed. This is
the pattern the 1527 ringbuf already uses between the GPU and the loader (tail written by the GPU,
head by the host, `nv_gpu_ringbuf_map.cpp:56-104`). It avoids three open questions: whether GPU
`atom.*` on registered memory is atomic against CPU atomics (hostNativeAtomicSupported is not
queried), whether the CPU can RMW device memory on a discrete GPU (it cannot), and whether the
loader may post into `CommSharedMem` (it may not; `occupy_flag` is GPU-atomics only,
`default_trampoline.cu:149-151`).

### Overlay layout (one per map that has fused GPU sites)

- **Header, CPU-written**: magic and version, map handler index, map generation `gen`,
  `cpu_write_seq` (incremented on every CPU-originated write of the base map, including a CPU
  probe's materialization), `sealed`, `dead`, capacity, the constant correction `corr` for
  add-or-create (R4), `base_keys_at_creation`, `cpu_new_keys` (CPU inserts of keys absent from the
  merged view), and a table of agents that may write the overlay (pid, device).
- **Header, GPU-written**: `gpu_claims` (slots claimed so far).
- **GPU part, per slot** (GPU-written): key (8 bytes inline, larger keys in a key arena), `state`
  (EMPTY -> CLAIMING -> READY by GPU CAS), `add.sum`, `add.count`, `lww.lock`, `lww.seq`,
  `lww.flags` (present or tombstone), `lww.sum_at_write`, `lww.value[value_size]`.
- **CPU part, per slot**, in a parallel array: `off_sum`, `off_count`, `seen_lww_seq`.
- Located in the segment on the Orin (registered with `cuMemHostRegister(DEVICEMAP|PORTABLE)` by
  every agent, `bpftime_shm_internal.cpp:1407-1474`, so it is device-reachable and readable by
  the loader without the agent), and in agent-owned device memory with an IPC handle on a discrete
  GPU (R8).

### Merged value of key k

- ADD: if the base has k: `base(k) + (sum - off_sum)`. Otherwise, if `count - off_count > 0`:
  `corr + (sum - off_sum)`. Otherwise k is absent.
- LWW: if `lww.seq > seen_lww_seq`, the GPU store is later in the order: value or absent
  (tombstone), plus `sum - lww.sum_at_write` if the map also takes adds. Otherwise the base
  decides.
- Why this is a valid serial order: a CPU write of k reads `sum`, `count` and `lww.seq` under the
  stripe lock and stores them as offsets. Every GPU unit globally performed before those reads is
  ordered before the CPU write; every later one after it. GPU units are totally ordered among
  themselves by their atomics on the slot. CPU writes are ordered by the lock, and the `seq` values
  they observe never decrease. So the merge is the last element of one total order (a version
  handshake between two single-writer registers).
- **Admission (max_entries, E2BIG).** A key that exists only in the overlay is admitted when its
  slot is claimed, and the decision never changes afterwards.
  - The claiming lane reads `cpu_new_keys` and increments `gpu_claims`. If
    `gpu_claims + base_keys_at_creation + cpu_new_keys > max_entries`, it releases the claim and
    sends the unit as a synchronous request. The host applies Linux's E2BIG rule on the merged
    state there, including room freed by deletes.
  - Every claimed slot is therefore a key the serial order admitted. GPU readers and CPU readers
    see the same membership, and no read ever shows more than max_entries keys.
  - The bound is conservative (deletes do not lower `gpu_claims` until a quiescent reclaim). That
    costs speed, never exactness.

### What each party does

| party | operation on a map with an overlay |
|---|---|
| GPU fused site | adds, LWW stores, tombstones into the overlay only; never writes the base, never holds a base pointer |
| GPU fused read (value used) | if `cpu_write_seq == 0` and not `sealed`/`dead`: merged value from the overlay alone (the base is empty or untouched since the overlay was created; created non-empty -> `cpu_write_seq` starts at 1), copied into the thread's private stack slot; else a synchronous `MAP_LOOKUP_MERGED` request |
| GPU conditional op (NOEXIST/EXIST outside an idiom, `add_if_exists`) | evaluated on the overlay when `cpu_write_seq == 0`, else a synchronous request of the same op |
| loader (syscall server, own process) | lookup: copy base value and add the overlay delta, under the stripe lock; update/delete: apply the flags against the merged presence, write the base, record offsets, `cpu_write_seq++`; get_next_key: base keys, then overlay-only keys whose merged presence holds; lookup_and_delete: read and delete under one stripe lock |
| CPU probe in the agent | the same merged operations (they share libbpftime's map layer); a lookup that returns a writable pointer first **materializes** k (`base(k) := merged(k)`, offsets := current GPU words, in place through `with_existing_value`, `bpftime_hash_map.hpp:699-709`) and bumps `cpu_write_seq`, since the CPU may now write the base through the pointer; safe because no GPU thread writes the base of an overlaid map |
| watcher serving a host request | the same merged operations; new request types `MAP_ADD_OR_CREATE`, `MAP_ADD_IF_EXISTS`, `MAP_LWW_STORE`, `MAP_LOOKUP_MERGED` execute the idiom-level unit on the host (R5) |

Because the merge lives in libbpftime's map layer (`bpftime_shm.cpp` / `map_handler.cpp`), the
loader's syscall server runs it in the loader process directly on the segment. No channel from the
loader to the agent is needed, and none is created.

### Map states (per map, dynamic)

- **PLAIN**: no overlay; every site uses today's helpers. This is the state of any map with a GPU
  site the analyzer cannot fuse.
- **OVERLAY**: created when every GPU site referencing the map, in every `cuda__` program in the
  agent's handler table, is fusible (the agent sees all program handlers at
  `init_attach_ctx_from_handlers`, `bpf_attach_ctx.cpp:190-285`, and can scan their LDDW
  immediates). Created before any patched module runs, so no GPU thread has written the base
  through a pointer.
- **SEALING -> PLAIN**: a GPU program attached later that touches an OVERLAY map with a site that
  cannot be fused. The agent sets `sealed`; fused sites check the header word they already load
  and take the synchronous request path from then on. The agent waits until no kernel launched
  before sealing can still run (watcher's per-stream event query, `probed_launch_activity.cpp:49-72`),
  materializes every key into the base, and only then instantiates the late link. If quiescence
  cannot be proven (the unseen-launch latch is set: graphs, unresolved CUDA 13 handles), the late
  link is deferred. A probe that attaches later is always legal; a lost or double-counted effect is
  not.

### The hazards the task lists

- **Loader reads run in another process directly on the segment**: the merge runs there (above).
- **The request slot is unsafe for CPU posters**: never used by the CPU; nothing new is posted
  into `CommSharedMem` from the host side.
- **Hash value pointers move on update/delete** (`replace_locked`, `bpftime_hash_map.hpp:528-538`;
  free list on delete, 755-781): the GPU never holds base pointers in OVERLAY; deferred effects are
  applied by key at read time, never through a captured pointer. Overlay slots never move; they are
  reclaimed only at quiescence.
- **Maps closed and fds reused** (`syscall_context.cpp:273-296`; `handler_manager.cpp:116-123`):
  the overlay is bound to (handler index, generation). Close marks it `dead`; fused sites see `dead`
  in the header word and drop their effect (the map no longer exists to any reader). Storage is
  freed only when no registered agent references it. A new map at the reused fd gets a new overlay,
  and the agent rewrites the per-module site table (below) when it instantiates the new link.
  Unfused sites keep today's fd-reuse hazard (`map_info` has no generation); this design does not
  change it.
- **`map_info` is a snapshot per module** (`nv_attach_impl.cpp:2366-2385`, never refreshed): fused
  sites do not use `map_info`. They read `__bpftime_site[site_id]`, a `__device__` (not
  `__constant__`) table in the trampoline that the agent writes at module load next to
  `__bpftime_pid_tgid` and can rewrite at any time with `cuMemcpyHtoD`.

### Reads that cannot be intercepted

- ARRAY mmap: arrays never get overlays. Their GPU path is direct since 1b3d9c99, and pointers
  stay put.
- `BPFTIME_RUN_WITH_KERNEL`: the server forwards map commands to the kernel
  (`syscall_context.cpp:603,629,642,653,671`). Overlays are disabled, so the map stays PLAIN.
- Batch commands fall through to the real `bpf()` (`syscall_context.cpp:852-854`) and fail today
  [inferred]. Unchanged; if they are implemented, they go through the merged operations.

## R4. Idioms as units

### Recognition (bytecode, in the agent)

A forward abstract interpretation over the eBPF CFG keeps, per pc, for each register and 8-byte
stack slot: kind (constant c, map fd f, stack pointer +off, map-value pointer from call site s with
nullness, scalar), the defining pc, and a **construction class**. The class is used for reporting
and for one shortcut (skipping `match.any` for module-constant keys); grouping never depends on it.

| class | sources |
|---|---|
| grid-uniform | literals; helper 14 (a module constant since aa6eb973); 504, 508 |
| block-uniform | 503 out-params; 509 (a block is resident on one SM) |
| warp-uniform | 510 |
| lane-varying | 505, 511; 502 under the sound rule (each lane reads `%globaltimer` itself; upstream's table calls it uniform) |
| derived values | the join of their operands |
| loads through a map-value pointer | the key's class if no GPU site in any GPU program writes that map (loader- or CPU-written maps); else lane-varying |

A def-use pass gives every helper call's R0 uses through register copies and stack spills. Patterns
match on the CFG with side conditions.

**Rewrite.** An idiom region can be non-contiguous: B in the cuda-counter retprobe is a head chunk at
pc 28-42 and a tail chunk at pc 69-76, with the unrelated block `LBB1_5` (pc 43-68) between them.
The rewrite therefore:

- puts the argument setup and the fused call in the head chunk, starting at the lookup's argument
  setup;
- follows them with `ja` to the join (the region's post-dominator, pc 77);
- overwrites the rest of the head chunk with `ja +0`, so every instruction keeps its pc and every
  jump offset elsewhere stays valid;
- leaves the tail chunk in place but unreachable.

Conditions: the head chunk has room for the setup, the call and the jump (at most 10 instructions;
B's head chunk has 15), and every edge into a tail chunk comes from the head chunk. Otherwise the
idiom is not fused. r1-r5 are already dead after the region, because both arms contain helper
calls.

### Fused helpers (private ids in an unused range below bpftime's private slots 8187-8191, `bpftime_direct_map_helpers.hpp:9-19`; MAX_EXT_FUNCS 8192, `llvmbpf.hpp:12-13`)

| id (name) | arguments | unit |
|---|---|---|
| ADD_OR_CREATE | r1 map, r2 key ptr, r3 delta, r4 init, r5 site | add_or_create |
| ADD_IF_EXISTS | r1 map, r2 key, r3 delta, r5 site | add_if_exists |
| LWW_STORE | r1 map, r2 key, r3 value ptr, r4 flags, r5 site | update(ANY / NOEXIST / EXIST), result unused |
| TOMBSTONE | r1 map, r2 key, r5 site | delete, result unused |
| GPU_READ | r1 map, r2 key, r3 copy destination (free stack slot), r5 site | read-only lookup; returns r3 or NULL |
| PRINTK_REC | r1 interned format id, r2 size, r3-r5 args | bpf_trace_printk with a constant format |

### The eight shapes

**A. Counter, 10 sites** (cuda-counter 48/52; threadscheduling 90/95, 100/105;
threadscheduling_dynamic_hook 93/98, 103/108). cuda-counter entry, pc 22-37 (disassembly in
dir2-read/disasm/cuda-counter.txt): `call 1`; `if r0 == 0 goto` the insert arm; non-null arm
`lock *(u64*)(r0+0) += r6` (BPF_ATOMIC ADD without FETCH); null arm `update(map, key, fp-0x30, 1)`.
Conditions: same map fd and key slot at both calls, with no store to the key slot between them; R0
used only by the null test and as the base of one non-fetch atomic add at offset 0; both arms join
at the call's immediate post-dominator; `init - d` is a site constant (here both are r6 = 1 and the
slot written from r6, so `corr = 0`); every ADD_OR_CREATE site on the map shares that constant.
Becomes `ADD_OR_CREATE(map, &pid, r6, 1, site)`. Against SIS: exact. The first-touch race
disappears: the 32 lanes form one group (same pid) and add 32 with count 32; at a merge the key
exists with value `corr + 32`. Today: 31 of the first 32 increments are lost (`calls=97`).

**B. Plain RMW + EXIST self-write, else NOEXIST insert, 9 sites** (rocm-counter 48/51/53 call_count;
cuda-counter and rocm-counter total_time_ns 73/76/79 and 74/77/80). In the cuda-counter retprobe,
pc 31-76, both updates are **one** `call 2` at pc 76 (`LBB1_4`): the EXIST arm sets r3 = r0 (the
pointer escapes as the update value) and r4 = 2 and jumps there, the NOEXIST arm sets
r3 = fp-0x28, r4 = 1 and falls through. Conditions: the stored value is `load(r0) + d`, with d not
derived from `load(r0)` (here r7 = globaltimer - *tsp); the escape is only that self-write; the
insert value equals d (slot fp-0x28 was written from r7 at pc 21). Becomes
`ADD_OR_CREATE(map, &pid, r7, r7, site)`. Against SIS: exact sum of all deltas. Linux and today
lose updates; under today's lockstep a warp adds one delta instead of 32.

**C. Leader lookup + update(ANY), 4 sites** (cudagraph 56/58 call_count, 79/81 total_time_ns). Shape:
`v = p ? *p + d : d; update(k, &v, ANY)`. R0 is read for the arithmetic only. Becomes
`ADD_OR_CREATE(k, d, d)`. It runs once per launch (single-thread predicate), so there is nothing to
group. Exact, also across concurrent launches.

**D. Entry/exit timestamps, 9 sites** (cuda-counter 44/66/70; rocm-counter 44/67/71; cudagraph
53/75/78).
- Entry `update(start_ts, &x, &ts, ANY)` becomes LWW_STORE. The group is the lanes with the same x;
  within a group the highest active lane is the last writer, as in today's lane order.
- Exit `lookup(start_ts, &x)` is read at pc 19 and **again at pc 105, after the delete at pc 27**
  (the printk reloads `*tsp`). It becomes GPU_READ into a private stack slot chosen past the
  program's deepest stack offset, so both loads read the snapshot taken at the lookup. Linux gives
  the same when the element is not reused. Here it is not even deleted: the delete uses `&pid`
  (fp-0x1c), not `&x`, a source bug the survey noted. The design preserves it.
- GPU_READ is not fused in programs attached through memcapture. That pass moves r10 down by each
  captured line's rounded length before the program runs (`ptxpass_kprobe_memcapture/main.cpp:63-127`:
  a `SUB64_IMM` on r10), and the agent does not know those lengths when it picks the slot.
- `delete(start_ts, &pid)` becomes TOMBSTONE. pid is module-constant, so the slot is pre-resolved;
  a tombstone on an absent key is a no-op in the merge.
- Against SIS: same as Linux.

**E. Per-thread record, 2 sites** (threadscheduling 86, dynamic_hook 89; 80-byte value, BPF_ANY, key
from `(block_x << 16) | (thread_x & 0xFFFF)`). Becomes LWW_STORE. Keys are per lane, so the matched
groups are singletons: no aggregation, one slot claim per lane, seqlock write of 80 bytes. The
capacity (max_entries 4,096) is enforced in the merge: an insert beyond max_entries distinct merged
keys is E2BIG in the serial order (the result is unused). Same as Linux, with the caveat in R1.

**F. Leader publish, 1 site** (gpu_shared_map 55, 1501 = HASH on the Orin): LWW_STORE, once per
launch. Same as Linux.

**G. printk, 21 sites.**
- The format is built by immediate stores to the stack (cuda-counter pc 38-54, 77-104), so the
  abstract interpretation knows its bytes. The agent interns the format in a segment table, sets
  r1 = format id and turns the stack stores into no-ops.
- Becomes PRINTK_REC: one 48-byte record (format id, size, globaltimer, global thread id, three
  args) per lane into the agent's per-SM ring. A warp claims its records with one `atomicAdd` on the
  ring tail and writes them at `base + rank`.
- Payloads are never merged: line count and per-thread order are kept. Non-constant formats keep
  the synchronous printk.
- Against Linux: same order class, no loss, later real-time appearance (table in R1).

**H. Benchmark hash ops, 3 sites** (micro `cuda_probe.bpf.c` 163/171/182, literal key 0).
- `update(hash_map, 0, 1, ANY)` becomes LWW_STORE: one group, identical value.
- `lookup` + non-atomic `(*val)++` with no insert arm becomes ADD_IF_EXISTS, evaluated
  GPU-confined: no key 0 exists, so it is a no-op, matching the NULL lookup.
- `delete(0)` becomes TOMBSTONE.
- Same as Linux.

**First-touch and lost update in summary.** SIS forbids both. The design meets that for A, B and C
by construction: one unit per execution, adds summed, existence decided by the merge. Linux allows
both, so the design guarantees more there. For the plain RMW Linux allows lost updates, and only the
recognized RMW shapes are made exact; any other plain RMW stays synchronous.

## R5. Soundness direction and the synchronous path

A site is fused only if all of these are proven; otherwise it keeps its original helper call
(today's synchronous path):

1. The map argument is a constant fd at the call (LDDW src=1) and the map type has an overlay
   rule (HASH; 1501, which is HASH on the Orin, `bpftime_shm.hpp:137-154`). Arrays and GPU map types
   keep their direct paths; the only change there is the optional exactness upgrade of recognized
   plain RMWs to `red.add`, legal because any outcome of the plain race includes the atomic one.
2. R0 is unused, or used only inside the recognized idiom, or only by loads (GPU_READ).
3. The key bytes are defined in the program before the call (a known stack slot), and the key is 64
   bytes or less.
4. Read-your-writes: no later site of the same program execution may read the same map with a
   possibly equal key before the effect is published, except through GPU_READ, which reads the
   overlay.
5. The idiom's CFG and value conditions of R4 hold, and no jump enters the region.
6. The map is OVERLAY-eligible: every GPU site on it, in every GPU program, is fusible.
7. The pass reports the fused ids in its configuration (`print_config`). Otherwise the agent does
   not rewrite: an unregistered id would make `generate_ptx` call `exit()` in the application
   (llvmbpf `llvm_jit_context.cpp:907,936-939`; the registration list is `core.cpp:234-253`).
8. The step-0 feature probe accepted the warp instructions on this device; otherwise the per-lane
   lowering is used (still no host call).

At run time a fused site still falls back to an exact synchronous request when its slot cannot be
claimed (R6), when a read or conditional op finds `cpu_write_seq != 0`, or when the map is `sealed`.

**The synchronous path stays.** Its waiting rules change in three places:

- New request types `MAP_ADD_OR_CREATE`, `MAP_ADD_IF_EXISTS`, `MAP_LWW_STORE` and
  `MAP_LOOKUP_MERGED` execute the idiom-level unit on the host. There `serve_cuda_request`
  (`bpf_attach_ctx_cuda.cpp:126-187`) gains branches calling the merged map operations, and a new
  keyed add-or-create exists for HASH under the stripe lock. The fix_size hash exposes only
  lookup, update and delete today (`fix_hash_map.hpp:26-49`).
- **Drain first.** A synchronous request on a map with an overlay is answered with merged state,
  so all published GPU effects on that map are ordered before it. A synchronous printk drains the
  agent's printk rings before printing, which keeps per-thread order when a ring is full.
- **Warp-batched requests** (optional, chosen by the cost model): lanes with the same request type
  and map write up to 32 sub-requests into one request. `GPU_HELPER_MAX_BUF` is 1 MiB per key and
  value area (`gpu_helper_protocol.h:12-16`). The watcher executes them in lane order and the group
  waits on one `flag2`. Per-lane results are the same as today's lane-turn order, and no other
  request can interleave between the lanes, which is stronger than today. A warp costs one round
  trip instead of 32. The lane-turn loop stays for builds below sm_70.

## R6. Storage: where, how big, sized by what, when full, who drains

| buffer | where (Orin / discrete) | size | sized by | when full | drained or merged by |
|---|---|---|---|---|---|
| ADD/LWW overlay, GPU part | segment / agent device memory + IPC handle | 64 B per ADD slot; 32 B + value rounded to 8 per LWW slot | slots = next_pow2(2 x max_entries), capped at 65,536 per map and at a total budget (default 16 MiB of the 128 MiB segment, `bpftime_config.hpp:81`) | no free slot within 16 probes: that unit becomes a synchronous request (exact). Over budget at creation: the map stays PLAIN | never drained eagerly; every CPU reader merges; CPU probes materialize single keys; slots are reclaimed only when every registered writer agent is quiescent |
| overlay, CPU part | segment / segment | 32 B per slot | same as the GPU part | - | written by CPU parties under the stripe lock |
| per-module site table `__bpftime_site[1024]` | module device global | 32 B per entry (overlay address, generation, pre-resolved slot for module-constant keys), 32 KiB per module | fixed; ids assigned per agent | ids exhausted: further sites are not fused | agent writes at module load (all three global-write paths) and at link refresh |
| printk ring | segment / pinned host (cost model) | per agent: nSM x 4,096 x 48 B (Orin Nano, 8 SMs [inferred from its 1,024 cores; read from the device attribute at start]: 1.5 MiB) | SM count and the drain period | the warp's leader sends a drain-then-print request and waits (backpressure, no loss); `BPFTIME_GPU_PRINTK_DROP=1` instead drops with a counted miss (Linux's ftrace overruns lose lines) | watcher at every wake, on drain-first requests, at detach and in atexit (new: today the atexit handler only stops the watcher, `bpf_attach_ctx_cuda.cpp:88-109`) |
| format table | segment | interned strings, 1,000 B each at most | distinct formats | full: the site keeps the synchronous printk | read by the watcher |

Direction 1's storage question: nothing here is sized by the grid. Overlays are sized by the map's
key universe (max_entries) and rings by the SM count, so launches that no hook notes (graphs,
CUDA 13 handles, captures) cannot outgrow a launch-time size. The per-thread GPU array maps of
direction 1 are a separate problem; this design does not use per-thread storage.

Example sizes: cuda-counter's call_count, total_time_ns and start_ts at 1,024 entries each [inferred
max_entries] are 3 x 2,048 slots x 64-96 B, about 0.5 MiB. threadscheduling's thread_mapping
(4,096 x 80 B) is 8,192 x 128 B = 1 MiB.

## R7. The analyzer: where it runs, its inputs, how its decisions reach code generation, its records

### Where and when

- **Per program, in the agent at link instantiation**, in `nv_attach_impl::create_attach_with_ebpf_callback`
  (`nv_attach_impl.cpp:181-285`). That is the first place with instructions, the fd-indexed
  `map_basic_info` and the attach type together; upstream's PR #624 runs its GPU verifier there.
  It takes about a millisecond for a few hundred instructions [estimate]. PREVAIL is not used (0.6-1.9
  s per 4,096 instructions in upstream's own measurement, and it rejects fetch atomics).
- **Per map, in `init_attach_ctx_from_handlers`** (`bpf_attach_ctx.cpp:190-285`): eligibility over
  every `cuda__` program handler that references the map, and overlay creation or lookup in the
  segment.
- **Per module, in `hack_fatbin`** (`nv_attach_impl.cpp:962-1157`): site ids are assigned per
  agent, so they are unique across all programs that chain on one module.
  - The module's site table `__bpftime_site[1024]` (32 B per entry, 32 KiB of device global per
    module) is filled on all three paths that write module globals today, as
    `__bpftime_pid_tgid` is: substitution, `write_trampoline_globals` (`nv_attach_impl.cpp:2366-2385`);
    the late path, `nv_attach_fatbin_record.cpp:263-292`; directly-run, `nv_attach_impl.cpp:1323-1352`.
    Fused sites therefore also work when an attach falls back to the late path
    (`nv_attach_impl.cpp:264-277`).
  - Sites beyond 1,024 per agent are not fused.
- **Offline**, as a CLI (`bpftime-siteplan`) on `.bpf.o` files with BTF line info, for the survey
  check below.

### Inputs

The stored instructions (`nv_attach_entry.instuctions`, `nv_attach_impl.hpp:76-88`), the map
descriptors (type, key size, value size, max_entries; `bpf_attach_ctx_cuda.cpp:505-554`), the
attach type (kprobe, kretprobe, memcapture), the other programs per map, the cost parameters from
start-up (below), the pass's capability list, and offline the line info plus a `cpu_reads_map`
column.

### Carrier to code generation, and why each listed obstacle is harmless

1. The agent rewrites its copy of the program (`entry.instuctions`; the segment's copy is not
   touched) into fused calls with the site id as an immediate in r5 (r1 for PRINTK_REC). The
   decision is in the bytecode, so:
   - **no hook in `generate_ptx` is needed for correctness**: the fused call compiles like any
     registered helper;
   - **no eBPF pc in IR** is needed: the site id is a constant argument;
   - **memcapture's prepended instructions** shift pcs but not the site id
     (`ptxpass_kprobe_memcapture/main.cpp:53-57`). Its per-access recompilation reuses the same
     rewritten words;
   - **O3 merging** of two fused calls (SimplifyCFG sinking) would turn the site id into a phi. A
     non-constant site id selects the tier-0 generic fused helper, which reads the site table at run
     time and is correct;
   - **unregistered ids exit()**: the ids are added to `compile_ebpf_to_ptx_from_words`'s list
     (`core.cpp:234-253`) and defined in the trampoline, and the agent rewrites only if the pass's
     configuration lists them;
   - **the patch-cache key** (sha256 of the request, `nv_attach_impl.cpp:994-1027`) changes with
     the rewritten words, so cached PTX cannot carry a stale plan.
2. **Tier 0 (trampoline)**: `_bpf_helper_ext_<fused>` is defined in `default_trampoline.cu`. Warp
   instructions are written as inline-asm PTX text, because clang compiles the trampoline for sm_61
   (`trampoline/Makefile:3,26`) and would refuse sm_70+ builtins. The module's `.target` is
   rewritten to sm_87 before nvPTXCompiler (`nv_attach_utils.cpp:52-65`); step 0 verifies that the
   text is accepted. A second trampoline variant without them serves devices below sm_70.
3. **Tier 1 (IR inlining, the performance tier)**: a new hunk in the overlay patch on
   `llvm_jit_context.cpp` (already overlaid, `vm/compat/llvm-vm/CMakeLists.txt:20-37`) adds,
   between `optimizeModule` and codegen in `generate_ptx` (llvmbpf `llvm_jit_context.cpp:947-950`
   pre-overlay), a call `bpftime_gpu_inline_fused(M, plan)` followed by a second light optimization,
   the same shape as the CPU JIT's `inline_array_map_lookup_helpers` between two `optimizeModule`
   calls (llvmbpf 515-549). The pass replaces each fused call whose site id is a constant with
   inline IR built from the plan entry: delta constant -> `popc(group) * c`; module-constant key ->
   the pre-resolved slot from `__bpftime_site[s]`, with no hash probe; key size 4 -> `match.any.sync.b32`.
   The plan reaches the pass as a new `RuntimeInput.site_plan` field
   (`NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT`, `core.hpp:69-80`, so older passes ignore it),
   then a new `compile_ebpf_to_ptx_from_words` overload, then a new `llvmbpf_vm` setter added
   through the descriptor-patch overlay.
4. **PTX text** is used only for the warp primitives inside tiers 0 and 1, as inline-asm strings
   with the `convergent` attribute, so LLVM 15's sm_61 NVPTX target never has to select them.
   Rewriting the probe's PTX text would mean reverse-engineering the fd from shared registers
   (`mov.u64 %rd35, 13` reused by two calls in the kept cuda-counter PTX) with no pc link, so it is
   not used for decisions.

### Tier-1 fast path for ADD_OR_CREATE (what one warp executes)

```
mask  = activemask                         // already used by the trampoline
grp   = match.any.sync.b64 key, mask       // lanes with the same key (b32 for 4-byte keys)
lead  = ffs(grp) - 1
n     = popc(grp)
sum   = d is constant c ? n * c
      : (redux.sync.add.u32 of 22/21/21-bit limbs of d over grp, recombined)  // redux is 32-bit only:
                                            // 32 x 2^22 < 2^32, so 3 limbs give an exact 64-bit sum mod 2^64
if (lane == lead) {
    h = ld.global [__bpftime_site + s]     // device memory, L2-cached
    if (h.header.sealed | h.header.dead) -> sync / drop
    slot = key is module-constant ? h.slot : probe_or_claim(h.table, key)   // CAS EMPTY->CLAIMING->READY
    if (slot == FULL) -> MAP_ADD_OR_CREATE host request with (key, sum, n)
    red.global.add.u64 [slot.sum],   sum   // posted: no return value, no fence
    red.global.add.u64 [slot.count], n
}
```

About 20-30 instructions plus two posted reductions per group; no `membar.sys` per operation. The
writer needs no fence for exactness. The reductions are linearized when they are performed;
`host_call` already issues `membar.sys` before any later synchronous request of the same thread
(`default_trampoline.cu:152-158`), and kernel completion orders the rest. Without `match.any` or
`redux` (devices below sm_70, or a refused feature probe), each lane issues its own `red.add`, using
only primitives the trampoline already emits (`atom.*`, `activemask`): still no host call, still
exact.

### Output records, and the mechanical check

One record per helper call, with the survey's field names (`file, line, section, helper, map,
map_type, host_call_today, result_use, key_scope, operation, cpu_reads_map, aggregatable,
frequency, idiom, class, reason`) plus `decision` and the extras `pc`, `site_id`, `fused_id`,
`combine` (`match_any | module_constant | none`), `tier`, `fallback_reason`.

- `host_call_today`: a table of the generic trampoline at 52126f33 by (helper, map type).
- `result_use`: from def-use.
- `key_scope`: from the construction class of the key (grid-uniform -> constant, block-uniform ->
  per_block, warp-uniform -> per_warp, lane-varying -> per_thread, no key -> none). The check
  compares it only for the 38 map operations: for printk the survey used `none`, `per_block` and
  `constant` for argument sets of the same class.
- `operation`: from the helper, the constant flags and the idiom role.
- `frequency`: `per_launch` when the site is control-dependent on a single-thread predicate (all
  thread and block indices compared to 0, or their sum), else `per_thread_per_launch`.
- `aggregatable`: the key is not lane-varying for map ops; every argument is not lane-varying for
  printk.
- `cpu_reads_map`: **an input** offline (the survey's column) and a runtime observation online.
  `class` = `gpu_local_already` if `host_call_today` is no; `sync` if `decision` is sync;
  `eliminate` if `cpu_reads_map` is no and the site is a map operation; else `defer`.

The 20/39 split cannot be derived from the programs: B's total_time_ns and B's call_count have the
same shape and differ only in whether the loader reads the map. That is why it is an input here,
and a run-time bit (`cpu_write_seq`, merge traffic) in the system. The mechanism does not care:
eliminate and defer are the same overlay.

Check script `check_plan.py`: run the CLI on the 22 programs, join with `records.json` verified
records deduplicated on (file, line, section, helper), compare per field, print the tallies.

Two details make the join work:

- **A shared call has one pc but two source lines** (cuda-counter retprobe pc 76 serves lines 76
  and 79). The CLI emits one record per incoming arm, with the line of that arm's argument setup.
- **rocm-counter has no built object.** The CLI compiles `rocm_probe.bpf.c` with `clang -target bpf`
  [inferred to build: it is ordinary eBPF C].

Expected outcome:

- **class**: 109 / 20 / 39 / 0 per record, provided every shape of R4 is matched; any mismatch is
  an analyzer bug.
- **key_scope of the 38 map operations**: constant 24, per_block 8, per_warp 4, per_thread 2,
  equal to the survey site by site, given 509 block-uniform and 510 warp-uniform.
- **aggregatable**: not 51 under any consistent rule. Derived site by site with the classes of R4:
  - **Map operations**: 36 of 38 aggregatable (all but the two per-thread record stores), equal to
    the survey under either rule.
  - **printk**, 21 sites:

    | printk sites | arguments | sound rule (502 varying) | upstream rule (502 uniform) | survey |
    |---|---|---|---|---|
    | atomizer 48, 50, 57, 59, 68, 74 | block idx, grid dim, values of loader-written 1503 maps | yes | yes | yes |
    | gpu_shared_map 52, 73; directly_run 79 | none | yes | yes | yes |
    | cuda-counter 86, rocm-counter 87 | block idx | yes | yes | yes |
    | cuda-counter 55, rocm-counter 56 | block idx, `%globaltimer` | no | yes | yes |
    | cuda-counter 82, rocm-counter 83 | `%globaltimer` minus `*tsp`, `*tsp` from start_ts, which the entry probe writes | no | no | yes |
    | launchlate 114, 117; launchlate-kernel-gpu-shared-map 119, 122 | `%globaltimer`, values of CPU-written maps | no | yes | no |
    | host_map_test 108, 154 | thread idx | no | no | no |

  - **Totals**: sound rule 36 + 11 = **47**; upstream's table 36 + 17 = **53**. The survey's 51
    treats the timer as uniform in cuda/rocm-counter ("assumes the warp reads %globaltimer once")
    but not in launchlate, whose first statement is the same timer read (`launchlate.bpf.c:88`). It
    also treats a GPU-written `*tsp` as uniform within a block.
  - The survey needs one correction; the analyzer is not tuned per site. In this design printk
    payloads are never merged anyway (G); `aggregatable` only predicts cost.

Example record (cuda-counter entry, survey line 48):

```json
{"file": "example/gpu/cuda-counter/cuda_probe.bpf.c", "line": 48,
 "section": "kprobe/_Z9vectorAddPKfS0_Pf", "helper": "bpf_map_lookup_elem (1)",
 "map": "call_count", "map_type": "BPF_MAP_TYPE_HASH", "host_call_today": "yes",
 "result_use": "pointer_rmw", "key_scope": "constant", "operation": "accumulate",
 "cpu_reads_map": "yes", "aggregatable": true, "frequency": "per_thread_per_launch",
 "idiom": "A counter: lookup + atomic add / NOEXIST insert", "class": "defer",
 "decision": "fuse:ADD_OR_CREATE", "pc": 26, "site_id": 3, "combine": "module_constant",
 "tier": 1, "fallback_reason": null}
```

### Start-up cost model

At the watcher's start (it already makes the primary context current,
`bpf_attach_ctx_cuda.cpp:264-276`), on a private non-blocking stream, a calibration kernel of about
2-5 ms [estimate] runs once. Its results are cached in the segment keyed by device UUID, clock and
driver, so later agents and start-up are not delayed. It measures:

- `c_rt`: host round trip, from 16 calls;
- `c_red_same` and `c_red_distinct`: posted 64-bit reductions per warp, same address and distinct
  addresses, in segment memory and (discrete) in device memory;
- `c_ld`: dependent uncached load latency;
- `c_dtoh`: discrete only, small device-to-host copy;
- features: `match`, `redux`, `red.sys`, and whether sysmem atomics count exactly across two
  contexts.

Per site, over the admissible set:

- grouping: `match_any` vs `module_constant` (skip the match) vs `none` (per-lane keys by
  construction);
- sharding: S = 1 or nSM sub-slots for a module-constant hot key, when
  `W x c_red_same > S x W/S x c_red_distinct + S x c_merge`;
- tier: inline vs call, by a code-size budget (memcapture can produce thousands of sites);
- placement on a discrete GPU (R8);
- for the synchronous remainder: warp-batched when two or more lanes are active.

On the Orin, sync is chosen only when a site is inadmissible. With c_rt about 10 us against tens of
nanoseconds per reduction, an admissible fused site is cheaper for every thread count, so the model
never has to trade exactness for speed.

The one case where sync wins on cost is a discrete-GPU map that CPU probes also touch, when step 0
shows that reductions on pinned host memory are not exact over PCIe. The overlay must then live in
device memory, and every CPU-probe access pays a device-to-host copy on an application thread. The
model keeps the map PLAIN when `r_cpu x c_dtoh > r_gpu x (c_rt - c_fused)`: CPU-probe accesses per
second against GPU executions per second. At link time these rates are estimated from the
frequency classes (a per-thread GPU site against a CPU probe on a function); later they are
measured, but they act only at a link refresh, because leaving OVERLAY needs a seal.

## R8. Discrete GPUs

What changes is placement, the round-trip cost and the loader's read cost; the algorithm, the
records and SIS do not change.

- **Overlays: agent-owned device memory** (`cuMemAlloc` in the application's context), so GPU
  reductions hit L2 at nanoseconds. An IPC handle (`cuIpcGetMemHandle`, available on discrete GPUs)
  is published in the overlay header. The loader's syscall server imports it lazily in its own
  context and reads slots with `cuMemcpyDtoH` (`c_dtoh`, about 10 us [inferred]); the server already
  keeps a CUDA context for 1502/1503 (`nv_gpu_shared_array_map.cpp:70-128`). A single-slot read is
  an aligned 8-byte copy of a GPU-written word; that it is single-copy atomic against concurrent
  reductions is [inferred] and part of step 0 on that machine.
- **Why the agent owns the overlay and not the server**: server-owned device memory would be freed
  when the loader exits while the application's patched kernels still write it (a GPU fault in the
  application). Agent ownership instead loses deltas if the agent dies. Mitigation: the watcher
  mirrors the overlay's GPU part into the segment every 100 ms and at detach and exit, so a crash
  loses at most the last period on a discrete GPU. The Orin has no such loss: the overlay is in the
  segment.
- **Maps that CPU probes also touch** (known at link time from the handler table): every CPU-probe
  access would need a device-to-host copy, so the model either places that map's overlay in pinned
  host memory, if the step-0 test shows sysmem reductions are exact over PCIe, or keeps the map
  PLAIN.
- **printk rings: pinned host memory** (posted PCIe writes; the watcher reads directly), unless the
  measured posted-write bandwidth is below the record rate, in which case device rings plus
  watcher `cuMemcpyDtoHAsync` on a non-blocking stream.
- **The round trip** is measured at start-up (PCIe flag traffic plus watcher latency) and only
  changes how much the remaining sync sites cost; the batched sync path matters more there.
- **1501 on IPC GPUs**: its host path hands the GPU a staging copy and its NOEXIST insert never
  succeeds (read from code, dir2-read). Fused sites do not use that path; the overlay merges over
  the 1501 implementation's lookup and iteration.

## R9. Interactions

- **Several agents on one segment.** The request slot is shared and every agent zeroes it at
  start (`bpf_attach_ctx_cuda.cpp:615`; CLAUDE.md:2318-2319): an existing limitation this design
  does not fix, but it relies on the slot less.
  - Overlays are per map, shared, and found by name; creation happens under the segment's mutex.
  - GPU reductions from several contexts on the Orin go to the same physical memory; that they are
    atomic across contexts is [inferred] and tested in step 0 with two victim processes.
  - On a discrete GPU each agent has its own device overlay shard; readers sum the shards listed
    in the header.
  - printk rings are per agent, and each agent's watcher drains its own.
  - Slot reclaim needs every registered writer agent to be quiescent.
- **CPU probes on the same maps.** They use the merged operations (R3). CPU-side atomics through a
  materialized base pointer are CPU-only words, so they are exact. CPU writes bump `cpu_write_seq`,
  which sends later GPU reads and conditional ops of that map to the synchronous merged request.
  launchlate's CPU-written `last_uprobe_time` is an ARRAY and keeps its direct path.
- **Graph launches and other launches no hook notes** (`probed_launch_activity.cpp:16-47`): fused
  paths never depend on launch notes. Overlays are readable at any time. printk rings are drained
  on every watcher wake (the unseen-launch latch keeps the watcher waking) and on drain-first
  requests. Only slot reclaim and sealing need quiescence, and both wait (or defer) when the latch
  is set.
- **Map close and fd reuse**: R3 (generation, `dead`, pinned until no agent references the
  overlay, site table refreshed).
- **printk ordering.** Per GPU thread, program order: a thread stays on its SM and appends in
  order. Across threads, globaltimer order within a drain batch, merged over the SM rings, like
  ftrace's per-CPU merge (`kernel/trace/trace.c:3735-3752`). Against host-side output (launchlate's
  CPU uprobe prints), GPU lines appear up to one drain period later. `BPFTIME_GPU_PRINTK_SYNC=1`
  keeps today's synchronous printk for tools that need real-time interleaving.
- **Several probes on one kernel** (entry + exit, as in cuda-counter, or memcapture). Sites of
  different programs on one map share the overlay; the eligibility check spans all programs. Fused
  code is inlined or calls the shared trampoline helper, so it adds no symbol and cannot collide
  where `__probe_func__<kernel>` would for two entry programs on one kernel (an existing issue,
  `ptxpass_kprobe_entry/main.cpp:45-97`). memcapture sites get tier 0 beyond the code-size budget.
- **Detach and exit.** Probes gated off by `__bpftime_probes_enabled` stop issuing fused calls;
  in-flight reductions still land and are merged. The last detach does not wait for running kernels
  in substitution mode (`nv_attach_impl.cpp:159-170`, [inferred]), which is harmless here. The new
  atexit step drains printk rings (and, on a discrete GPU, mirrors overlays) before the watcher
  stops.

## R10. First slice (measurable on the Orin)

### Step 0: feasibility probes (no dev change, one test program)

1. Through the agent's own `compile_ptxs` path (`.version 8.1`, `.target` rewritten to sm_87),
   assemble a module containing `match.any.sync.b32/.b64`, `redux.sync.add.u32`,
   `vote.ballot.sync`, `shfl.sync`, `red.global.add.u64`, `nanosleep`.
2. A calibration kernel:
   - `c_rt`, with `%globaltimer` stamps inside `host_call` at lock acquired, request written,
     `flag1` set, `flag2` seen, released. This gives the first measured breakdown of the ~10 us.
   - `c_red_same` and `c_red_distinct` on segment memory at 1, 32, 3,136 and 31,264 warps.
   - `c_ld`.
   - Exactness of reductions read by a CPU thread during the kernel: monotonic, final equals the
     expected sum, including values crossing 2^32.
   - The same exactness from two processes on one segment.
3. The outcome decides tier-0 instruction selection. If anything is refused, the per-lane fallback
   is the slice.

### What to build (slice 1: idiom A only, Orin, tier 0)

- **Analyzer subset**: CFG, stack-constant and def-use tracking, the A matcher, eligibility, site
  ids, rewrite with padding, capability check.
- **Pass**: register the ADD_OR_CREATE id in `core.cpp:234-253` and report it in the pass
  configuration.
- **Trampoline**: ADD_OR_CREATE (match.any, popc or limb redux, `red.add`; per-lane fallback),
  `__bpftime_site[]`, regenerated `trampoline_ptx.h`.
- **Agent**: overlay creation in the segment, site table writes in `write_trampoline_globals`, and
  the `MAP_ADD_OR_CREATE` host request for slot overflow.
- **libbpftime map layer**: merged lookup, get_next_key, update, delete and lookup_and_delete for
  HASH with an ADD overlay; CPU-probe materialization; `cpu_write_seq`.
- **Not in slice 1**: LWW, GPU_READ, printk, B/C/H idioms, tier 1, warp-batched sync, discrete
  GPUs.

### Programs and expected numbers (10 interleaved rounds, the baseline-sub-20261111 procedure)

| case | today (aa6eb973 measured, or [estimate]); re-measured at 52126f33 as set D | slice 1 [estimate] | check |
|---|---:|---:|---|
| CUDA Counter (minimal) | 2,928.9 us | about 2,570 us (-12%: 1 of 8 host calls per thread) | `calls=128` (today 97) |
| hash-counter micro, new mode, key 0 (minimal) | about 330 us | 49-52 us | count = 128 |
| same, key = lane % 4 (minimal) | about 330 us | 50-55 us | each of 4 keys = 32 |
| same (small, 1,024 threads) | about 9.2 ms | 35-45 us | count = 4,096 |
| same (large, 100,352 threads, 1 timed launch, bounded timeout) | about 0.9 s | 70-250 us | count = 2 x 100,352 |
| threadscheduling example | not measured | sync remains for its per-thread record | sum(sm_histogram) = sum(warp_histogram) = threads x launches |
| controls: the 24 cases of the subset | as measured | unchanged within noise | - |

The CUDA Counter gain grows with later slices [estimates, same per-call basis]: printk records
(slice 2) about 1,850 us; LWW, GPU_READ and tombstone (slice 3) about 760 us; idioms B, C and H
(slice 4) about 60-90 us. Tier 1, warp-batched sync and the discrete GPU follow.

### Measurement

- The 24-case runner with its checks (time window, cpu5 at 1728000, GPU at 1020000000, no leftover
  processes, no build, fresh segment), plus the new cases. Every run takes a total timeout.
- D (52126f33) against N (slice), 10 rounds, rotating order, Mann-Whitney p as in the earlier
  rounds.
- GPU-timed per-warp latency of the fused path with globaltimer in a test probe.

### Exactness tests

1. Counts as in the table, at every geometry.
2. **Witness ring** (test build only): every fused lane also writes (site, key, delta) into a 1527
   ring. A checker replays the records in any serial order and compares with the loader's merged
   reads; this is an oracle independent of the overlay code.
3. Loader reads during kernels: monotonic per key, never above the final.
4. A loader delete of `call_count[pid]` between launches: the final value equals the adds after the
   delete.
5. A loader update to 1,000 mid-run: the final value is 1,000 plus the adds after it.
6. A CPU uprobe doing an atomic add on the same key: the final value equals GPU plus CPU adds.
7. Two victim processes on one segment: the final value is the sum of both.
8. Overflow: more distinct keys than slots, so the synchronous request path is taken and the counts
   are still exact.
9. Negative programs: `v = __sync_fetch_and_add(p, 1)` with v used, an extra read through p, and a
   jump into the region. Each must stay unfused (the PTX still contains `_bpf_helper_ext_0001` for
   that site) and keep today's result.

### Mutations (each must fail a test above; sources restored and hashed after each, as in hostcall-l1)

| mutation | fails |
|---|---|
| `popc(activemask)` instead of `popc(match group)` | key = lane % 4 counts (each key gets 128) |
| count reduction removed | delete-then-recreate case (count decides creation) |
| CPU update does not record offsets | test 5 (the old adds reappear) |
| merge reads `sum` as two 32-bit halves | the 2^32 crossing test in step 0 / test 3 |
| matcher ignores an extra use of R0 | negative program 2 changes its result |
| eligibility check skipped (map with one unfusible site) | a probe mixing a fused site and a pointer write: witness mismatch |
| slot-full fallback removed (writes to slot 0) | overflow test |
| capability check skipped with an old pass | the application exits (detected as a failed run) |

## Inferences and open items carried by this design

- PTX acceptance of `match.any`/`redux`/`red`/`nanosleep` text in the retargeted module (step 0).
- Atomicity of GPU reductions on registered memory across contexts, and the visibility of posted
  reductions to CPU readers on the Orin (step 0). The design needs only GPU-GPU atomicity and
  single-copy-atomic aligned 64-bit reads.
- All projected numbers are estimates from the measured analogs named in section P. The uncached
  reduction throughput on this board has never been measured.
- LLVM 15's NVPTX backend is not asked to select sm_70+ instructions (inline asm), so its sm_87
  support does not matter.
- The survey's `aggregatable` column is inconsistent on `%globaltimer` (R7), so 51 cannot be
  reproduced by a consistent rule.
- The admission bound for overlay-only keys is conservative (deletes free room only at a
  quiescent reclaim). It costs synchronous fallbacks, not exactness.
- A late GPU program with an unfusible site on an overlaid map is deferred while quiescence
  cannot be proven (unseen launches); its probes start later than they were asked to.
- On a discrete GPU, an agent killed by SIGKILL loses the overlay deltas since the last segment
  mirror (100 ms period). The Orin keeps them in the segment.
