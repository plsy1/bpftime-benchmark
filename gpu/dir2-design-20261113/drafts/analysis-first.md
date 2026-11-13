# UnitFold: per-call-site execution of GPU eBPF helpers, decided by load-time bytecode analysis

Angle: analysis first, minimal runtime change. Everything about the current code is read at dev
`52126f33` (worktree `/home/jetson/src/bpftime-daily-state/worktrees/4708f4092d1e/candidate`, paths
below are relative to it; llvmbpf lines are submodule `87bac39a` before the overlay). Nothing was
built or run. "(inferred)" marks a claim that combines code paths not seen running; "(external)"
marks a claim from CUDA/PTX documentation not re-read this session.

UnitFold is our name for this design. The name has two parts. *Units*: the analyzer recognises
multi-instruction idioms such as lookup-then-add as one unit and rewrites each unit into one call
of a new helper. *Fold*: GPU effects that were not sent to the host are folded into the map by
whichever process touches the map next, before that process reads or writes it.

## 0. Overview

### Problem, in measured numbers
- A host call costs about 9-11 us on the Orin, and the lanes of a warp take turns. The loop is in
  `attach/nv_attach_impl/trampoline/default_trampoline.cu:142-171`.
- Measured on 11-11 (`results/gpu/hostcall-l1-20261111/README.md:36-41`):
  - CPU Hash map lookup (32 threads, one call each): 339 us per launch.
  - The same probe on an ordinary ARRAY, which runs on the GPU since 1b3d9c99: 48 us.
  - Baseline: 39 us.
  - CUDA Counter: 2,877 us per launch, 264 host requests per launch (8 per thread plus first-touch
    inserts).
- Today's synchronous path also loses data:
  - CUDA Counter's `call_count` reads 97 (`results/gpu/compare-20261110/README.md:87`;
    `hostcall-l1-20261111/README.md:32-33`). 128 entry-probe executions happened: 32 threads × 4
    launches (inferred from the request counts at `hostcall-l1-20261111/README.md:26-27`).
  - The mechanism (read from the code, not traced): all 32 lanes of the first warp miss on the
    lookup, so 32 `BPF_NOEXIST` inserts race and only one lands. That matches 128 − 97 = 31 lost.

### Three ideas
1. **Units, not calls, decided on bytecode before compilation.** The analyzer runs in the agent at
   CUDA link instantiation (`runtime/src/attach/bpf_attach_ctx.cpp:636-648`). For each helper call
   site it computes:
   - the result use;
   - the provenance of the key and the value (constant, launch-invariant, block-invariant,
     hardware position, per-thread);
   - which recognised idiom unit the call belongs to.

   Each decision is expressed by rewriting the bytecode: the unit's instructions become one call to
   a new, registered helper id (600-604). The site's descriptor travels in immediates set into
   argument registers. Because the encoding contains no pc, it survives O3 call merging, memcapture's
   prepended instructions and per-access recompilation. No IR hook and no PTX text rewrite is
   needed.
2. **Correctness rests only on facts about one thread.** Uniformity only chooses the cost.
   - Soundness-critical analyses: idiom shape, key equality, result and pointer confinement,
     liveness, and value invariance of a single thread.
   - Aggregation per warp is done by exact run-time grouping: lanes with equal keys are found at
     run time and their deltas summed, or their identical printk lines counted. No leader is
     elected in place of the others.
   - Static uniformity only picks a cheaper exact variant (`popc(activemask) * d` when the key and
     the delta are provably launch- or block-invariant) and predicts the benefit.
3. **Fold on access.** Deferred GPU effects live in per-(map, agent) *ledgers* inside the shared
   Boost segment.
   - Within a ledger, every word is written by one side only: the GPU writes and the CPU reads,
     except for a few CPU-written control words that the GPU only reads.
   - Every host access to a ledgered map first folds the pending effects of the touched key into
     the map. This covers loader syscalls, CPU-probe helpers and watcher-served GPU host calls, all
     at the `bpf_map_handler` level and under the key's hash stripe lock.
   - So the host-visible state is current at every host access, with no flush channel to the
     agent and no kernel-completion hook. Graph launches, which no hook sees, therefore do not
     matter for correctness.
   - GPU-local reads (the *eliminate* case) are validated against a CPU-written per-map
     host-mutation epoch and fall back to the synchronous path when it changed.

### The four mechanisms in this design
| survey term | UnitFold mechanism | helper |
|---|---|---|
| eliminate | GPU-local read of the ledger's latest written value, validated by the host-mutation epoch (`res_lookup`), plus ledgered writes | 603 (+600-602) |
| aggregate per warp | run-time grouping inside the deferred helpers (exact Σ / line counts); static fast path when the key is invariant | inside 600-604 |
| defer | ledger effect (`dfr_add` / `dfr_set` / `dfr_del`) or printk ring record (`dfr_printk`) | 600, 601, 602, 604 |
| synchronous round trip | the unchanged `call 1/2/3/6` path (`host_call`), plus four new host ops used only as fallbacks of the new helpers | 1, 2, 3, 6 |

### What changes, and what does not
| component | change | size (estimate) |
|---|---|---|
| agent, `bpf_attach_ctx.cpp:636-648` | call the analyzer between `create_map_basic_info` and `create_attach_with_ebpf_callback`; pass rewritten instructions on | ~20 lines |
| new `attach/nv_attach_impl/site_analysis/` | analyzer: abstract interpreter ported from PR #624 (no PREVAIL), unit matcher, rewriter with jump fixup, re-analysis check | ~3k lines C++ |
| `core.cpp:234-253`, pass `main.cpp` | register ids 600-604; each pass exports `ptxpass_site_exec_abi()` (looked up with `dlsym`; absent = old pass) | ~15 lines |
| trampoline `default_trampoline.cu` | helpers 600-604, warp grouping, ledger slot protocol, fallbacks through the existing `host_call` | ~600 lines CUDA |
| `gpu_helper_protocol.h:22-30` | four fallback ops (UNIT_ADD, UNIT_SET, UNIT_DEL, LOOKUP_COPY), carrying the map generation | ~40 lines |
| `bpf_attach_ctx_cuda.cpp:125-187` | serve the four ops; drain the printk ring each watcher turn and at exit (`:88-109`) | ~150 lines |
| `bpf_map_handler` (`map_handler.hpp:71`, `map_handler.cpp:135,391-440,441,617,773,1797`) | `gen` field, ledger list pointer, fold hook in each entry point, epoch bump on mutation and pointer-returning lookup | ~250 lines |
| `bpftime_hash_map.hpp` | `fold_add_locked` (in-place add or insert under the stripe lock, the shape of `with_existing_value` at `:699-709`) and a locked fold-then-op section | ~80 lines |
| `create_map_basic_info` (`bpf_attach_ctx_cuda.cpp:505-554`) | for HASH maps with this agent's ledger, `extra_buffer` = device address of the ledger header | ~10 lines |
| module global `__bpftime_defer_ctl` | written on all three load paths next to `__bpftime_pid_tgid` | ~30 lines |

Unchanged:
- the request protocol and its waiting rules;
- lane serialisation in `host_call`;
- the watcher's sleep policies;
- all GPU map types and the ARRAY fast path;
- the loader programs and libbpf.

## R1. Reference semantics

### Actions
An execution consists of *actions*:
- (a) one execution, by one GPU or CPU thread, of a recognised idiom unit (R4);
- (b) one execution of any other helper call;
- (c) one BPF atomic instruction;
- (d) one host-side map operation: a loader `bpf()` command, a CPU-probe helper call, or a
  watcher-served GPU host call;
- (e) one printk line.

A deferred action *commits* when its ledger record is published on the GPU: the `atom.add` on a
slot's counter, or the even version store of a slot's writer record (R3, R6). A synchronous
action commits when it returns.

### Reference SR (serial-per-action with idiom atomicity)
An execution is correct when, for every map M separately, there is a total order ≺ over the
actions on M such that:
1. **Legality.** Replaying the actions in ≺ order on Linux's sequential map specification gives:
   - every value the program uses;
   - every helper return value that is live;
   - every value a host read returns.

   The specification includes flags (EEXIST, ENOENT), E2BIG on full maps and ENOENT on deletes
   (`kernel/bpf/hashtab.c:1084-1113,1147-1149`, L4T 6.8.12).
2. **Program order.** Actions of one thread appear in program order.
3. **Real-time order.** If action a commits before action b begins, then a ≺ b.

Printk lines form their own sequence, with the same three rules.

*Exact* means SR. It is measured against a serial order of action executions, not against today's
bpftime, which itself violates SR:
- The counter idiom's first-touch race gives 97 instead of 128.
- A plain read-modify-write through a host-call pointer collapses to about one update per warp,
  because `host_call` answers all 32 lookups before any lane continues
  (`default_trampoline.cu:142-171`).

SR is per map: like Linux, it gives no atomicity across maps.

### Observation points (where the host-visible state is current)
At every host access that reaches a `bpf_map_handler` entry point, the state reflects every GPU
action that committed before the access began. The entry points are:
- lookup, update, delete, lookup-and-delete;
- `get_next_key` at the start of an iteration (key NULL);
- the cached-handler and direct-map diagnostic branches (`bpftime_shm_internal.cpp:290-418`).

This holds in every process (loader, agent, other agents) and does not depend on kernel
completion, stream synchronisation or a periodic flush.

`get_next_key` in the middle of an iteration may miss keys whose first GPU effect committed after
the iteration began, as Linux iteration may miss concurrent inserts.

Excluded, by construction:
- ARRAY mmap reads (`syscall_context.cpp:1036-1048`): ARRAY maps are never ledgered; they keep the
  1b3d9c99 device path.
- batch commands: they fall through to the real `bpf()` today and do not work
  (`syscall_context.cpp:852-854`).
- `run_with_kernel`: no ledgers are created.

### What Linux guarantees, and what UnitFold guarantees, per idiom
Linux atomicity is per helper call and per BPF_ATOMIC instruction:
- A plain read-modify-write through a lookup pointer is a data race.
- A lookup miss followed by a `BPF_NOEXIST` insert can lose increments.
- An update of an existing hash key replaces the element, so adds through an older pointer are
  lost (`kernel/bpf/hashtab.c:1125-1185`).
- `trace_printk` lines are merged by timestamp and can be lost on overrun.

SR histories are a subset of the histories Linux allows. Every result UnitFold produces is one a
Linux kernel could produce; where noted below, UnitFold excludes Linux's lossy interleavings.

| idiom (R4) | Linux | today's bpftime | UnitFold |
|---|---|---|---|
| A counter (lookup; miss → NOEXIST insert; hit → atomic add) | each add atomic; first-touch insert race loses increments | first warp's misses lose up to 31 per warp (97/128) | **more**: the unit is atomic; final = initial + Σd |
| B non-atomic RMW, EXIST self-write, else NOEXIST insert | lost updates allowed | about one update per warp | **more**: atomic unit, Σd |
| C leader lookup + update(ANY) of `*p+d` | racy across concurrent leaders | exact unless launches overlap | **more** (atomic unit) |
| E entry/exit timestamps (update ANY / lookup / delete) | per call linearizable | per call (synchronous) | **same**; GPU-local read validated or synchronous |
| F per-thread record update(ANY) | per call; which keys enter a full map depends on arrival | same | **same**, except one E2BIG corner (below) |
| G leader publish update(ANY) | per call | same | **same** |
| H printk | lines may be lost; interleaved by timestamp; asynchronous to user output | synchronous, no loss | **same as Linux or more**: no loss, per-thread order, cross-thread order by reservation; **less than today's bpftime**: a line can appear after output that the application prints later |
| J benchmark ops (update/lookup+`(*v)++`/delete, constant key) | per call; `++` racy | `++` collapses per warp | **more** for `++`, same for the others |

### Where UnitFold guarantees less, stated precisely
1. **printk real-time position.** A line is printed within one watcher turn of its commit: at most
   1 ms, because the park backstop is 1000 us (`runtime/src/attach/cuda_watcher_policy.hpp:27-28`).
   Lines are also printed at exit and before any synchronous printk. They are not printed before
   the GPU thread continues.
2. **Full-map insert order after a re-touch.**
   - New keys from deferred effects are inserted in claim order, which is the order in which their
     ledger slots were claimed.
   - Suppose a thread first touches k1, then k2, while k2's slot is older because k2 was deleted
     by the host earlier and touched again. If only one entry is free, k2 may get in and k1 not.
   - The fold counts and reports every such E2BIG drop.
   - This cannot happen when the key domain bound (R6) is at most `max_entries`.
3. **CPU plain stores through a pointer, during a mode switch.** A plain CPU store through a
   lookup pointer, made while the map switches from deferred to synchronous mode (R3, layer 4),
   can be ordered before GPU effects that committed just before it.
4. **Frozen maps.** UnitFold applies effects committed before a freeze, as Linux would for program
   writes. Today's bpftime returns EPERM to GPU writes after a freeze
   (`map_handler.cpp:445-448`); after the freeze UnitFold switches the map to synchronous mode,
   which restores that behaviour.

## R2. Difference from gpu_ext and from upstream automatic warp execution

| | gpu_ext (ASPLOS '27 text; its repository) | upstream `official/revision/automatic-warp-execution` | UnitFold |
|---|---|---|---|
| unit of decision | tool rewritten by hand to GPU map types, warp-leader `if`, fitted buffers | one yes/no per program, opt-in (`BPFTIME_GPU_AUTO_WARP_EXECUTION`), unmerged | each call site, or each recognised idiom unit, automatically at link time |
| what runs | leader writes into shards; "merged at kernel completion" with "relaxed, eventual consistency" (paper; no implementation found by gpu_ext's own audit) | the whole handler in one leader lane (activemask + `vote.ballot`) | every lane's action, reduced exactly |
| aggregation | election (leader writes) | election: the leader alone runs; `*v=*v+1` with a uniform key counts once per converged group (read, not run) | reduction: Σ of the group's deltas, or a repeat count for identical printk lines; leaders apply the group's sum, never stand in for it |
| when the host sees effects | at merge points (kernel completion) | immediately (the leader's call is synchronous) | at every host access: readers fold committed effects themselves |
| admission | – | rejects atomics, printk/puts/perf output/exit, per-thread map writes, non-GPU maps | counter atomics, printk, per-thread records and CPU HASH maps are the main targets; anything unproven falls back per site |
| maps | GPU map types (1502/1503/1513) and ringbuf | shared GPU maps only (1501/1503/1504/1513) | ordinary HASH (and 1501, which becomes HASH on the Orin); loaders unchanged |
| hardware | per-SM shards in prose | `vote.ballot` text, PTX ≥ 6.0 | only cost parameters (R8); baseline grouping uses only primitives already in the trampoline |

Sources: `results/gpu/overlap-survey-20261113/README.md`;
`e27d009b:bpftime-verifier/src/gpu/warp_execution_eligibility.cpp:121-214`;
`e27d009b:attach/nv_attach_impl/pass/ptxpass_core/src/core.cpp:336-353,403-514`.

### Why UnitFold's defer and aggregate are exact rather than eventual
1. **Effects are recorded as composable summaries, never as values to overwrite later.**
   - An add is a monotone counter `D` per key.
   - An overwrite or delete is a versioned last-writer record that also snapshots `D` (`wD`).
   - The fold of a key computes exactly the composition of all committed effects on top of the
     current map value. It does so under the key's stripe lock, and only immediately before a host
     operation on that key, so the map value it composes with is the value those effects would
     have met (Fold lemma, R3).
2. **No merge point is needed.** Readers fold, so nothing depends on seeing kernel completion. The
   hooks cannot see completion for graph launches or CUDA 13 kernel-handle launches
   (`nv_attach_impl.cpp:1561-1592`).
3. **Aggregation is a reduction over the lanes that executed the unit.** It is computed after
   grouping lanes by the exact key bytes at run time, so it equals the lanes' individual actions
   placed next to each other in ≺. They are concurrent, so any adjacent placement is legal.
4. **Reads are validated, not assumed.** A GPU-local read uses the ledger only if no host mutation
   of M happened since the value was written (epoch check); otherwise it is synchronous.

### Why UnitFold decides per call site rather than per program
- A program with one printk carrying `%s` (not deferrable) still defers its counters.
- A program whose hash map is touched by a CPU pointer writer keeps those sites synchronous and
  defers its printk.
- The decision is carried per site in the bytecode, not as a program flag in `RuntimeInput`.

## R3. "Only GPU probes touch this map" is unknowable at load time: a dynamic answer

UnitFold never needs that fact. It layers five mechanisms.

### Layer 1: fold on access (merge on read and before every write)
- **Where the hook sits.** The fold hook is in `bpf_map_handler`'s entry points:
  - `map_lookup_elem` (`map_handler.cpp:135`), `map_update_elem` (`:441`),
    `map_delete_elem` (`:773`, and its counted wrapper `:1924`);
  - `bpf_map_get_next_key` (`:617`), `map_lookup_and_delete_elem` (`:1797`);
  - the `diagnostic_direct_*` functions (`:391-440`).

  Every path converges there:
  - the loader's syscalls (`syscall_context.cpp:600-680`, through
    `bpftime_shm_internal.cpp:290-418`);
  - CPU-probe helpers (`runtime/src/bpf_helper.cpp:401-422`, `from_syscall=false`, including the
    cached-handler and direct-map branches at `bpftime_shm_internal.cpp:295-315`);
  - the watcher's host calls (`bpf_attach_ctx_cuda.cpp:136,152,159`).
- **Cost when a map has no ledger:** one null test on a new `offset_ptr` field in the handler.
- **The fold for key k:**
  1. Take the stripe lock of k in the HASH map (bpftime_hash_map's stripe; `CLAUDE.md:761`).
  2. For each ledger of M whose recorded generation equals M's current `gen`, find k's slot and
     apply its pending effects (R6 gives the fold function).
  3. Run the host operation under the same lock.
  4. For a mutation (update, delete, lookup-and-delete), and for any lookup that hands out a
     writable pointer (CPU-probe lookups, watcher-served GPU lookups), increment M's host epoch
     in each ledger of M (release store).
  5. Unlock.

  `get_next_key(NULL)` first folds every READY slot of every ledger of M in claim order, taking
  each key's stripe lock in turn.
- **Invariant I.** For every key k of a ledgered map, the stored value of k changes only:
  - (i) in a fold of k, or
  - (ii) in a host operation on k that runs in the same stripe-lock section right after a fold of k.

  The one exception is GPU or CPU writes through pointers; layer 4 handles it.
- **Fold lemma.**
  - *Claim.* Under Invariant I, for every effect e on k that committed after the last fold of k,
    the stored value of k when the next fold applies e equals its value when e committed.
  - *Consequence.* Applying committed effects at fold time, in their per-slot order (adds commute;
    a writer record supersedes the adds before its `wD` snapshot), yields the same value as
    applying them at their commit points. Choosing the commit points as linearization points gives
    an SR order.
  - *Why the order respects the rules.* The order respects real time because the host operation's
    fold reads every word with an acquire load after the operation began. An effect that committed
    earlier is visible, given the visibility discipline in the table below (inferred, same
    assumption as today's request protocol and 1527). Program order holds because a thread's
    effects on one key go to one slot in issue order.
- **Effects in flight during a fold or host mutation.**
  - An add whose `atom.add` lands after the fold's load of `D` is ordered after the host operation.
    That is legal because the add had not committed when the operation began.
  - A writer record that is mid-update (odd version) makes the fold apply only the adds.
    - This is safe: a later application of the record overwrites the value with `wval ⊕ (D - wD)`,
      which already contains those adds.
    - The record is then linearized after the host operation, which is legal because it had not
      returned.
- **No channel to the agent is needed.** The loader folds in its own process. The ledger and every
  word the fold reads are in the segment. No CPU process ever posts to `CommSharedMem`, whose
  `occupy_flag` is taken with GPU atomics only (`default_trampoline.cu:149`).

### Layer 2: word discipline (no word written by both CPU and GPU)
The design relies on three things:
- (a) GPU atomics on mapped page-locked memory are atomic with respect to other GPU threads
  (CUDA programming guide, "mapped memory"; it also says they are *not* atomic from the host's
  point of view: (external)).
- (b) `membar.sys` followed by a CPU acquire load orders GPU writes before CPU reads. This is the
  discipline of the request protocol (`default_trampoline.cu:152-166`;
  `bpf_attach_ctx_cuda.cpp:303-317`) and of the 1527 ring (`default_trampoline.cu:430-485`;
  `runtime/src/bpf_map/gpu/nv_gpu_ringbuf_map.cpp:56-104`).
- (c) An aligned 64-bit GPU store or atomic is seen whole by a CPU load (inferred; checked by a
  torn-read stress test in R10).

It does not rely on CPU and GPU atomics on one word being mutually atomic: that is open
(`hostNativeAtomicSupported` not queried).

| word | written by | read by | ordering |
|---|---|---|---|
| slot `state`, `claim_seq`, `key` | GPU (CAS EMPTY→CLAIMING, stores, store READY) | GPU, CPU | GPU: key, `membar.sys`, READY; CPU: acquire `state`, then key |
| slot `D` (add counter, u64) | GPU `atom.add` only | GPU, CPU | GPU `membar.sys` after the add; CPU acquire load |
| slot writer record `wver`, `wkind`, `wD`, `wepoch`, `wval` | GPU, under an agent-private device-memory lock | GPU, CPU | seqlock: `wver` odd, `membar.sys`, fields, `membar.sys`, `wver` even |
| fold state (`folded_wver`, `folded_D`) per slot | CPU only, under the key's stripe lock | CPU | – |
| ledger `mode`, `host_epoch` | CPU only (release stores) | GPU (`ld.volatile` after `membar.sys`) | – |
| ledger stats, `claim_ctr` | GPU atomics | CPU | relaxed |
| printk ring `head`; record fields, then `seq` | GPU | CPU | record, `membar.sys`, `seq` = reservation index + 1; CPU acquire `seq` |
| printk ring `tail` | CPU only | GPU | release store |

The fold's in-place add writes the *map's* element, not a ledger word. That element is not written
by GPU code while the ledger is in DEFER mode (layer 4).

### Layer 3: host-mutation epoch and mode word (validation of GPU-local reads)
- **`res_lookup(k)` (helper 603) protocol:**
  1. `e1 = host_epoch`.
  2. Find k's slot without claiming one.
  3. Seqlock-read the writer record (odd → fall back).
  4. Load `D`.
  5. `e2 = host_epoch`.
  6. Use the record only if `wkind ∈ {SET, DEL}` and `wepoch == e1 == e2`.
  7. SET → copy `wval ⊕ (D - wD)` into the program's stack scratch and return its address.
     DEL → return NULL (or `c ⊕ (D - wD)` for an upsert-add map with adds after the delete).
- **Otherwise it falls back to the new `LOOKUP_COPY` host op.** The watcher folds k and copies the
  value out; no pointer escapes, so the epoch is not bumped and reads do not thrash.
- **Linearization.**
  - A validated read linearizes at the `e1` load.
  - If a host mutation completed before the read began, its epoch increment (release, after the
    mutation) precedes `e1`, and the read falls back.
  - A host mutation still in progress (folded and mutated, not yet bumped) is concurrent, so
    ordering the read before it is legal.
  - A SET records `wepoch` = the epoch it read inside its critical section. A host mutation that
    races with the SET either folded it (and bumped after) or not (and the SET is ordered later);
    in both cases later reads see `wepoch ≠ host_epoch` and go synchronous.
- **`mode` (CPU-written) is checked by every deferred helper:**
  - DEFER: use the ledger.
  - SYNC: run the unit through its fallback host op.
  - DEAD: drop the effect (map closed: Linux semantics, below).

  A ledger in SYNC mode stays foldable, so effects committed around a mode change are never lost.
- **Where the epoch lives.** The epoch lives in each of M's ledgers, and a host mutation bumps all
  of them. This keeps the device-side check to one header load. `map_info`, the per-module
  snapshot (`default_trampoline.cu:101`; written once at module load,
  `nv_attach_impl.cpp:2366-2385`), carries only the ledger's address in `extra_buffer`. All
  changing state is behind that pointer, so snapshots never go stale.

### Layer 4: pointer writers (the only writes Invariant I cannot see)
- **The problem.** Plain stores through a map-value pointer bypass every hook. GPU code gets such
  pointers from synchronous lookups: today `host_call` returns a device alias of the live element
  (`bpf_attach_ctx_cuda.cpp:136-144,559-574`). CPU probes get them from `bpf_map_lookup_elem`.
- **The rule for GPU code.** A map is in DEFER mode only while *no GPU site anywhere* writes
  through a lookup pointer of M, atomically or not.
  - A GPU `atom.add` through a pointer would meet the fold's CPU in-place add on the same word, and
    whether CPU and GPU atomics on registered memory are mutually atomic is open (layer 2).
  - This includes unrewritten counter sites: another link, a failed match, or analysis switched
    off.
  - The analyzer classifies pointer uses of every GPU program it sees at link time and sets
    `gpu_ptr_writer` on M's handler in the segment. A map with that flag gets no ledger, or its
    ledgers flip to SYNC.
- **The rule for CPU programs.** CPU programs that write through pointers *only with atomic adds*
  are allowed when M's ledgers carry only add effects. The fold's add and the probe's add are both
  CPU atomics, and adds commute. Any other CPU pointer write (plain store, plain RMW, xchg/cmpxchg,
  or an add on a map whose ledgers carry SET/DEL) sets `cpu_ptr_writer` and flips M's ledgers to
  SYNC.
  - The same analyzer runs on CPU programs when the agent instantiates them.
  - A program found after a ledger exists flips all of M's ledgers to SYNC.
- **The residual window.** Effects already committed when the flip happens can still be folded
  after a racing CPU plain store (R1, deviation 3). A handshake that closes it (per-SM in-flight
  counters read by the flipping CPU) is left for later, because it needs store-load ordering across
  CPU and GPU, which is not established on this board.

### Layer 5: identity, close and fd reuse
- **Generation.** The handler gets `gen`, a 64-bit counter from a segment-wide counter at creation.
  Today `id` is just the fd (`map_handler.hpp:89-102,370`). Each ledger records `(fd, gen)`.
- **Close.** `close()` in the loader (`syscall_context.cpp:273-296` → `bpftime_shm::close_fd`,
  `bpftime_shm_internal.cpp:852-857` → `clear_id_at`) gets a hook that sets every ledger of the
  closed map to DEAD and leaves it allocated. Modules cannot be unloaded and keep its device
  address (`nv_attach_impl.cpp:249-277`).
- **Effects on a closed map.**
  - GPU effects on a DEAD ledger are dropped.
  - In Linux, the program would still reference the old map object, which no user-space fd can
    reach any more, so dropping is unobservable.
  - Fallback host ops carry `gen`; the watcher drops a request whose `gen` differs from the
    handler now at that fd.
  - Today's synchronous sites (unchanged `call 1/2/3`) keep their current behaviour: after fd reuse
    they reach the new map with stale sizes. That is a pre-existing hazard, which UnitFold does not
    widen.
- **Maps missing from `map_info`.** A map created after the module loaded, or with fd ≥ 1024, is
  absent from `map_info` (`bpf_attach_ctx_cuda.cpp:505-554`). The analyzer never rewrites a site
  whose map is not in the snapshot it was given.

### How the five layers answer the constraints in the task
| fact | answer |
|---|---|
| loader reads run in another process directly on the segment | the fold runs in the reading process at the handler; ledgers are in the segment; no agent involvement |
| request slot unsafe for CPU posters | never used by CPUs; the fold reads GPU-written words only with loads |
| hash value pointers unstable across update/delete | ledgers name keys by bytes, never by element pointer; the fold's add is in place under the stripe lock (`fold_add_locked`, shaped like `with_existing_value`, `bpftime_hash_map.hpp:699-709`), never `elem_update` (which would `replace_locked`, `:528-538`, and strand pointers that synchronous lanes and CPU probes hold); `res_lookup` returns a stack copy |
| maps closed, fds reused | `gen` in ledger and fallback requests; DEAD mode |
| `map_info` is a snapshot | it holds a pointer to live ledger state; absent maps are never rewritten |
| CPU probes in the agent touch the same maps | they fold through the same handler entry points; their lookups bump the epoch; their pointer writes are classified (layer 4) |

## R4. Idioms as units

### How the analyzer recognises a unit (bytecode level, one program)
A unit is a single-entry region of the CFG, anchored at a `call 1` (lookup) or a `call 2`/`call 3`.
It is accepted only when all of these hold:
- **K (key equality).** Every helper in the unit gets the same key. Either the key register points
  to the same stack slot with no store into its bytes inside the unit, or the stack contents have
  the same value number (PR #624's `stack_bytes` tracking, extended with value numbers).
- **P (pointer confinement).** The lookup result `r0` and every copy of it are used only:
  - in the null test;
  - in loads and stores at one offset and width `(off, w)` inside the unit;
  - in a `lock ... +=` at `(off, w)`;
  - as the value pointer of the unit's own `BPF_EXIST` update.

  A use after the unit's exit, a store of the pointer, or a use as another helper's argument
  rejects the unit.
- **V (value algebra).** The unit's effect, as a function of the old value, is in the add family:
  - `f(x) = x ⊕ d`, with `⊕` adding d at `(off, w)`;
  - `f(⊥) = c ⊕ d` (upsert) or `f(⊥) = ⊥` (add-if-present);
  - d is a register value, and c is a constant template (other value bytes constant, the add field
    of the inserted value equal to `d + c0` for a constant `c0`).

  "Inserted value equals delta" is proven by value numbering: the NOEXIST value's slot holds the
  same value number as d.
- **L (liveness at exits).** Everything the unit *defines* is dead at every unit exit, unless the
  rewrite reproduces it with the same value number. That covers registers and stack bytes written
  inside the unit, such as the loaded old value, shape C's `new_cnt` stack slot, the update's r0,
  and r0-r5 clobbered by the new call. Values the unit only reads (the key slot, the delta's stack
  slot that cuda-counter's printk reads again at @107) are kept as they were.
- **C (control).** No jump from outside the region lands inside it. The region contains no helper
  call or atomic other than the unit's own.

### Rewriting
- A general bytecode patcher inserts and removes instructions and fixes every jump offset that
  crosses an edit. This is the kernel verifier's `adjust_jmp_off` idea; memcapture prepends
  instructions without fixups (`ptxpass_kprobe_memcapture/main.cpp:53-57`).
- The unit becomes argument setup plus one `call 60x`, then a jump to the unit's join.
- Site descriptors are immediates: a 64-bit `desc` through `LDDW`, and `site_id` (an agent-wide
  counter) in r5.
- After rewriting, the abstract interpreter re-runs on the new program. The unit is kept only if
  the abstract state at each exit equals the original's on all live locations, *including value
  numbers*, not only scopes and constants. Otherwise that unit stays synchronous. This is a light
  translation validation.

### The eight shapes (59 bpftime host-call sites; `dir2-read/groups.txt`)
| shape | sites | bytecode (example) | unit op | helper |
|---|---:|---|---|---|
| A counter | 10 | cuda-counter entry: `call 1` @26; `if r0==0` @27; `lock *(u64*)(r0+0) += r6` @28; miss: `r4 = 1; call 2` @36-37 with value slot r10-0x30 = r6 = 1 (@20-21) | UPSERT_ADD(d=r6, c=0) | 600 |
| B non-atomic RMW, EXIST self-write, else NOEXIST | 9 | cuda-counter retprobe: `call 1` @31; hit: `r1=*(u64*)(r0+0); r1+=r7; *(u64*)(r0+0)=r1` @33-35, `r3 = r0; r4 = 2` @40-41; miss: `r3 = r10-0x28; r4 = 1` @71-75; **one merged** `call 2` @76 | UPSERT_ADD(d=r7, c=0) | 600 |
| C leader lookup + update(ANY, `p ? *p+d : d`) | 4 | cudagraph `call_count`, `total_time_ns` (`example/gpu/cudagraph/cuda_probe.bpf.c:56-58,79-81`) | UPSERT_ADD | 600 |
| E entry/exit timestamps | 9 | entry `update(start_ts, &blk, &ts, ANY)`; exit `lookup` (value used: @19 and @105), `delete` | SET; validated read; DEL | 601; 603; 602 |
| F per-thread record | 2 | `update(thread_mapping, &(blk<<16\|tid), &info, ANY)` (`threadscheduling.bpf.c:86`) | SET (no aggregation) | 601 |
| G leader publish | 1 | `update(counter_per_thread, &k, &v, ANY)` (`gpu_shared_map.bpf.c:55`) | SET | 601 |
| H printk | 21 | format built by immediate stores, e.g. cuda-counter entry @38-54; `call 6` @60 | deferred line, format interned | 604 |
| J benchmark hash ops | 3 | `update(k0, ANY)`; `p = lookup(k0); if (p) (*p)++`; `delete(k0)` (`benchmark/gpu/micro/cuda_probe.bpf.c:157-184`) | SET; ADD_IF_PRESENT(d=1); DEL | 601; 600; 602 |

The bytecode pcs are from `dir2-read/disasm/cuda-counter.txt` (llvm-objdump of
`example/gpu/cuda-counter/.output/cuda_probe.bpf.o`).

### Shape A (counter)
- **Today (R1).** All 32 lanes' lookups are answered before any lane branches. On first touch every
  lane misses and 32 NOEXIST inserts race: one succeeds and 31 return EEXIST, ignored. Later warps
  hit and add through device aliases of the live element (`bpf_attach_ctx_cuda.cpp:559-574`).
  Measured: 97 of 128.
- **UnitFold.** One action per lane: `UPSERT_ADD(k, 1)`.
  - Grouping: the key is `pid`, which is launch-invariant: helper 14 returns the module global
    that the agent wrote (`default_trampoline.cu:411-422`), so the static fast path applies.
  - The group leader does `atom.add(slot.D, popc(activemask) * 1)`.
  - At the next host access the fold inserts `c ⊕ ΔD` if k is absent, or adds `ΔD` in place.
  - Result: 128 of 128, the SR result.
- **The first-touch race does not exist in the unit.** Presence is evaluated by the fold, under the
  stripe lock, against the stored value, and the Fold lemma makes that equal to presence at commit
  time. Linux allows the loss; UnitFold does not produce it.

### Shape B (non-atomic RMW)
- **Bytecode details.**
  - The looked-up pointer is passed as the update's *value* (`r3 = r0` @40, flag 2 = EXIST).
  - Clang has merged the EXIST and NOEXIST updates into one call instruction @76 with path-dependent
    `r3`/`r4`. The matcher treats @76 as two logical calls by the reaching definitions of `r4`.
- **As a unit.** "If present `v := v + d`, else insert d" is UPSERT_ADD(d, c=0), the same op as A.
- **Linux** allows lost updates: two CPUs read v and both write v+d. The EXIST self-write also
  replaces the element (a new element is built from the pointer), so concurrent in-place updates of
  the old element are lost.
- **Today's bpftime** collapses converged lanes to about one update per warp (upstream #740 fixed
  the same loss in mem_trace by making it atomic; dev lacks #740).
- **UnitFold** gives Σd. The difference from Linux is only in the lossy interleavings Linux allows.

### Shape C (leader-only lookup + update ANY of a computed value)
- **The unit.** The value read is used only to form `*p + d` (or d on a miss) for an `ANY` update
  of the same key. The value is used, but only inside the unit (rule P), so the unit is
  UPSERT_ADD.
- **The fold adds in place, where Linux would replace the element.**
  - The difference is visible only to a concurrent holder of an old pointer, whose reads are racy
    in Linux anyway.
  - The map value is the same.
  - The site runs per launch, so aggregation saves nothing; the gain is the absent round trip.

### Shape E (entry/exit timestamps)
- **Entry SET.** Lanes are grouped by key (block index, block-invariant, so the static path
  applies). The group writes one record with the value of its highest active lane. Concurrent
  writers of one key may be ordered in any way, as in Linux, where the last writer wins.
- **Exit lookup.** cuda-counter's exit lookup (@14, `r6 = r0` @15) is used only by loads (@19 and
  @105, the second *after* a `delete` @27) and never written through. It becomes
  `res_lookup(map, &blk, dst = r10 - off, desc, site)`.
  - `off` is a stack range below the program's deepest access. The eBPF stack is 512 bytes and the
    GPU frame is 592 bytes per thread (`vm/compat/llvm-vm/local-call-return-block.patch:12-30`).
  - The pointer stays readable for the rest of the program, as an RCU-protected pointer to a
    deleted or replaced element does in Linux.
  - The analyzer requires that the program itself does not write in place to M between the lookup
    and the pointer's last use: no pointer stores or atomics on M's values. Helper writes are fine,
    since they replace in Linux too.
- **Exit delete.** In cuda-counter the delete deletes `&pid`, a key different from the block key
  the entry writes (`records.json`: cuda-counter:70 key_scope constant). It becomes `dfr_del`.

### Shapes F and G (blind overwrites)
- The result is unused (proven dead), so failures (E2BIG, flags) are unobservable except through the
  final map state, which the fold computes in claim and version order.
- **F** keys are per-thread. Run-time grouping finds singleton groups; the static analysis
  predicts this, so the rewrite selects the non-grouping variant.

### Shape H (printk)
- **Preconditions.**
  - The fmt bytes `[r1, r1+r2)` are constant in the stack lattice (built by immediate stores, as
    cuda-counter @38-54).
  - r2 is constant.
  - The format has no `%s`/pointer-dereferencing conversion. All 21 surveyed formats use only `%lu`
    and `%u` (grep of the 8 files).
  - The return value is dead.
- **The rewrite.** r1 becomes the interned format id and r2 a packed (size, site). The arguments
  stay in r3-r5, exactly the arguments of `_bpf_helper_ext_0006` (`default_trampoline.cu:387-409`).
  The stores that built the format are now dead, and LLVM may remove them (inferred).
- **Aggregation.** Lanes whose (format id, three arguments) are identical become one record with a
  count. The drain prints the line `count` times, so the line count is exact.

### Shape J (benchmark)
- `p = lookup(k0); if (p) (*p)++;` has no insert, so it is ADD_IF_PRESENT: `f(⊥) = ⊥`,
  `f(x) = x + 1`.
- The fold adds in place if k is present and drops the delta otherwise. By the Fold lemma, presence
  at fold time equals presence at commit time.

### Per-map kind consistency
- A map's ledgers carry one *add kind* (UPSERT with constant c, or IF_PRESENT), fixed at ledger
  creation (CAS on the handler's ledger descriptor).
- A later site of a different add kind on the same map stays synchronous.
- SET and DEL may mix with adds on one key inside one agent's ledger (`wD` orders them).
- If two agents defer on one map, every agent's sites on it must be add-only: no SET, DEL or
  `res_lookup`. Adds commute across ledgers, versions do not, and one agent's GPU-local read would
  miss another agent's adds. A later agent that finds another agent's ledger with SET, DEL or
  `res_lookup` keeps *all* its sites on M synchronous. Its host calls bump the epoch, so the first
  agent's GPU-local reads fall back. A later agent that would introduce SET, DEL or `res_lookup`
  next to another agent's add ledger also stays synchronous.

## R5. Soundness direction

- **Static fallback.** A site that matches no unit, fails any of K/P/V/L/C, uses a map type other
  than HASH (or 1501 remapped to HASH: `syscall_context.cpp:566-577`), or has a map missing from
  `map_info` keeps its original `call 1/2/3/6`. The same holds when:
  - the matched pass `.so` does not export the optional symbol `ptxpass_site_exec_abi`. The agent
    looks it up with `dlsym` next to `print_config` and `process_input`
    (`nv_attach_impl.cpp:601-614`), so an older pass is detected without depending on how
    `from_json` treats unknown config keys;
  - `run_with_kernel` is set;
  - the rewrite fails re-analysis.
- **Dynamic fallback.** Each new helper falls back when:
  - `__bpftime_defer_ctl` is 0 (module path that did not write it);
  - `extra_buffer` is null (no ledger);
  - `mode != DEFER`;
  - the ledger is full;
  - the epoch check fails;
  - the printk ring is full;
  - the grouping scratch tag check fails (grouping only falls back to per-lane ledger ops).

  The fallback runs the *unit's* semantics through one host call:
  - `UNIT_ADD(gen, key, Σd, kind, c)`: the watcher folds k and applies the add under the stripe
    lock;
  - `UNIT_SET`, `UNIT_DEL`;
  - `LOOKUP_COPY`;
  - printk: the existing `TRACE_PRINTK`, after the watcher drains the ring.

  Fallbacks are therefore exact (SR) and one per group, not one per lane.
- **The synchronous path stays.** Its waiting rules are unchanged: per-module lock, `occupy_flag`,
  32 lane turns, `membar.sys` and the flag1/flag2 handshake (`default_trampoline.cu:134-175`).
  Two things are added on the host side of a synchronous call to a ledgered map:
  - the keyed fold, O(1);
  - an epoch bump for mutations and pointer-returning lookups.

  No GPU thread holds an uncommitted effect when it posts a request: every new helper commits
  (`membar.sys`) before returning, and the existing `fence_system()` before `flag1`
  (`default_trampoline.cu:157-158`) orders the thread's earlier effects before the host serves it.
- **Unregistered ids.** The ids the rewriter can emit and the ids `compile_ebpf_to_ptx_from_words`
  registers come from one shared header, and a unit test asserts that the first set is contained
  in the second. An unregistered id or a map-value LDDW makes `generate_ptx` call `exit()` in the
  application (`llvm_jit_context.cpp:936-939`, inferred, not run). The capability gate keeps a new
  id from reaching a stale pass `.so`.
- **What uniformity may decide.** Uniformity facts never decide correctness except in the static
  fast path, and that path uses only *value invariance*: a value computed only from constants, pid
  (helper 14), block index/dim and grid dim, never from loads of map values. This is a property of
  one thread's dataflow; it does not depend on convergence. Hardware-position values (`%smid`,
  `%warpid`) and `%globaltimer` always take the run-time grouping path.

## R6. Storage: where deferred and aggregated data lives, how big, what happens when full, who drains

### Ledger (one per map M and agent; in the Boost segment)
- **Allocation and addressing.** The agent allocates it with Boost interprocess when the analyzer
  first defers a site on M. The agent already allocates in the segment
  (`runtime/agent/agent.cpp:1195-1199`). The device address comes from the agent's
  `cuMemHostRegister` of the whole segment (`bpftime_shm_internal.cpp:1407-1475`), translated as
  `gpu_address_of_map_value` does (`bpf_attach_ctx_cuda.cpp:559-574`).
- **Header (128 B).** magic, `fd`, `gen`, `key_size`, `value_size`, add kind, add field
  `(off, w = 8)`, constant c, capacity `C`, probe limit `P = 16`, `mode`, `host_epoch`, `claim_ctr`,
  stats (claims, full fallbacks, E2BIG drops), and the device address of its lock array.
- **Slot.** `state` u32, `claim_seq` u64, `key[ks8]`, `D` u64, `wver` u64, `wkind` u32, `wD` u64,
  `wepoch` u64, `wval[vs8]`, padded to 64 B. `ks8` and `vs8` are the sizes rounded up to 8.
- **CPU fold state.** A parallel array of `C × 16 B`, in the segment.
- **Device lock array.** `C × 4 B` of agent-private `cuMemAlloc` memory, for SET/DEL writers. Only
  this agent's GPU code writes this ledger's records, so a device-memory lock (fast L2 atomics)
  is enough.
- **Sizing.**
  - `C = clamp(next_pow2(2 × min(max_entries, keybound)), 64, C_max)`.
  - `keybound` is the analyzer's bound on the number of distinct keys: 1 for constant or pid keys;
    `num_SM` for `%smid` keys; `num_SM × max_warps_per_SM` for `(smid, warpid)` keys; otherwise
    `max_entries`.
  - Per-ledger budget `C_max = 64 Ki` slots; total ledger budget 16 MiB of the 128 MiB default
    segment (`BPFTIME_SHM_MEMORY_MB`).
  - When the budget is exhausted the analyzer ranks maps by predicted saving (frequency × the
    round trips removed) and leaves the rest synchronous.
- **Examples (estimates).**
  - cuda-counter `call_count` (pid key): 64 slots × ~128 B = 8 KiB.
  - threadscheduling `thread_mapping` (4,096 entries, 80-byte value): 8,192 slots × ~192 B ≈ 1.5 MiB.
  - `sm_histogram` (8 SMs on the Orin Nano): 64 slots.
- **When full.**
  - Claiming probes P slots. If none matches or is EMPTY, the group falls back to its unit host op,
    which is exact; the ledger's full counter counts it.
  - Slots are not reclaimed in the first version, so a key without a slot stays synchronous from
    then on. Mixing is still exact, because a key's effects go either to its one slot or through the
    fold-then-op host path.
  - Reclamation needs proof that no GPU thread still holds a slot index. That needs completion
    knowledge, which the hooks lack for graph launches. It is future work, using per-SM in-flight
    counters.
- **Who drains.** Nobody needs to: every accessor folds. Optionally the watcher folds dirty ledgers
  every 10 ms while kernels may run, which only bounds the next accessor's work.
- **Lifetime.**
  - A ledger is never freed while its agent lives: modules keep its address and cannot be
    unloaded.
  - After the agent exits (pid gone from the alive-agent set), any process that has folded the
    ledger completely frees it.
  - Effects of an application that exits before the loader reads therefore survive. Today they
    are already in the map, so this matches today.

### printk ring (agent-private; one per agent)
- **Memory.** Pinned host memory, allocated with `cuMemHostAlloc(DEVICEMAP)` by the agent. It is
  not in the segment: only the agent's watcher prints, as today
  (`bpf_attach_ctx_cuda.cpp:162-170` → `bpftime_trace_printk`).
- **Records.** `{seq u64, fmt_id u32, count u32, args[3] u64}` = 40 B. N = 64 Ki records (2.5 MiB)
  by default.
- **Reservation.** The warp leader reserves g records for its g distinct lines with one CAS on
  `head` after checking `head - tail < N`.
- **Full ring.** The lane calls `TRACE_PRINTK` synchronously. The watcher drains the ring first and
  then prints, so per-thread order holds.
- **Drained by the watcher**, on every loop turn (≤ 1 ms, and on every request), and in
  `stop_cuda_watcher_thread_at_exit` (`bpf_attach_ctx_cuda.cpp:88-109`) before the join. A record
  reserved but not committed stops the drain until the next turn; at exit the drain waits at most
  100 ms.
- **The format table** is agent-local, built by the analyzer from constant stack bytes.

### Warp grouping scratch (agent-private device memory)
- **Size.** `num_SM × max_warps_per_SM` entries of `{tag u64, key[32] u64, val[32] u64}`: 8 × 48 ×
  520 B ≈ 195 KiB on the Orin Nano (sm_87: 48 warps per SM, (external)).
- **Indexing.** By `(%smid, %warpid)`; both are already read in the trampoline.
- **The tag.** The tag is the warp's launch-unique id. It is written by the lowest active lane and
  checked after `__syncwarp`, so a warp rescheduled to another slot (PTX: `%warpid` may change after
  preemption, (external)) only loses aggregation, never correctness.
- **Primitives.** Only activemask, `bar.warp.sync`, `%laneid` and memory; LLVM's `popc` for the
  static path. No ballot, shfl, match or redux, which are untested as PTX text after the sm_87
  retarget. Those are an optimisation enabled by a start-up acceptance test.

### Module global `__bpftime_defer_ctl`
- It points to `{printk ring, grouping scratch, num_SM, max_warps}`.
- It is written on all three load paths: substitution (`write_trampoline_globals`,
  `nv_attach_impl.cpp:2366-2385`), late (`nv_attach_fatbin_record.cpp:263-292`) and directly-run
  (`nv_attach_impl.cpp:1323-1352`), next to the `__bpftime_pid_tgid` write
  (`nv_attach_utils.cpp:155-171`).
- If it is unwritten (0), every new helper falls back.

## R7. The analyzer

### Where and when it runs
- **Process:** the traced CUDA application (the agent).
- **Step:** CUDA link instantiation, in `bpf_attach_ctx`'s CUDA branch
  (`runtime/src/attach/bpf_attach_ctx.cpp:636-648`): after `create_map_basic_info(1024)` and
  before `create_attach_with_ebpf_callback`, which stores the instructions into `nv_attach_entry`
  (`nv_attach_impl.cpp:217-233`).
- **Why there.** Running on the runtime side gives segment access (ledgers, handler flags) that
  `nv_attach_impl` does not need. The rewritten instructions flow through the existing
  `nv_attach_private_data.instructions`, so the `nv_attach_impl` interface is unchanged except for a
  capability query.
- **Scope.** One link (one program) at a time. Cross-program facts (add kind, ledger existence,
  pointer writers) live in the handler's ledger descriptor in the segment, not in the
  last-link-wins `map_basic_info` (`nv_attach_impl.cpp:233`).

### Inputs
1. The program's stored original instructions (`prog->get_insns()`, `bpf_attach_ctx.cpp:639`).
2. The fd-indexed `MapBasicInfo` snapshot (type, key/value size, `max_entries`).
3. The attach type (entry, return, memcapture) and the kernel names.
4. M's handler state in the segment: `gen`, ledger descriptor (add kind, kinds in use, other
   agents), `ptr_writer`, frozen.
5. Cost parameters from start-up calibration (R8).
6. `BPFTIME_GPU_SITE_EXEC=off|analyze|on` (default `off` until the first slice is validated;
   `analyze` writes records without rewriting), and per-site overrides for tests and mutations.

### Passes
1. **CFG**, dominators and post-dominators; programs with BPF-to-BPF calls are left unchanged in
   the first version.
2. **Abstract interpretation**, ported from PR #624's worklist over raw eBPF
   (`official/codex/gpu-simt-verifier-clean:bpftime-verifier/src/gpu/uniformity_analysis.hpp:18-72`:
   per-pc registers, pointer provenance, `map_fds`, 512 stack bytes, stack pointer slots). It is
   copied without PREVAIL headers; dev builds have `ENABLE_EBPF_VERIFIER` OFF
   (`cmake/StandardSettings.cmake:105`). Its two-valued `Uniformity` is replaced by a product
   lattice:
   - `const(c)` / `map_fd(n)` / `stack_ptr(off)` / `map_value_ptr(site)` / scalar;
   - provenance scope `{const, launch, block, warp-position, thread}` with a timing flag for
     `%globaltimer`;
   - a value number for equality.

   Helper 14 maps to the constant `(tgid<<32)|tgid` that the agent writes into modules
   (`nv_attach_utils.cpp:155-159`). Helpers 503/504/508 → block or launch scope; 509 → block scope
   (a block runs on one SM); 510 → warp-position; 505/511 → thread; 502 → thread scope with the
   timing flag. The out-parameters of 503/505 write those scopes into stack bytes.
3. **Def-use and liveness**: `result_use` (unused / null_check_only / value_used / pointer_rmw) and
   rule P's pointer confinement.
4. **Unit matcher** (R4).
5. **Decision and cost.** Per site, choose among {unchanged, 600, 601, 602, 603, 604} and the
   grouping variant; check per-map kind consistency against the handler descriptor; create the
   ledger and set `extra_buffer` in this link's `map_basic_info` copy (and through the handler for
   later links).
6. **Rewrite and re-analysis.** Rewrite with jump fixup, then re-analyse (R4).

### How decisions reach code generation, around each known obstacle
| obstacle | how UnitFold avoids it |
|---|---|
| `generate_ptx` has no hook; one `optimizeModule` (`llvm_jit_context.cpp:903-975`) | no LLVM change: the decision is in the bytecode the pass already compiles (`core.cpp:226-264`) |
| IR carries no eBPF pc | no pc is used: the callee id is the mechanism, and immediates in argument registers are the site's descriptor |
| O3 may merge, sink or duplicate calls | calls to different helper ids cannot be merged into one call (inferred: SimplifyCFG sinking requires the same callee). Calls to the same id with different descriptors may be sunk with a phi, which stays correct because the descriptor is data. Duplication is harmless. Checked in R10 by dumping PTX. |
| unregistered helper ids `exit()` the application | ids 600-604 registered in `core.cpp:234-253`; containment test; capability gate on the pass's exported `ptxpass_site_exec_abi` symbol |
| memcapture prepends instructions and compiles once per access (`ptxpass_kprobe_memcapture/main.cpp:60-147`) | pc-free encoding; `res_lookup` (stack scratch) is disabled under memcapture in the first version, because its prepended stores use stack slots this design has not mapped |
| new callee names vs the extern filter (`core.cpp:280-283`, `_bpf_helper_ext_\d{4}`) | four-digit ids (`_bpf_helper_ext_0600`...), so declarations are filtered like the existing ones; no per-site callees, so two probes on one kernel never define the same new function |
| patch cache keyed by the request JSON (`nv_attach_impl.cpp:994-1027`) | rewritten words are part of the key. Site ids come from an agent-wide counter in analysis order; each link is analysed once and the cache is per session (`nv_attach_impl.cpp:1103-1107,1716`), so all PTX files patched for one link share one rewritten program and its cache entries |

### Output records (survey schema plus decision)
For each logical call site, the analyzer emits one JSON record with the survey's fields:
- `file`, `line`, `section`: in the agent, `program_name` and `pc`; in the offline driver, from
  `.BTF.ext` line info;
- `helper`, `map`, `map_type` (after the integrated-GPU remap);
- `host_call_today`, from a function that mirrors the trampoline dispatch
  (`default_trampoline.cu:233-485`);
- `result_use`, `key_scope` (from the provenance scope), `operation` (from the unit role);
- `cpu_reads_map`: a declared **external input**, `unknown` in deployment;
- `aggregatable` (three-valued: yes / no / timing);
- `frequency`: `per_launch` when the site is dominated by an early return unless all six thread and
  block indices are zero, else `per_thread_per_launch`;
- `idiom` (shape A-J);
- `class`.

It adds `decision`, `fallback`, `unit_id`, `site_id` and `pc`.

A logical site is (call pc, reaching definition of the flags argument). Clang's merged update @76
in cuda-counter's retprobe is therefore two records, attributed to the source lines of their
`r4 = 2` @41 and `r4 = 1` @75 instructions.

**`decision` is oracle-free.** It is defined from the program alone:
- `defer_unit` for every member of an A/B/C/J unit;
- `defer_set`, `defer_del` for blind overwrites and deletes with a dead result;
- `resident_read` for a value-used, read-only lookup;
- `defer_out` for printk;
- `unchanged` when `host_call_today = no`;
- `sync` otherwise.

**`class` is a stated function of `decision` and the external input:**
```
class(unchanged, _)        = gpu_local_already
class(d, cpu_reads_map=no) = eliminate          (d ≠ unchanged)
class(resident_read, yes)  = sync
class(d, yes | n/a)        = defer              (d ∈ defer_*)
```
In deployment `cpu_reads_map` is unknown. The runtime makes no class choice: eliminate and defer
compile to the same helpers, and whether a map stays GPU-local is decided by the epoch at run time.

### The mechanical check
- **Driver.** An offline driver (`bpftime-site-analyze`) links the same analyzer library and reads
  each `.bpf.o` of the 22 bpftime programs with libbpf.
  - Map types come from the object; the integrated-GPU remap of `bpftime_shm.hpp:137-154` is
    applied.
  - Line info comes from `.BTF.ext`.
  - rocm-counter is analysed although it cannot attach.
- **Join.** Records are joined with `records.json` on (file, line, section, helper). The survey's
  `idiom` field is missing on 48 records and is not used.
- **What it asserts, headline first:**
  1. **Per-record `decision`** for all 168 GPU sites: `unchanged` exactly on the 109 with
     `host_call_today = no`, and a non-`sync` decision on all 59 host-call sites. Each decision
     must agree with the survey's own fields:
     - `defer_set` ↔ (overwrite, unused);
     - `defer_del` ↔ (delete, unused);
     - `resident_read` ↔ (read, value_used);
     - `defer_out` ↔ (output, unused);
     - `defer_unit` ↔ an accumulate, insert_if_absent or update_if_exists member.
  2. Per-record `key_scope` equality for map sites.
  3. Aggregatable, per record, with the stance below.
  4. Class counts 109 / 20 / 39 / 0 and per-record class equality.
- **Why the class check comes last.** It is nearly vacuous: with `class = eliminate` iff
  `cpu_reads_map = no`, the 20/39 split follows from the input column for almost any decision
  table. Only the three `resident_read` rows (which would become `sync` if CPU-shared) test
  anything. The decision check is the one that tests the analyzer.

The design's decision rules were applied to the 59 host-call records with a hand-written
provenance table standing in for the analyzer's state (script and output:
`/tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/dir2-design/analysis-first/r7_check.py`,
`.../r7_records.json`). This is a simulation, not the analyzer:
- **decisions:** `defer_unit` 24, `defer_out` 21, `defer_set` 7, `defer_del` 4,
  `resident_read` 3.
  - Against the survey's (operation, result_use): **0 mismatches**.
  - The `defer_unit` members are 9 (accumulate, pointer_rmw), 2 (accumulate, unused),
    2 (accumulate, value_used: cudagraph's shape C lookups), 8 (insert_if_absent, unused) and
    3 (update_if_exists, unused).
- **class:** 20 eliminate, 39 defer, 0 sync, 109 unchanged; 0 per-record mismatches (nearly
  vacuous, see above).
- **`key_scope`** (38 map sites): 0 mismatches.
- **aggregatable:** *no consistent rule reproduces the survey's 51 record by record*.

| uniformity rule | aggregatable | records differing from the survey |
|---|---:|---|
| PR #624's helper table (502 uniform, 509 varying) | 47 | 12: the 8 threadscheduling histogram sites (keys from `%smid`) flip to no; launchlate:114/117 and launchlate-kernel-gpu-shared-map:119/122 flip to yes |
| hardware positions warp-invariant, `%globaltimer` not | 47 | 4: cuda-counter:55/82, rocm-counter:56/83 (survey yes) |
| hardware positions and `%globaltimer` warp-invariant | 55 | 4: the four launchlate printk sites (survey no) |

- **What the survey did.** It took two stances on `%globaltimer`. It accepted "the warp reads
  `%globaltimer` converged" for cuda-counter and rocm-counter, and rejected the same assumption for
  launchlate, where the read also comes first.
- **What the analyzer reports.** It reports the third value `timing` for these eight sites:
  47 yes + 8 timing + 4 no. The check passes when the 47 and the 4 match the survey record by
  record and the eight timing sites are listed. The survey's 51 = 47 + the four timing sites it
  accepted.
- **Why this is harmless.** Run-time grouping makes the stance irrelevant for correctness. It
  affects only the predicted saving.

## R8. Generality: discrete GPUs

### The model
Hardware enters only through parameters measured at agent start-up:

| parameter | meaning |
|---|---|
| `RTT_sync` | one host call, measured with a no-op request through the existing protocol |
| `c_ld_sys`, `c_st_sys`, `c_atom_sys` | segment (pinned host) memory, uncontended and with 32 and 1024 warps on one word |
| `c_atom_dev` | device memory atomics |
| `num_SM`, `max_warps_per_SM`, warp size, compute capability | device shape |
| integrated flag | from the device attribute |
| `site_exec_ptx_ok` | nvPTXCompiler accepts `match.any.sync`, `redux.sync` and `nanosleep` text at the detected arch (acceptance test) |
| `host_native_atomic` | the device attribute; recorded, not relied on |

The calibration is one small PTX module loaded by the agent once, in about 10 ms (estimate).

### Cost model per warp-group
- `sync ≈ lanes × RTT_sync`, because lanes take turns.
- `defer ≈ c_group + (2..3) × c_ld_sys + c_atom_sys(contention)`.
- `res_lookup ≈ 4 × c_ld_sys` against `RTT_sync`.

The analyzer defers when the estimate is lower, and picks the grouping variant: the static `popc`
path, memory grouping, or `match.any` if accepted.

### What changes on a discrete GPU
- **Placement.** The ledger stays in pinned host memory (the segment, registered by every agent as
  today). Only there can the loader fold without the agent, which is what makes R3 hold. GPU writes
  go over PCIe.
  - Rule (a) of layer 2 (GPU atomics on mapped page-locked memory are atomic among GPU threads)
    holds on discrete GPUs as stated in the CUDA guide (external).
  - The design never needs CPU-GPU atomicity, so `hostNativeAtomicSupported = 0` (typical on x86
    discrete GPUs, (external)) does not matter.
- **Costs.** `c_ld_sys` and `c_atom_sys` become PCIe round trips (around 1-2 us, an estimate to be
  measured), and `RTT_sync` grows. Aggregation matters more, and a device-memory key-to-slot cache
  becomes worthwhile: the slot index is looked up in device memory and only `D` and the record go
  over PCIe.
- **Older GPUs (P40, sm_61; no independent thread scheduling).** The comment at
  `default_trampoline.cu:128-133` notes that a lane spinning on a lock held by another lane of its
  own warp never lets that lane run on such GPUs. UnitFold's helpers process a warp's groups one
  after another, so only one lane per warp ever holds or waits for a slot lock or a CLAIMING slot.
  Spinning on another warp's lock is fine without independent thread scheduling. This is argued,
  not tested: the design is validated only on sm_87, and a run on an sm_61 board is needed before
  claiming it there.
- **Grouping scratch and device locks** are device memory on both kinds of GPU. The printk ring
  stays in pinned host memory: GPU writes to it are posted.
- **Semantics.** SR is unchanged. A "device-home" ledger with only kernel-completion visibility
  (S4) would be cheaper, but it breaks loader-side folding and needs completion hooks that graph
  launches bypass. It is not offered.
- **1501 on a CUDA-IPC GPU** is a real device hash whose host-call lookup returns one staging copy
  (`nv_gpu_shared_hash_map.cpp:47-62`; read, not run). The analyzer does not ledger 1501 there, so
  those sites stay synchronous until that map type is fixed.

## R9. Interactions

- **Several agents on one segment.**
  - Ledgers are per (map, agent), listed in the handler.
  - The fold visits all of them.
  - Cross-agent SET/DEL ordering is not representable, so only add kinds may span agents (R4).
  - The one `CommSharedMem` is shared and zeroed by each agent at start (`CLAUDE.md:2318-2319`).
    UnitFold sends fewer requests through it but does not fix it.
- **CPU probes on the same maps.**
  - They fold on access, so their reads see GPU effects.
  - Their lookups bump the epoch, so GPU-local reads fall back.
  - Their pointer writes are classified at instantiation (R3, layer 4).
  - Atomic adds through CPU pointers commute with ledger adds; GPU pointer writes of any kind keep
    the map out of DEFER (R3, layer 4).
- **Graph launches and unnoted kernel-handle launches.**
  - Correctness never uses launch or completion knowledge.
  - The printk ring is drained by the watcher's turns (≤ 1 ms backstop) and on ring-full fallbacks,
    which reach the watcher like any request.
  - The only thing a graph launch can delay is printk output, not map state.
- **Map close and fd reuse.** Covered by R3 layer 5: `gen`, DEAD mode, generation-checked fallbacks.
- **printk ordering.**
  - Per-thread order is kept.
  - Cross-thread order is reservation order, which is real-time consistent.
  - Deferred lines precede any later synchronous line, because the drain runs first.
  - With respect to the application's own output, a line can appear up to one watcher turn late.
  - No loss.
- **Multiple probes on one kernel.**
  - New helpers are shared trampoline functions, so they add no per-probe symbols. The existing
    `__probe_func__<kernel>` name clash for two entry probes (inferred,
    `ptxpass_kprobe_entry/main.cpp:45-97`) is neither fixed nor worsened.
  - Two programs deferring on one map share its ledger and must agree on the add kind.
  - Site ids are agent-unique, so format tables and statistics never collide.
- **Return probes and divergence.** Return probes run before every `ret`/`exit` with predicates
  (`ptxpass_kretprobe/main.cpp:50-60`), and memcapture probes after each memory access
  (`ptxpass_kprobe_memcapture/main.cpp:45-56,147`), so lanes may be divergent. Grouping uses `activemask` at the call, so a subset of lanes is grouped correctly.
- **Interleaving of deferred and synchronous sites.**
  - A synchronous site on a ledgered map is a host operation: the fold runs first, then the
    operation, then the epoch bump.
  - The issuing thread's own deferred effects are ordered before it (`membar.sys` before `flag1`).
  - A synchronous lookup on a DEFER-mode map whose result is written through is a pointer writer,
    so the map would not be in DEFER mode (layer 4). A read-only one is a plain linearizable read.
- **`run_with_kernel`, frozen maps, 1504 (backed by a kernel map) and per-CPU maps**: not
  ledgered. Freeze flips existing ledgers to SYNC after a fold.

## R10. First slice (about five weeks; measurable on the Orin)

### What to build (UPSERT_ADD and ADD_IF_PRESENT only)
1. The analyzer core: ported worklist, lattice, liveness, matcher for shapes A, B, C and the J
   lookup, rewriter with jump fixup, re-analysis, the offline driver, and the R7 check over the
   `.bpf.o` files.
2. Helper 600 (`dfr_add`) in the trampoline, with the static `popc` path and memory grouping, and
   `UNIT_ADD` as the fallback host op.
3. The ledger with add effects only (no writer record yet), created by the agent and wired into
   `extra_buffer`.
4. The handler hooks (fold before lookup, update, delete, `get_next_key`, lookup-and-delete, and the
   direct-map branches), `fold_add_locked`, `gen`, and DEAD on close. The epoch is not needed yet,
   because no GPU-local reads exist.
5. The pointer-writer flags of R3 layer 4. They are needed for soundness already in this slice:
   - `gpu_ptr_writer` comes from the same analyzer over every GPU program;
   - `cpu_ptr_writer` comes from running it over CPU programs at instantiation; only atomic adds are
     allowed next to an add ledger.
6. Registration of 600, the capability gate, and `BPFTIME_GPU_SITE_EXEC`.

Not in the slice: SET, DEL, `res_lookup` and the printk ring (slice 2, about four more weeks).

### Programs
- Upstream runner cases "CPU Hash map lookup (minimal)" (shape J) and "CUDA Counter (minimal)"
  (shapes A + B).
- `example/gpu/threadscheduling` (shape A with `%smid` and `(smid, warpid)` keys: memory-grouping
  path).
- A new micro case "CPU Hash counter (minimal / xlarge)": the shape A idiom on a HASH keyed by pid,
  added to `benchmark/gpu/micro`.

### Expected numbers against today (inferences; the proxy is the measured ARRAY case)
- **CPU Hash map lookup (minimal):** about 50 us per launch, against 339.0 us.
  - The proxy is the ordinary-ARRAY lookup at 48.2 us, which does one segment access per lane plus
    `membar.sys`.
  - The new path does one slot probe and one `atom.add` per warp.
- **CUDA Counter (minimal): about 1.8 ms per launch, against 2,877.5 us.**
  - Slice 1 removes the `call_count` lookup (and first-touch inserts) and the `total_time_ns` lookup
    and update: 3 of 8 host calls per thread, 160 instead of 264 requests per launch.
  - At (2,877.5 − 39)/264 ≈ 10.75 us per call, that is 39 + 160 × 10.75 ≈ 1.76 ms.
  - **`call_count` reads 128, against 97 today.**
  - Slice 2 would remove the remaining 5 per thread and approach the array-path cost.
- **threadscheduling:** 3 to 1 host call per thread, so about a third of the probe overhead.
  `Σ sm_histogram = Σ warp_histogram = threads × launches`. Today these sums fall short by
  first-touch losses per new key.
- **CPU Hash counter xlarge** (many warps, one key):
  - Today: about threads × 10 us, because all host calls serialize on the module lock and the slot.
  - Slice 1: about warps × `c_atom_sys(contended)`.
  - This is the case the 300 us datum warns about: contended traffic on one uncached host word
    (`CLAUDE.md:2185-2190`). That datum is CAS *spinning* by 32 warps, not one `atom.add` per warp,
    so the slice measures `c_atom_sys` under contention before claiming any number.

### How to measure
- dev `52126f33` against the same tree plus slice 1, built with the compare flags.
- The upstream `benchmark/gpu` runner, legacy-launch victims, 10 interleaved rounds as in
  hostcall-l1.
- Before each batch: `jetson_clocks` checked (cpu5 1728000, GPU 1020000000), no leftover processes,
  no bpftime segment, total timeouts outside `LD_PRELOAD`, containers removed afterwards.
- Host requests counted from the watcher's debug lines in one smoke round.
- Ledger statistics (claims, full fallbacks, E2BIG drops) dumped at exit.
- The calibration output (`c_*`) is reported with the results.
- PTX kept (`BPFTIME_CUDA_KEEP_PTX=1`) to confirm one `call _bpf_helper_ext_0600` per unit, with
  the descriptor reaching it after O3.

### Exactness tests
| test | what | pass condition |
|---|---|---|
| T1 first touch | counter on an empty HASH, N threads × L launches, 1 to 1024 warps | `call_count == N × L` |
| T2 non-atomic RMW | `total_time_ns`-style sum; every lane also writes its delta to a 1512 per-thread map | sum equals Σ of per-thread deltas |
| T3 concurrent reader | loader does `lookup_and_delete` of the key every 100 us while kernels run | Σ of all values read + final value == N × L (tests fold-before-mutation) |
| T4 CPU probe on the same key | key pre-populated by the loader; a CPU uprobe adds atomically through its lookup pointer on the same pid key during the kernels | final == initial + GPU count + CPU count. Without pre-population the CPU's own idiom (not a unit on the CPU) can lose an increment legally: its lookup misses, a fold inserts the GPU's pending value, and its NOEXIST insert returns EEXIST |
| T5 close and fd reuse | the loader closes the map mid-run and creates a new map that takes the fd | the new map receives no GPU effects; the DEAD drop counter is non-zero |
| T6 ledger full | capacity forced to 2 with 8 keys | all 8 keys exact (fallback path) |
| T7 application exits first | the loader reads after the CUDA process has exited | exact (dead agent's ledger folded) |
| T8 torn reads | 1024 warps add `0x0000000100000001` per op while a CPU loop reads `D` | every read has equal halves |
| T9 survey | offline driver over the 22 objects | R7 asserts |

### Mutation checks (each must fail a named test)
| mutation | must fail |
|---|---|
| M1: no fold in `map_lookup_elem` | T1 (the loader reads 0 or a partial count) |
| M2: fold after the host op instead of before | T3 |
| M3: fold without the stripe lock | T3 under stress |
| M4: CPU resets `D` to 0 instead of keeping `folded_D` | T3, T4 |
| M5: group sum over the whole active mask instead of the key group | threadscheduling histogram sums |
| M6: matcher drops the "inserted value == delta" check (fixture with init 1, delta 2) | analyzer fixture tests |
| M7: matcher drops the liveness check of r0 at the join (fixture returns `*p`) | analyzer fixture tests |
| M8: no `gen` check in the fold | T5 |
| M9: READY stored before the key (no `membar.sys`) | key-set check in T1 with many keys (folded keys outside the valid domain) |
| M10: helper 600 removed from the registered list | id containment unit test, before any application exit |

## Risks and open items
See the `risks` list returned with this design.
