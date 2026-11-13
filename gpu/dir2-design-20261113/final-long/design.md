# UnitFold: exact per-call-site execution of GPU eBPF helper calls

Final design for direction 2 (GPU-to-CPU host calls). It merges three drafts (analysis-first,
semantics-first, performance-first) after two reviews (correctness and novelty, feasibility).
Both reviews picked analysis-first as the base.

**Conventions**
- **Code citations.** Every `path:line` is in the dev worktree
  `/home/jetson/src/bpftime-daily-state/worktrees/4708f4092d1e/candidate` at commit 52126f33. They
  were re-read for this document.
  - llvmbpf lines refer to the submodule `vm/llvm-jit` at 87bac39a, before the patch overlay.
  - Lines on other refs name the ref.
  - Measured numbers name their results directory under `/home/jetson/src/results/gpu/`.
- **Tags.**
  - **[new]** marks a mechanism invented for this design. It does not exist in any code.
  - **[inferred]** marks a claim that combines code that was read but never run.
  - **[external]** marks a claim from CUDA or PTX documentation that was not re-read for this
    document.
- **Constraints of this round.** Nothing was built and nothing ran on the GPU. The only programs
  run were Python scripts over `records.json`.

**Files**
- This document:
  `/tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/dir2-design/final/design.md`.
- R7 simulation: `.../dir2-design/final/r7_check.py`, `.../final/r7_output.txt` and
  `.../final/r7_records.json`, all in the same scratchpad directory.
- Inputs:
  - `.../dir2-read/BRIEF.md`;
  - the reader and checker output `/tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/tasks/w63akje27.output`;
  - `/home/jetson/src/results/gpu/callsite-survey-20261113/`;
  - `/home/jetson/src/results/gpu/overlap-survey-20261113/README.md`.

## Overview

### The problem, from code and measurements

**How a host call works today.** A GPU probe's helper call becomes a synchronous GPU-to-CPU host
call when its target is:
- a HASH map or any other CPU map;
- a 1501 map, which is created as an ordinary HASH on the Orin (`runtime/include/bpftime_shm.hpp:137-154`, applied at `runtime/syscall-server/syscall_context.cpp:566-577`);
- any delete, except on an ordinary ARRAY;
- printk or puts.

The routing is in `attach/nv_attach_impl/trampoline/default_trampoline.cu:233-422`.

The call goes through the single request slot of the shared segment:
1. `host_call` takes a per-module device lock, then `occupy_flag` in `CommSharedMem`.
2. It writes the request and executes `membar.sys`.
3. It sets `flag1` and spins on `flag2`.
4. The lanes of a warp take turns over a 32-step loop (`default_trampoline.cu:134-175`).

The watcher takes `flag1` with an acquire CAS and answers with a release store of `flag2`
(`runtime/src/attach/bpf_attach_ctx_cuda.cpp:309, 317`).

**Measured cost on the Orin** (aa6eb973, `hostcall-l1-20261111/README.md:26-41`; 32 threads, 3
timed launches per section):

| case | per launch |
|---|---:|
| baseline | 39.9 us |
| CPU Array lookup, served on the GPU since 1b3d9c99 | 48.2 us |
| CPU Hash lookup | 341.1 us |
| CUDA Counter | 2,877.5 us |

- CUDA Counter makes 1,056 host requests over 4 launches, that is 264 per launch:
  8 per thread × 32 threads, plus 32 first-touch inserts in the first launch.
- One host call therefore costs (2,877.5 − 48)/264 ≈ 10.7 us.
- Since 52126f33 the watcher runs the AC3 policy. A call in a 32-call burst costs 11.8 us without
  load and 15.7 us under full load (`watcher-policy-20261112/v3/README.md:57-58`).

**Today's path also loses data.** The CUDA Counter loader prints `calls=97` in every run
(`hostcall-l1-20261111/README.md:32-33`; `baseline-sub-20261111/README.md:31`), but 4 launches × 32
threads = 128 entry probes ran. The cause, read from the code and not traced:
- `host_call` answers all 32 lookups of a warp before any lane branches.
- On first touch all 32 lanes miss, and 32 `BPF_NOEXIST` inserts race. One lands; 31 increments are lost.
- A plain `*v += d` through the shared pointer collapses in the same way to about one update per
  warp.

**The survey's starting point.** The call-site survey (`callsite-survey-20261113`) classifies
bpftime's 168 GPU call sites:

| class | sites |
|---|---:|
| already served on the GPU | 109 |
| eliminate | 20 |
| defer | 39 |
| need a synchronous answer | 0 |

Of the 59 host-call sites, 51 are marked aggregatable.

### What UnitFold does, in one paragraph

**Name.** *Units*: the analyzer recognises multi-instruction idioms, such as lookup followed by an
add or an insert, as one unit, and rewrites each unit into one call of a new registered helper.
*Fold*: GPU effects that were not sent to the host are folded into the map by whichever host
process touches that key next, before it reads or writes the key.

**How it works.**
1. A load-time analyzer in the agent classifies every helper call site from the eBPF bytecode and
   decides per site or per unit. The decision travels to code generation as a bytecode rewrite to
   new helper ids, which needs no pc.
2. The GPU commits a deferred unit with plain stores into a seqlocked *record* in the shared
   segment, then executes `membar.sys` before the helper returns. There is no host round trip.
   Lanes that share a target are reduced first.
3. Every host access path folds the key's committed records into the map under the key's stripe
   lock, right before its own operation. This covers loader `bpf()` commands, CPU probes in the
   agent and watcher-served calls.
4. Host-visible state is therefore current at every host access, with:
   - no flush;
   - no kernel-completion hook;
   - no channel from the loader to the agent;
   - no CPU-GPU atomic or store-buffering assumption.
5. The first touch of a key is a synchronous host operation, so key presence and map capacity stay
   host-decided.
6. Anything the analyzer cannot prove keeps today's synchronous path.
7. Hardware enters only as measured cost parameters and as capability bits from start-up litmus
   tests.

### The four execution modes

| task mode | UnitFold mechanism | helper |
|---|---|---|
| eliminate (no host call; data on the GPU) | Deferred records that no host process ever folds: in steady state no host work happens. The ABSENT negative cache makes add-if-present and delete of absent keys free. A GPU-local read (READ_LOCAL) is exact only with the store-buffering capability (R3.7); otherwise it uses one LOOKUP_COPY host operation per group | 600-603 |
| aggregate per warp | lanes with an equal key form one group and commit once, with count n and the sum of their deltas (R3.3) | inside 600-604 |
| defer | a record commit (ADD, SET, DEL) or a printk ring record (OUT), folded or drained by the host later | 600, 601, 602, 604 |
| synchronous round trip | today's `call 1/2/3/6` path, unchanged; plus four new host operations used only as exact per-group fallbacks (UNIT_ADD, UNIT_SET, UNIT_DEL, LOOKUP_COPY) | 1, 2, 3, 6 |

### Coined names, defined

| name | meaning |
|---|---|
| UnitFold | this design: units plus fold-on-access |
| unit | one atomic step of the reference semantics: a recognised multi-instruction idiom, any other helper call, or one BPF_ATOMIC instruction |
| ledger [new] | the per-map structure in the segment that holds GPU effects not yet folded into the map |
| slot | one key's entry in a ledger. Only the host creates slots, so a key has at most one |
| record [new] | the GPU-written state of one slot for one agent and shard: a version word `ver` and two halves of cumulative totals |
| shadow [new] | the GPU's own copy of a record in device memory. The commit computes from it and stores to the record |
| watermark | the CPU's copy of the record totals it has already folded. It is written only by the CPU |
| fold | applying a key's not-yet-folded record totals to the map element, under the key's stripe lock |
| straggler | a GPU commit that lands after a host operation has read the records, although the GPU started it before that operation's effects reached it. Stragglers are ordered after that host operation |
| hint | a CPU-written word the GPU reads (slot state LIVE, ABSENT or DEAD). It saves work; correctness never depends on how fresh it is, except for the ABSENT negative cache, which relies on H3 |
| device block [new] | per (ledger, agent) device memory: locks, shadows, the key-to-slot cache, `devmode` and the printk counter |
| flip [new] | the agent-local drain that turns a ledger synchronous when a late pointer writer appears (R3.6) |
| sweep [new] | folding the pending re-creations of a map before an operation whose outcome depends on capacity |
| strict mode | the default: one record per slot (no shards) and one deferring agent per map, which makes every host read linearizable |
| UNIT_ADD, UNIT_SET, UNIT_DEL, LOOKUP_COPY [new] | new host operations. Each runs one unit's semantics on the host under the stripe lock. UNIT_* on a key without a slot also creates the slot: the "attach" on first touch |

### What changes, and what does not

| component | change | size (estimate) |
|---|---|---|
| `runtime/src/attach/bpf_attach_ctx.cpp:245-248` (between the two instantiation loops of `init_attach_ctx_from_handlers`) | run the analyzer over every program handler; decide units, pointer writers and ledgers; create ledgers | ~40 lines + library |
| new library `runtime/src/site_exec/` [new] | bytecode CFG, abstract interpreter (ported from PR #624's worklist, without PREVAIL), unit matcher, in-place rewriter, records; offline driver `bpftime-site-analyze` | ~3k lines C++ |
| `bpf_attach_ctx.cpp:639` | pass the rewritten words instead of `prog->get_insns()` | ~5 lines |
| `bpf_attach_ctx_cuda.cpp:505-554` (`create_map_basic_info`) | `extra_buffer` of a ledgered HASH = this agent's device block | ~10 lines |
| `attach/nv_attach_impl/pass/ptxpass_core/src/core.cpp:234-253` and each pass | register ids 600-604; export `ptxpass_site_exec_abi` (looked up with `dlsym` next to `print_config`/`process_input`, `attach/nv_attach_impl/nv_attach_impl.cpp:601-614`) | ~20 lines |
| `default_trampoline.cu` | helpers 600-604, grouping, record commit, fallbacks through the existing `host_call` | ~500 lines CUDA |
| `attach/nv_attach_impl/trampoline/gpu_helper_protocol.h:22-30` | opcodes UNIT_ADD, UNIT_SET, UNIT_DEL, LOOKUP_COPY carrying the map generation | ~40 lines |
| `bpf_attach_ctx_cuda.cpp:124-187` (`serve_cuda_request`) | serve the four opcodes; fold before serving sync map requests on ledgered maps; flip-request polling; printk drain (later slice) | ~200 lines |
| `runtime/src/handler/map_handler.cpp` / `.hpp` | ledger pointer, `gen`, fold hook in every HASH entry point (R3.5), merging copy for loader lookups | ~300 lines |
| `runtime/src/bpf_map/bpftime_hash_map.hpp` | locked variants: fold-then-update, fold-then-delete, in-place atomic add (shaped like `with_existing_value`, `:699-709`), a relaxed free-element counter | ~120 lines |
| `runtime/src/bpf_helper.cpp:470-622` | the typed HASH helpers take the generic path when the handler has a ledger | ~10 lines |

**Unchanged:**
- the request protocol and lane serialisation of `host_call`;
- the watcher's sleep policies;
- all GPU map types and the ARRAY direct path;
- the loaders, libbpf and the tools.

## R1. Reference semantics

### R1.1 Actions and units

An execution consists of the following actions:

- **GPU units.** Each is executed by one GPU thread, in program order across that thread's probe
  executions (kprobe, kretprobe, memcapture). A unit is one of:
  - a recognised idiom instance (R4);
  - any other helper call;
  - one BPF_ATOMIC instruction.
- **Host operations:**
  - a loader `bpf()` map command, which runs in the loader process directly on the segment
    (`syscall_context.cpp:600-679`);
  - a CPU-probe map helper call (`runtime/src/bpf_helper.cpp:401-422, 470-622`);
  - a request served by the watcher (`bpf_attach_ctx_cuda.cpp:124-187`), including the new
    UNIT_* operations;
  - a map close.
- **printk lines.**

**Plain pointer accesses are not actions.** Plain loads and stores through map-value pointers
outside a recognised unit are data races left to the memory model, as in Linux.

**Precondition PW** (enforced in R3.6, [new]). On a map with a ledger, no attached program (GPU or
CPU) performs any of these through a lookup pointer:
- a plain store;
- a non-ADD atomic;
- on the GPU, any atomic at all.

The one exception is a flip that timed out (deviation D5).

**Intervals.**
- A GPU unit lasts from its first instruction to the return of its helper.
- A deferred unit *commits* when the even `ver` store of its record is performed. That store is
  followed by `membar.sys` before the helper returns, so the commit lies inside the interval.
- A synchronous unit commits when its host operation runs.
- A host operation lasts for its call.

**Happens-before (hb).** Real time between the CPU and the GPU can be observed only through
communication. hb is the transitive closure of:
- program order;
- fence-then-flag message passing in either direction (the request protocol, the 1527 ring, record
  publication);
- kernel completion observed by a stream or device synchronisation or an event;
- ordinary CPU-side synchronisation (system calls, locks, inter-process communication).

### R1.2 SR: the reference

An execution is **exact** if, for every map M, a total order ≺ exists over the units and host
operations on M such that:

1. **(po)** Each thread's actions on M appear in its program order. Threads here are GPU threads,
   CPU probe threads and loader threads.
2. **(rt)** If the end of a happens-before the start of b, then a ≺ b.
3. **(seq)** Replaying ≺ on Linux's sequential map specification reproduces:
   - every value returned to a probe;
   - every host return value;
   - every host read;
   - the final contents.

   The specification includes flags, EEXIST, ENOENT and E2BIG (`kernel/bpf/hashtab.c:1084-1113`,
   L4T 6.8.12). A recognised unit replays as its own instructions, executed atomically.

**printk.** Each agent has a sequence of lines satisfying (po) and (rt), in which every executed
line appears exactly once.

**What "exact" is measured against.** Against this serial order, not against today's bpftime,
which no such order explains: 97 instead of 128, and one update per warp for a plain RMW. Tests
compare with oracles computed from SR (R10).

**Scope.** SR is per map, like Linux, which gives no atomicity across maps. Commits are fenced
(R3.3), so a host reader that observes a GPU thread's later commit on any map also observes that
thread's earlier commits on every map (H2). This is cross-map program order through message
passing; it is not cross-map atomicity.

### R1.3 Observation points

**At every host access, state is current.** A host access is any operation that reaches one of the
`bpf_map_handler` entry points listed in R3.5, in any process. That covers the loader, the agent's
CPU probes, the watcher and other agents. Such an access reflects every unit whose end
happens-before the start of the access.
- `get_next_key(NULL)` sweeps pending re-creations first.
- Iteration started earlier may miss keys re-created later, as Linux iteration may miss concurrent
  inserts.
- None of this needs kernel completion, stream synchronisation, a periodic flush, launch tracking
  or a live agent. The loader folds the records of a dead agent like those of a live one.

**Not observation points:**
- ARRAY mmap reads (`syscall_context.cpp:1036-1048`): arrays are never ledgered.
- Batch commands, which fall through to the real `bpf()` (`syscall_context.cpp:852-854`).
- `run_with_kernel`, which creates no ledgers.

### R1.4 Linux, today's bpftime and UnitFold, per idiom

**What Linux guarantees.** Atomicity per helper call and per BPF_ATOMIC instruction. Beyond that:
- a plain RMW through a lookup pointer is racy;
- a lookup miss followed by a `BPF_NOEXIST` insert can lose increments;
- an update of an existing hash key replaces the element, so writes through older pointers are
  lost;
- user-space reads copy one element, with no snapshot across keys;
- printk merges per-CPU buffers by timestamp and can lose lines on overrun.

**Refinement.** Every SR execution is a serial schedule Linux could produce. UnitFold's outcomes are
therefore Linux-legal outcomes, apart from the deviations in R1.5. UnitFold additionally excludes
Linux's lossy interleavings for the recognised idioms.

The letters A to J are the survey's idiom groups (R4). Site counts come from the survey's 59
host-call sites.

| idiom (sites) | Linux | today's bpftime | UnitFold | vs Linux |
|---|---|---|---|---|
| A counter: lookup; miss → update(NOEXIST, init); hit → atomic add (10) | each op atomic; the first-touch race loses increments | the first warp loses up to 31 (97/128) | ADD-or-create unit: final = initial + Σd; first touch is a host operation | **more** |
| B non-atomic RMW, EXIST self-write, else NOEXIST insert (9) | lost updates (racy RMW, element replacement) | about one update per warp | ADD-or-create: exact sum | **more** |
| C leader lookup + update(ANY, `p ? *p + d : d`) (4) | racy across overlapping launches | exact unless launches overlap | ADD-or-create | **more** |
| E entry SET / exit READ / DEL of timestamps (9) | per call; last writer wins | per call (synchronous) | SET and DEL deferred, linearizable per key. READ is GPU-local only with capability H7, otherwise one LOOKUP_COPY per group | **same** |
| F per-thread record update(ANY) (2) | per call; which keys fit a full map depends on order | same | SET; the first touch of each key is host-decided (E2BIG exact) | **same** |
| G leader publish update(ANY) (1) | per call | same | SET | **same** |
| H printk (21) | per-CPU order, timestamp merge, loss on overrun | synchronous, no loss | per-thread order, reservation order across threads, no loss; appears up to one watcher turn late | **same order**, **more** on loss, **less** on real-time position (D3) |
| J benchmark: update / lookup + `(*v)++` / delete, constant key (3) | per call; `++` racy | `++` collapses per warp | SET / ADD-if-present / DEL | **same / more / same** |
| any unrecognised pointer RMW | racy | racy (collapse) | stays synchronous; its map gets no ledger (PW) | **same** |
| host lookup_and_delete during GPU adds | adds through stale pointers can vanish | same | every commit falls on exactly one side; Σ of returned values + final = total | **more** |

**Host reads in strict mode.** They are linearizable: one record per key, read as one seqlock
snapshot. The opt-in modes are covered by D1.

### R1.5 Where UnitFold guarantees less, stated exactly

Each deviation has a counter in the ledger statistics, dumped at exit, where one is possible.

- **D1 (opt-in modes only).** Applies with SM shards (S > 1) or several deferring agents on one map.
  - The pending state of a key spans several records. A host read taken while commits run is
    *regular*: it contains every commit whose end happened-before the read and none that started
    after it ended.
  - Successive reads are monotone when deltas do not wrap. The read is not linearizable unless a
    double collect is stable: all `ver` words unchanged on the second pass, which proves a
    snapshot.
  - A destructive operation (delete, update, lookup_and_delete) splits each record at its own read.
    A commit on one record can therefore fall after the operation while a commit that it
    happened-before, on another record, falls before it.
  - Conservation stays exact: every commit is counted exactly once, and Σ of returned values +
    final = total.
  - Counter: `unstable_reads`. Strict mode, the default, has no D1.
- **D2 (closed maps).** bpftime frees a map on close (`runtime/src/handler/handler_manager.cpp:116-123`).
  Linux keeps it alive while a program references it.
  - Commits on a closed map are never folded and are dropped. No user-space fd reaches the old
    object, so in Linux the effect would be unobservable too.
  - Counter: `dead_drops`.
- **D3 (printk position).** A deferred line is printed at most one watcher turn after its commit,
  and always before any later synchronous line. The park backstop is 1,000 us
  (`runtime/src/attach/cuda_watcher_policy.hpp:27`). Lines are not printed before the GPU thread
  continues. Opt out with `BPFTIME_GPU_PRINTK_SYNC=1`.
- **D4 (E2BIG of straggler re-creations).** A GPU straggler that re-creates a key the host deleted
  (R3.4) is admitted or refused (E2BIG) at the first sweep or fold that sees it, not at its commit.
  - The sweep runs before every host insert and delete near capacity, so no capacity change by
    another host operation can fall between the commit and that decision.
  - What remains: when several such stragglers are pending and the map has fewer free entries than
    stragglers, the ones that get in follow sweep order, which may differ from their happens-before
    order.
  - Counter: `e2big_sweep`. It cannot happen while the map has more free entries than pending
    re-creations.
- **D5 (flip timeout).** A late pointer writer requires a flip (R3.6). If the drain does not finish
  within `T_flip` (default 1 s):
  - commits still in flight are applied at a later fold by **element replacement**, never in place;
  - a concurrent write through an older pointer can then be lost. That is the Linux outcome for a
    concurrent `update`, but here the replacement is triggered by the fold.
  - Counter: `late_stragglers`. A run with 0 had no D5.
- **D6 (frozen maps).** Deferred GPU writes after `BPF_MAP_FREEZE` are applied, as Linux allows for
  program writes.
  - This changes behaviour: today a GPU host-call update on a frozen map gets EPERM, because it runs
    with `from_syscall=true` (`runtime/src/handler/map_handler.cpp:445-448, 776-779`).
  - The new UNIT_* operations run with program semantics (`from_syscall=false`).
  - Unrewritten synchronous sites keep today's EPERM.

### R1.6 Proof sketch (strict mode, ADD effects; SET and DEL analogous)

**Invariant I.** For a ledgered key k, the stored element of k changes only in one of these ways:
- in a fold of k;
- in a host operation on k that runs in the same stripe-lock section right after a fold of k;
- by CPU atomic adds through a pointer, which PW allows only next to add-only ledgers and which
  commute.

The proof rests on the precondition PW and on the hook coverage of R3.5.

**Lemma 1 (snapshot).** A two-half record read (R3.4) returns totals (sum, count) that the record
held at one instant, between the reader's two loads of `ver`.
- It relies on H2 and H4: the writer stores an odd `ver`, executes `membar.sys`, writes the inactive
  half, executes `membar.sys`, then stores the even `ver`.
- The reader takes the half selected by the last even value and accepts it unless the writer
  started overwriting that half.
- The reader never waits on an odd `ver`.

**Lemma 2 (fold).** Let F be a fold of k that reads snapshot s and watermark w under k's stripe
lock.
- Every commit included in s − w has landed. By Invariant I, k's element is unchanged since the
  previous fold, so it equals its value when each of those commits landed.
- Adds commute, and the count carried with the sum decides re-creation. Applying s − w at F
  therefore gives the same element as applying each commit at its own commit point.
- The order respects (rt): a commit whose end happened-before F's start is in s, by H2 and the
  trailing `membar.sys`.
- Commits not in s landed after F's read, so ordering them after F's host operation is consistent
  with (rt).

**Lemma 3 (aggregation counts lanes).** A lane belongs to exactly one group: the lanes with an equal
key among those active at that call. Its delta is in exactly one group sum, committed once, and
the group's count is the number of lanes, not 1. Divergent lanes only make groups smaller.

**Lemma 4 (presence and capacity).** Every transition that adds a key to M is a host operation under
a stripe lock:
- a UNIT_* first touch;
- a host insert;
- a fold re-creation.

Every host operation that changes the key count (insert or delete) sweeps pending re-creations
first when the map is near capacity (R3.4), so a re-creation's E2BIG decision sees the capacity its
commit saw. Every GPU removal (a deferred DEL) is applied by a fold before a host insert fails with
E2BIG. The base map's own element accounting therefore linearizes the presence and capacity object.
The exception is D4's order among several concurrent stragglers.

**Theorem (sketch).** The objects are linearizable:
- each per-key object, with GPU units at their commits and host operations at their stripe-lock
  sections (Lemmas 1-2);
- the presence and capacity object (Lemma 4).

Every recognised unit touches one key of one map, and every remaining synchronous unit is one
helper call. By the locality of linearizability (Herlihy and Wing), the execution satisfies SR.

**Program order.** A GPU thread's unit n+1 starts after the trailing `membar.sys` of unit n.

## R2. Difference from gpu_ext and upstream automatic warp execution

**What the two systems do** (`overlap-survey-20261113/README.md:44-60`):
- **gpu_ext** (ASPLOS '27, camera-ready text):
  - It describes warp-leader execution and per-SM shards "merged at kernel completion" with
    "relaxed, eventual consistency".
  - gpu_ext's own audit (2026-09-04) found no implementation of this.
  - Its low-overhead results come from tools rewritten by hand: GPU map types, a warp-leader `if`,
    and buffer sizes fitted to the workload.
- **`official/revision/automatic-warp-execution`** (upstream; unmerged, opt-in through
  `BPFTIME_GPU_AUTO_WARP_EXECUTION`):
  - It decides yes or no once per program.
  - It rejects any atomic, printk, puts, perf output, exit, or write to a per-thread map.
  - An admitted program runs in the lowest active lane only. A non-atomic `*v = *v + 1` with a
    uniform key is admitted and counts once per converged group.

| | gpu_ext | upstream auto-warp | UnitFold |
|---|---|---|---|
| reference for "correct" | eventual consistency | observational equivalence to today's per-thread execution, including its collapse | SR (R1.2): per-map linearizability over units with a happens-before clause, tested against oracles |
| unit of decision | tool, by hand | program, yes/no | call site or idiom unit, automatic, with a per-site fallback |
| when the host sees an effect | after a merge at kernel completion; never for kernels that do not complete | immediately (nothing is deferred) | at every host access: the reader folds; no merge point exists |
| aggregation | election: the leader writes | election: the leader runs the handler | reduction: Σ of the group's deltas plus its lane count (Lemma 3) |
| host writes against deferred effects | no rule | not applicable | the fold before every host operation places each commit on exactly one side (Lemma 2) |
| atomics, printk, CPU maps | avoided by rewriting tools | program rejected | counter atomics and RMW idioms become exact ADD units; printk becomes ordered records; HASH is the main target |
| storage | workload-fitted constants (16,384 warps × 44 events) | none | sized by distinct keys (≤ `max_entries`) and SM count; a full ledger means an exact synchronous fallback, never a drop (R6) |
| hardware assumptions | not stated | `vote.ballot` text | GPU-to-CPU message passing and 8-byte single-copy atomicity, gated by litmus tests; no CPU-GPU atomics, no store buffering (R3.1) |

### Why defer and aggregate are exact rather than eventual

1. **Effects are cumulative totals, not values to overwrite later.** The fold composes them with the
   map value that the effects themselves would have met (Lemma 2), at the moment any host process
   looks. No staleness window exists for completed units.
2. **No merge point.** Visibility never depends on kernel completion. The hooks do not see
   completion for graph launches or for CUDA 13 kernel-handle launches
   (`nv_attach_impl.cpp:1561-1592`; `attach/nv_attach_impl/probed_launch_activity.cpp:16-24`).
3. **Aggregation is a reduction.** It is done over lanes that hold equal key bytes, so it equals
   those lanes' individual units placed next to each other in ≺. They are concurrent, so any
   adjacent placement is legal.
4. **Storage pressure degrades to the exact synchronous path.** The fallback runs the unit's own
   semantics on the host.

### Why per call site rather than per program

- Each unit carries its own descriptor (immediates in argument registers), its own fallback and
  its own aggregation mode.
- A program keeps some sites synchronous and defers the rest. For example, a printk with `%s`
  stays synchronous while its counters are deferred.
- Two units of one program on one map can behave differently, for example an ADD unit and a
  synchronous read.

## R3. "Only GPU probes touch this map" is unknowable at load time: a dynamic answer

UnitFold never needs that fact. The loader reads in its own process directly on the segment, CPU
probes in the agent use the same handlers, and other agents may attach later. The design therefore
makes every host access path compose GPU effects itself, at the time of the access (fold on
access). "Eliminate" is the run-time outcome in which no host path ever folds a ledger.

### R3.1 Hardware facts the design rests on

**Core facts.** The core relies only on facts that the code at 52126f33 already relies on. Each one
is also checked by a start-up litmus test. Deferral is enabled only if H2, H3 and H4 pass.

| id | fact | used for | precedent at 52126f33 |
|---|---|---|---|
| H1 | GPU atomics among threads of one context on device memory (`cuMemAlloc`) | device locks, shadows, key-to-slot cache, printk reservation counter | module lock `__bpftime_module_comm_lock` (`default_trampoline.cu:118-122, 146-148`) |
| H2 | GPU-to-CPU message passing to a CPU thread **in another process** on segment memory: GPU stores, `membar.sys`, GPU store of a flag word; a CPU acquire load of the flag sees the stores | record publication; printk records | 1527 ring: `membar.sys` then tail store (`default_trampoline.cu:478-480`), drained by the loader with an acquire load (`runtime/src/bpf_map/gpu/nv_gpu_ringbuf_map.cpp:65-68`); request protocol (`default_trampoline.cu:152-158` → `bpf_attach_ctx_cuda.cpp:309`) |
| H3 | CPU-to-GPU message passing: CPU stores, then a release store of a flag; a GPU load of the flag followed by `membar.sys` sees the stores | responses of the UNIT_* operations (the existing protocol); ABSENT and DEAD hints | `flag2` release (`bpf_attach_ctx_cuda.cpp:317`), GPU spin then `membar.sys` (`default_trampoline.cu:159-163`); 1527 head read before the record write (`default_trampoline.cu:461-470`) |
| H4 | aligned 8-byte single-copy atomicity on segment memory in both directions | record `ver`, sum and count; hint words | 1527 64-bit head and tail (`gpu_helper_protocol.h:86-103`) |
| H5 | GPU-scope ordering among GPU threads: `bar.warp.sync` orders memory among the lanes of a warp; release and acquire through a device-memory lock | group accumulation; lock-protected sections | standard PTX [external]; the trampoline already uses `__syncwarp` and device CAS locks (`default_trampoline.cu:146-171`) |

**What the core never needs:**
- GPU atomics on segment memory. The GPU only loads and stores segment words.
- A CPU-GPU atomic read-modify-write on one word (`hostNativeAtomicSupported` is not queried).
- CPU-GPU store buffering.
- The CPU posting into `CommSharedMem`, whose `occupy_flag` is taken with GPU atomics only
  (`default_trampoline.cu:149`).

**Capability bits.** They are measured at start-up, cached per device and driver, and never needed
by the core.

| bit | fact | enables |
|---|---|---|
| C-SB | CPU-GPU store buffering on segment memory (`st; fence; ld` on both sides, not both stale) | GPU-local reads (READ_LOCAL, R3.7) |
| C-XA | GPU atomics on segment memory are atomic across two CUDA contexts | an optional single-word record variant shared by agents (not used by default) |
| C-PTX | `shfl.sync`, `vote.sync`, `match.any.sync`, `redux.sync` and `nanosleep` accepted as PTX text in a module retargeted to sm_87 (`attach/nv_attach_impl/nv_attach_utils.cpp:52-65`) | faster grouping and reductions (R3.3) |
| C-SCHED | a kernel launched by the agent on a private stream gets scheduled within `T_flip` while application kernels occupy the GPU | an exact flip (R3.6); without it, D5 may occur |

### R3.2 Layout and word ownership

**Ledger** [new]. One per ledgered map M, in the Boost segment, allocated by the agent that creates
it. The agent already allocates in the segment, for example its alive-pid set
(`runtime/agent/agent.cpp:1195-1199`). The whole segment is registered for the GPU with
`cuMemHostRegister(DEVICEMAP|PORTABLE)` by every agent
(`runtime/src/bpftime_shm_internal.cpp:1407-1475`), so the loader can read the ledger without the
agent, as it reads the 1527 ring.

| part | content | written by | read by |
|---|---|---|---|
| header | magic, ABI version, fd, `gen`, key and value size, kind (UPSERT_ADD with constant template c, ADD_IF_PRESENT, LWW), add field (offset, width ∈ {4, 8}, aligned), capacity C, shard count S, owner agent pid, mode hint, flip request and acknowledgement words, statistics | CPU (host processes) | CPU, GPU (hints only) |
| CPU index | open addressing, 2C entries {key, slot}; slot creation under a per-ledger interprocess mutex, so a key has at most one slot | CPU | CPU |
| slot state | per slot, one 8-byte hint: LIVE / ABSENT / DEAD | CPU | GPU (one load per group commit), CPU |
| watermarks | per record: folded `ver`, sum and count (and for LWW the folded write sequence) | CPU, under the key's stripe lock | CPU |
| records | per (slot, shard): `ver` (u64) and two halves. ADD half: {sum, count}. LWW half: {sum, count, wseq, wkind, sum_at, count_at, value} | the CPU zeroes a record once, at slot creation, before the slot index is published in a UNIT_* response; afterwards only the GPU writes it (plain stores) | CPU |
| absent list | slots whose key a host operation deleted while they were bound | CPU | CPU |

**Device block** [new]. One per (ledger, agent), in agent-private device memory allocated with
`cuMemAlloc` in the primary context. That is the context the watcher retains
(`bpf_attach_ctx_cuda.cpp:262-276`) and that cudart uses.

| part | content | written by |
|---|---|---|
| `devmode` | DEFER / SYNC (flip) | the agent's flip kernel |
| lock table | NL entries (default 2C, power of two) of {4-byte lock, staging sum}, indexed by a hash of the key bytes (and of the shard). Every lane of a group computes the same index from its own key bytes; colliding keys only share a lock. Below, "the key's lock" or "slot lock" means this entry | GPU (H1) |
| shadows | per (slot, shard): `ver`, sum, count; LWW value. Protected by the lock of hash(key of the slot) | GPU, under that lock |
| key-to-slot cache | 2C entries {key, slot or NOSLOT}, filled after a UNIT_* response; duplicates are harmless because the host assigns one slot per key | GPU |
| ledger address | device address of the ledger in the segment, translated as `gpu_address_of_map_value` does (`bpf_attach_ctx_cuda.cpp:559-574`) | agent at creation |

**How a module finds it.** `map_info[fd].extra_buffer` is set to the device block address
(`create_map_basic_info`, `bpf_attach_ctx_cuda.cpp:505-554`).
- Today the trampoline uses `extra_buffer` only for ARRAY and the GPU map types
  (`default_trampoline.cu:238-272`), so a HASH entry carrying it changes no existing path.
- `map_info` is a per-module snapshot (`default_trampoline.cu:101`, written once at module load,
  `nv_attach_impl.cpp:2366-2385`). The snapshot holds only this pointer; all changing state lives
  behind it or in the segment.

**Single writer per word.** Every segment word has one writing side at a time.
- The GPU writes only records. The one exception is the CPU zeroing a record at slot creation,
  before any GPU learns the slot.
- The CPU writes everything else in the ledger.
- The element of the map itself is written only by host code: folds and host operations.
  Precondition PW excludes GPU writes through pointers.

### R3.3 GPU side: committing an ADD unit (helper 600) [new]

**Grouping** decides which lanes form one group:
- **static.** The analyzer proved that the key value and, for the cheapest variant, the delta are
  invariant within a block. They are computed only from constants, helper 14, the outputs of 503,
  504 and 508, and map fds. This is a property of one thread's dataflow and does not depend on
  convergence. All active lanes then form one group.
- **dynamic.** Used for any other key, including `%smid`, `%warpid` and `%globaltimer`, whose values
  can change under preemption [external].
  - In slice 1, every lane is its own group, processed in lane-turn order as in `host_call`.
  - In opt-in shard mode, the shard is a hash of the block index modulo S: a value every lane of a
    group computes identically. `%smid` is not used, because it may change under preemption.
  - With C-PTX, `match.any.sync` forms the groups.
- No memory scratch indexed by `%smid`/`%warpid` is used. PTX documents `%warpid` as unique only
  within a CTA [external].

**Sum** of a group:
- n × d when the delta is a proven constant;
- otherwise each lane adds its delta with a device atomic into the staging word of the lock-table
  entry `hash(key)` while the leader holds that lock. Every lane of a group has the same key bytes,
  so all lanes compute the same entry without any broadcast primitive.
- With C-PTX, a `shfl.sync` tree replaces the staging word.

```
_bpf_helper_ext_0600(fd, key, delta, desc, site):         // dfr_add [new]
  B = map_info[fd].extra_buffer                            // device block, or 0
  if B == 0: each lane in turn: host_call(UNIT_ADD, ...); return 0   // defensive: rewritten code always has a block
  for each group G of activemask (static: one; dynamic: lane-turn or match.any):
    leader = lowest lane of G; n = popc(G)
    L = &B.locktab[hash(key bytes, shard) & (NL-1)]        // every lane computes it; equal keys -> equal L
    if lane == leader:
      lock(L)                                              // device CAS (H1)
      s = keycache_find(B, key)                            // device memory; a slot index, NOSLOT or NONE
    syncwarp(G)
    if delta not constant: atomicAdd(&staging(L), delta)   // every lane of G, device memory
    syncwarp(G)
    if lane == leader:
      sigma = constant ? n*d : take(staging(L))
      h = (s is a slot index) ? ld.volatile B.ledger.slot_state[s] : NONE   // hint (H3)
      if h == DEAD: drop                                   // closed map (D2)
      else if kind == IF_PRESENT and h == ABSENT: no-op    // negative cache; ordered at this load
      else if B.devmode != DEFER or h != LIVE:
          r = host_call(UNIT_ADD, gen, fd, key, sigma, n, kind, c, field)   // exact, one per group, L still held
          if s == NONE: keycache_insert(B, key, r.slot or NOSLOT)
      else:                                               // the commit
          sh = &B.shadow[s][shard]; v = sh.ver
          sh.sum += sigma; sh.count += n
          st.volatile rec.ver = v + 1                       // odd: this half is being replaced
          membar.sys
          st.volatile rec.half[((v >> 1) + 1) & 1] = {sh.sum, sh.count}
          membar.sys
          st.volatile rec.ver = v + 2                       // commit point
          membar.sys                                        // commit performed before return
          sh.ver = v + 2
      unlock(L)
    syncwarp(G)
  return 0
```

**Properties:**
- **Payload.** It is cumulative: the totals since the ledger was created. It carries no incarnation
  tag, and the CPU's watermarks track what has been folded.
- **Two halves.** The two-half layout lets readers read without waiting (R3.4), and a writer that
  dies mid-section leaves the committed half intact.
- **Fallbacks.** A fallback holds the slot lock across its host call, so a later group on the same
  slot cannot return before the earlier group's effect is applied. The host call takes the module
  lock and `occupy_flag` as today and needs no GPU lock, so no deadlock arises.
- **Older GPUs.** Only one lane per group locks, and groups of a warp run one after another, so the
  pre-Volta hazard noted at `default_trampoline.cu:128-133` does not arise [inferred].
- **Cost per commit.** On the GPU: one device CAS, one uncached load (the hint), four uncached
  stores and three `membar.sys`. Step 0 measures it (R10).
- **Optional variant.** For a proven odd constant delta, a single-word record updated by
  `red.add` would suffice. An odd constant is invertible mod 2^64, so the sum determines the count.
  The variant is not in the core and is adopted only if step 0 shows the locked section dominates.

### R3.4 Host side: record read, fold, host operations [new]

**Record read.** Wait-free; it never waits on an odd `ver`:
```
read_record(r): loop
  v1 = load_acquire(r.ver); c = v1 >> 1                 // index of the last committed write
  (sum, count) = r.half[c & 1]                           // two 8-byte loads (H4)
  fence (load-load); v2 = load(r.ver)
  if v2 <= 2c + 2: return (2c, sum, count)               // write c+2, which reuses half c&1, has not started
```

**Fold** of key k, under k's stripe lock. Stripes are those of `bpftime_hash_map`, whose writers
lock the stripe of the key's head (`runtime/src/bpf_map/bpftime_hash_map.hpp:132-134`).

```
fold(M, k):
  s = index_find(k); if none: return
  for each record r of s (S of them; A agents in multi-agent mode):
    (v, sum, count) = read_record(r)
    dsum += sum - wm[r].sum; dcount += count - wm[r].count   // u64, mod 2^64
  [LWW: if a record's wseq > wm.wseq: apply its SET or DEL first, then the adds after sum_at/count_at]
  if dcount == 0: return
  e = find_locked(k)
  UPSERT_ADD:     e ? atomic_add(field(e), dsum)           // in place: 4- or 8-byte CPU atomic
                    : insert_locked(k, c ⊕ dsum)           // re-creation (sweep rule below); E2BIG -> drop, count
  ADD_IF_PRESENT: e ? atomic_add(field(e), dsum) : drop
  LWW SET:        replace_locked or insert_locked (value ⊕ adds after it); LWW DEL: delete_locked
  wm[r] := (v, sum, count)  (release stores)
```

**Fold function per effect kind:**

| kind | in place or replacement | effect on lock-free lookups (`bpftime_hash_map.hpp:112-134`) | effect on held pointers (CPU probes, synchronous GPU lanes) |
|---|---|---|---|
| ADD (both kinds), key present | in place, 4- or 8-byte atomic add on the aligned field | they read the field before or after the add (H4 on the CPU) | they see the add, like a concurrent BPF atomic add in Linux. PW allows only commuting CPU atomic adds through pointers |
| UPSERT re-creation | insert of a new element | as any insert | not applicable (no element existed) |
| SET | `replace_locked` (`:528-538`): a new element is linked and the old one is retired | they see the old or the new element whole | writes through old pointers are lost, which is Linux's replacement semantics |
| DEL | delete (`:755-778`) | as any delete | as any delete |

**Lock-free fast path for pointer lookups (CPU probes)** [new].
- Read every record's `ver` of k's slot and compare it with `wm.ver`.
- If none is pending, do today's lock-free lookup and re-check the `ver` words.
- If they are unchanged, nothing committed and nothing was folded meanwhile, so the result is
  linearizable at any instant of the window.
- Otherwise fall back to the slow path: lock, fold, find.
- Cost: one CPU index probe and S cached loads per lookup. R10 measures it.

**Loader lookup (merging copy)** [new]. Today the syscall server copies the value with `memcpy`
after the lookup has returned, outside any lock (`syscall_context.cpp:615-622`). For a ledgered map
the server instead calls a new handler entry, `map_lookup_copy`:
1. Lock k's stripe, fold k, copy the value into a private buffer, unlock.
2. Copy to the caller's buffer after unlocking. A bad user buffer then cannot fault under the
   stripe lock, the same reason `elem_delete` copies after unlocking (`bpftime_hash_map.hpp:770-775`).

**Host operations on a ledgered key.** Each runs as "fold, then operate" under one stripe lock,
through new locked variants of `update`/`elem_delete` (`bpftime_hash_map.hpp:715-778`):

| operation | sequence |
|---|---|
| lookup (CPU probe) | fast path or (fold, find) |
| loader lookup | `map_lookup_copy` |
| update ANY/EXIST/NOEXIST | sweep if it may insert; lock; fold; apply flags against the folded state; hint := LIVE if present; unlock |
| delete | sweep if near capacity; lock; fold; delete; hint := ABSENT; add slot to the absent list; unlock |
| lookup_and_delete | sweep if near capacity; lock; fold; copy merged value; delete; hint ABSENT; unlock; copy out |
| `get_next_key(NULL)` | sweep; then the base iteration |
| UNIT_ADD (watcher) | check `gen` (mismatch → drop); sweep if it may insert; lock; fold; run the unit (present → add Σ; absent → UPSERT inserts c ⊕ Σ with Linux's E2BIG, IF_PRESENT no-ops); find or create the slot (NOSLOT if the ledger is full); hint := LIVE or ABSENT; unlock; return the slot |

**Stragglers.** A GPU commit can land after a host operation's fold read, even though the GPU began
it before the operation's hint store reached it. Without store buffering the host cannot exclude
such commits, and UnitFold does not try. Lemma 2 orders each straggler after the host operation,
because it landed after the operation's read:

| operation the straggler follows | result at the next fold |
|---|---|
| overwrite | adds on top of the new value |
| delete, UPSERT kind | re-creates the key |
| delete, IF_PRESENT kind | dropped |
| lookup_and_delete | re-creates the key (conservation: Σ of returned values + final = total) |

No GPU-local read of ADD keys exists, so no GPU observer can have seen a straggler before the host
operation.

**Sweep** (re-creations and capacity) [new].
- A re-creation changes the number of keys, so its E2BIG decision must not move.
- Before any host operation that changes the key count, the host sweeps the absent list: for each
  listed slot whose record count moved, it takes that key's stripe lock alone and folds it, oldest
  first. Operations that change the key count: a host insert of an absent key, a host delete or
  lookup_and_delete of a present key, a UNIT_* insert, and a fold that would re-create a key.
  Sweeping before deletes too means no host operation can change capacity between a straggler's
  commit and its E2BIG decision.
- **Lock order.** The sweep runs before the operation takes its own stripe lock and never holds two
  stripe locks. A fold that discovers a needed re-creation while holding k's lock releases it,
  sweeps, re-locks and re-folds.
- **Cost bound.** One cached load per absent-list slot. Locks are taken only for slots with movement.
- The sweep runs only when M's free elements (a new relaxed counter in `bpftime_hash_map`) do not
  exceed the absent list's length plus a slack of 64. Below that, every pending re-creation fits
  whatever its order.
- **What remains.** Only D4: the relative order of several pending stragglers on a nearly full map.

**Pending GPU deletes and E2BIG.** A deferred DEL leaves the base element in place until folded. A
host insert that hits E2BIG on a map with LWW records therefore first folds every slot whose
record shows a newer write, and then retries. This is O(C), but only on the E2BIG path.

### R3.5 Every host access path is hooked

A test of the handler's ledger pointer (one load; null for unledgered maps) sits in:

| entry point | callers |
|---|---|
| HASH branches of `map_lookup_elem` / `map_update_elem` / `map_delete_elem` (`runtime/src/handler/map_handler.cpp:135, 441, 773`) and their counted wrappers (`:1906, 1914, 1924`) | loader syscalls through `bpftime_shm::bpf_map_*` (`bpftime_shm_internal.cpp:290-418`); generic CPU helpers (`bpf_helper.cpp:401-422`, `from_syscall=false`); the watcher (`bpf_attach_ctx_cuda.cpp:136, 152, 159`) |
| typed fast paths `map_lookup_hash` / `map_update_hash` / `map_delete_hash` (`map_handler.cpp:302, 336, 355`) | the typed CPU helpers `bpftime_hash_map_*_elem_helper` (`bpf_helper.cpp:470-492, 574-595, 600-621`), chosen for a CPU program whose maps include a HASH (`runtime/src/attach/bpf_attach_ctx.cpp:116-170, 499-555`). These helpers take the generic path when the handler has a ledger: one extra test next to their existing type test (`bpf_helper.cpp:487-492`) |
| `map_lookup_and_delete_elem` (`map_handler.cpp:1797`) | loader `BPF_MAP_LOOKUP_AND_DELETE_ELEM` (`syscall_context.cpp:650`) |
| `bpf_map_get_next_key` (`map_handler.cpp:617`) | loader `BPF_MAP_GET_NEXT_KEY` (`syscall_context.cpp:668`) |
| `diagnostic_direct_*` (`map_handler.cpp:391-440`) and the cached-handler branches (`bpftime_shm_internal.cpp:295-315`) | A/B diagnostic modes; for ledgered maps they route to the generic entry points |
| loader `BPF_MAP_LOOKUP_ELEM` (`syscall_context.cpp:600-622`) | becomes `map_lookup_copy` (R3.4) |
| close (`syscall_context.cpp:273-296` → `bpftime_shm::close_fd`, `bpftime_shm_internal.cpp:852` → `clear_id_at` → `map_free`, `handler_manager.cpp:116-123`) | marks every ledger of M DEAD, then frees the map |

**Not hooked:** batch commands, which fall through to the kernel and do not work today
(`syscall_context.cpp:852-854`), and ARRAY mmap (arrays are never ledgered).

**Why not in the hash map itself.** The hook does not go into `bpftime_hash_map`, because that
class is also the per-CPU hash's inner table. Only `BPF_MAP_TYPE_HASH` handlers get ledgers.

### R3.6 Pointer writers: decided before any ledger exists, then flip [new]

**Why PW is needed.** Plain stores through a map-value pointer bypass every hook. A GPU `atom.add`
through a device alias (`bpf_attach_ctx_cuda.cpp:136-144`) would meet the fold's CPU in-place add
on one word, and CPU-GPU atomicity is not established.

**The scan.** Between the two loops of `init_attach_ctx_from_handlers`
(`bpf_attach_ctx.cpp:229-245` instantiates the non-CUDA handlers, `:248-259` the CUDA links), the
analyzer reads every program handler in the segment: GPU and CPU programs, attached anywhere. It
sets two flags per map:

| flag | set when | effect |
|---|---|---|
| `gpu_ptr_writer(M)` | any GPU site that will **not** be rewritten (unmatched, fallback, or analysis off) stores through, or runs any atomic through, a lookup pointer of M | M gets no ledger |
| `cpu_ptr_writer(M)` | any CPU program does a plain store, a non-ADD atomic, or any atomic other than ADD through a lookup pointer of M (conservative, bytecode level) | M gets no ledger; CPU atomic ADD through pointers is allowed only next to add-only ledgers |

This runs before any patched module is created, so no GPU thread can have written a ledgered map
through a pointer.

**Late arrival.** A program appearing later can be a pointer writer on a ledgered M: an auto-refresh
re-runs `init_attach_ctx_from_handlers`, or another agent's analysis flags it. M is then flipped to
synchronous:

1. **Request.** The requester stores `flip_req = e+1` in M's ledger header.
   - The owner agent's watcher polls this word on every turn. That is at most 1 ms with the park
     backstop (`cuda_watcher_policy.hpp:27`); the watcher wakes at least that often.
   - If the requester is the owner, it skips the polling.
   - If the owner's pid is not in the alive-agent set, no GPU writer of M is left and the requester
     proceeds directly.
2. **Flip kernel.** The owner launches a one-block kernel on a private non-blocking stream in its
   primary context:
   - store `devmode = SYNC`, then `__threadfence()`;
   - acquire and release every entry of the device block's lock table.

   Every section that read DEFER held its lock, so it has finished. Every later section acquires a
   lock after the kernel released it and therefore sees SYNC (H1, H5). The owner waits on an event,
   not on `cuCtxSynchronize` (which `run_attach_entry_on_gpu` uses at `nv_attach_impl.cpp:1364`, and
   which would also wait for application work). Then it stores the acknowledgement.
   The only precedent for an agent-launched kernel is `run_attach_entry_on_gpu`
   (`nv_attach_impl.cpp:1190`, `cuLaunchKernel` at `:1360`). Whether such a kernel is scheduled
   while application kernels hold every SM is unknown; that is C-SCHED, measured in step 0.
3. **Fold and attach.** After the acknowledgement, the requester:
   - folds every slot of M;
   - sets the mode hint SYNC;
   - attaches the late program.

   M then behaves as an unledgered map. Old rewritten sites see SYNC and use the UNIT_* operations,
   which are exact.
4. **Timeout.** If no acknowledgement arrives within `T_flip` (default 1 s):
   - The late program is attached anyway, so attach time is bounded.
   - The ledger switches to *replacement folds*: any later movement of a record is applied by
     `replace_locked` with the merged value, never in place. A CPU in-place add therefore never
     meets a GPU or CPU pointer write on one word.
   - Movement found after the timeout is counted in `late_stragglers`. This is D5 in R1.5.
   - Only commits whose section began before the flip request can still move, so the counter stops
     growing once those warps finish.

**Not flipped:**
- **Loader-originated events**: close (DEAD), freeze (D6) and ordinary host writes. Their exactness
  comes from fold-on-access alone and never depends on the agent.
- **Recognised units of a late program** on a ledgered M. They join the ledger if their kind
  matches; otherwise they are rewritten with a host-only descriptor bit and always use UNIT_*.

### R3.7 GPU-local reads (READ_LOCAL): why an epoch alone is not enough

**The scenario.** analysis-first validated a GPU-local read with a CPU-written host-mutation epoch
read before and after the record. Under message passing alone, this cycle is possible:
1. A GPU SET reads the old epoch and commits after a host delete's fold read has missed it. The SET
   is then ordered after the delete.
2. A GPU read sees that SET's record and still reads the old epoch on both sides, so it is ordered
   before the delete.
3. Together: SET ≺ read ≺ delete ≺ SET.

Excluding this needs store buffering between the host's epoch store followed by its record read,
and the GPU's record store followed by its epoch read (C-SB).

**Decision:**
- **Without C-SB**, a value-used read-only lookup (`resident_read`) is served by **LOOKUP_COPY**:
  one host operation per group that folds and copies into a stack slot, with no pointer returned.
  It is exact, at one request per warp instead of 32.
- **With C-SB**, READ_LOCAL is enabled:
  - The host stores an odd epoch, fences, reads the records, and waits for any odd record `ver` to
    turn even. The wait is bounded by one GPU section, which makes no host call. If the owner's
    liveness record is stale, the host uses the committed half.
  - A GPU writer stores an odd `ver`, executes `membar.sys`, and reads the epoch; it aborts to UNIT_*
    if the epoch is odd.
  - A read is valid if the epoch before the record, the epoch after it, and the record's write epoch
    are equal and even.
  - This adapts semantics-first's cut, keeping its host wait on an odd `ver` [new]. That wait is
    the only one in the design, and it exists only behind C-SB.
- READ_LOCAL copies into a stack slot of the probe's own frame. cuda-counter's retprobe dereferences
  the pointer again at pc 105, after the delete at pc 27. READ_LOCAL is therefore **disabled under
  memcapture**: that pass moves `r10` down by the rounded length of the captured line and stores the
  line above it (`attach/nv_attach_impl/pass/ptxpass_kprobe_memcapture/main.cpp:63-127`), and only
  the pass knows that length.

### R3.8 Identity, close, fd reuse, snapshots, several agents

- **Generation** [new]. The handler gets `gen`, a 64-bit value taken from a segment-wide counter at
  creation. Today `id` is just the fd (`runtime/src/handler/map_handler.hpp:89-102, 370`). The
  ledger header and every UNIT_* request carry `gen`, and the watcher drops a request whose `gen`
  differs from the handler at that fd.
- **Close.** The hook (R3.5):
  - marks the header and every slot hint DEAD;
  - detaches the ledger from the handler;
  - leaves the ledger and the device block allocated until their agent exits. Patched images
    cannot be unloaded (`nv_attach_impl.cpp:2387-2390`) and keep the device block's address.

  Commits on a DEAD ledger are never folded (D2).
- **Takeover after the owner dies** [new]. Records are cumulative and the CPU watermarks persist,
  but a new agent's device block would start its shadows at zero.
  - When an agent finds M's owner pid outside the alive-agent set, it takes ownership with a CPU CAS
    on the owner field.
  - Before any of its modules can commit, it seeds every shadow of its new device block from the
    record's committed half: `ver` = the last even value (2c), with sum and count taken from half
    c & 1. It copies them in with `cuMemcpyHtoD`.
  - If the dead owner left `ver` odd (killed mid-section), the half it was writing is ignored. The
    new owner's first commit stores the next odd value and then 2c + 2, so `ver` stays monotone
    and readers never see totals go backwards.
  - The watermarks are untouched; the restarted application's counts continue exactly (T9).
- **fd reuse.** A new map at the same fd has a new `gen` and no ledger until analysed. Old modules'
  rewritten sites still point at the old device block, so they see DEAD or a `gen` mismatch. Old
  unrewritten sites reach the new map with stale sizes: a pre-existing hazard that UnitFold does
  not widen.
- **Snapshots.** `map_info` is a snapshot and is last-link-wins in `nv_attach_impl`
  (`nv_attach_impl.cpp:233`). The device-block address of (M, agent) never changes after creation,
  so every snapshot carries the same value.
  - A map that is absent from the snapshot (created later, or fd ≥ 1024,
    `bpf_attach_ctx_cuda.cpp:505-554`) is never rewritten.
  - Cross-program facts (kind, flags, owner) live in the segment, not in `map_info`.
- **Several agents** (several CUDA processes on one segment).
  - In strict mode, the first agent to create a ledger for M owns it; ownership is a CPU CAS on the
    handler's ledger pointer.
  - Other agents rewrite their units on M with the host-only bit, so each of their groups is one
    exact UNIT_* request.
  - Opt-in multi-agent mode adds per-agent records (A > 1) and accepts D1.
  - Pre-existing and not fixed: every agent shares the one `CommSharedMem` and zeroes it at start
    (`bpf_attach_ctx_cuda.cpp:615`; `CLAUDE.md:2316-2319`). UNIT_* requests inherit that hazard. A
    per-agent request slot is recommended as a prerequisite for multi-agent use (R9).

### R3.9 The constraints from the task, answered

| fact | answer |
|---|---|
| loader reads run in another process, directly on the segment | the fold runs in the reading process, inside `bpf_map_handler`, and reads only segment words; there is no agent involvement and no new channel for correctness |
| the request slot is unsafe for CPU posters | no CPU ever posts; the CPU only loads GPU-written record words |
| hash value pointers are not stable across update and delete | ledgers name keys by bytes and slots, never by element pointer. Folds add in place under the stripe lock (never through `elem_update`, which would replace) or replace or insert explicitly. Loader reads copy under the lock |
| maps can be closed and fds reused | `gen` in the header and in requests; DEAD hint; the ledger outlives the map |
| `map_info` in a module is a snapshot | it carries only a stable device-block pointer; absent maps are never rewritten |
| CPU probes in the agent may touch the same map | they fold through the same entry points (typed ones included); their pointer writes are classified (R3.6) |

## R4. Idioms as units

### R4.1 Recognition, at the bytecode level, in the agent

**The analysis.** A forward abstract interpretation runs over the stored eBPF words.
- It ports the worklist of upstream PR #624 without PREVAIL headers. PR #624 tracks, per pc,
  registers, pointer provenance, map fds and 512 stack bytes (PR #624 branch, file
  `bpftime-verifier/src/gpu/uniformity_analysis.hpp`). Dev builds have `ENABLE_EBPF_VERIFIER` off
  (`cmake/StandardSettings.cmake:105`).
- BPF-to-BPF calls are left unchanged in the first version.

**Per-pc state.** For each register and each 8-byte stack cell, a product of:
- **kind**: constant, map fd (LDDW src=1), stack pointer + offset, map-value pointer (producing
  site, map, key value number, maybe-null), or scalar.
- **scope**, used for reporting and for the static grouping test:
  CONST ⊑ PROCESS ⊑ GRID ⊑ BLOCK ⊑ WARP ⊑ THREAD, plus two flags:
  - *timing* for `%globaltimer`;
  - *position* for `%smid` and `%warpid`.
- **value number**, for symbolic equality.

**Helper sources:**

| helper | scope | source |
|---|---|---|
| 14 | PROCESS | the module global the agent writes, `(tgid<<32)\|tgid` (`default_trampoline.cu:410-422`; `nv_attach_utils.cpp:155-159`) |
| 503 out-parameters | BLOCK | |
| 504, 508 | GRID | |
| 505, 511 | THREAD | |
| 509 | BLOCK, position-flagged | |
| 510 | WARP, position-flagged | |
| 502 | THREAD, timing-flagged | |

**Join rules.**
- Joins are control-dependent: a value defined under a branch whose predicate has scope p gets at
  least p. PR #624 lacks this.
- A lookup's pointee inherits the scope of its key.

**Static grouping.** It requires scope ⊑ BLOCK *without* the position and timing flags. That test
is single-thread value invariance (R3.3).

**Acceptance conditions.** A unit is a single-entry CFG region R, anchored at a lookup (`call 1`)
or at an update or delete (`call 2`/`call 3`). It is accepted only when all of these hold:

| rule | condition |
|---|---|
| K (key equality) | every helper in R gets the same key: the same stack slot with no store into its bytes inside R, or equal value numbers |
| P (pointer confinement) | the lookup result and its copies are used only by the null test; by loads and stores at one (offset, width) inside R; by a BPF_ATOMIC ADD (no FETCH) at that field; or as the value pointer of R's own `BPF_EXIST` update. No use after R's exit, no spill escaping R, no use as another helper's argument |
| V (value algebra) | the effect on the old value is add-family. `f(x) = x ⊕ d` at the field. `f(⊥) = c ⊕ d` (UPSERT), where the inserted value's field is proven equal to d + c0 by value numbers and its other bytes are constants; or `f(⊥) = ⊥` (ADD_IF_PRESENT). The field is 4- or 8-byte aligned |
| L (liveness) | everything R defines (registers, stack bytes, the update's r0, r0-r5 clobbered by the new call) is dead at every exit, unless the rewrite reproduces it. Values R only reads, such as the key slot or a delta slot read again later, keep their bytes |
| C (control) | no jump from outside R lands inside it; R contains no helper call or atomic other than its own |

**Merged calls.** A call instruction that clang merged across paths is treated as several logical
calls, one per incoming edge, distinguished by the reaching definition of its constant flags
argument. Example: cuda-counter's retprobe pc 76 serves source lines 76 and 79.

### R4.2 Rewrite

**In place, slice 1.** The unit's entry block is overwritten from its first argument-setup
instruction:
1. argument setup;
2. `call 60x`;
3. `ja` to the region's join.

Every other instruction of R becomes `ja +0`. All of R's in-edges were internal, so those
instructions are dead. No pc outside R changes, so no jump offset needs fixing.

The three slice-1 regions fit (pcs from `dir2-read/disasm/cuda-counter.txt`):

| region | slots | rewritten instructions | padding | why it is correct |
|---|---|---|---|---|
| cuda-counter entry, shape A (pc 22-37, 16 slots, join 38) | 16 | pc 22-25 kept (key pointer and map LDDW); pc 26 `r3 = r6`; 27 `r4 = desc`; 28 `r5 = site`; 29 `call 600`; 30 `ja 38` | pc 31-37 `ja +0` | r0 and r6 are dead at 38; slot `r10-0x30` is never read again |
| cuda-counter retprobe, shape B (pc 28-42 + 69-76, join 77) | 8 used | pc 28-29 map LDDW; 30 `r2 = r8`; 31 `r3 = r7`; 32 `r4 = desc`; 33 `r5 = site`; 34 `call 600`; 35 `ja 77` | pc 36-42 and 69-76 `ja +0` | the printk path pc 43-68 is reached only from pc 16, outside R; r1-r5 are redefined before use at 105-110; slot `r10-0x28` is kept, and is read at pc 107 |
| micro hash_lookup, shape J (`benchmark/gpu/micro/cuda_probe.bpf.c:167-176`) | | | | the feasibility review found it fits in 7 slots in its disassembly. That object is not built in the dev worktree; check at slice start [inferred] |

**Units that do not fit stay unchanged** in slice 1. A general patcher that inserts instructions
and fixes jump offsets (the kernel's `bpf_patch_insn_single` idea) follows in slice 2. It is needed
for units with extra arguments: the READ stack slot and the printk descriptor.

**Validation.** After rewriting, a structural self-check runs. On any failure the original words go
to the pass. It checks that:
- every call id is in the registered set;
- every jump stays in range and lands on an instruction start;
- there is no LDDW src=2;
- the new call's argument registers are defined;
- the region's live-out registers and stack bytes are unchanged.

Semantic correctness of the rewrite is tested by differential execution, not re-analysis (T14 in
R10).

### R4.3 The eight shapes (59 bpftime host-call sites)

Letters follow `callsite-survey-20261113/records.json` as grouped in `dir2-read/groups.txt`.

| shape | sites | example (bytecode) | unit | helper |
|---|---:|---|---|---|
| A counter | 10 | cuda-counter entry: `call 1` @26; `if r0==0` @27; `lock *(u64*)(r0+0) += r6` @28; miss: `r4 = 1; call 2` @36-37 with value slot `r10-0x30` = r6 = 1 (@20-21) | UPSERT_ADD(d = r6, c = 0) | 600 |
| B non-atomic RMW, EXIST self-write, else NOEXIST | 9 | cuda-counter retprobe: `call 1` @31; hit: `r1=*(u64*)(r0+0); r1+=r7; *(u64*)(r0+0)=r1` @33-35, `r3 = r0; r4 = 2` @40-41; miss: `r3 = r10-0x28; r4 = 1` @71-75; one merged `call 2` @76 | UPSERT_ADD(d = r7, c = 0) | 600 |
| C leader lookup + update(ANY, `p ? *p+d : d`) | 4 | cudagraph `call_count`, `total_time_ns` (`example/gpu/cudagraph/cuda_probe.bpf.c:56-58, 79-81`) | UPSERT_ADD | 600 |
| E entry/exit timestamps | 9 | entry `update(start_ts, &blk, &ts, ANY)` (@13-19); exit `lookup` (value used @19 and @105); `delete` (@22-27) | SET; READ; DEL | 601; 603; 602 |
| F per-thread record | 2 | `update(thread_mapping, &((blk<<16)\|tid), &info80, ANY)` (`example/gpu/threadscheduling/threadscheduling.bpf.c:86`) | SET (no aggregation) | 601 |
| G leader publish | 1 | `update(counter_per_thread, &k, &v, ANY)` (`example/gpu/gpu_shared_map/gpu_shared_map.bpf.c:55`) | SET | 601 |
| H printk | 21 | format built by immediate stores (cuda-counter @38-54); `call 6` @60 | OUT record, format interned | 604 |
| J benchmark hash ops | 3 | `update(k0, 1, ANY)`; `p = lookup(k0); if (p) (*p)++`; `delete(k0)` (`benchmark/gpu/micro/cuda_probe.bpf.c:158-184`) | SET; ADD_IF_PRESENT(d = 1); DEL | 601; 600; 602 |

### R4.4 What each shape becomes, and its result against SR

**A, counter.**
- **Today.** `host_call` answers all 32 lookups of a warp before any lane branches. On first touch
  all lanes miss and 32 `BPF_NOEXIST` inserts race: one succeeds and 31 return EEXIST, which is
  ignored. Later warps hit and add through device aliases of the live element
  (`bpf_attach_ctx_cuda.cpp:136-144, 559-574`). Measured result: 97 of 128.
- **UnitFold.** One `UPSERT_ADD(k, d)` per lane.
  - The key `pid` comes from helper 14, so it is PROCESS-scoped, and d = r6 = 1 is a constant. The
    static path applies: one group, Σ = popc × 1.
  - The first touch of k in the process is one UNIT_ADD. Under the stripe lock the host inserts
    `c ⊕ Σ` with Linux's E2BIG check and creates the slot.
  - Every later group commits a record.
  - Result: 128 of 128.
- **The first-touch race does not exist in the unit.** Presence is decided by a host operation
  under the stripe lock (Lemma 4). Linux allows the loss; UnitFold never produces it.

**B, non-atomic RMW.**
- **Shape.** The looked-up pointer is passed as the update's *value* (`r3 = r0` @40, flag 2), and
  clang merged both updates into the one call @76. As a unit, "if present `v := v + d`, else insert
  d" is UPSERT_ADD(d, c = 0).
- **Linux** allows lost updates in two ways: two CPUs read v and both write v + d, and the EXIST
  self-write replaces the element, which loses concurrent in-place updates
  (`kernel/bpf/hashtab.c:1171-1185`).
- **Today's bpftime** collapses converged lanes to about one update per warp. Upstream #740 fixed the
  same loss in one example by making it atomic.
- **UnitFold** gives Σd, using the staging-sum path because d = `%globaltimer − *tsp` varies per
  lane. It differs from Linux only by excluding the lossy interleavings.

**C, leader lookup + update(ANY) of a computed value.**
- The value read is used only to form `*p + d` (or d on a miss) for an ANY update of the same key.
  It is a use inside the unit (rule P), so the unit is UPSERT_ADD.
- The fold adds in place where Linux replaces the element. Only a concurrent holder of an old
  pointer could tell the difference, and such a read is racy in Linux too.
- The site runs once per launch on one lane (all six indices zero, `example/gpu/cudagraph/cuda_probe.bpf.c:44-47`), so there is nothing to aggregate. The gain is the removed round trip.
- The result is exact even across overlapping launches.

**E, entry/exit timestamps.**
- **Entry SET.** Lanes are grouped by key: the block index, a static group. The group's
  highest-numbered active lane acts as leader and commits its own value, so no broadcast is needed.
  The lanes are concurrent and the last writer wins, as in Linux.
- **Exit READ.** cuda-counter's exit lookup (@14; `r6 = r0` @15) is used only by loads, at @19 and at
  @105, which is after the delete @27. It is never written through.
  - It becomes `resident_read`: LOOKUP_COPY by default, or READ_LOCAL with C-SB (R3.7).
  - In both cases the value lands in a reserved stack slot below the program's deepest access. The
    eBPF stack is 512 bytes; the GPU frame has 64 · frames + 10 eight-byte slots
    (`vm/compat/llvm-vm/local-call-return-block.patch:22-30`).
  - The pointer stays readable until the program exits, as an RCU-protected pointer to a deleted
    element does in Linux.
  - The analyzer requires that the program itself does not write in place to M between the lookup
    and the last use.
- **Exit DEL.** cuda-counter deletes `&pid`, which is not the block key the entry writes (survey
  record cuda-counter:70, key_scope constant). The key is therefore never present.
  - The first DEL is one UNIT_DEL that finds the key absent and marks the slot ABSENT.
  - Later DELs read the ABSENT hint and do nothing, with no host call.

**F and G, blind overwrites.**
- The result is unused (proven dead).
- E2BIG on a full map is decided at the key's first touch, which is a host operation (Lemma 4).
- F's keys are per-thread, so groups are singletons. The analyzer predicts this and picks the
  non-grouping variant.

**H, printk.**
- **Preconditions:**
  - the format bytes `[r1, r1+r2)` are constant in the stack lattice;
  - r2 is constant;
  - the format has no `%s` or pointer-dereferencing conversion (all 21 surveyed formats use only
    `%lu`/`%u`);
  - the return value is dead.
- **Rewrite.** r1 becomes the interned format id and r2 packs (size, site). The arguments stay in
  r3-r5, as in `_bpf_helper_ext_0006` (`default_trampoline.cu:386-408`).
- **Line count.** Lines are never merged: the count is observable. With C-PTX, lanes with identical
  (format, arguments) may share one record carrying a repeat count, which the drain prints that many
  times.

**J, benchmark.**
- `p = lookup(k0); if (p) (*p)++;` has no insert, so it is ADD_IF_PRESENT: `f(⊥) = ⊥`,
  `f(x) = x + 1`.
- In the micro benchmark's lookup mode nothing inserts key 0: the loader only selects the program
  (`benchmark/gpu/micro/cuda_probe.c:100-107`). The first UNIT_ADD marks the slot ABSENT, and every
  later unit is one hint load and a no-op.
- The update and delete become SET and DEL in slice 2.

**First-touch race and lost updates, summarised.**
- SR forbids both, and UnitFold meets that by construction for A, B, C and J.
- Linux allows both, so UnitFold guarantees more there.
- For plain RMWs that are *not* recognised, Linux semantics stay: their maps get no ledger (PW),
  and their sites keep today's path.

### R4.5 Kind consistency per map

- A ledger carries one add kind, fixed when it is created: UPSERT with template c, or IF_PRESENT.
- SET and DEL may share a ledger with adds of one agent. They use the combined LWW record (R3.2),
  in which `sum_at` and `count_at` order adds against writes.
- A unit of another kind on the same map is rewritten with the host-only bit and runs as UNIT_*,
  which is exact.
- READ_LOCAL is allowed only on a ledger owned by one agent.

## R5. Soundness direction

### Static fallback

The original instructions are kept, unchanged, whenever:
- a site matches no unit, or fails any of K/P/V/L/C;
- the map fd is not constant at the call;
- the map type is not `BPF_MAP_TYPE_HASH` (1501 counts, because it is created as HASH on the Orin);
- the map is absent from `map_info`;
- the map has `gpu_ptr_writer` or `cpu_ptr_writer` (R3.6);
- the ledger budget is exhausted;
- `run_with_kernel` is set;
- a counting build is used (`BPFTIME_ENABLE_MAP_USAGE_STATS`, the precedent of 1b3d9c99 at
  `bpf_attach_ctx_cuda.cpp:478-484`);
- the rewrite does not fit or fails the self-check;
- the matched pass `.so` does not export `ptxpass_site_exec_abi` with a version that covers the ids
  (R7.4);
- `BPFTIME_GPU_SITE_EXEC` is `off` (the default until slice 1 is validated) or `analyze` (records
  only).

### Dynamic fallback

Each fallback runs the **unit's** semantics in one host operation per group, so it stays exact.

| condition | outcome |
|---|---|
| `extra_buffer` is 0 | UNIT_* |
| `devmode` is SYNC | UNIT_* |
| the hint is not LIVE (for UPSERT) | UNIT_* |
| no slot is cached | UNIT_*, which also creates the slot |
| the ledger is full (NOSLOT) | UNIT_* every time for that key |
| the printk ring is full | synchronous printk after the watcher drains the ring |
| DEAD, or a `gen` mismatch | drop (D2) |
| IF_PRESENT with ABSENT | no-op |

### The synchronous path stays

Its protocol is unchanged: per-module lock, `occupy_flag`, 32 lane turns, `membar.sys`, and the
flag1/flag2 handshake (`default_trampoline.cu:134-175`). Its waiting rules change in four places:

1. **Fold before serving.** A synchronous lookup, update or delete on a ledgered map folds the key
   first, under the stripe lock: the watcher does more host work per request. The issuing thread's
   own earlier commits are ordered before it by its trailing `membar.sys` and by the existing
   `membar.sys` before `flag1` (`default_trampoline.cu:157-158`).
2. **Held slot lock.** A UNIT_* fallback holds the slot's device lock while it waits for the host.
   Other groups on that slot wait for up to one round trip.
3. **Drain first** (slice 3). A synchronous printk waits until the watcher has drained the printk
   ring up to its first uncommitted record.
4. **Warp-batched requests** (later slice, from performance-first). The lanes of a warp that share
   a request type and a map write up to 32 sub-requests into one request. The watcher serves them in
   lane order, so per-lane results equal today's lane-turn order. The warp waits once.

### Unregistered ids are impossible

An unregistered helper id or a map-value LDDW makes `generate_ptx` call `exit()` in the traced
application:
- a static `ExitOnError` at `vm/llvm-jit/src/llvm_jit_context.cpp:907`, invoked at `:936-939`;
- "Ext func not found" at `vm/llvm-jit/src/compiler_utils.cpp:366-369`.

Four measures prevent it:
1. Ids 600-604 are registered in `compile_ebpf_to_ptx_from_words` (`core.cpp:234-253`). memcapture
   compiles through the same function (`ptxpass_kprobe_memcapture/main.cpp:134-137`).
2. The ids the rewriter can emit and the ids registered there come from one header, and a unit test
   asserts containment.
3. The agent rewrites only for a pass that exports `ptxpass_site_exec_abi`, found with `dlsym`
   (R7.4). A stale pass therefore never sees the ids.
4. Defense in depth: an overlay hunk on `llvm_jit_context.cpp` (the overlay mechanism of
   `vm/compat/llvm-vm/CMakeLists.txt:16-34`) replaces the static `ExitOnError` with error
   propagation. A failed compile then falls back instead of exiting.

The names `_bpf_helper_ext_0600`-`0604` match the extern filter (`core.cpp:280-283`). They are
shared trampoline functions, so two probes on one kernel never define them twice.

### What uniformity may decide

Static facts select only the static group path and the predicted benefit. That path uses only value
invariance within a block, a property of one thread's dataflow. Position-flagged and timing-flagged
values always take the dynamic path.

## R6. Storage: where, how big, sized by what, when full, who drains

### Sizing

- **Capacity:** C = next_pow2(min(`max_entries`, keybound)), clamped to [64, 65,536].
- **keybound** is the analyzer's bound on distinct keys:
  - 1 for CONST or PROCESS keys;
  - `num_SM` for `%smid` keys;
  - `num_SM` × max warps per SM for `(smid, warpid)` keys;
  - otherwise `max_entries`.
- **Shards:** S = 1 in strict mode, or `num_SM` if opted in.
- **Agents:** A = 1 in strict mode.

| structure | where (Orin; discrete in R8) | size | sized by | when full | drained or folded by |
|---|---|---|---|---|---|
| ledger header + CPU index | segment | 256 B + 2C × (ks8 + 8) | C | index full: no new slot (NOSLOT) → UNIT_* for that key, exact | not applicable |
| slot state + watermarks | segment | C × (8 + S·A·24) B | C, S, A | not applicable | written by host folds |
| records | segment | ADD: C·S·A × 64 B (`ver` + 2 × {sum, count}, padded); LWW: C × round64(8 + 2·(48 + vs8)) | C, S, A, value size | not applicable (one per slot) | **nobody needs to**: every host access folds. Optionally the watcher folds dirty slots every 100 ms to bound later fold work |
| device block | device memory (agent) | NL × (4 B lock + 8 B staging) + C·S × 24 B shadows + 2C × (ks8 + 8) cache + LWW value shadows; NL = 2C | C, S | the cache is full: insert fails → the group uses UNIT_* | seeded from the committed records at a takeover (R3.8) |
| printk ring (slice 3) | segment, per agent | N × 48 B {seq, fmt_id, count, globaltimer, args[3]}; default N = 32 Ki (1.5 MiB) | line rate × watcher period × 4 | reservation fails (device counter − host-written head ≥ N, checked before a CAS reservation) → synchronous printk; the watcher drains first | the agent's watcher on every turn (≤ 1 ms), before any synchronous printk, and at exit (a new drain step in `stop_cuda_watcher_thread_at_exit`, `bpf_attach_ctx_cuda.cpp:88-104`, waiting at most 100 ms [new]); orphan drain by the loader when the agent is dead |
| format table (slice 3) | segment | ≤ 1,000 B per interned format | distinct formats | full → that site keeps synchronous printk | read by the drainer |

### Examples [inferred from source sizes]

- **cuda-counter `call_count`** (pid key): C = 64. ADD records 4 KiB, index 2 KiB, slots 2 KiB,
  device block about 4 KiB.
- **threadscheduling `sm_histogram`** (`%smid` key; 8 SMs on the Orin Nano, read from the device
  attribute at start): C = 64.
- **threadscheduling `thread_mapping`** (4,096 entries, 80-byte value, LWW): C = 4,096. Records
  4,096 × 320 B ≈ 1.25 MiB; index 128 KiB; device shadows about 0.5 MiB.

### Budget

The default ledger budget is 16 MiB (`BPFTIME_GPU_LEDGER_BUDGET_MB`) of the 128 MiB segment
(`runtime/include/bpftime_config.hpp:78-81`). The same segment already holds:
- every map (the config comment notes that 128 MiB fits memleak's 1M-entry hash);
- `CommSharedMem` with two 1 MiB staging buffers (`gpu_helper_protocol.h:12-16`).

When the budget would be exceeded, the analyzer ranks maps by predicted saving (frequency × round
trips removed) and leaves the rest synchronous. That is a static fallback, so it stays exact.

### Reclamation and lifetime

**Reclamation.** Slots are never reclaimed in the first versions. A key without a slot stays on
UNIT_*, which is exact. Reclaiming a slot would require proving that no GPU thread still caches its
index. That would take a flip-style lock sweep per slot; it is future work, and nothing waits on the
application to complete.

**Lifetime:**
- A ledger and its device blocks live while any agent that holds the block address lives. Modules
  cannot be unloaded (`nv_attach_impl.cpp:2387-2390`).
- After the owner exits (its pid leaves the alive set), the ledger stays. It is either taken over
  by the next agent that defers on M, which seeds its shadows from the records (R3.8), or freed
  with the map. Effects committed before an exit or a SIGKILL are in the segment and are folded by
  the next reader (T9).

### Aggregated data

Aggregated data needs no storage of its own. Lanes reduce at the instruction, through the device
staging word under the slot lock or through shuffles, and the result goes straight into the
record. Nothing is buffered in a thread across units, so nothing needs a flush before a synchronous
call or at exit.

### Direction 1's storage question

Nothing here is sized by the grid:
- ledgers by `max_entries` and the key bound;
- rings by the line rate and the SM count.

Launches that no hook notes (graphs, CUDA 13 handles, captures) therefore cannot outgrow a
launch-time size.

## R7. The analyzer

### R7.1 Where and when it runs

The analyzer runs in the agent, the traced CUDA process. It is the first place where everything it
needs is together:
- the stored instructions;
- the fd-indexed map metadata;
- the attach type;
- segment access.

The syscall server stores `cuda__` programs verbatim and runs no verifier on them.

| step | location | what it does |
|---|---|---|
| global step | between the two loops of `init_attach_ctx_from_handlers` (`runtime/src/attach/bpf_attach_ctx.cpp:229-245` and `:248-259`) | analyses every program handler, CPU and GPU; computes per-site decisions, pointer-writer flags and kind consistency; creates or joins ledgers and device blocks; stores the rewritten words per GPU program. It runs before any CUDA link is instantiated, so before any patched module exists |
| per link | the CUDA link branch (`bpf_attach_ctx.cpp:629-648`) | passes the rewritten words at `:639` instead of `prog->get_insns()`; `create_map_basic_info(1024)` at `:643-644` carries the device-block addresses. The `nv_attach_impl` interface is unchanged apart from one capability query (R7.4) |
| late links | an auto-refresh re-runs `init_attach_ctx_from_handlers`; the global step analyses only new handlers | pointer writers trigger the flip (R3.6) |
| offline | `bpftime-site-analyze` [new], the same library | reads `.bpf.o` files with libbpf, map relocations and `.BTF.ext` line info, for the R7 check |

**Cost.** Upstream measured 2.2-2.5 ms on x86 for the uniformity pass alone on a 4,096-instruction
program (`bpftime-verifier/test/device-verifier-phase-attribution-20260904.md` on
`official/revision/automatic-warp-execution`, as read by the semantics draft). That is small next
to the per-PTX nvPTXCompiler work [inferred].

**CUDA context for the device blocks.** The global step runs before the link branch checks
`cuda_ctx` (`bpf_attach_ctx.cpp:623-628`). Device blocks are therefore allocated inside a helper
like `nv_attach_impl::with_cuda_context` (`nv_attach_impl.cpp:2306`, used at `:2370` to write
module globals), with the device's primary context current [inferred].
- If no CUDA context exists at that point, no ledger is created and no site is rewritten. The link
  branch then skips CUDA attach anyway.

### R7.2 Inputs

1. The stored words of every program handler.
2. The map handlers: type after the Orin remap, key and value size, `max_entries`, `gen`, frozen
   state, ledger pointer and owner.
3. The attach type of each link (entry, return, memcapture) and its kernel names.
4. The fd-indexed `MapBasicInfo` view (`bpf_attach_ctx_cuda.cpp:505-554`).
5. The start-up cost and capability parameters (R8).
6. Configuration:
   - `BPFTIME_GPU_SITE_EXEC=off|analyze|on` (default off until slice 1 is validated);
   - strict mode or opt-in shards and multiple agents;
   - `BPFTIME_GPU_PRINTK_SYNC`;
   - per-site overrides for tests and mutations;
   - whether this is a counting build or `run_with_kernel`.

### R7.3 Passes

1. CFG, dominators and post-dominators.
2. Abstract interpretation (R4.1).
3. Def-use and liveness. This gives `result_use` (unused, null_check_only, value_used,
   pointer_rmw) and the pointer-confinement rule P.
4. Unit matching (R4.1, R4.3), with one logical site per (call pc, reaching definition of the flags
   argument).
5. Per-site decision. The `decision` values are:
   - `defer_unit` for every member call of an A, B, C or J-lookup unit;
   - `defer_set` and `defer_del` for an overwrite or delete whose result is dead;
   - `resident_read` for a read-only lookup whose value is used;
   - `defer_out` for a printk with a constant format;
   - `unchanged` when `host_call_today` is no;
   - `sync` otherwise.

   Each decision also records a grouping mode (static or dynamic) and a fallback reason.
6. Dry-run rewrite and self-check (R4.2) for every unit the runtime implements and the gates allow.
   A unit that does not fit or fails the check is marked unrewritten **here**. The rewritten words
   do not depend on any ledger address: the device block reaches the code through `map_info` at
   run time.
7. Global step, over the outcome of pass 6: pointer-writer flags (an unrewritten pointer-writing
   site counts), kind consistency, budget ranking, ledger creation. A unit whose map ends up without
   a ledger keeps its original words. Its pointer accesses then belong to a map with no ledger,
   which is today's behaviour.

### R7.4 How a decision reaches code generation, around each obstacle

| obstacle (from the code) | how UnitFold avoids it |
|---|---|
| `generate_ptx` has no hook and runs a single `optimizeModule` (`vm/llvm-jit/src/llvm_jit_context.cpp:903-950`) | no LLVM change is needed: the decision is in the bytecode, which the pass compiles as before (`core.cpp:226-264`) |
| IR carries no eBPF pc | no pc is used. The callee id is the mechanism, and the site's descriptor travels as immediates in argument registers: `desc` (kind, field offset and width, template id, grouping, host-only bit) and `site` |
| O3 may merge, sink or duplicate calls | calls to the same id with different immediates can only be sunk into one call with phi'd arguments. The helper reads the descriptor at run time, so that is still correct [inferred: SimplifyCFG sinking keeps arguments as phis]. Duplication is harmless. Checked by a PTX dump in R10 |
| unregistered helper ids `exit()` the application | registration, a containment test, the capability gate and an ExitOnError overlay (R5) |
| memcapture prepends instructions and compiles once per access (`ptxpass_kprobe_memcapture/main.cpp:53-137`) | the encoding has no pc; the prepended stores do not touch the unit's registers; READ_LOCAL is disabled under memcapture |
| the extern filter only matches four-digit helper names (`core.cpp:280-283`) | the new names `_bpf_helper_ext_0600`-`0604` match it |
| the patch cache is keyed by the sha256 of the request, which contains the instruction words (`nv_attach_impl.cpp:994-1027`) | rewritten words get their own cache entries. Site ids come from an agent-wide counter in analysis order, and each program is analysed once per session |
| the pass is a `.so` loaded per attach type | each pass exports `int ptxpass_site_exec_abi(void)` [new]. `nv_attach_impl` looks it up with `dlsym` next to `print_config`/`process_input` (`nv_attach_impl.cpp:601-614`) and records it per pass configuration. A new query, `site_exec_abi_for(attach_type, func_name)` [new], tells the analyzer what the matched pass supports. A missing symbol means "rewrite nothing for this link". This does not depend on how `from_json` treats unknown keys of `RuntimeInput` (`ptxpass/core.hpp:71-80`) |
| `map_info` is a snapshot and last-link-wins (`nv_attach_impl.cpp:233`) | it carries only the stable device-block pointer; cross-program facts live in the segment |

### R7.5 Output records and the mechanical check

**Records.** One record per *logical* call site, with the survey's field names: `file`, `line`,
`section`, `helper`, `map`, `map_type`, `host_call_today`, `result_use`, `key_scope`, `operation`,
`cpu_reads_map`, `aggregatable`, `frequency`, `idiom`, `class`. Added fields: `decision`,
`fallback_reason`, `unit_id`, `site_id`, `pc`, `grouping`, `slice`.

How the fields are produced:
- **`file`, `line`.** Taken from `.BTF.ext` line info offline. In the agent they become program name
  and pc.
- **Merged calls.** A merged call gives one record per incoming flags edge, attributed to the line
  of that edge's argument setup. cuda-counter's retprobe pc 76 gives two records: line 76 through
  `r4 = 2` @41, and line 79 through `r4 = 1` @75.
- **`host_call_today`.** A function that mirrors the trampoline dispatch
  (`default_trampoline.cu:233-422`).
- **`key_scope`.** From the scope lattice: CONST and PROCESS → constant, BLOCK → per_block,
  WARP → per_warp, THREAD → per_thread.
- **`cpu_reads_map`.** An **external input**. It is `unknown` in deployment and joined from the
  survey offline, because R3 makes it unknowable at load time.
- **`aggregatable`.** Three-valued:
  - **no** if any key or argument source is THREAD-scoped without the timing flag;
  - **timing** if any source is `%globaltimer`;
  - **yes** otherwise.
- **`class`.** A stated function of `decision` and the external input:
  ```
  class(unchanged, _)        = gpu_local_already
  class(d, cpu_reads_map=no) = eliminate            (d ≠ unchanged)
  class(resident_read, yes)  = sync
  class(d, yes | n/a)        = defer                (d ∈ defer_*)
  ```

**Row filter.** `records.json` has 180 bpftime records after deduplication on (file, line,
section). They split into 109 gpu_local_already, 20 eliminate, 39 defer and 12 `cpu_side`. The 12
are CPU-side call sites and are excluded, which leaves the 168 GPU sites.

**What the check asserts, headline first:**
1. The per-record `decision` for all 168 GPU sites:
   - `unchanged` exactly on the 109 with `host_call_today = no`;
   - a non-`sync` decision on all 59 host-call sites;
   - agreement with the survey's own fields: `defer_set` ↔ (overwrite, unused); `defer_del` ↔
     (delete, unused); `resident_read` ↔ (read, value_used); `defer_out` ↔ (output, unused);
     `defer_unit` ↔ a member of an accumulate, insert_if_absent or update_if_exists unit.
2. `key_scope` per record for the 38 map sites. For printk the survey is inconsistent (none vs the
   argument scope), so it is reported, not asserted.
3. `aggregatable`: 47 yes + 8 timing + 4 no, record by record, with the timing records listed.
4. The class tally 109 / 20 / 39 / 0 and class per record. This is **derived**: given the input
   column `cpu_reads_map`, the 20/39 split follows for almost any decision table. Only the three
   `resident_read` rows, which would become `sync` if a CPU read the map, test anything. Assertion
   1 is the one that tests the analyzer.

**Result now (a simulation, not the analyzer).** `final/r7_check.py` applies the decision rules to
the 59 records, using a hand-written provenance table in place of the analyzer's state. Output:
`final/r7_output.txt`.
- **Decisions:** `defer_unit` 24, `defer_out` 21, `defer_set` 7, `defer_del` 4, `resident_read` 3.
  Against (operation, result_use): **0 mismatches**.
- **`defer_unit` members:** 9 (accumulate, pointer_rmw), 2 (accumulate, unused), 2 (accumulate,
  value_used: cudagraph's C lookups), 8 (insert_if_absent, unused), 3 (update_if_exists, unused).
- **Class:** 20 eliminate, 39 defer, 0 sync, plus 109 unchanged; 0 per-record mismatches.
- **`key_scope`:** 38 map sites, 0 mismatches.
- **Aggregatable:** 47 yes / 8 timing / 4 no.
  - The yes and no records agree with the survey record by record.
  - The 8 timing records are cuda-counter:55/82, rocm-counter:56/83, launchlate:114/117 and
    launchlate-kernel-gpu-shared-map:119/122.
  - The survey marked the first four aggregatable ("the warp reads `%globaltimer` once") and the
    last four not, although launchlate's timer read also comes first. Its 51 = 47 + those 4.
  - No consistent rule reproduces 51: PR #624's helper table gives 47 with 12 flips;
    hardware-invariant positions with a varying timer give 47 with 4 flips; a uniform timer gives
    55 with 4 flips.
  - Grouping is decided at run time from key bytes, so this stance affects only the predicted
    benefit, never correctness.
- **Slice 1 rewrites 20 sites:** A 10, B 9, J lookup 1.

**The real check (slice 1).**
- The offline driver links the analyzer library and reads the `.bpf.o` files of the 22 bpftime
  programs.
- rocm-counter has no built object in dev. The check builds `rocm_probe.bpf.c` with the examples'
  BPF flags (`clang -target bpf -g`); this needs no ROCm.
- The driver joins on (file, line, section, helper) and asserts items 1-4 above.
- Mutation M15 (R10) mutates the check itself.

## R8. Generality: discrete GPUs and other hardware

### R8.1 Parameters measured at start-up

The agent measures these at its first module load:
- a calibration kernel, plus a reader thread in a helper process for the litmus tests;
- cached in the segment per (device UUID, driver, build), so later agents and start-ups pay nothing.

| parameter | meaning |
|---|---|
| `c_rt` | one no-op host call through the existing protocol |
| `c_commit(k)` | one record commit with k warps contending on one slot (k = 1, 32, 3,136, 31,264) and on distinct slots |
| `c_ld_sys`, `c_st_sys`, `c_fence_sys` | segment load, store and `membar.sys` |
| `c_atom_dev` | device-memory atomic |
| `c_hld` | host load of a GPU-written record line |
| device shape | `num_SM`, warps per SM, compute capability, integrated flag |
| capability bits | C-SB, C-XA, C-PTX, C-SCHED (R3.1) |
| litmus results | H2, H3, H4 |

### R8.2 Cost model (per group of g lanes, at a site executed f times per launch)

- **Synchronous:** `T_sync = g · c_rt`. It is serialized across the whole GPU by the single request
  slot, so throughput is at most `1/c_rt`.
- **Deferred:** `T_def ≈ c_atom_dev + c_ld_sys + 4·c_st_sys + 3·c_fence_sys`. When W warps hit one
  slot, they serialize on its lock: about `(W/S) · T_def` per slot.
- **First touch:** one `c_rt` per (key, agent).
- **Reader cost:** a host read of a key costs about `S·A·c_hld`.

The analyzer defers when a site is feasible and `f·T_def + first touches < f·T_sync`. On the Orin,
with `c_rt` ≈ 10 us and an estimated `T_def` of a few µs [inferred], this holds for every feasible
site at 32 threads. For hot keys at large W, the model:
- reports the predicted serialization;
- suggests the opt-in shard mode (D1 applies);
- never silently weakens semantics.

Hardware changes these numbers, not the protocol, the proof or the records.

### R8.3 What changes on a discrete GPU

- **Placement.** Records, index, hints and the printk ring stay in pinned host memory: the segment,
  which every agent registers as today (`bpftime_shm_internal.cpp:1407-1475`).
  - This is what lets the loader fold without the agent (R3).
  - It keeps committed effects reachable after an agent SIGKILL.
  - The device block stays in device memory, where locks, shadows and the key-to-slot cache are
    local.
  - A device-resident ledger with kernel-completion visibility (S4 in BRIEF) would be cheaper, but
    it would break both properties and is not offered.
- **Costs.**
  - Commits become posted PCIe writes plus `membar.sys`.
  - Hint loads become PCIe reads, an estimated 1-2 us [inferred]. Hints are optional for
    correctness (R3.3), so the cost model may drop them on PCIe and rely on UNIT_* responses and
    straggler folding. The exception is the ABSENT negative cache, which then costs a host call.
  - `c_rt` grows, so the benefit grows.
- **1501 on CUDA-IPC GPUs** is a real device hash map (`runtime/src/bpf_map/gpu/nv_gpu_shared_hash_map.cpp:47-62`).
  Its host-side lookup and insert paths are not ledger targets until they are checked, so its sites
  stay synchronous.
- **Older GPUs** (sm_61, no independent thread scheduling):
  - Only one lane per group locks, and groups run one after another. Commit sections contain no
    intra-warp spin, so the hazard at `default_trampoline.cu:128-133` does not arise [inferred].
  - This is argued, not tested; claiming it needs a run on an sm_61 board.
- **Primitives.** The core uses only what the trampoline already emits: `activemask`,
  `bar.warp.sync`, `popc`, unscoped `atom.*`, and `membar.sys`/`gl`. The trampoline is compiled for
  sm_61 (`attach/nv_attach_impl/trampoline/Makefile:3, 26`) and retargeted to sm_87
  (`nv_attach_utils.cpp:52-65`).

## R9. Interactions

| interaction | behaviour | why it stays exact |
|---|---|---|
| several agents on one segment | **Strict default:** one owner agent per ledger; other agents' units on M use UNIT_*. **Opt-in:** per-agent records (D1). Flip requests go through the segment (R3.6) | each record has one writing context, so cross-context GPU atomics (C-XA) are never needed. Pre-existing and not fixed: one `CommSharedMem` per segment, zeroed by each agent at start (`bpf_attach_ctx_cuda.cpp:615`), and any watcher may serve any request (`CLAUDE.md:2316-2319`). A per-agent request slot is a prerequisite for multi-agent use |
| CPU probes on the same maps | their lookups fold first, including the typed paths (R3.5); their writes are host operations; atomic adds through pointers are allowed next to add-only ledgers; other pointer writes keep M out of DEFER (R3.6) | Invariant I; the commuting CPU atomics run on CPU words only |
| graph launches and unnoted CUDA 13 handle launches (`probed_launch_activity.cpp:16-24`; `nv_attach_impl.cpp:1561-1592`) | their units commit and are folded like any other | correctness never uses launch or completion knowledge; the flip uses the agent's own kernel, not application quiescence |
| map close and fd reuse | `gen`, DEAD, ledgers outliving maps (R3.8) | rewritten sites can never touch the map at a reused fd |
| printk ordering (slice 3) | each thread in program order; across threads in reservation order (consistent with happens-before); every deferred line before any later synchronous line (drain first); up to one watcher turn behind the application's own output (D3) | no loss: a full ring falls back to synchronous printk after a drain |
| several probes on one kernel (entry + return, memcapture, two entry probes) | new helpers are shared trampoline functions, so they add no per-probe symbols; site ids are unique per agent; programs that defer on one map share its ledger and must agree on the kind | the existing `__probe_func__<kernel>` name clash for two entry probes (`ptxpass_kprobe_entry/main.cpp:53`) is neither fixed nor widened |
| return probes and divergence | return probes run before every `ret`/`exit`, predicate included (`ptxpass_kretprobe/main.cpp:50-61`); memcapture probes run after each memory access, so lanes may diverge | groups form from `activemask` at the call (Lemma 3) |
| deferred and synchronous sites on one map | a synchronous request on a ledgered map folds first; the thread's own commits precede it (trailing `membar.sys`, then `membar.sys` before `flag1`) | a synchronous lookup whose result is written through would be a pointer writer, which keeps M out of DEFER |
| detach | the last detach waits up to 2 s for patched launches, then switches probes off (`nv_attach_impl.cpp:160-170`); ledgers outlive the detach | in-flight commits are folded by the next reader |
| agent exit or SIGKILL mid-section | the committed half selected by the last even `ver` stays valid; a section that never finished never committed | the unit is not in ≺, like a probe killed half-way |
| `run_with_kernel`, frozen maps, 1504, per-CPU maps, LRU, arrays | not ledgered (frozen maps: D6 for already-ledgered maps) | not applicable |

## R10. First slice: the smallest end-to-end piece measurable on the Orin

### Step 0 (gate, about one week, a standalone test program; nothing in dev changes)

1. **Litmus tests** run between a GPU kernel and a CPU thread **in another process** that maps a
   Boost segment registered as bpftime registers it. Each uses the design's exact fences, 10^7
   iterations, and randomized delays.
   - **H2:** GPU stores, `membar.sys`, flag.
   - **H3:** CPU release, then GPU load and `membar.sys`.
   - **H4:** torn reads. 1,024 warps write `ver`/sum patterns with equal halves while the CPU reads.
   - **C-SB, C-XA:** reported only.
   - Fence-free variants are **reported, not required to fail**: stronger hardware is not a defect.
2. **PTX acceptance** in a module retargeted to sm_87 through the agent's compile path:
   `shfl.sync`, `vote.ballot.sync`, `match.any.sync`, `redux.sync`, `nanosleep`,
   `red.global.add.u64` (C-PTX).
3. **Calibration:**
   - a breakdown of the ~10 us host call with `%globaltimer` stamps inside `host_call`: lock taken,
     request written, `flag1` set, `flag2` seen, released;
   - `c_commit(k)` at k = 1, 32, 3,136 and 31,264 warps, on one slot and on distinct slots;
   - `c_ld_sys`, `c_st_sys`, `membar.sys` alone;
   - C-SCHED: launch latency of a one-block agent kernel while a saturating kernel runs.
4. **Exit.** If H2, H3 and H4 pass, build slice 1. Otherwise UnitFold stops at the analyzer and the
   R7 check, and every site keeps today's path.

### What to build (slice 1, about four weeks after step 0)

1. **Analyzer library and offline driver** (R4.1, R7): units A, B and J-lookup; decision records for
   all sites; pointer-writer scan for GPU and CPU programs; in-place `ja +0` rewrite and self-check.
   No general patcher, no value-number re-analysis.
2. **Helper 600** in the trampoline:
   - static group path (n × d, or the staging sum under the slot lock);
   - per-lane path in lane-turn order for dynamic keys;
   - record commit;
   - UNIT_ADD fallback.
   - No `match.any`, no `%smid`/`%warpid` scratch.
3. **Ledgers for ADD kinds**, strict mode only (S = 1, one owner agent). Device block, flip kernel
   and flip request, with `T_flip` = 1 s.
4. **Host side:**
   - hooks in every HASH entry point of R3.5, typed paths included;
   - the fast path;
   - `map_lookup_copy`;
   - the fold, the sweep and the relaxed free counter;
   - `gen` and DEAD;
   - the UNIT_ADD opcode in `serve_cuda_request`;
   - fold-before-serve for synchronous requests.
5. **Registration and gating:** id 600 in `core.cpp:234-253`, `ptxpass_site_exec_abi`, the
   containment test, the ExitOnError overlay hunk, and `BPFTIME_GPU_SITE_EXEC`.

**Not in slice 1:** SET, DEL, LOOKUP_COPY, READ_LOCAL, printk, idiom C, `match.any`/`redux`, shards,
multi-agent records, warp-batched synchronous requests, tier-1 inlining, discrete GPUs.

### Programs and expected numbers

**Anchors.** Measured on aa6eb973 (`hostcall-l1-20261111`, `baseline-sub-20261111`):
- probed-launch floor about 48 us (CPU Array lookup 48.2 us);
- one host call about 10.7 us.

The baseline is **re-measured at 52126f33** as build D, because the AC3 watcher changed the
per-call cost (11.8/15.7 us in bursts, `watcher-policy-20261112/v3/README.md:57-58`). `c_commit`
comes from step 0; no number below assumes it.

| case | today (measured on aa6eb973) | slice 1, expected [inferred] | exactness check |
|---|---:|---:|---|
| micro "CPU Hash map lookup" (minimal; J lookup on key 0, which nothing inserts) | 341.1 us | ≈ 48-50 us from the second launch: one hint load per warp, no host call; one UNIT_ADD once per process | map stays empty; 0 requests after the first launch |
| new micro mode "hash-lookup, key present" (the loader inserts key 0 = 0 first) [new benchmark mode] | ≈ 330-340 us (like the other hash cases) | ≈ 48 us + `c_commit(1)` | final = 32 × launches |
| new micro mode "hash-counter" (A idiom on the pid key, cloned from cuda-counter's entry) [new] | ≈ 330-340 us | ≈ 48 us + `c_commit(1)`; one UNIT_ADD in the first launch | final = 32 × launches |
| CUDA Counter (minimal) | 2,877.5 us; 264 requests per launch; `calls=97` | 160 requests per launch (the 5 per thread that remain: entry SET, entry printk, exit READ, exit DEL, exit printk) + 2 UNIT_ADD per process; ≈ 48 + 160 × 10.7 + 2·`c_commit(1)` ≈ 1.76 ms + 2·`c_commit(1)` | **`calls=128`**; `total_time_ns` = Σ of per-thread deltas (T2) |
| threadscheduling example | 3 host calls per thread | 1 per thread (the `thread_mapping` SET stays synchronous) + 2 per-lane commits per thread | Σ `sm_histogram` = Σ `warp_histogram` = threads × launches |
| hash-counter small (1,024 threads) / large (100,352 threads, one timed launch, total timeout) | ≈ 11 ms / ≈ 1.07 s per launch [inferred: threads × 10.7 us] | **no number** until step 0 measures `c_commit(32)` and `c_commit(3,136)`; strict mode serializes all warps on one slot lock | final = threads × launches |
| CPU-probe lookup on a ledgered HASH (a CPU uprobe looking up in a loop) | ns per lookup on dev | reported: fast-path overhead (index probe + S loads) | unchanged values |

### How to measure

- **Builds.** D (52126f33) against N (52126f33 + slice 1), both built with the compare flags.
- **Runner.** The upstream `benchmark/gpu` runner with legacy-launch victims; 10 interleaved rounds
  in alternating order; Mann-Whitney p, as in `hostcall-l1-20261111`.
- **Before every batch:**
  - jetson_clocks checked (cpu5 1728000, GPU 1020000000);
  - no leftover processes;
  - no bpftime segment;
  - root-owned `/dev/shm` test segments removed;
  - total timeouts set outside `LD_PRELOAD`;
  - containers removed afterwards.
- **One smoke round** at debug log level counts host requests from the watcher's
  `CUDA Received call request` lines.
- **At exit,** ledger statistics are dumped: commits, UNIT_* fallbacks, `dead_drops`,
  `e2big_sweep`, `late_stragglers`, `unstable_reads`.
- **PTX is kept** (`BPFTIME_CUDA_KEEP_PTX=1`) to confirm one `call _bpf_helper_ext_0600` per unit,
  with the descriptor reaching it after O3.

### Exactness tests (oracles from SR, never today's output)

| test | what | pass condition |
|---|---|---|
| T1 first touch | counter on an empty HASH; N threads × L launches; 1-1,024 warps; partial warps | final = N·L |
| T2 non-atomic RMW | `total_time_ns`-style sum; every lane also writes its delta into a 1512 per-thread map | sum = Σ per-thread deltas |
| T3 real-time completeness (two-sided) | the application stamps each launch's completion (`CLOCK_MONOTONIC` after `cudaStreamSynchronize`); the loader polls the key every 100 us, stamping start and end | for every read v: N·#(completed before read start) ≤ v ≤ N·#(started before read end); successive reads monotone |
| T4 print and clear | the loader runs `lookup_and_delete` every 100 us-1 ms while kernels run | Σ returned + final = N·L |
| T5 host overwrite | loader `update(ANY, X)` mid-run | final − X lies between #units started after the update returned and #units completed after it began |
| T6 CPU probe, typed path | a CPU uprobe whose program uses **only HASH maps** (so it gets `bpftime_hash_map_*_helper`) adds atomically 1 per launch to the GPU's key; variant: the uprobe reads the key and checks v ≥ the committed count | final = N·L + L; reads pass |
| T7 close and fd reuse | the loader closes the map mid-run and creates a map that takes the fd | the new map gets no GPU effects; `dead_drops` > 0 |
| T8 ledger full | capacity forced to 2 with 8 keys | all 8 keys exact through UNIT_ADD |
| T9 application exits or is killed first | the loader reads after the CUDA process exited, and after a SIGKILL during kernels; then the application is started again under the same loader (takeover, R3.8) | the first read equals the commits that completed (witness ring, T15); after the second run, final = both runs' completed commits |
| T10 torn reads | H4 stress on records | no torn half ever accepted |
| T11 zero delta | the key is deleted by the host, then the GPU adds d = 0 | the key is re-created with c (count decides, not the sum) |
| T12 full map | `max_entries` = 4; the host deletes and re-inserts while the GPU re-touches deleted keys | first-touch E2BIG equals Linux's NOEXIST; no more than `max_entries` keys ever; `e2big_sweep` reported |
| T13 flip | a late CPU program with a plain store through a lookup pointer of M is attached mid-run | it attaches within `T_flip`; final = SR oracle; `late_stragglers` = 0 when C-SCHED holds; a forced-timeout variant reports > 0 and still loses no committed delta |
| T14 translation validation | the original words run in the llvmbpf CPU JIT over random serial schedules of the same units and are compared with the GPU run's final state; near-miss fixtures (init ≠ delta, pointer escape, a store to the key slot inside R, a FETCH add, a jump into R) | equal final states; near-misses are not rewritten |
| T15 witness ring (test builds) | every deferred lane also writes (site, key, delta) into a 1527 ring drained by the loader | a replay matches the loader's merged reads |
| T16 divergence | a return probe on a kernel whose lanes return at different points | count = #threads |
| T17 R7 survey check | offline driver over the 22 objects | R7.5 assertions |

### Mutation checks (each must fail the named test; sources restored and hashed after each)

| mutation | must fail |
|---|---|
| M1 no hook in `map_lookup_hash` (typed path) | T6 |
| M2 fold after the host operation instead of before | T4, T5 |
| M3 fold without the stripe lock | T4 under stress |
| M4 the CPU resets record words instead of keeping watermarks | T4, T5 (and a second writer appears) |
| M5 group count = `popc(activemask)` for per-lane-key groups | threadscheduling histogram sums; T1 with key = lane % 4 |
| M16 a new owner does not seed its shadows from the records | T9 second run (totals go backwards) |
| M6 re-creation decided by Δsum ≠ 0 instead of Δcount | T11 |
| M7 no odd `ver` store (or no `membar.sys` between `ver` and data) | T10, T15 |
| M8 matcher accepts init ≠ delta | T14 fixture |
| M9 matcher drops the liveness check of r0 at the join | T14 fixture |
| M10 no `gen` check / DEAD ignored | T7 |
| M11 helper 600 removed from the registered list | containment unit test (before any application exit) |
| M12 flip kernel skips the lock sweep | T13 under stress (witness mismatch) |
| M13 sweep removed | T12 |
| M14 loader lookup copies outside the fold section | T3 (non-monotone reads under stress) |
| M15 check script: cudagraph:56's value use treated as "needed now" | T17 reports 20/38/1 |

### Exit criteria

1. Step 0 passed (H2-H4). C-SB, C-XA, C-PTX and C-SCHED are reported.
2. T17 passes with the real analyzer library.
3. T1-T16 pass, and M1-M16 each fail their named test.
4. Measured on the Orin:
   - CUDA Counter `calls=128` and 160 requests per launch;
   - CPU Hash map lookup within 1.2× of CPU Array map lookup;
   - hash-counter within noise of 48 us + measured `c_commit(1)`;
   - CPU-probe lookup overhead reported.

### Later slices

| slice | content |
|---|---|
| 2 | SET, DEL, LOOKUP_COPY, idiom C, general patcher |
| 3 | printk ring, drain and orphan drain; `BPFTIME_GPU_PRINTK_SYNC` |
| 4 | C-PTX grouping (`match.any`, `shfl`/`redux` reductions); warp-batched synchronous requests; tier-1 IR inlining of helper 600 through the llvmbpf overlay between `optimizeModule` and codegen (`llvm_jit_context.cpp:947-948`) |
| 5 | READ_LOCAL under C-SB; opt-in shards and multi-agent records; discrete GPUs |

**Extension.** A plain RMW on GPU-local maps that loses counts today (mem_trace's
`*val = *val + 1` on 1503, ARRAY `(*v)++`) can be rewritten into one atomic add (ATOMIC_LOCAL). These
are not host-call sites, so this is outside the 59.

## Open questions

1. Do H2, H3 and H4 hold on `cuMemHostRegister`'ed segment memory with a reader in another process
   on the Orin (step 0)? If not, every site keeps today's path.
2. What is `c_commit` (one device CAS, one uncached load, four uncached stores, three `membar.sys`)
   uncontended and under contention? Is strict mode (S = 1) acceptable for hot constant keys at
   100k threads, or must the shard mode (with D1) become the default there?
3. C-SCHED: is an agent-launched flip kernel scheduled within `T_flip` while application kernels
   occupy every SM? Otherwise D5 may occur.
4. C-PTX: are `shfl.sync`, `match.any.sync` and `redux.sync` accepted as PTX text in the retargeted
   sm_87 module?
5. C-SB: does CPU-GPU store buffering hold? It decides whether the three `resident_read` sites run
   on the GPU (READ_LOCAL) or as one LOOKUP_COPY per group.
6. Does LLVM 15 O3 sink calls to helper 600 with different descriptors? This is harmless for
   correctness, but it disables the constant-descriptor fast path; check by PTX dump.
7. How much do fold hooks cost on CPU-probe lookups (fast path) and loader lookups (always locked)?
8. The survey's `aggregatable` column is inconsistent on `%globaltimer`: 51 = 47 + 4 timing records.
   The survey should be corrected rather than the analyzer tuned.
9. Does the micro hash_lookup unit fit in place? Its object is not built in the dev worktree.
10. Multiple agents: the shared, zeroed `CommSharedMem` needs a per-agent request slot first.
11. Slots are never reclaimed in the first versions, so churning keys fall back to UNIT_* (exact,
    slower). Reclamation needs a per-slot lock sweep.
12. Is the D6 behaviour change for frozen maps (Linux semantics instead of today's EPERM) acceptable
    to users?
13. Discrete GPUs: PCIe costs of posted stores, `membar.sys` and hint loads; correctness without
    independent thread scheduling (sm_61) is argued, not tested.
14. The D4 corner (E2BIG order among several pending stragglers on a nearly full map) is counted,
    not excluded. Is a store-buffering-free exclusion possible?
15. Takeover after an owner dies: is seeding the shadows from the committed record (including a
    `ver` left odd by a SIGKILL) correct on the hardware? T9's second run checks it.
16. How much do colliding keys contend in the hashed lock table (NL = 2C)? Only cost is at stake;
    measure in step 0 with distinct keys.

## Coverage of the reviewers' must-fix items

**Correctness and novelty review**

| # | item | where |
|---|---|---|
| 1 | hook every host path, typed fast paths included; test with an all-HASH CPU program | R3.5; T6, M1 |
| 2 | upsert creation independent of the delta value | count carried with sum (R3.2-R3.4); T11, M6 |
| 3 | no unordered multi-word pending state | two-half seqlock with a single `ver` (R3.3-R3.4, Lemma 1); T10, M7 |
| 4 | no lost data in a cut with incarnations | no incarnations; cumulative totals with CPU watermarks; the host never waits on odd `ver` (R3.4); stragglers ordered after (Lemma 2) |
| 5 | core relies only on facts already relied on; extras as capability bits | R3.1 (H1-H5 with precedents; C-SB, C-XA, C-PTX, C-SCHED gated) |
| 6 | real-time clause; commit before return; per-map scope | R1.1-R1.2 (hb clause, trailing `membar.sys`, per-map SR plus message-passing note) |
| 7 | multi-record reads stated; strict mode | D1; strict default (R1.5, R3.8) |
| 8 | fold function per kind; in place vs replacement | R3.4 table |
| 9 | loader copy inside the fold section | `map_lookup_copy` (R3.4); M14 |
| 10 | flip or late writers: no CPU in-place add vs GPU pointer write; bounded attach | init-time scan + flip kernel + timeout with replacement folds (R3.6, D5); T13, M12 |
| 11 | CPU plain stores through pointers | classified, the map is kept out of DEFER (R3.6, PW); D5 is the only residual |
| 12 | memcapture stack copy | READ_LOCAL disabled under memcapture (R3.7) |
| 13 | presence and capacity host-linearized; no SB counter pair | first touch via UNIT_*, sweep, E2BIG fold of pending deletes (R3.4, Lemma 4); T12, M13 |
| 14 | committed effects host-reachable without the agent, also on discrete GPUs | records in the segment and pinned host memory (R6, R8.3); T9 |
| 15 | exact per-group fallbacks; no unregistered ids | R5; M11 |
| 16 | bound hot-key cost by measured contention | R8.2, R10 step 0; opt-in shards |
| 17 | R7 headline decision; derived class; 47 + 4 timing; per-edge records; rocm-counter built | R7.5 |
| 18 | shared `CommSharedMem` | R3.8, R9 (documented prerequisite) |

**Feasibility review**

| # | item | where |
|---|---|---|
| 1 | typed paths hooked + test | R3.5; T6 |
| 2 | no `(%smid, %warpid)` scratch | R3.3 (static, lane-turn, staging in a lock table indexed by a hash of the key bytes; `match.any` after step 0) |
| 3 | pointer-writer exclusion as a precondition; writable lookups and the epoch | PW in R1.1, R3.6. The epoch is not used: READ_LOCAL needs C-SB (R3.7) |
| 4 | exact hardware list; no SB in the core; litmus before enabling | R3.1, R10 step 0 |
| 5 | slice 1 = A + B + J lookup, in place, T14 instead of re-analysis, conservative CPU scan | R10, R4.2, R3.6 |
| 6 | fold must not lock every lookup; measure | fast path (R3.4); R10 CPU-probe row |
| 7 | slot claim; slot sizes; budget vs segment | host-only slot creation under a ledger mutex (R3.2); R6 sizes and budget |
| 8 | capability gate via dlsym; containment test | R7.4, R5 |
| 9 | memcapture stack reservation | R3.7 (disabled) |
| 10 | never wait for application quiescence; flip test | R3.6 (flip kernel, not application quiescence; `T_flip`); T13 |
| 11 | projections consistent with scope and anchors; nothing for contended cases | R10 table |
| 12 | real R7 check with the library; rocm-counter; per-edge records; 47/8/4 | R7.5 |
| 13 | discrete: ledger host-visible; PCIe as parameters | R8.3 |

## Reviewer disagreements and how they were decided

1. **Slice-1 scope.**
   - Correctness review: slice 1 should exercise every load-bearing mechanism. Feasibility review:
     A + B + J lookup only.
   - **Decided: A + B + J lookup.** It already exercises the analyzer, rewrite, record commit, fold,
     sweep, UNIT_* first touch, typed hooks and flip.
   - C needs its own matcher, and its example uses graph launches. It moves to slice 2.
2. **Rewrite mechanism.**
   - analysis-first: a general patcher with jump fixups. Feasibility review: in-place `ja +0`.
   - **Decided: in place in slice 1** (all slice-1 regions fit, R4.2); the patcher comes in slice 2,
     where units need extra arguments.
3. **Validating rewrites.**
   - analysis-first: value-number re-analysis. Feasibility and semantics-first: differential
     testing.
   - **Decided: T14 plus a structural self-check.** Value-number equality remains a matcher
     precondition.
4. **Hot keys.**
   - Correctness review: a sharding option chosen by measured contention. The correctness review
     also demands linearizable reads or a stated anomaly.
   - **Decided: strict mode (S = 1, linearizable) is the default.** Sharding is opt-in, with D1
     stated. The cost model reports predicted serialization and never weakens semantics by itself.
5. **Litmus negative controls.**
   - semantics-first: they must fail. Feasibility review: report only.
   - **Decided: report only.** Hardware stronger than required must not block the design.
6. **Late pointer writers.**
   - Correctness review: drain with a handshake, or decide over all handlers before ledgers exist;
     bounded time. Feasibility review: flip to SYNC with a fold; never wait for quiescence.
   - **Decided: both.** Decide at init over all handlers. For late arrivals, an agent-local flip
     kernel that does not wait for application quiescence, bounded by `T_flip`; on timeout,
     replacement folds and a counted D5.
   - Loader-originated events never depend on the flip: close, freeze and writes are exact through
     fold-on-access alone.
7. **GPU-local reads.**
   - The correctness review called analysis-first's epoch-validated `res_lookup` sound.
   - **Decided otherwise.** Under message passing alone, a straggler SET, a GPU read and a host
     delete can form a cycle that only store buffering excludes (R3.7).
   - READ_LOCAL therefore requires C-SB; the default is one LOOKUP_COPY per group.
8. **Frozen maps.**
   - analysis-first flipped to SYNC to restore EPERM; semantics-first kept EPERM.
   - **Decided: Linux semantics for deferred units (D6).** Keeping EPERM exactly would make a
     loader-originated freeze depend on the agent.
9. **Placement of the printk ring.**
   - analysis-first: agent-private pinned memory. semantics-first: the segment.
   - **Decided: the segment.** The loader can then drain lines of a dead agent (required by must-fix
     14).
10. **Ranking of the runners-up.**
    - Correctness ranked semantics-first second; feasibility ranked performance-first second.
    - **Effect:** semantics-first supplies the semantics, proof, hardware table and tests;
      performance-first supplies the measurement plan, the eligibility scan, the witness ring and
      the later fast paths.

## What was taken from which draft

- **analysis-first (base):**
  - unit matching on bytecode (K/P/V/L/C);
  - rewrite to registered ids with pc-free immediates;
  - the analyzer placed in the agent's CUDA link branch;
  - the dlsym capability gate;
  - fold-on-access in `bpf_map_handler` under the stripe lock (Invariant I);
  - per-(map, agent) ledgers with single-writer words;
  - `gen` and DEAD for close and fd reuse;
  - the pointer-writer classification for GPU and CPU programs;
  - oracle-free `decision`, class derived from `cpu_reads_map`, three-valued `aggregatable`, and the
    R7 simulation script (extended here);
  - the first-touch explanation of 97;
  - several tests (T1, T2, T4, T7-T10).
- **semantics-first:**
  - unit linearizability with program order, real time and sequential specification, and the
    carve-outs (here D1, D2);
  - the lemma-plus-locality proof and presence and capacity as a host-linearized object (Lemma 4);
  - the hardware-assumption table with litmus gating and a reader in another process;
  - the complete hook list including typed paths;
  - the merging copy for loader lookups;
  - count carried with sum;
  - two-half records;
  - the ABSENT negative cache;
  - the first touch as a synchronous host operation;
  - fold-of-deletes before E2BIG;
  - the segment-resident printk ring with orphan drain;
  - the ExitOnError replacement;
  - control-dependent scopes and per-edge records for merged calls;
  - tests T3, T5, T14, T16 and the mutation of the check script;
  - the ATOMIC_LOCAL extension.
- **performance-first:**
  - the eligibility scan over all handlers in `init_attach_ctx_from_handlers`;
  - step 0 (host-call breakdown by `%globaltimer`, contended commit costs, PTX acceptance);
  - re-measuring the baseline at 52126f33 and the geometry series;
  - the witness-ring oracle (T15);
  - `match.any`/`redux` grouping (22/21/21-bit limbs for 64-bit sums) and warp-batched synchronous
    requests as later slices;
  - tier-1 IR inlining as a later slice;
  - `BPFTIME_GPU_PRINTK_SYNC`;
  - the odd-constant single-word variant idea (red.add without a count).
- **New in this synthesis** [new]:
  - the device-memory shadow and slot lock with staging sums (no broadcast primitive, no
    `%warpid` scratch);
  - straggler ordering without incarnations;
  - the sweep rule and its free-element trigger;
  - the lock-free fast path with a `ver` recheck;
  - the flip kernel with a segment request word, `T_flip` and replacement folds (D5);
  - the hashed lock and staging table and the takeover seeding;
  - the store-buffering analysis that gates READ_LOCAL;
  - strict mode as the default;
  - the host-only descriptor bit;
  - D4 and D6 as stated deviations.
