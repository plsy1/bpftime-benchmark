# Exact per-call-site execution of GPU eBPF helper calls: a semantics-first design

Dev code: `/home/jetson/src/bpftime-daily-state/worktrees/4708f4092d1e/candidate` at 52126f33.
All `file:line` citations are at that commit, unless they name another ref. llvmbpf citations are for submodule 87bac39a, before the overlay.
`[inferred]` marks statements that were read or reasoned but never run or measured.
Inputs:
- `/tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/dir2-read/BRIEF.md` (checked facts);
- the reader/checker output `/tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/tasks/w63akje27.output`;
- `/home/jetson/src/results/gpu/callsite-survey-20261113`;
- `/home/jetson/src/results/gpu/overlap-survey-20261113`.

Scratch check script: `/tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/dir2-design/check_rules.py`.

## Overview

### The problem, from the code

A GPU probe's helper call becomes a synchronous host call for:
- HASH and every other CPU map (on the Orin, 1501 too, because it is created as HASH: bpftime_shm.hpp:137-154);
- every delete except on an ordinary ARRAY;
- printk and puts.

The routing is in default_trampoline.cu:233-408. The call goes through ONE request slot per segment (CommSharedMem: bpftime_shm_internal.cpp:1072-1079). The lanes of a warp take turns over a 32-step loop (default_trampoline.cu:129-174), so the cost is about 10 us per call, serialized across the whole GPU (CLAUDE.md:2185-2190). In the micro benchmark (32 threads, 3 timed launches) a CPU-hash case costs 317-341 us per launch against a 39-40 us baseline. The array direct path added by 1b3d9c99 costs 47-48 us (`results/gpu/hostcall-l1-20261111/README.md`).

The synchronous path is also lossy for the counter idioms:
- The 32 lookups of a warp finish before any lane branches. On first touch every lane misses, and only one `BPF_NOEXIST` insert lands.
- A plain `*v += d` through the shared pointer collapses to one update per warp.

See BRIEF "Semantics"; the overlap survey reports the same collapse for upstream #740. So "today's bpftime" cannot be the reference for exactness.

The call-site survey classifies bpftime's 59 host-call sites as:
- 20 eliminate;
- 39 defer;
- 0 that need a synchronous answer;
- 51 aggregatable (same target across a warp).

### The design in one paragraph

A load-time analyzer in the agent recognizes, per call site, *units*: a counter or read-modify-write idiom spanning several instructions, or a single helper call. It gives each unit an *effect* with a sequential specification: add-or-create, add-if-present, overwrite, delete, read or output. It rewrites the unit in the bytecode into a call to a new registered helper, which carries a site id as an immediate.

At run time a deferred unit is *committed by the GPU at the moment it completes*, into a host-readable per-key slot in the shared segment. There is no host round trip. Lanes of a warp with the same target are combined first.

Every host-side access path to the map *merges* the committed effects when it reads. This holds for the loader's `bpf()` commands, CPU probes in the agent, and the watcher serving a remaining synchronous call. So a host read is current with respect to every unit that completed before the read began. That does not depend on launch hooks, completion events or the agent being alive.

Host writes (update, delete, lookup-and-delete, close, reclaim) are ordered against GPU commits by a *cut*: write a control word, fence, wait for the shard's version to be even, read. This is a store-buffering protocol that needs no CPU–GPU atomic read-modify-write on any shared word.

Whatever the analyzer cannot prove stays on today's synchronous path. Hardware enters only as measured cost parameters and as capability bits from litmus tests run at start-up.

### The task's four modes in this design

| task mode | decisions and mechanisms here |
|---|---|
| eliminate (no host call; data on the GPU) | READ_LOCAL: a read served from the GPU-written LWW slot. The ABSENT negative cache (add-if-present or delete of an absent key with no host call). Any deferred effect that no host path ever merges (the survey's GPU-only maps) costs no host work in steady state |
| aggregate per warp | the group commit: lanes with equal keys reduce and commit once, with count n (L3). ATTACH and a READ_LOCAL fallback copy are also one request per group, not per lane |
| defer | DEFER_ADD, DEFER_SET, DEFER_DEL, DEFER_OUT: commit at unit completion into the segment, merged by readers, folded lazily |
| synchronous round trip | SYNC: the original instructions, today's per-lane host_call. Remaining SYNC sites still send one request per lane; only ATTACH and READ_LOCAL's fallback copy are per group |

### Terms used below (coined here)

| term | plain meaning |
|---|---|
| unit | one atomic step of the reference semantics: a recognized multi-instruction idiom, or any other helper call, or one BPF_ATOMIC instruction |
| effect | what a unit does to one map key, as a function of its old value: ADD(d), ADD-IF-PRESENT(d), SET(v), DEL, READ, OUT (printk line) |
| PES (pending-effect store) | the per-map region in the shared segment that holds GPU-committed effects not yet folded into the map's own storage |
| slot / shard | the PES entry for one key / one of its per-(agent, SM) sub-entries that a GPU warp writes |
| incarnation | a per-key counter in a host-written control word; every host write of the key starts a new one, and GPU contributions are tagged with the incarnation they saw |
| cut | the host protocol that ends one incarnation: it decides exactly which GPU contributions came before a host write |
| merge-on-read | a host read returns base value ⊕ committed contributions of the current incarnation |
| fold | moving committed contributions into the base map (an optimization; never needed for correctness) |
| ATTACH | a new synchronous host-call opcode run on a key's first touch: it creates the key in the base map (capacity check) and its slot |
| site id | a 32-bit immediate placed in a free helper argument; it indexes a per-module site table |

### Contributions (what a paper would claim)

1. **A reference semantics for GPU eBPF helpers: unit linearizability (R1).** It is stronger than Linux's per-helper atomicity for recognized idioms, and a refinement of it: every outcome is a Linux-legal schedule. It makes "exact" testable with an oracle.
2. **Commit-at-completion plus merge-on-read (R3).** Deferred effects are visible to every host reader as soon as the unit completes. No flush, launch tracking or loader-to-agent channel is needed, so graph launches and CUDA 13 handle launches (unnoted today) lose nothing.
3. **A fence-only cut protocol that orders host writes against concurrent GPU commits without losing or doubling a unit (R1.5, R3).** No CPU–GPU atomic read-modify-write is used anywhere.
4. **Idiom-level units recognized in bytecode with sound fallback (R4, R5, R7).** The counter and read-modify-write idioms become exact adds. The decision is per call site and the tools stay unmodified.
5. **A cost model whose only hardware inputs are measured parameters and litmus capability bits (R8).** The same protocol runs on integrated and discrete GPUs.

### Summary of guarantees

| what | guarantee |
|---|---|
| final map state, all host writes, all values returned to probes | exact against unit linearizability (R1.2) |
| host read of a key | current with respect to every unit completed before the read began. Linearizable for unsharded keys (S = 1), at quiescence, and whenever the double collect is stable. Otherwise, for sharded ADD keys read during commits, regular and monotone (carve-out C1) |
| printk | no lost lines; per-thread order exact; timestamp merge across threads within a drain batch; lines appear up to one watcher period after the unit (≤ 1 ms, 100 us with WFE) |
| anything not proven | today's synchronous path, unchanged semantics |

## R1 Reference semantics

### R1.1 Model

**Actors:**
- GPU threads. Each runs a sequence of probe executions: kprobe, kretprobe and memcapture invocations, in program order.
- CPU probe threads in the agent.
- *Host operations*:
  - the loader's `bpf()` map commands, which run in the loader process on the segment (syscall_context.cpp:600-679);
  - CPU-probe map helpers (bpf_helper.cpp:401-422, 470-622);
  - synchronous GPU host calls served by the watcher (bpf_attach_ctx_cuda.cpp:124-187);
  - map close (syscall_context.cpp:273-296; handler_manager.cpp:116-123).

**Objects:**
- one per (map uid, key);
- one per map for *presence and capacity*: the set of present keys and its size against `max_entries`;
- one output stream per agent for printk/puts.

**Units:** a unit is one of:
- (i) a recognized idiom instance (R4), for example "lookup k; if null insert(k, 1, NOEXIST) else atomic add 1 through the pointer";
- (ii) any other helper call;
- (iii) one BPF_ATOMIC instruction.

Each unit has a sequential specification:
- for (ii) and (iii), Linux's helper and instruction semantics: lookup returns the value or NULL; update with ANY/NOEXIST/EXIST returns EEXIST, ENOENT or E2BIG as in hashtab.c:1084-1096 and 1025-1044; and so on;
- for (i), the sequential execution of the idiom's own instructions as one atomic block.

Plain loads and stores through map-value pointers *outside* recognized units are not units. They are left to the hardware memory model, as Linux leaves them: a data race, with lost updates legal (map_array.rst:42-45, hashtab.c:1171-1185 per the semantics reader). Any site with such accesses stays synchronous, with today's semantics (R5).

**Interval of a GPU unit:** from its first instruction to its *commit*:
- the shard version store that publishes it, for a deferred unit;
- the response of the host call, for a synchronous one.

**Interval of a host operation:** its call duration.

### R1.2 The reference: unit linearizability

An execution is **exact** if there is a total order σ of all units and host operations such that:

- **(po)** σ respects each thread's program order: a GPU thread's or CPU thread's units, and a loader thread's syscalls.
- **(rt)** If X completes before Y starts in real time, X precedes Y in σ.
- **(seq)** Running the sequential specifications in σ order from the initial state produces:
  - every value returned to every probe;
  - every value returned by every host read;
  - every printk line sequence restricted to one thread;
  - the final map contents.

This is linearizability with units as the operations. For the counter idiom it rules out the first-touch loss and the per-warp collapse. Every execution counts.

It is **not** today's bpftime behaviour. Today's lockstep sync path produces states that no σ explains: 1 instead of 32 after a warp's first touch. Tests compare against an oracle computed from σ semantics, never against today's output (R10).

Two qualifications are part of the definition, so that it can be met and tested:

- **(C1) Sharded ADD reads.** A host read of an ADD key whose contributions are spread over several shards, taken while commits run, is *regular and monotone*, not necessarily linearizable:
  - It contains every contribution committed before the read began and none committed after it ended, but not necessarily a single cut of those in between.
  - With non-negative deltas, successive reads by one reader never decrease.
  - This holds even for unit-increment counters, because a group commit adds n (1 to 32) in one step. Example: three shards commit +32, +5 and +32 in that order, the second unit starting after the first committed. A reader that sees 0, 5, 0 returns 5, which was never a state.
  - The read is linearizable when its double collect is stable. The reader collects every shard's `ver` and half, then re-reads every `ver`. If no `ver` changed, no commit happened in between, and the values coexisted at one instant. It retries up to K times; if no collect is stable, it returns the regular value and counts an "unstable read" per map.
  - A map can be set to one shard (S = 1), which makes every read linearizable at the cost of contention (R8). Reads at quiescence (O5) are exact.
- **(C2) Map close.** bpftime frees a map on close (handler_manager.cpp:116-123), unlike Linux, where an attached program keeps its maps alive. A unit whose map was closed under it has no effect and is counted as a miss. Linux counts skipped kprobe executions the same way (bpf_trace.c:117-129 per the semantics reader). This is a pre-existing bpftime deviation; the design only makes it safe: no write lands in a reused fd's new map.

### R1.3 Observation points: where the host-visible state is current

| point | what is guaranteed there | mechanism |
|---|---|---|
| O1: any host read: loader lookup, get_next_key, lookup_and_delete; CPU-probe lookup | includes every unit committed before the read starts (C1 for sharded in-flight sums) | merge-on-read in `bpf_map_handler`, R3.3 |
| O2: any host write: update, delete, lookup_and_delete, close | its linearization point is the cut. Each GPU unit falls on exactly one side | cut protocol, R3.3, Lemma L1 |
| O3: CPU-probe lookup returning a pointer | the base value behind the pointer contains every unit committed before the lookup | fold-before-return (R3.3); afterwards as Linux |
| O4: GPU synchronous lookup (remaining sync sites) | same as O3 | watcher folds before it answers |
| O5: quiescent point: a detected completion of every noted launch that may touch the map, or process exit after the context is gone | exact final state; printk lines of those launches emitted | watcher event query (probed_launch_activity.cpp:16-72; bpf_attach_ctx_cuda.cpp:334-358) triggers drain and fold |

O1 does not depend on hooks. Kernel completion implies its units have committed, and a commit is visible in the segment. So any read that happens after the application learns of completion includes all the kernel's map effects. The application can learn of it through `cudaDeviceSynchronize`, events, or any other means. Synchronize and graph launch are not hooked (nv_attach_impl.cpp:436-534), and CUDA 13 handle launches are not noted (nv_attach_impl.cpp:1561-1592). Neither matters for map effects.

Completion events (one per stream, polled every 1000 us, or 100 us with WFE: cuda_watcher_policy.hpp:27-28) are used only for:
- O5;
- prompt printk drain;
- eager folding and reclamation.

Correctness never waits for them.

### R1.4 What Linux guarantees, and what this design guarantees, per idiom

Linux guarantees:
- atomicity per helper call and per BPF_ATOMIC;
- that a plain read-modify-write through a lookup pointer is racy;
- that a NOEXIST first-touch insert can lose increments (EEXIST is ignored);
- that an update of an existing hash key replaces the element, so adds through older pointers are lost;
- that user-space reads copy one element with no snapshot across keys;
- that per-CPU values are read CPU by CPU;
- for printk, per-CPU order with a timestamp merge and possible overrun loss.

These are the semantics reader's Q1a-Q1g citations to hashtab.c, arraymap.c, syscall.c and bpf_trace.c in L4T 6.8.12.

**Refinement argument.** Every σ-execution is a schedule Linux could produce: probes run one at a time, a legal interleaving. So whatever this design outputs is a Linux-legal outcome, except under C1 and C2, and it additionally excludes the lossy interleavings.

| shape (survey sites) | Linux | this design | vs Linux |
|---|---|---|---|
| A counter: lookup; null → update(NOEXIST, d); else atomic add d (10) | each op atomic; first-touch race loses increments | unit = ADD-or-create(k, d): exact count; first touch is an ATTACH under the map's capacity object | **more** (no first-touch loss) |
| A, host read while running | element read linearizable (8 B word) | regular and monotone; linearizable with a stable double collect, S = 1 or at quiescence (C1) | **less** (same as Linux per-CPU reads) |
| B plain RMW + EXIST self-write, else NOEXIST insert (9) | racy RMW and element replacement lose updates | unit = ADD-or-create(k, d): exact sum | **more** |
| B, host read of a time sum while running | linearizable | regular and monotone unless the double collect is stable (C1) | **less** (same as Linux per-CPU reads); none of the 6 B time-sum sites has a host reader (survey: cpu_reads_map = no) |
| C leader lookup; v = p ? *p + d : d; update(ANY, v) (4) | racy across overlapping launches | unit = ADD-or-create: exact | **more** |
| D entry SET / exit READ / DEL (9) | last writer wins; a pointer read after a replacement may see the old element or a reused one | SET and DEL linearizable per key (unsharded slot); READ returns a snapshot taken at the lookup, kept in the probe's own stack | **same** for writes; **more** for reads (no reused-element values) |
| E per-thread record SET (2) | last writer wins; E2BIG depends on order | the same; E2BIG decided at ATTACH, the unit's σ point for capacity | **same** |
| F leader publish SET (1) | last writer wins | the same | **same** |
| G printk (21) | per-CPU order, timestamp merge, loss on overrun | per-thread order, timestamp merge within a drain batch, no loss (a full ring takes the sync path and drains first) | **same** order, **more** on loss, delayed visibility ≤ one drain period |
| H benchmark update / lookup + (*v)++ / delete (3) | as A-D | SET / ADD-IF-PRESENT / DEL units | **same** / **more** / **same** |
| any unrecognized pointer RMW | racy | stays sync: Linux semantics (today's behaviour) | **same** |
| host lookup_and_delete concurrent with GPU adds | adds through stale pointers can vanish | every unit falls on exactly one side of the cut (L1) | **more** |
| map closed while units run | map stays alive | units after the close have no effect, counted (C2) | **less** (pre-existing) |

### R1.5 Proof sketch

Let k be a key, inc the incarnation in its control word `ctl[k]`, and `ver` the version word of one GPU shard of k.

**Lemma L1 (cut).** A GPU commit does `st ver ← odd; membar.sys; ld ctl[k]`. A host write does `st ctl[k] ← inc+1; full fence; ld ver`. By the store-buffering guarantee (assumption A3, R1.6) the two loads cannot both miss the other side's store. So either:
- (a) the GPU sees inc+1 and tags its contribution with the new incarnation, which places it after the host write in σ; or
- (b) the host sees the odd version. It then waits for the matching even version and reads the committed shard (A2). The contribution carries the old tag, is part of the cut, and goes before the host write.

If both see each other, the GPU tags inc+1, the host waits and finds no old-tag change: case (a). In no case is a contribution counted on both sides or on neither.

No GPU section that read the old incarnation can commit after the host's wait ends. Any such section had already stored the odd version before the host's load (by L1), so the host waited for it.

**Lemma L2 (reads).** Shards are written under a seqlock: odd version, data into the inactive half, `membar.sys`, then even version. Readers take version, half, version, and retry on change. With GPU→CPU message passing (A2), a reader that sees an even version v sees the data committed with v. Merge-on-read is wrapped in the per-key host sequence `hseq[k]`, which folds and cuts make odd while they change base and watermarks. So a reader never combines a new base with an old watermark. It returns base ⊕ Σ over shards of (half.sum − watermark.sum) for shards whose tag equals inc. Every commit that completed before the reader loaded that shard is included. That gives O1's "completed before the read began" and the regular-read bound of C1. A stable double collect (unchanged `ver` on every shard) makes the read a snapshot.

**Lemma L3 (aggregation counts lanes).** Within a warp, a lane takes part in exactly one group: the lanes with the same key, among those active at that instruction (R3.2). Its delta is in exactly one group sum, which is committed once. The committed count n is the number of lanes in the group, not 1. Upstream's leader-only election counts one per converged group (overlap survey); here every lane counts. Partial and divergent warps (kretprobe before each `ret`, memcapture after each ld/st: kretprobe main.cpp:50-60, memcapture main.cpp:45-56) only make groups smaller. [inferred: lanes that execute the group step together read one consistent `activemask`. This is the standard warp-aggregated-atomics pattern; independent thread scheduling may split a group further, which keeps the count exact.]

**Lemma L4 (presence and capacity are a host object).** Only host-executed operations change which keys are present and how many there are:
- ATTACH on first touch, which inserts into the base map with its E2BIG check (bpftime_hash_map.hpp:715-745);
- host writes;
- the fold of GPU tombstones.

Deferred GPU units only change values of present keys (ADD, SET) or mark a tombstone. A tombstoned key keeps its base element until a host fold deletes it under the cut. A host insert that finds the map full folds tombstones first and retries (R3.3). So the base map's own element accounting linearizes the presence/capacity object. That covers the coupling between keys through `max_entries`, which per-key locality alone would miss.

**Theorem (sketch).** Each per-key object is linearizable:
- GPU units at their commit points;
- host writes at their cuts (L1);
- host reads at the read points (L2, with C1 for sharded in-flight sums);
- ATTACH and folds as host operations under the key's stripe lock (bpftime_hash_map.hpp:715-745 already serializes host writers per stripe).

The presence/capacity object is linearizable (L4). Every recognized unit touches one key of one map (R4), and every remaining sync unit is a single helper call. So by the locality of linearizability (Herlihy and Wing), the whole execution is linearizable over units, which is the reference of R1.2.

Program order of a GPU thread holds: its unit k+1 starts after unit k's commit store. Printk: R3.7.

### R1.6 Hardware memory-model assumptions (all [inferred], gated by litmus tests)

| id | assumption | used by | today's evidence |
|---|---|---|---|
| A1 | GPU device-scope atomics among threads of one context on device memory (cuMemAlloc) | device-memory lock table, R3.2 | standard CUDA; the module lock already relies on it (default_trampoline.cu:122, 146-148) |
| A2 | Message passing GPU → CPU: GPU stores; `membar.sys`; GPU store of a flag; a CPU process (the loader) acquire-loads the flag, then sees the stores | shard publication, printk records | the 1527 ring depends on exactly this: `membar.sys` then tail store (default_trampoline.cu:478-480), loader acquire in drain_data (nv_gpu_ringbuf_map.cpp:56-104) |
| A3 | Store buffering CPU ↔ GPU: GPU `st; membar.sys; ld` against CPU `st; dmb (seq_cst fence); ld` on segment memory: not both loads stale | the cut (L1) | **none**. BRIEF lists CPU/GPU visibility as open; fence placement in host_call has no mutation test (results/gpu/fixes-20261109/README.md:66-68) |
| A4 | Message passing CPU → GPU: CPU release store of `ctl`; GPU load after `membar.sys` sees it | control words, PES directory | the host_call response path (`flag2` release, bpf_attach_ctx_cuda.cpp:317; GPU spin on flag2 then `membar.sys`, default_trampoline.cu:159-163) |
| A5 | Aligned 8-byte single-copy atomicity for `ver` and `ctl` on both sides | seqlock, control | the 1527 head/tail rely on it (gpu_helper_protocol.h:86-103) |

These are not assumed silently. R10's step 0 runs litmus tests for A2-A5 between a GPU kernel and a CPU thread in **another process** mapping the segment, with the exact fences of the design. The tests include a negative control without fences, to show they can detect a violation.

- If A3 or A2 fails, PES is disabled and every site stays synchronous: today's behaviour.
- If only A3 fails, a weaker mode is possible: defer only on maps with no host writes, detected by the cut path being entered, which then switches the map to sync. That needs its own proof and is not part of this design.

## R2 Difference from gpu_ext and from upstream automatic warp execution

Facts from the overlap survey and the semantics reader:

**gpu_ext:**
- Paper §4.4.3 and §5.3 claim "relaxed, eventual consistency": warp-leader shard updates without atomics, merged into canonical snapshots at kernel completion by an asynchronous daemon.
- There is no merge rule (sum or last writer), no staleness bound, and no rule for maps the host writes.
- It is not implemented: the low-overhead results come from tools rewritten by hand with GPU map types and workload-fitted sizes.

**Upstream `official/revision/automatic-warp-execution`** (e27d009b; unmerged, opt-in through `BPFTIME_GPU_AUTO_WARP_EXECUTION`):
- It makes one yes/no decision per program.
- It rejects any atomic, printk, puts, perf output or exit, any CPU-map update/delete, and per-thread map writes (warp_execution_eligibility.cpp:121-214 on that branch).
- An admitted program runs once per *converged group* in the lowest active lane.
- A plain `*v = *v + 1` with a uniform key is admitted and counts once per group, which equals today's lockstep collapse.

| question | gpu_ext (paper) | upstream auto-warp | this design |
|---|---|---|---|
| reference for "correct" | eventual consistency; approximate statistics | "observational equivalence" to today's per-thread execution, including its collapse | unit linearizability (R1.2); oracle-testable |
| when the host sees a deferred effect | at the next kernel-completion merge (unbounded for long kernels) | not deferred | when the unit completes: commit into a host-readable slot plus merge-on-read (O1); no daemon, no completion dependency |
| merge rule | unspecified | n/a | per effect: ADD sums deltas (commutative); SET/DEL linearizable per key on one unsharded slot; OUT keeps every line |
| host writes vs GPU effects | no rule | n/a | the cut (L1): each GPU unit falls on exactly one side of each host write; no loss, no double count |
| aggregation | warp leader writes a shard (what is counted is unspecified) | leader lane runs the whole handler; counts groups | lanes with equal keys reduce their deltas; the commit carries the lane count (L3) |
| granularity | per tool, by hand | per program, yes/no | per call site (unit), with a sync fallback per site |
| atomics, printk, CPU maps | avoided by rewriting tools | program rejected | atomic-add idioms *become* deferred ADD units; printk becomes ordered OUT records; CPU maps (HASH) are the main target |
| tool changes | tools rewritten with GPU map types | none | none: ordinary maps and helpers |
| storage bound | workload-fitted constants (for example 16,384 warps × 44 events) | n/a | sized by distinct targets (≤ max_entries) × shards; a full store means *sync fallback*, never a drop (R6) |
| non-atomic `*v += 1` | n/a | admitted, counts one per converged group | recognized as an ADD unit and exact, or kept sync if not recognized |

What makes defer and aggregate **exact rather than eventual**:
1. Effects become host-visible at unit completion, and readers merge them, so there is no staleness window for completed units.
2. Host writes are linearization points thanks to L1.
3. Aggregation sums every participating lane.
4. Idioms are defined by their own sequential code, and the rewrite can be validated against it by differential execution (R10 T8).
5. Storage pressure degrades to the exact sync path instead of losing data.

What makes it **per call site**:
- every unit gets its own decision, effect, aggregation mode and fallback;
- a site id carried as an immediate lets two units of one program on the same map behave differently;
- unrecognized sites in the same program stay synchronous while the rest are deferred.

## R3 The dynamic answer: pending-effect store, merge on read, cut on write

Whether only GPU probes touch a map cannot be known at load time:
- the loader reads the segment directly (syscall_context.cpp:600-679);
- CPU probes use the same structures (bpf_helper.cpp:401-622);
- other agents may attach later.

The design therefore never needs that fact. Every host access path merges and orders GPU effects at run time, and "eliminate" becomes the dynamic outcome in which no host path ever merges.

### R3.1 Layout

**PES directory** (segment): one 64 B entry per map fd, plus per-map regions:
- `map_uid`: a new 64-bit creation counter stamped into the map handler at creation;
- `kind`: NONE, ADD, LWW or SYNC. First claim wins, by a CPU CAS among host processes;
- `lww_owner_agent`;
- `state`: OPEN or CLOSED;
- region offset;
- an agent list.

The kind lives in the segment, not in a module, for three reasons:
- substituted modules cannot be unloaded (nv_attach_impl.cpp:249-277);
- later sessions, the late path and other agents arrive after hack_fatbin;
- a later site whose effect conflicts with the claimed kind simply stays SYNC.

**Per-map region** (segment, allocated by the agent with Boost interprocess, as the agent already does for its pid set: agent.cpp:1195-1199):
- **key index**: open addressing, 2 × K entries of {key, slot}. Only the host inserts, with a release store. The GPU reads it. Entries become tombstones only under the cut (reclaim).
- **per slot, host-written control line**:
  - `ctl` = {incarnation, state ∈ LIVE/ABSENT/CLOSED/RECLAIMING, key copy};
  - `hseq` (the host sequence for readers);
  - per (agent, shard) watermarks {count, sum[f]} for the current incarnation.
- **per slot, per agent, per shard, GPU-written line**:
  - `ver`, a seqlock: odd while writing;
  - two halves {tag = incarnation, count, payload}. The committed half is selected by `ver`, so a writer that dies mid-write leaves the other half intact.
  - ADD payload: one 64-bit sum per added field (offset, width).
  - LWW payload: {present-or-tombstone, value[value_size]}.
- **ADD slots** have S shards per agent, chosen by `%smid % S`. **LWW slots** have one shard and one owning agent, so writes to a key have a single total order.

**Per agent, device memory** (cuMemAlloc in the agent, which already holds the primary context: bpf_attach_ctx_cuda.cpp:264-276). [inferred: reachable only from kernels running in that context. A module loaded into another context gets its own table, or its sites stay SYNC.]
- a **lock table** of 4096 words;
- the device addresses of the PES regions.

Each module gets this in globals written at module load, next to `map_info` and `constData`, in all three paths:
- write_trampoline_globals (nv_attach_impl.cpp:2366-2385);
- try_loading_ptxs (nv_attach_fatbin_record.cpp:263-292);
- run_attach_entry_on_gpu (nv_attach_impl.cpp:1323-1352).

**Per module, device memory:** the site table (site id → {fd, map_uid, kind, effect, key_size, value_size, field offset/width, agg mode, region device address, agent shard base}) and a small direct-mapped site cache (key hash → slot).

**Per agent, segment:** one printk ring per SM, with header {head (host-written), committed-tail hint}. Records: {commit word, globaltimer, grid thread id, smid, fmt_len, arg1..3, fmt bytes}. The reservation counter is in device memory.

### R3.2 GPU side: commit protocol

Two-level, like host_call's own shape:
- spin on a **device-memory** lock;
- then make **uncontended** stores to the host-visible version word.

The measured reason: spinning on host-memory `occupy_flag` cost 300 us per call, against 10 us behind the device-memory module lock (CLAUDE.md:2185-2190; default_trampoline.cu:122, 146-149).

```
pes_add(fd, key_ptr, delta, site, flags)            // helper 520; DEFER_ADD
  T = site_table[site]; mask = activemask(); me = laneid
  pending = mask
  while (pending):                                  // peel groups of equal keys
    leader = ffs(pending) - 1
    if T.key_uniform: peers = pending               // proven by the analyzer (R7)
    else:             peers = {l in pending : key(l) == key(leader)}   // dynamic check
    n = popc(peers); sum = Σ_{l in peers} delta(l)  // shfl.sync tree, or per-lane commits
    if me == leader: commit_add(T, key, n, sum)
    pending &= ~peers
  syncwarp(mask); return 0

commit_add(T, k, n, sum):                           // one lane
  i = site_cache(T, k) or index_probe(T.region, k)  // miss -> ATTACH (sync) returns i, or ABSENT/NOSPACE
  c0 = ld.volatile ctl[i]                           // pre-check without the section (A4)
  if c0.state == ABSENT and !T.create and c0.key == k: return   // ADD-IF-PRESENT: no effect; σ point = this load
  L = &devlock[hash(i, shard)]; while atomicCAS(L, 0, 1) != 0: ;   // device memory (A1)
  sh = shard(T, i, agent, smid % S); v = ld.volatile sh.ver           // even
  st.volatile sh.ver = v + 1                        // odd: entering
  membar.sys                                        // store-buffering fence (A3)
  c = ld.volatile ctl[i]                            // {inc, state, key}
  if c.state == ABSENT and !T.create:                            // became ABSENT since the pre-check
      st.volatile sh.ver = v; atomicExch(L, 0); return              // no effect (σ: before any later insert)
  if c.state != LIVE or c.key != k:
      st.volatile sh.ver = v; atomicExch(L, 0); goto ATTACH      // abort: data unchanged
  h = sh.half[(v/2) & 1]
  if h.tag != c.inc: h = {tag: c.inc, count: 0, sum: 0}           // old incarnation was cut
  h.count += n; h.sum += sum
  sh.half[(v/2 + 1) & 1] = h
  membar.sys                                        // release (A2)
  st.volatile sh.ver = v + 2
  atomicExch(L, 0)
```

The other effects:
- **SET / DEL (LWW):** the same section on the single LWW shard. The payload half becomes {present, value} or {tombstone}. Lanes with the same key commit one value: the highest active lane's. Within a converged group the lanes are concurrent, so σ may order that lane last.
- **ADD-IF-PRESENT:** the same as ADD, except that ATTACH, finding the key absent, sets the slot ABSENT. Later units do one `ctl` load (the pre-check), do nothing and make no host call.
- **READ (READ_LOCAL):**
  - Read `ctl` (A4), then seqlock-read the LWW shard.
  - If the tag equals inc and a value is present: copy value_size bytes into a stack slot of the *probe's own frame* that the analyzer reserved (R7), and return its address. The pointer stays valid until program exit; cuda-counter's retprobe reads `*(r6+0)` again at pc 105, after a delete at pc 27.
  - Tombstone or ABSENT: return NULL.
  - Tag different from inc (the value lives in base): synchronous copy-lookup. The watcher merges, then copies into the same stack slot.
  - Lanes with the same key read once and broadcast.
- **OUT (printk/puts):**
  - Reserve `popc(mask)` records with one device-memory atomic. The leader checks against the host's `head` (A4).
  - Each lane writes its record, `membar.sys`, then its commit word (A2).
  - Ring full: fall back to synchronous printk (R3.3: the watcher drains first).

The section contains no host call and no other lock. It is short and bounded unless the warp is preempted or its context dies. No intra-warp spinning happens: one lane per group enters. Without `shfl.sync`, the per-lane fallback runs in host_call's lane-turn loop, so the sm_61-era hazard (default_trampoline.cu:131-133) does not arise.

### R3.3 Host side

All protocols run in whichever process does the access, through one layer: `bpf_map_handler`'s HASH branches (R3.4). They need no agent: the loader merges a dead agent's committed shards like a live one's.

**Merge-on-read** (lookup, the copy in lookup_and_delete, get_next_key presence):
```
do { h1 = acquire(hseq[k]); if odd: retry
     c = ctl[k]; b = base lookup copy of k              // lock-free, bpftime_hash_map.hpp:112-134
     for each agent a, shard s:  seqlock-read half; if half.tag == c.inc: acc += half.sum - wm[a][s].sum
     (ADD, S > 1: collect twice, up to K times, until equal; else count an unstable read, C1)
     LWW: if half.tag == c.inc: value = half.present ? half.value : ABSENT else value = b
     h2 = hseq[k] } while (h1 != h2)
```

**Cut** (any host write to k). It runs inside the hash map's own stripe-locked section, through a locked variant of `update`/`elem_delete` (bpftime_hash_map.hpp:715-781) with a hook before the write. So the cut, the wait and the base change happen under one stripe lock:
```
hseq[k]++ (odd); ctl[k] = {inc+1, new state}; seq_cst fence (dmb)            // A3
for each agent a, shard s:  seqlock-read: wait until ver even (bounded-wait policy below), read the
                            committed half, re-read ver, retry if it changed (a new-incarnation commit
                            may overwrite the other half meanwhile)
pend = Σ (half.tag == inc ? half.sum - wm.sum : 0)          // or the LWW value/tombstone
apply the write to (base ⊕ pend) per the sequential spec:
   update ANY/EXIST → base := v (pend discarded: overwritten)
   delete           → base removed (pend discarded)
   lookup_and_delete → return base ⊕ pend, then remove
   NOEXIST on a key present by *merged* presence (an LWW tombstone counts as absent although its
   base element remains) → EEXIST, no state change (no cut needed: read-only)
wm[*] := 0; hseq[k]++ (even)
```

**Fold** (agent watcher, or any host process; changes no abstract state):
```
under k's stripe lock: hseq odd; for each shard: read committed half; if tag == inc:
   base field += half.sum - wm.sum  (in place, an atomic 8-byte add through with_existing_value,
   bpftime_hash_map.hpp:699-709, so CPU-probe atomic adds through pointers are not lost);
   wm := (count, sum)
hseq even
```
For LWW, fold copies the value into base (update ANY) without changing the incarnation. Fold-of-tombstone is a cut that deletes base and sets ABSENT; if a newer SET arrived, it aborts.

**Fold-before-return:**
- A CPU-probe lookup on a map with a PES region folds k before returning the base pointer (O3).
- The watcher does the same for a GPU sync lookup (O4).
- After that the pointer behaves as in Linux.

**ATTACH** (new opcode in serve_cuda_request, bpf_attach_ctx_cuda.cpp:124-187), under the stripe lock:
1. Find or insert the index entry and allocate the slot and this agent's shards. If the region is full, return NOSPACE and the unit goes SYNC.
2. Run the unit synchronously on base:
   - ADD-or-create: insert the identity value plus d if absent (E2BIG → NOSPACE: the unit fails exactly like the original NOEXIST insert), else in-place add;
   - ADD-IF-PRESENT: absent → ctl ABSENT, no effect;
   - SET: base update with its capacity check;
   - DEL: base delete.
3. Set `ctl` LIVE or ABSENT (a cut if it changes the incarnation).
4. Return the slot.

A first touch therefore costs one round trip per (key, incarnation, warp group), and from then on the key is lock-free.

**Capacity-dependent host inserts:** if the base map is full and the map has LWW tombstones, fold the tombstones (cuts) and retry (L4).

**Close:**
- Cut every live slot of the map, setting state CLOSED.
- Set the directory entry to CLOSED.
- Then the existing close path frees the map (syscall_context.cpp:273-296).

A GPU unit that later finds CLOSED, or whose site-table `map_uid` differs from the directory's, has no effect and counts a miss (C2). It never touches the map now behind the reused fd.

**Reclaim** (region nearly full): pick LRU slots, fold, cut to RECLAIMING, tombstone the index entry, free the slot. Site caches validate `ctl.key` inside the section, so a stale cached slot aborts and re-probes.

**Bounded wait on an odd version.** The waiter holds a stripe lock while it waits. Deadlock is impossible because GPU sections make no host call and take no host lock.

The policy:
1. Spin about 50 us, then yield.
2. After 10 ms, check the owner agent's liveness record in the segment: pid, plus a context generation that the agent clears in its atexit handler (bpf_attach_ctx_cuda.cpp:88-109).
3. If the owner is dead or its context is gone, the section never completed. Use the committed half selected by `ver − 1` and mark the shard orphaned.
4. If the owner is alive (a preempted warp, a debugger), keep waiting. The bound is the owner's liveness, and this is stated, not hidden.

### R3.4 Where the host protocols live (every HASH entry point crosses one layer)

Two facts fix the layer:
- `bpftime_hash_map` is also the per-CPU hash's inner table, so the hook must not be there.
- All HASH access goes through `bpf_map_handler` (git grep finds no direct `hash_map_impl` use outside map_handler.cpp).

The HASH branches of:
- `map_lookup_elem` / `map_update_elem` / `map_delete_elem` (the dispatchers used with from_syscall=true by the loader and the watcher, and false by generic CPU helpers: map_handler.cpp:1906-1928, bpftime_shm.cpp:88-144);
- the typed fast paths `map_lookup_hash` (map_handler.cpp:302-316), `map_update_hash` (336-353) and `map_delete_hash` (355-367), which the CPU typed helpers call directly (bpf_helper.cpp:470-490, 574-598, 600-622);
- `map_lookup_and_delete_elem` (map_handler.cpp:1797ff);
- `bpf_map_get_next_key` (map_handler.cpp:617ff).

Each gets one test: "map has a PES region". It is a per-handler pointer and costs one load when absent.

For PES maps the loader's BPF_MAP_LOOKUP_ELEM changes from pointer plus memcpy (syscall_context.cpp:615-622) to a merging copy. Pointer-returning paths (CPU probes, the watcher) fold first (O3, O4).

### R3.5 How the listed facts are accounted for

| fact | how the design handles it |
|---|---|
| loader reads run in another process directly on the segment | the merge code is in `bpf_map_handler` and reads only the segment (PES region, base, control lines): no agent involvement, no channel needed |
| the request slot is unsafe for CPU posters (occupy_flag is GPU-atomics only, default_trampoline.cu:149) | no CPU process ever posts into CommSharedMem; the loader never asks the agent to flush |
| hash value pointers are not stable across update/delete (bpftime_hash_map.hpp:528-538, 755-781) | deferred effects are keyed and applied by host code under the stripe lock (fold through `with_existing_value`, cut through update/delete), never through a captured pointer; READ_LOCAL returns a snapshot in the probe's stack |
| maps can be closed and fds reused (handler_manager.cpp:116-123) | `map_uid` in the directory and in the site table; close cuts every slot to CLOSED; uid mismatch → no effect, counted |
| map_info in a module is a snapshot (nv_attach_impl.cpp:233, 2366-2385) | sizes come from the snapshot, which is valid for the uid it was taken with; liveness, kind and incarnation come from the live directory and control lines in the segment, read per commit |
| CPU probes in the agent touch the same maps | their lookups fold before returning (O3); their writes are host writes (cut); their atomic adds through pointers commute with GPU ADD contributions (separate words) |
| several agents | per-agent shards; the cut and merge visit every agent's shards; LWW keys have one owning agent (others: SYNC) |

### R3.6 Exclusions (these maps or builds are never deferred)

- **`run_with_kernel` maps:** loader reads go to the kernel (syscall_context.cpp:603-671), so a merge cannot happen there.
- **Any map that was mmapped by the loader:** after mmap, reads are plain memory accesses (syscall_context.cpp:1036-1048). That is ARRAY only, and ARRAY is already direct.
- **ARRAY and the GPU map types 1502/1503/1504/1512/1513/1527:** already GPU-local or kernel-backed.
- **PERCPU_\*, LRU_HASH, LPM and queue/stack:** they have different presence semantics, or the GPU host-call path returns CPU 0's slot or NULL (map_handler.cpp:171-183).
- **Counting builds (`BPFTIME_ENABLE_MAP_USAGE_STATS`):** they keep host calls so every operation is counted, the precedent of 1b3d9c99 (bpf_attach_ctx_cuda.cpp:482-484).
- **Frozen maps:** a GPU host-call update on a frozen map gets EPERM today (from_syscall=true: map_handler.cpp:445-448, 776-779). ATTACH returns the same EPERM to keep that behaviour, and a freeze is a cut that sets every slot to a state whose units return EPERM. Linux allows program writes to frozen maps. That is a pre-existing difference, kept on purpose.

### R3.7 Output (printk) ordering argument

Per thread:
- a thread's OUT record is committed (commit word after `membar.sys`) before the thread executes its next unit;
- the drain emits a ring's records in reservation order;
- a thread's records sit in its SM's ring;
- a sync printk drains first.

So a thread's lines appear in program order.

One exception: a warp that is preempted and resumed on another SM may put consecutive lines in two rings. The drain then orders them by globaltimer, and ties are broken by ring id [inferred rare; the litmus step also measures the timer resolution].

Across threads the guarantee is the timestamp merge within each drain batch, the analogue of trace_pipe's per-CPU merge.

## R4 Idioms as units

### R4.1 Recognition machinery (bytecode level, in the agent)

The analysis is a forward abstract interpretation over the stored eBPF instructions, with BPF-to-BPF calls (dev's reachability CFG already handles them: bpftime_prog_reachability.cpp:123-304). The per-pc state covers r0-r10 and the 64 eight-byte cells of the 512-byte stack. Each value carries:

- **a value number (VN)** for symbolic equality, plus a constant when known. Upstream PR #624's analysis has neither: no constant lattice (uniformity_analysis.cpp:267-304 on that branch). B needs "the inserted value equals the added value": cuda-counter's retprobe stores r7 at fp-0x28 (pc 21), adds r7 (pc 34) and inserts fp-0x28 (pc 72-75).
- **provenance**:
  - map fd from LDDW src=1 (the fd is the handler index: bpftime_shm_internal.cpp:772-784);
  - stack pointer with a constant offset;
  - map-value pointer {producing call pc, map fd, key VN, maybe-null}.
- **scope** in the lattice CONST ⊑ PROCESS ⊑ GRID ⊑ BLOCK ⊑ WARP ⊑ THREAD, with these helper sources:
  - 14 → PROCESS: the module global the agent writes (default_trampoline.cu:410-422);
  - 503 out-parameters → BLOCK;
  - 504, 508 → GRID;
  - 505, 511 → THREAD;
  - 509 (smid) → BLOCK, since a resident block stays on one SM [inferred];
  - 510 → WARP;
  - 502 (globaltimer) → THREAD (conservative; R7.5 shows why this choice is visible in the check).
  
  A lookup's pointee gets its key's scope, as in PR #624 (uniformity_analysis.cpp:418-429 on that branch). Unlike PR #624, joins are **control-dependent**: a value defined under a branch whose predicate has scope p gets at least p at the join. That closes the gap the analysis-tools reader found (PR #624 has no control dependence and relies on rejecting varying branches).
- **use lists** of every map-value pointer: null test; LDX, STX or ATOMIC at an offset; passed as a helper argument (which helper, which register); spilled and filled; escaping (any other use).

A **unit** is matched on a single-entry CFG region R:
- R starts at the key-slot setup feeding a lookup;
- all its exits go to one join pc J;
- no instruction outside R jumps into R except at its entry.

R may be non-contiguous. In cuda-counter's retprobe, B spans pc 28-42 and 69-76, and the unrelated "no tsp" printk path at pc 43-68 sits between them in the text.

### R4.2 The eight shapes

pcs are from the disassemblies in `/tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/dir2-read/disasm/` (dev or source-identical objects).

**A, counter** (10 sites):
- cuda-counter `cuda_probe.bpf.c:48,52`;
- threadscheduling `:90,95,100,105`;
- threadscheduling_dynamic_hook `:93,98,103,108`.

Bytecode, cuda-counter entry pc 20-37:
```
20 r6 = 1; 21 *(u64*)(fp-0x30) = r6; 22-23 r2 = fp-4; 24 r1 = map; 26 call 1
27 if r0 == 0 goto 30; 28 lock *(u64*)(r0+0) += r6; 29 goto 38
30-37 r2 = fp-4; r3 = fp-0x30; r1 = map; r4 = 1 (NOEXIST); call 2
```
threadscheduling has the same shape at pc 51-70 (key = smid) and pc 74-93 (key = smid<<16 | warpid&0xffff).

Match conditions:
- one map fd at both calls;
- the same key slot, with no store to it inside R;
- the non-null edge holds exactly one ATOMIC ADD without FETCH at a constant offset o, width w;
- the null edge holds update(NOEXIST) whose value VN at o equals the added VN, with the other bytes of the value equal to the identity (here value_size = w = 8);
- r0 is dead at J;
- the pointer has no other use.

Unit: **ADD-or-create(k, o, d)**.

Result under the reference:
- the count equals the number of executions;
- the first-touch race is gone: concurrent first touches are ATTACH calls serialized by the stripe lock. The first inserts d; the others find the key and add.

Linux allows losing these (hashtab.c:1084-1089: EEXIST ignored), and today's lockstep loses up to all but one lane per warp on first touch.

**B, non-atomic RMW + EXIST self-write, else NOEXIST insert** (9):
- rocm-counter `:48,51,53` (call_count);
- cuda-counter `:73,76,79` and rocm-counter `:74,77,80` (total_time_ns).

Bytecode, cuda-counter retprobe:
```
28 r1 = map; 30 r2 = r8; 31 call 1; 32 if r0 == 0 goto 69
33 r1 = *(u64*)(r0+0); 34 r1 += r7; 35 *(u64*)(r0+0) = r1
36-41 r2 = fp-0x1c; r1 = map; r3 = r0; r4 = 2 (EXIST); 42 goto 76
69-75 r2 = fp-0x1c; r3 = fp-0x28; r1 = map; r4 = 1 (NOEXIST)
76 call 2        <- ONE call instruction serves both source lines 76 and 79
```

Match conditions:
- an LDX/ADD/STX at one offset o with the same VN chain;
- update(EXIST) whose r3 is the lookup pointer itself and whose other value bytes are untouched;
- update(NOEXIST) whose value VN at o equals the added VN (fp-0x28 holds r7, stored at pc 21);
- the merged call at pc 76 is matched per incoming edge: r3/r4 are a phi of {r0, 2} and {fp-0x28, 1}.

Unit: **ADD-or-create(k, o, d)**.

Result: an exact sum. Linux and today allow lost updates. Linux loses them through the racy RMW and through element replacement by the self-write (hashtab.c:1171-1185; bpftime_hash_map.hpp:528-538). Today loses them through the per-warp collapse. The unit removes outcomes Linux allows, which is legal by refinement (R1.4).

rocm-counter has no built object in dev, so the check builds it (R7.5).

**C, leader lookup + update(ANY)** (4): cudagraph `:56,58` (call_count) and `:79,81` (total_time_ns).

Bytecode, entry pc 40-56:
```
43 call 1; 44 r1 = 1; 45 if r0 == 0 goto 48; 46 r1 = *(u64*)(r0+0); 47 r1 += 1
48 *(u64*)(fp-0x48) = r1; ... 55 r4 = 0 (ANY); 56 call 2
```
That is `v = p ? *p + d : d` with value_size = 8.

Unit: **ADD-or-create**. The leader predicate (pc 14-25 test six indices against 0) gives frequency per_launch and a single active lane, so there is nothing to aggregate. Result: exact, also for overlapping launches, where Linux and today race.

**D, entry timestamp / exit read / delete** (9):
- cuda-counter `:44,66,70`;
- rocm-counter `:44,67,71`;
- cudagraph `:53,75,78`.

The parts:
- **Entry**: cuda-counter pc 13-19, update(start_ts, &x, &ts, ANY), r0 unused. Unit **SET**, key scope BLOCK. Lanes with one key commit the highest active lane's ts.
- **Exit read**: cuda-counter pc 11-16, lookup(&x); the pointer is used only by LDX, at pc 19 and pc 105. pc 105 comes after the delete at pc 27 and several other calls. Unit **READ**, served by READ_LOCAL from the LWW slot written by the entry SET. The snapshot lives in a reserved slot of the probe's stack: cuda-counter's retprobe uses fp-0x70 at most, so fp-0x78 is free. Result: some committed entry timestamp of that block, as in Linux (last writer wins), and never a reused element's bytes.
- **Delete**: cuda-counter pc 22-27 deletes &pid, while start_ts is keyed by x (a source bug: it nearly always deletes nothing). Unit **DEL**. ATTACH of an absent key sets the slot ABSENT, a negative cache, so later deletes of that key cost no host call. cudagraph:78 deletes the right key after reading it (pc 38, then 44).

All three D sites of a map are kind LWW, so they are consistent.

**E, per-thread record** (2):
- threadscheduling `:86`, pc 41-50: update(thread_mapping, &((bx<<16)|(tx&0xffff)), &info80, ANY);
- the dynamic_hook copy `:89`.

The key is THREAD scope, so there is no aggregation; equal keys are still grouped by the dynamic peel. Unit **SET**; the value is 80 bytes in the LWW payload. Result: last writer wins per key, and E2BIG once the 4096 entries are full happens at the σ point of each key's first touch (ATTACH), as Linux decides it at the insert.

**F, leader publish** (1): gpu_shared_map `:55`, update(counter_per_thread, &pid, &newv, ANY) by the single thread whose index sum is 0. Unit **SET**. Result: as Linux.

**G, printk** (21): result unused (for example cuda-counter pc 38-60: format on the stack, then call 6). Unit **OUT**. Lines are never merged, because the line count is observable (upstream auto-warp rejects printk for the same reason). Only the record reservation is batched per warp. Result: R1.4's G row.

**H, benchmark hash micro-ops** (3), benchmark/gpu/micro `cuda_probe.bpf.c`:
- `:163` update(key 0, 1, ANY): **SET**;
- `:171` lookup + `(*val)++` with no insert: **ADD-IF-PRESENT**;
- `:182` delete key 0: **DEL**.

Each is its own program, and the loader enables one mode per run. If all three were attached at once, the map would hold kinds LWW and ADD, so its sites would stay SYNC (R7.3). In lookup and delete mode nothing creates key 0. So the slot goes ABSENT after the first ATTACH, and later units cost no host call.

### R4.3 Shape → decision on the 59 sites

| shape | sites | effect | decision | aggregation |
|---|---:|---|---|---|
| A | 10 | ADD-or-create | DEFER_ADD | warp-uniform reduce (keys CONST/BLOCK/WARP) |
| B | 9 | ADD-or-create | DEFER_ADD | warp-uniform reduce (pid) |
| C | 4 | ADD-or-create | DEFER_ADD | none (one lane per launch) |
| D | 9 | SET 3, READ 3, DEL 3 | DEFER_SET / READ_LOCAL / DEFER_DEL | per-key group |
| E | 2 | SET | DEFER_SET | none (per-thread keys) |
| F | 1 | SET | DEFER_SET | none (one lane) |
| G | 21 | OUT | DEFER_OUT | batched reservation only |
| H | 3 | SET, ADD-IF-PRESENT, DEL | DEFER_SET / DEFER_ADD / DEFER_DEL | warp-uniform (key 0) |

Total: DEFER_ADD 24, DEFER_SET 7, READ_LOCAL 3, DEFER_DEL 4, DEFER_OUT 21. SYNC is 0 under one-mode benchmark runs, and 3 if all three H programs share the map.

A member call of a unit (for example A's lookup and its insert) carries the unit's decision.

### R4.4 Beyond the 59: lossy GPU-local idioms

The same recognition applies to GPU-local sites that today lose counts under lockstep (semantics reader Q2-I):
- mem_trace `*val = *val + 1` on 1503 (a `__memcapture` probe);
- benchmark ARRAY `(*val)++`;
- launchlate histogram `*count += 1`;
- host_map_test's tid%10 counter.

Their decision would be **ATOMIC_LOCAL**: rewrite the plain RMW unit into one atomic add on the direct path. Upstream fixed one such case by hand (#740). These sites are not host calls, so this is an extension, not part of R10.

## R5 Soundness direction and the synchronous path

**The analysis is a must-analysis.** A site gets a non-SYNC decision only if every condition of its unit holds on every path into the region. The following force SYNC, leaving the original instructions untouched:
- the map fd is not a constant at the call (for example spilled and reloaded with lost provenance);
- the key slot is written inside R;
- the pointer escapes or is used outside the unit;
- a FETCH atomic or CMPXCHG appears. llvmbpf's CMPXCHG puts a pre-exchange plain load in r0 (compiler.cpp:1690-1719), so CAS-based idioms are never recognized;
- the init value is not provably equal to the delta;
- the value has bytes the unit cannot account for;
- a map type is outside R3.6;
- the map's claimed kind conflicts;
- the region does not fit the rewrite (R7.4);
- the analysis fails at all (unknown helper id, unsupported instruction).

Run-time conditions fall back per unit, and stay exact by the cut:
- region or ATTACH NOSPACE;
- slot ABSENT for an add-or-create (ATTACH executes it);
- LWW owner is another agent;
- printk ring full;
- map CLOSED or uid mismatch (no effect, counted).

**The synchronous path stays,** with today's lane serialization, module lock and `occupy_flag` (default_trampoline.cu:129-174). Its waiting rules change in four ways:

1. A sync LOOKUP on a map with a PES region folds k before answering (O4). The watcher does extra host work under the stripe lock; the GPU lane waits longer by that much.
2. A sync UPDATE or DELETE on such a map is a host write and runs the cut. The watcher may wait for another warp's commit section to finish (the R3.3 bounded-wait policy). This cannot deadlock:
   - a section makes no host call and takes no host lock;
   - on sm_87, independent thread scheduling lets a lane in a section progress while another lane of the same warp waits in host_call [inferred: the retargeted module runs as sm_87 code].
3. A sync printk (or puts) first drains every printk ring up to its first uncommitted record. The calling thread's earlier records are committed before it entered host_call, so its order holds.
4. A new opcode, ATTACH, goes to serve_cuda_request. Unknown opcodes are only logged today (bpf_attach_ctx_cuda.cpp:184-186), so an old agent's watcher cannot serve it, and the PES ABI version must match within a segment (CLAUDE.md:220 already forbids mixing runtime versions).

**Self-validation of rewritten words (no application exit).** An unregistered helper id or a map-value LDDW makes `generate_ptx` call `exit()` through a static `ExitOnError` (llvm_jit_context.cpp:907, 936-939; compiler_utils.cpp:362-369). So after rewriting, the analyzer re-checks the new program, and on any failure the original words go to the pass:
- every call id is in the registered set: core.cpp:234-253 plus the new ids;
- every jump stays in range and lands on an instruction start;
- no LDDW src=2 is present;
- the new calls' argument registers are defined;
- the reserved stack slots are inside 512 B and unused;
- the region's live-out registers are unchanged.

Separately, replacing the static `ExitOnError` in `generate_ptx` with error propagation (one more hunk in the existing overlay of llvm_jit_context.cpp, vm/compat/llvm-vm/CMakeLists.txt:20-44) is recommended for every PTX failure, not only this design's.

## R6 Where deferred and aggregated data lives, how big, what happens when it is full, who drains it

**Location on the Orin:** the shared segment. It is the only memory both the GPU and another process (the loader) can reach without the agent: the whole Boost segment is cuMemHostRegister'ed by every agent (bpftime_shm_internal.cpp:1407-1475), and the 1527 ring already works this way (nv_gpu_ringbuf_map.cpp:11-104). Locks and per-module caches are in device memory: they need to be fast and are never read by the host.

Aggregated data has **no storage of its own**. Lanes combine at the instruction (shuffles or per-lane commits) and the result goes straight into the slot. Nothing is buffered in a thread across units, so nothing has to be flushed before a sync call or at exit.

**Sizes.** All sizes are bounded by targets, not threads. This answers direction 1's storage question for these effects: the store grows with distinct keys, never with grid size.

| structure | where | size | sized by |
|---|---|---|---|
| PES directory | segment | 1024 × 64 B = 64 KiB | map_info's fd range (default_trampoline.cu:100-101) |
| key index per map | segment | 2K × (round8(key_size) + 8) | K = min(max_entries, K_cap): a hash map never holds more than max_entries keys; ABSENT (negative) entries are capped separately (default 256 per map) |
| ADD slot | segment | 64 B control + A·S·16 B watermarks + A·S·128 B shards (two halves each) | A attached agents, S shards (default number of SMs [inferred 8 on Orin Nano 8 GB]; cost parameter R8) |
| LWW slot | segment | 64 B control + 2 × round64(16 + value_size) | value_size |
| printk ring | segment | per agent per SM, R KiB (default 64) | line rate × drain period × safety factor 4 |
| lock table | device memory (agent) | 4096 × 4 B | contention (hash of slot, shard) |
| site table and site cache | device memory (module globals) | sites × 64 B + 256 × 16 B | number of rewritten sites |

Examples [inferred from source sizes]:
- call_count with K = 1024, A = 1, S = 8: 1024 × (64 + 128 + 1024) B ≈ 1.2 MiB, plus a 32 KiB index;
- thread_mapping (K = 4096, value 80 B): 4096 × (64 + 2 × 128) ≈ 1.3 MiB;
- printk: 8 × 64 KiB = 512 KiB per agent.

Budget: a PES budget (`BPFTIME_PES_BUDGET_MB`, default 16) out of the 128 MiB segment (bpftime_config.hpp:81). A map whose region does not fit keeps all its sites SYNC.

**When full:**
- **Key index or slots full:**
  1. Reclaim LRU or dead slots: fold, then cut to RECLAIMING (R3.3).
  2. If still full, the unit takes the synchronous path. Exact, by the cut: ADD commutes with pending contributions; SET is a host write.
- **Negative-cache cap:** the next delete or add-if-present of an absent key is a plain sync call.
- **printk ring full** (the leader sees `tail − head ≥ capacity` against the host-written head): the lane does a sync printk, and the watcher drains every ring first.
- Nothing is ever dropped silently. The only "no effect" outcome is C2 (closed map), and it is counted.

**Who drains or folds, and when:**

| what | who | when | needed for correctness? |
|---|---|---|---|
| ADD/LWW contributions | nobody needs to | merge-on-read makes them visible | no |
| fold of a key | the process doing a CPU-probe lookup or a GPU sync lookup; the agent watcher | O3/O4; eagerly at O5 (detected completion); every T_fold = 100 ms for hot keys, to bound merge cost | no |
| LWW tombstone fold | host insert into a full map; watcher at O5 | on demand | yes for capacity (L4), done on demand |
| reclaim | the watcher | region ≥ 90 % full | no (pressure only) |
| printk rings | the agent watcher | every wake (≤ 1 ms park backstop; 100 us with WFE), at O5, before a sync printk, in its atexit handler before it stops (bpf_attach_ctx_cuda.cpp:88-109) | yes for output |
| orphaned printk rings and shards of a dead agent | the loader (syscall server) on its next map command or ringbuf poll, or the next agent | after the owner's liveness record is stale | yes for output; shards are merged anyway |

Printk lines written by units after the agent's final drain (kernels still running at exit; nothing waits for them today: bpftime_shm_internal.cpp:65-87) are emitted by the orphan drain. They go to the loader's log sink, not the agent's. That difference is acceptable and noted.

## R7 The analyzer

### R7.1 Where it runs

In the agent (the traced CUDA process), in two steps:

1. **Per link**, in `nv_attach_impl::create_attach_with_ebpf_callback` (nv_attach_impl.cpp:181-285). This is the first place where the stored instructions (`prog->get_insns()`, bpf_attach_ctx.cpp:639), the fd-indexed `map_basic_info` (bpf_attach_ctx.cpp:643-644) and the attach point (195-202) are together. Upstream PR #624 puts its verifier at the same place. Output: per-program site facts, stored in `nv_attach_entry.extras`, which is unused today (nv_attach_impl.hpp:76-88).
2. **Per module**, in `hack_fatbin` (nv_attach_impl.cpp:962-1157), where all hook entries exist:
   - consolidate effects per map across the module's programs;
   - claim kinds in the segment directory (first claim wins: R3.1);
   - allocate the PES regions;
   - finalize decisions;
   - rewrite each entry's words before the RuntimeRequest is built (994-1005).

   This step runs before a module is loaded, in substitution mode and on the late path alike (nv_attach_impl.cpp:264-277), so both get the same code.

Upstream's x86 timing for a 4,096-instruction program is 2.2-2.5 ms for the uniformity pass alone (bpftime-verifier/test/device-verifier-phase-attribution-20260904.md on official/revision/automatic-warp-execution). That is negligible beside the per-PTX nvPTXCompiler work [inferred].

### R7.2 Inputs

- the stored instructions of every GPU program attached in the module;
- `map_basic_info[fd]`: type after the Orin remap, key/value size, max_entries (bpf_attach_ctx_cuda.cpp:505-554);
- the map handler's `map_uid` and frozen state;
- the attach kind (kprobe, kretprobe, memcapture) from the attach point;
- the segment's PES directory (kind claims, budget);
- the runtime config: PES on or off, whether counting builds or `run_with_kernel` are in use, the timer-scope policy;
- the start-up cost and capability parameters (R8).

### R7.3 Analysis

The pipeline:
- CFG;
- forward interpretation (R4.1);
- unit matching (R4.2);
- the per-map effect-kind check: ADD sites and LWW sites on one map → all its sites SYNC, and the map is claimed SYNC;
- aggregation mode from the key scope: ⊑ WARP → uniform; otherwise the dynamic peel;
- stack-slot reservation for READ snapshots, below the deepest used offset;
- frequency (per_launch when the path condition pins block and thread indices to constants).

### R7.4 How the decision reaches code generation

There are five constraints:
- `generate_ptx` has no hook (llvm_jit_context.cpp:903-975);
- IR carries no eBPF pc (compiler.cpp:562-623);
- unregistered helper ids exit the application;
- memcapture shifts pcs and compiles once per access (memcapture main.cpp:53-57, 60-147);
- O3, and clang before it (pc 76 above), may merge calls.

The carrier is therefore **a bytecode rewrite to new, registered helper ids, with the site id as an immediate**:

- **New ids:** 520 pes_add, 521 pes_set, 522 pes_del, 523 pes_read, 524 pes_out (printk), 525 pes_out_puts.
  - They are registered next to the existing ones in `compile_ebpf_to_ptx_from_words` (core.cpp:234-253).
  - They are defined in the trampoline (default_trampoline.cu, regenerated into trampoline_ptx.h per CLAUDE.md:2166-2174).
  - Their names match the filter's `_bpf_helper_ext_\d{4}` (core.cpp:280-283), so the probe's extern declarations are stripped as today.
- **Arguments:**

  | helper | r1 | r2 | r3 | r4 | r5 |
  |---|---|---|---|---|---|
  | pes_add | fd | key pointer | delta | site id | offset/width/create flag |
  | pes_set | fd | key | value pointer | the original flags | — |
  | pes_del | fd | key | — | — | — |
  | pes_read | fd | key | snapshot stack slot (inserted) | site id (inserted) | — |
  | pes_out | fmt | size \| site << 16 | a1 | a2 | a3 |
- **Rewrite in place when the region has room.**
  1. The unit's entry block is overwritten from the entry pc: argument setup, `call 52x`, `ja J`.
  2. Every other instruction of R becomes `ja +0`. They are dead after the rewrite, because all of R's in-edges were internal.
  3. No pc outside R changes, so no jump offset needs fixing.

  The A region in cuda-counter's entry (pc 22-37, 16 instructions) takes a 9-instruction sequence:
  ```
  22-23 r2 = fp-4
  24-25 r1 = map
  26    r3 = r6
  27    r4 = site
  28    r5 = flags
  29    call 520
  30    ja 38
  31-37 nop
  ```
  The delta operand is whatever the matched unit adds. In cuda-counter it is register r6, set before the region (pc 20). In threadscheduling it is the constant 1, set inside the region (pc 58 and 61), so the rewrite emits `r3 = 1` as an immediate move.

  The B region in the retprobe is rewritten at pc 28-35 (map, key r8, delta r7, site, flags, call 520, `ja 77`). Pc 36-42 and 69-76 become nops. The printk path at pc 43-68 is untouched, because it is reached only from pc 16, outside R.
- **Single-call units change the call's immediate in place** (call 2 → 521, call 3 → 522, call 6 → 524):
  - printk carries the site id in the high bits of r2, its constant format size, by editing the `mov r2, imm` (cuda-counter pc 59: `r2 = 0x2c`);
  - SET and DEL take their per-map data from an fd-indexed module table, so they need no site id.
- **Units that need an extra argument insert instructions with branch-offset fixups.** READ needs its snapshot stack slot, and some units need r5 = site. The fixup adjusts every relative jump and BPF-to-BPF call offset that crosses the insertion point, as the kernel's `bpf_patch_insn_single` does. The analyzer's CFG already has every edge. Survey line numbers are computed before the rewrite.
- **Why each constraint stops mattering:**
  - No IR hook is needed.
  - No pc is needed at run time: the site id travels as data, so LLVM can move or merge calls freely. Two calls with different immediates cannot be merged without a phi of the argument, which still carries the right value.
  - memcapture prepends to the *rewritten* words, and the immediates survive.
  - The patch cache keys on the request JSON, which contains the words (nv_attach_impl.cpp:1011-1027), so rewritten programs get their own entries. A `pes_abi` field in `RuntimeInput` (backward-compatible through `NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT`, core.hpp:77) versions the trampoline ABI.
  - The rewrite touches only the copy in `nv_attach_entry`, never the stored handler words. CPU-side instantiation of cuda__ programs, which is never compiled or run (bpftime_prog.cpp:664-684, 776-777), is unaffected.
  - Several probes on one kernel already share `__probe_func__<kernel>` names (kprobe_entry main.cpp:52-55). The design adds no new PTX function names; site ids are unique per (agent, program uid, unit).

### R7.5 Output records and the mechanical check

**Records.** One record per *source-level call site*, in the survey's schema plus `decision`.

The fields:
- `file`, `line`, `section`;
- `helper`, `map`, `map_type`;
- `host_call_today`, `result_use`, `key_scope`, `operation`;
- `cpu_reads_map`, `aggregatable`, `frequency`;
- `idiom`, `class`, **`decision`**;
- extra fields `pc`, `unit`, `effect`, `agg`, `site_id`, `fallback_reason`.

A bytecode call that clang merged gets one record per incoming constant-flags edge, with the line of that edge's argument setup. cuda-counter retprobe pc 76 gives two records: line 76 via r4 = 2 at pc 41, and line 79 via r4 = 1 at pc 75 [inferred: that the line info attributes those movs to those lines].

Line numbers come from `.BTF.ext` line info. The server stores only the instruction words (syscall_context.cpp:749-753). So the check runs the **same analyzer library offline** in a CLI over the `.bpf.o` files, reading ELF sections, map relocations and BTF line info. In the agent, records carry `section`, `pc` and program name instead.

**rocm-counter** has no built object in dev. The check builds it with the examples' BPF flags (clang -target bpf -g). It needs no ROCm.

**Derivation of each field** (all mechanical):

| field | rule |
|---|---|
| host_call_today | f(helper, runtime map type) at 52126f33. Helpers 1/2 are host calls unless the type ∈ {1502, 1503, 1504, 1512, 1513} or ARRAY with a bound extra_buffer. Helper 3 is a host call unless ARRAY. 6 and 501 always are. 14 is not (module global). 25 and 502-511 are not. 1527 lookup/update/delete are. Source: default_trampoline.cu:233-422 |
| result_use | from r0's use list: `pointer_rmw` if any store or atomic goes through it; `value_used` if only loads (or it is passed on); `unused` otherwise |
| key_scope | the scope lattice: CONST/PROCESS → constant, BLOCK → per_block, WARP → per_warp, THREAD → per_thread, no key → none |
| operation | the update flag constant (overwrite / insert_if_absent / update_if_exists), delete, read, `accumulate` for member calls of an ADD unit, output |
| aggregatable | map ops: key scope ⊑ WARP; printk: join of argument scopes ⊑ WARP |
| idiom | the matched shape letter |
| decision | from R4.3 |
| cpu_reads_map | always `unknown` from the analyzer (R3: not knowable at load time) |
| class | computed by the check after joining `cpu_reads_map` from the survey: if host_call_today = no → gpu_local_already; else if cpu_reads_map = no → eliminate; else if the result is used *outside* a recognized unit → sync; else defer. A printk's `n/a` counts as "read", because its output is consumed on the host |

**The 20 / 39 / 0 split comes only from the joined `cpu_reads_map` column.** `decision` never depends on it. Eliminate versus defer is a run-time outcome in this design: merge-on-read either happens or never does.

**Pre-check run now** with `check_rules.py` on the survey's verified records. The rule inputs were taken from the records and the sources, standing in for the analyzer.
- **Class:** the 59 host-call sites give **eliminate 20, defer 39, sync 0**, with no mismatch. Idiom letters: A 10, B 9, C 4, D 9, E 2, F 1, G 21, H 3.
- **Aggregatable, map operations:** 36 of 38, equal to the survey (the two per_thread thread_mapping records are false).
- **Aggregatable, printk:** the survey is **internally inconsistent** about `%globaltimer`.
  - It marks cuda-counter:55, 82 and rocm-counter:56, 83 (timer argument read at entry) aggregatable.
  - It marks launchlate:114, 117 and launchlate-kernel-gpu-shared-map:119, 122 (timer argument read at entry) not aggregatable.
  - With timer = THREAD (the analyzer's default): 47, differing exactly on the first four records.
  - With timer = WARP: 55, differing exactly on the last four.

  The check therefore asserts:
  - (a) 36 of 38 map sites;
  - (b) the printk count under the configured policy;
  - (c) that the set of differences equals the listed four.

  Then "51" is reproduced as 47 + 4 listed records. Aggregatable affects no printk decision: lines are never merged.
- **Field agreement:** `key_scope` for printk is also inconsistent in the survey (none versus argument scope), so it is reported, not asserted.

**Mutation checks of the check:**
- treat a value used inside an idiom as needed now: cudagraph:56 → sync, tally 20/38/1, so the check fails;
- drop control dependence from the scope analysis: the check must still pass on these programs, and R10 T8 must catch the divergent-key case.

## R8 Generality: integrated and discrete GPUs as parameters

### R8.1 The protocol over an abstract memory interface

The protocol uses three kinds of memory, each defined only by required properties:

| kind | holds | written by | read by | required properties |
|---|---|---|---|---|
| L (lock) | lock table, site cache, printk reservation counter | GPU (atomics) | GPU | A1 |
| P (published) | shard lines (`ver`, halves), printk records | GPU | host processes (loader, agent) | A2, A5 |
| C (control) | `ctl`, `hseq`, watermarks, directory, key index, ring head | host processes | GPU and host | A4, A5, and A3 between P.`ver` and C.`ctl` |

Each placement sets **capability bits** {A2, A3, A4, A5}, measured by the start-up litmus tests (R10 step 0). Deferral is enabled for a map only if its region's placement has all of them. Otherwise its sites stay SYNC.

### R8.2 Placements

| platform | L | P and C | host access path | notes |
|---|---|---|---|---|
| Orin (integrated, sm_87) | cuMemAlloc device memory (L2) | the shared segment, cuMemHostRegister'ed (bpftime_shm_internal.cpp:1407-1475), uncached by the GPU | plain loads and stores, CPU-cached | the only placement that needs no new mapping; the 1527 ring is the precedent for P |
| discrete, option D1 | device memory | pinned host memory (the segment, registered) | local DRAM | GPU stores are posted PCIe writes; each GPU load of `ctl` is a PCIe round trip [inferred ~1-2 us] |
| discrete, option D2 | device memory | device memory, mapped into host processes through BAR1 (gdrcopy; the server already reads 1502/1503 device memory with gdrcopy or cuMemcpyDtoH: nv_gpu_shared_array_map.cpp:70-128) | uncached PCIe reads, posted writes | GPU accesses are local. The host side of A3 relies on PCIe ordering: a read does not pass an earlier posted write on the same path [inferred; litmus decides] |
| discrete, option D3 (mirror) | device memory | device memory plus an agent-copied host mirror every δ | mirror | **not exact**: O1 weakens to "stale by ≤ δ". Off by default; listed because it is the cheapest reader path |

The placement changes *how* the cut is executed (which loads and stores cross PCIe). It does not change the protocol or the proof. That holds only while the capability bits hold, which is why they are measured, not assumed.

### R8.3 Cost parameters measured at start-up

The agent measures them at its first module load: one calibration kernel plus one host thread in a helper process, about 100 ms [inferred]. They are cached per device and driver version.

- `c_rt`: one synchronous no-op host call. Today about 10 us under contention (CLAUDE.md:2185-2190), a 69 us median for an isolated call under the AC3 watcher (2215-2217), 14-16 us with WFE (2257-2262).
- `c_lock`: an uncontended device-lock CAS.
- `c_pst`, `c_pld`: GPU store/load to P and C on each candidate placement.
- `c_fence`: `membar.sys`.
- `c_red`: a warp reduction (shfl tree, or the lane-turn fallback).
- `c_hld`, `c_hst`: a host-process load of a GPU-written P line, and a store to C.
- `n_SM`, warps per SM, and the number of attached agents A.

### R8.4 Cost model and decision rule

For a group of g lanes at a site executed f times per launch:

- **sync:** `T_sync = g · c_rt`, serialized across the whole GPU (one request slot), so throughput is at most `1/c_rt`.
- **deferred ADD/SET/DEL commit:** `T_c ≈ c_red + c_lock + 3·c_pst + 2·c_pld + 2·c_fence`. Serialized only per (key, agent, shard), so with W warps per SM on one key the wait is about `(W/S)·T_c` per SM.
- **READ_LOCAL:** `≈ 4·c_pld`, against `c_rt` for a sync copy.
- **first touch:** one `c_rt` per (key, incarnation, group).
- **host merge read of a key:** `≈ 2·A·S·c_hld` (double collect); r reads per second add `r·2·A·S·c_hld` to the readers.

Decision per site:
- the feasible mode (R7) with minimal `f·T + amortized first touch + reader cost`;
- per map, the shard count `S* = argmin_S [(W/S)·T_c·f + r·2·A·S·c_hld]`, clamped to [1, n_SM];
- S = 1 if the map is marked strict (C1);
- eager folding of a key when its reader cost exceeds the fold cost.

With `c_rt ≈ 10 us` and `c_p* ≈ 0.5-2 us` [inferred, R10 measures], deferral wins by one to two orders of magnitude per warp. The model mainly picks S, the placement on discrete GPUs, and folding.

### R8.5 What else changes on a discrete GPU

- `c_rt` grows (flags over PCIe), so the benefit grows.
- On CUDA-IPC platforms 1501 is a real GPU hash.
  - Its host lookup returns a per-map staging copy (nv_gpu_shared_hash_map.cpp:47-63; nv_gpu_shared_array_map.cpp:70-128).
  - Its NOEXIST insert never succeeds (nv_gpu_shared_hash_map.cpp:84-87; nv_gpu_shared_array_map.cpp:137-144).

  ATTACH, fold and cut need a correct keyed add-or-create and insert for `nv_gpu_shared_hash_map_impl` (a prerequisite fix). Deferral then avoids the staging pointer entirely, because effects are keyed.
- Device memory for P makes READ_LOCAL and commits cheap. Host merge costs PCIe reads, so the model lowers S and folds more eagerly.

## R9 Interactions

| interaction | what happens | why it stays exact |
|---|---|---|
| several agents on one segment | ADD: per-agent shards; cut and merge visit all agents' shards (the agent list is in the directory). LWW: one owning agent per map (first claim); other agents' LWW sites on that map stay SYNC. ATTACH carries the requesting agent id, so any watcher may serve it | L1 holds per shard; only one agent ever writes a given shard. Pre-existing and not fixed here: one CommSharedMem per segment, zeroed by each agent (bpf_attach_ctx_cuda.cpp:615; CLAUDE.md:2318-2319). A per-agent request slot is recommended as a prerequisite |
| CPU probes on the same maps | their lookups fold first (O3); their update/delete are host writes (cut); their atomic adds through pointers go to base and commute with GPU contributions in shards | merge = base ⊕ shards; the fold uses an in-place atomic add (`with_existing_value`, bpftime_hash_map.hpp:699-709), so a CPU add through a held pointer is not overwritten by a fold. A CPU-probe *plain* RMW is racy, as in Linux |
| graph launches and CUDA 13 handle launches the hooks do not see (cudaGraphLaunch not hooked; handle launches not noted: nv_attach_impl.cpp:436-534, 1561-1592) | their units commit and become visible like any other (O1). Only O5 (detected quiescence) and eager folding are unavailable for them | visibility never depended on launch tracking |
| map close / fd reuse | close cuts all slots to CLOSED, then frees; the site table holds the `map_uid` snapshot; a mismatch means no effect, counted (C2) | no unit can touch the new map at the reused fd; units on the closed map are ordered before the close or have no effect |
| printk ordering | per-thread program order exact: records are committed before the thread's next unit, and a sync printk drains first. Across threads: globaltimer order within a drain batch, as trace_pipe merges per-CPU buffers. Against host-side output streams: none, as in Linux | line count exact (no drop; backpressure to sync); see R3.7 |
| multiple probes on one kernel (kprobe + kretprobe, two kprobes, memcapture) | site ids are unique per (agent, program, unit); the effect-kind check spans every hook entry in the module (hack_fatbin); memcapture prepends to rewritten words | a thread's units from different probes commit in program order; kind conflicts on a map make its sites SYNC |
| late path, new sessions, substituted modules that cannot be unloaded | kind claims, map uids and incarnations live in the segment, not in modules; a later session's maps have new uids | stale modules can only reach maps whose uid matches their snapshot |
| detach (last detach does not wait for running kernels in substitution mode: nv_attach_impl.cpp:159-170) | PES regions outlive the detach; units still running commit normally | regions are freed only at segment cleanup or after the owner is dead and its shards are folded |
| process exit or killed kernel during a commit section | the owner's liveness record goes stale; a waiter recovers the shard from the committed half selected by `ver` (R3.3) | the section never completed, so the unit is not in σ, the same as a probe killed mid-way |
| preemption of a warp inside a section | host waiters wait (bounded only by the owner's liveness) | no deadlock: sections make no host call and take no host lock |

## R10 First slice: the smallest end-to-end piece measurable on the Orin

**Scope:** the ADD effect only (ADD-or-create and ADD-IF-PRESENT) on HASH maps, with:
- warp aggregation;
- merge-on-read;
- the cut;
- ATTACH;
- the analyzer for shapes A, B, C and H's lookup;
- the record emitter.

SET/LWW, READ_LOCAL and OUT come later. Their sites stay SYNC in this slice.

### Step 0 (gate): litmus tests and calibration

These run before anything is built on top of A2-A5.

- **Tests:** MP (GPU → CPU), MP (CPU → GPU) and SB on segment memory.
  - The GPU side is a kernel with exactly the design's instruction sequences.
  - The CPU side is a thread **in another process** mapping the segment, with the design's fences: `__atomic_thread_fence(seq_cst)`; also `dmb sy`.
- **Scale:** 10^7 iterations each, with randomized delays, plus the same runs **without** the fences as a negative control (it must show violations, or the test is too weak to count).
- **Also measured:**
  - single-copy atomicity of 8-byte `ver`/`ctl`;
  - the `%globaltimer` resolution;
  - whether `shfl.sync`/`vote.sync.ballot` survive the sm_61 → sm_87 retarget (the only new primitives; `match.any` and `redux` are not used);
  - the R8.3 costs.
- **If SB or MP fails:** the design stops at the analyzer and records (still useful for the survey check), and every site stays SYNC.

### What to build

1. **Analyzer v0 plus offline CLI.** Interpretation (R4.1), units A, B, C and H-lookup, the region rewrite, self-validation (R5), records (R7.5). This needs no GPU. Its first deliverable is the mechanical survey check: expect 20/39/0, 36 of 38 map sites, and the four listed printk differences.
2. **Map-handler layer** (R3.4). Directory, ADD regions, merge-on-read, cut, fold and `hseq`, in the HASH branches of `bpf_map_handler`. It is used unchanged by the syscall server, CPU helpers and the watcher.
3. **Watcher:** the ATTACH opcode (bpf_attach_ctx_cuda.cpp:124-187); fold-before-answer for sync lookups on PES maps; a final fold and liveness update in its atexit handler.
4. **Trampoline:**
   - helper 520 `pes_add` with the two-level lock and both aggregation modes (shfl, or lane-turn fallback);
   - registration of 520 in core.cpp:234-253;
   - the device lock table and the module globals written next to `map_info`.
5. **Switches:** off by default (`BPFTIME_GPU_PES=1` to enable). `pes_abi` in RuntimeInput.

### Programs and expected numbers

These are all [inferred], anchored to hostcall-l1 (32 threads, 3 timed launches per case, legacy-launch victim): baseline 39.9 us; ARRAY direct path 47.2/48.2 us; CPU hash cases 317.7-341.1 us; CUDA Counter 2,877.5 us.

| program / case | today | expected with the slice | why |
|---|---:|---:|---|
| micro "CPU Hash map lookup" (benchmark/gpu/micro cuda_probe.bpf.c:171, H: lookup + `(*val)++`, key 0 never inserted) | 341.1 us | 45-55 us after the first launch; about +10-70 us once for the first ATTACH | ADD-IF-PRESENT: the first ATTACH sets ABSENT; later launches do a site-cache hit and one `ctl` pre-check load per warp, and nothing else; anchored to the ARRAY path's 48 us |
| new micro case "CPU Hash counter" (A: lookup + `__atomic_add_fetch` + NOEXIST insert, key pid, cloned from cuda-counter:46-53 into benchmark/gpu/micro with a runner config) | ≈ 330-340 us (like the other hash cases) | 50-60 us | one commit per warp: `T_c` ≈ 4-6 accesses to host memory plus 2 `membar.sys` over the array path; the first touch is one ATTACH |
| CUDA Counter (minimal) | 2,877.5 us | ≈ 2,520-2,560 us | only call_count (A, 1 of the 8 host calls per thread left after layer 1: hostcall-l1 README) is deferred in this slice: about 32 × 11 us less |
| threadscheduling example (32-thread victim) | 3 host calls per thread | 1 (E stays SYNC) | sm_histogram and warp_histogram are A |

**Exactness against today**, by the lockstep deduction (BRIEF): the "CPU Hash counter" case ends today with `1 + 32·(L−1)` after L launches of one warp: launch 1's 32 lookups all miss and one insert lands. Expected with the slice: `32·L`.

### How to measure

The upstream `benchmark/gpu` runner with hostcall-l1's protocol:
- 10 interleaved rounds, alternating build D (dev 52126f33) and build N (the slice);
- before every batch: the clock check (cpu5 1728000, GPU 1020000000), no leftover processes, no stale segment;
- per-case validity lines;
- counts of host requests from the watcher's debug lines in a smoke round: expected for the counter case, 1 ATTACH per (key, warp group) per session instead of 32 per launch.

### Exactness tests (oracle = σ semantics, never today's output)

- **T1 counter oracle.** N threads × L launches with the A program. Final value read by the loader = N·L. B variant: d = threadIdx+1 → final = L·N(N+1)/2. Grids with partial warps (N not a multiple of 32).
- **T2 real-time completeness.** The app records each launch's completion time after `cudaStreamSynchronize`, with CLOCK_MONOTONIC. The loader polls the key every 100 us, logging start and end times. Post hoc, every read v must satisfy `N·#(completed before read start) ≤ v ≤ N·#(started before read end)`. Successive reads must be monotone (C1: regular and monotone). A variant with S = 1 checks linearizability directly: each read must equal Σ n over some prefix of the committed groups, which a debug build logs.
- **T3 print-and-clear.** The loader does `lookup_and_delete` every 1 ms while kernels run. Σ(returned values) + final = N·L exactly. This tests L1 under load.
- **T4 host overwrite.** The loader runs `update(ANY, X)` once in mid-run. Then final − X must lie between the number of units that started after the update returned and the number that completed after it began.
- **T5 CPU probe on the same map.** A uprobe on the app's launch wrapper adds 1 per launch to the same key. Final = N·L + L. A second variant reads the key in the uprobe and checks O3.
- **T6 first-touch storm.** 64 blocks touch a fresh key at once: exact (today loses).
- **T7 divergence.** A kretprobe on a kernel whose lanes return at different points (kretprobe main.cpp:50-60): count = #threads (L3).
- **T8 translation validation.**
  - For every rewritten unit, run the *original* program on the CPU (llvmbpf CPU JIT) over random serial schedules of the same units.
  - Compare the final map state with the GPU run's.
  - Run this on the survey programs, on synthetic idiom variants, and on near-misses that must be rejected: init ≠ delta, pointer escape, a store to the key slot inside the region, a FETCH add.

### Mutation checks (each must make a named test fail)

| mutation | expected failing test |
|---|---|
| M1 remove the GPU `membar.sys` between `ver` odd and the `ctl` load | SB litmus; T3 under stress |
| M2 remove the host fence between the `ctl` store and the `ver` load | SB litmus; T3 |
| M3 commit n = 1 per group (count elections, as auto-warp does) | T1, T7 |
| M4 loader lookup reads base only (no merge) | T2 (reads lag behind completed launches) |
| M5 the cut ignores the shard tag (old-incarnation contributions count in the new one) | T3 (double count) |
| M6 the cut does not wait for an even `ver` | T3 (loss) |
| M7 analyzer accepts init ≠ delta | T8 |
| M8 analyzer ignores a pointer escape | T8 |
| M9 fold without `hseq` | T2 (non-monotone reads) |
| M10 every site uses site 0 | T1 (wrong map or offset) |
| M11 the rewrite leaves a stale in-edge into R | self-validation rejects; the original words are used (checked by a unit test) |
| M12 check script: cudagraph:56's value use treated as "needed now" | the survey check (20/38/1) |

### Exit criteria for the slice

1. Litmus tests pass and their negative controls fail.
2. T1-T8 pass on N and M1-M12 fail as listed.
3. The survey check reproduces 20/39/0, 36/38, and the four listed printk differences.
4. The measured "CPU Hash counter" and "CPU Hash map lookup" cases are within 2× of the ARRAY direct path. If they are not, R8's parameters say why: for example, `c_pld` to registered memory is larger than assumed.

## Open questions and risks

1. **A3, store buffering between CPU and GPU on registered memory, is unproven.** The whole cut rests on it. Step 0 decides it. Failure means everything stays SYNC; no unsafe fallback exists.
2. **GPU access costs to registered host memory are unknown** (`c_pld`, `c_pst`, `membar.sys`). The only datum is the 300 us against 10 us contention figure (CLAUDE.md:2185-2190). If they are large, commits per warp approach a host call. The model (R8) then picks fewer shards and eager folding, but the R10 numbers would move.
3. **`shfl.sync` / `vote.sync` after the sm_61 → sm_87 retarget is unverified.** The lane-turn fallback keeps exactness without them, at higher cost.
4. **Bytecode recognition is shape-specific.** Other compilers or clang versions may produce shapes outside R4's patterns. Those sites stay SYNC: correct, but slower. T8 guards against the unsound direction only.
5. **Merged calls (cuda-counter pc 76) need per-edge records.** Line mapping needs `.BTF.ext`, which the server does not keep. The agent's records carry pcs; only the offline check has lines.
6. **The survey's printk `aggregatable` is internally inconsistent** on `%globaltimer`. "51" is reproduced as an analyzer count plus four listed records, not as a raw count.
7. **The bounded-wait policy holds a stripe lock while a section is open.** A long preemption delays every host op on that stripe (16 heads). Liveness-based recovery covers dead owners only.
8. **C1:** in-flight reads of sharded ADD keys are regular and monotone, not linearizable. This includes unit-increment counters, because a group commit adds n at once. They are linearizable only with a stable double collect (unchanged `ver`), S = 1, or at quiescence. Strict maps need S = 1.
9. **Multi-agent segments still share one request slot** (pre-existing). ATTACH and the remaining sync calls inherit that hazard until a per-agent slot exists.
10. **The PES ABI changes the segment layout** (directory, map uid in the handler). Mixed runtime versions on one segment must be refused, as CLAUDE.md:220 already requires.
