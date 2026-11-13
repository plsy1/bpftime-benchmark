# UnitFold: exact per-call-site execution of GPU eBPF helper calls

Design for direction 2 of `RESEARCH_DIRECTIONS.md` (main): GPU-to-CPU host calls. It merges three drafts (analysis-first, semantics-first, performance-first, in `drafts/`) after two reviews, which both chose analysis-first as the base. It was then revised after two red-team passes (semantics, feasibility). Every red-team issue and its status is in "Red-team issues and their status" at the end; two independent rechecks of the revision followed ("Independent recheck after the revision", "Second independent recheck").
- `final-long/design.md`: the longer synthesis before the red team, with the full test and mutation tables. Where it differs from this file, this file holds.
- `final-long/r7_check.py`, `r7_output.txt`, `r7_records.json`: the R7 simulation.
- `scripts/exec_mode.py`, `exec-mode.txt`, `records-exec.json`: what the runtime would run per site (added after the red team).
- `redteam/`: the LLVM 15 merge experiment and the `.BTF.ext` parser.
- `read/`: the read phase's fact sheet (`BRIEF.md`), disassemblies and notes. `scripts/` and `*workflow-output.json`: both workflows.

**Conventions.**
- Every `path:line` is in `/home/jetson/src/bpftime-daily-state/worktrees/4708f4092d1e/candidate` at 52126f33, re-read for this document. llvmbpf lines refer to submodule `vm/llvm-jit` at 87bac39a, before the overlay.
- **[new]** marks a mechanism invented here. **[inferred]** marks reasoning over code that was never run. **[external]** marks CUDA/PTX documentation not re-read.
- Nothing was built and nothing ran on the GPU. Only Python ran, over `records.json`, plus `opt-15` on hand-written IR (`redteam/`).

## The model in one page

**Problem.** A GPU eBPF probe that calls a helper on a CPU map stops and waits for the CPU (about 11 us per call on the Orin, lanes in turn). Today bpftime does this for every such call, and the counter idiom loses counts on the way (CUDA Counter: 97 of 128).

**Reference semantics (SR, R1).** For every map there is one total order of GPU *units* and host operations that keeps each thread's program order and every happens-before edge, and replaying it on Linux's sequential map specification gives every value anyone saw and the final contents. A *unit* is a recognised idiom (lookup + add or insert, for example) run as one atomic step, or any other helper call. SR refines what Linux promises (atomicity per helper call), and makes today's losses illegal.

**Four ways to run a call site**, each chosen at load time from facts the analyzer can prove:

| mode | what runs | static condition |
|---|---|---|
| GPU records | the GPU writes the unit's cumulative effect into a record in the shared segment; no round trip after the key's first touch | the unit is recognised (K, P, V, L, C in R4), its map is a HASH in `map_info`, no program writes through its lookup pointers |
| aggregated GPU records | lanes with the same map, site and key are reduced to one record write (sum plus lane count) | as above; the grouping is checked at run time, never trusted from the analysis |
| one round trip per group | an exact host operation per group of lanes (first touch, a value needed now, a full ledger) | the unit is recognised but its answer or presence must come from the host |
| today's synchronous call | unchanged | anything not proven |

**One invariant makes deferral exact: readers fold.** Every host access to a ledgered map, in any process (the tool's `bpf()`, CPU probes, the watcher), first applies that key's committed GPU records to the map under the key's stripe lock. The host's state is therefore current at every access, without a flush, a kernel-completion hook, a channel from the tool to the agent, or CPU-GPU atomics. Presence and capacity stay host-decided: a key's first touch is a host operation.

**Hardware enters twice, never in the semantics.**
- *Facts* (R3.1), litmus-tested (step 0; a result cached per device, driver and build): deferral is off unless GPU-to-CPU and CPU-to-GPU message passing and 8-byte single-copy atomicity on the registered segment hold; the three-party shapes decide whether real-time order holds across processes or only per pair.
- *Costs* (R8), measured at start-up: a round trip `c_rt`, a record commit `c_commit(k)` under k-way contention, uncached loads, fences. They choose among the legal modes. An integrated GPU (the Orin) and a discrete GPU differ only in these numbers: records stay in pinned host memory in both, so a tool can fold without the agent.

**What the default delivers on the 59 surveyed host-call sites** (`exec-mode.txt`). The survey labelled them 20 eliminate, 39 defer, 0 sync: what the sites *need*. What the full design *runs* by default is not the same:

| survey label | GPU records after the first touch | printk ring records | one round trip per group |
|---|---:|---:|---:|
| eliminate (20) | 17 | 0 | 3 |
| defer (39) | 18 | 21 | 0 |
| total (59) | 35 | 21 | 3 |

- The 3 round trips are the reads of a value the probe uses (`resident_read`, cuda-counter:66, rocm-counter:67, cudagraph:75). They become GPU reads only if CPU-GPU store buffering holds (C-SB, R3.7).
- Every key still costs one host operation per agent at its first touch.
- Slice 1 (R10) covers 20 of the 59 sites. 14 of them can run on the Orin (rocm-counter's 6 have no device attach here), and 10 are measured (threadscheduling's dynamic-hook copy probes another kernel).

## Overview

### The problem

**When a helper call becomes a host call.** A GPU probe's helper call turns into a synchronous host call for:
- HASH maps and other CPU maps;
- 1501, which is created as HASH on the Orin (`runtime/include/bpftime_shm.hpp:137-154`, applied at `runtime/syscall-server/syscall_context.cpp:566-577`);
- every delete except on ordinary ARRAY;
- printk and puts.

The routing is in `attach/nv_attach_impl/trampoline/default_trampoline.cu:233-422`.

**How a host call works.**
1. Take the module device lock.
2. Take `occupy_flag` in the segment's single `CommSharedMem`.
3. Write the request, execute `membar.sys`, set `flag1`, spin on `flag2`. Lanes take turns over a 32-step loop (`default_trampoline.cu:134-175`).
4. The watcher takes `flag1` with an acquire CAS and answers with a release store of `flag2` (`runtime/src/attach/bpf_attach_ctx_cuda.cpp:309, 317`).

**Measured** (aa6eb973, `hostcall-l1-20261111/README.md:26-41`):

| case | per launch | note |
|---|---|---|
| baseline | 39.9 us | |
| CPU Array lookup (served on the GPU since 1b3d9c99) | 48.2 us | |
| CPU Hash lookup | 341.1 us | |
| CUDA Counter | 2,877.5 us | 264 requests per launch on average over 4 launches; the timed launches carry 256 (the warm-up launch adds 32 first-touch inserts) |

- This gives about 11 us per host call ((2,877.5 − 48) / 256 = 11.05 us).
- At 52126f33 (AC3 watcher) a call in a 32-call burst costs 11.8 us without load and 15.7 us under load (`watcher-policy-20261112/v3/README.md:57-58`).
- **Data loss today.** The CUDA Counter loader prints `calls=97` in every run (`hostcall-l1-20261111/README.md:32-33`) where 128 probes ran.
  - All 32 lookups of a warp are answered before any lane branches.
  - The 32 first-touch `BPF_NOEXIST` inserts race and one lands.
  - A plain `*v += d` through the shared pointer collapses to about one update per warp.
- **Survey baseline.** The survey classifies 168 GPU sites as 109 already GPU-local, 20 eliminate, 39 defer, 0 sync; 51 of the 59 host-call sites are marked aggregatable.

### What UnitFold does

**Units.** A load-time analyzer in the agent recognises multi-instruction idioms (lookup + add or insert) as one *unit*. It rewrites each unit in the bytecode into one call of a new registered helper (ids 600-604), with the site descriptor in immediates (no pc).

**Commit.** The GPU commits a deferred unit with plain stores into a seqlocked *record* in the shared segment and executes `membar.sys` before returning. There is no round trip. Lanes with an equal key are reduced first.

**Fold.** Every host access path folds the key's committed records into the map under the key's stripe lock, right before its own operation. That covers loader `bpf()` commands, CPU probes in the agent and watcher-served calls. So host state is current at every host access, with:
- no flush;
- no kernel-completion hook;
- no loader-to-agent channel;
- no CPU-GPU atomics;
- no store-buffering assumption.

**Fallback.** The first touch of a key is a synchronous host operation, so presence and capacity stay host-decided. Whatever cannot be proven keeps today's path. Hardware enters only as measured costs and litmus-tested capability bits.

| task mode | UnitFold mechanism |
|---|---|
| eliminate | records that no host ever folds; an ABSENT negative cache makes add-if-present and delete of absent keys free. GPU-local reads (READ_LOCAL) only with store buffering (R3.7), else one LOOKUP_COPY per group |
| aggregate | lanes with an equal key commit once with the lane count and the sum of deltas |
| defer | record commit (ADD, SET, DEL) or printk ring record |
| sync round trip | today's `call 1/2/3/6`, unchanged; new host operations UNIT_ADD, UNIT_SET, UNIT_DEL and LOOKUP_COPY serve as exact per-group fallbacks |

### Coined names

| name | meaning |
|---|---|
| unit | an idiom, any other helper call, or one BPF_ATOMIC instruction, executed as one atomic step |
| ledger [new] | per-map structure in the segment holding unfolded GPU effects |
| slot | one key's entry in a ledger; only the host creates slots, so a key has at most one |
| record [new] | per (slot, shard): a version word `ver` plus two halves of cumulative totals. GPU-written, zeroed once by the CPU at slot creation |
| shadow [new] | the GPU's device-memory copy of a record |
| watermark | the CPU-only copy of what has been folded |
| fold | applying record deltas to the element under the stripe lock |
| straggler | a GPU commit landing after a host operation's read; ordered after that operation |
| hint | CPU-written slot state (NONE = 0, LIVE, ABSENT, SYNC, DEAD) that the GPU reads |
| device block [new] | per (ledger, agent) device memory: a header (the ledger's segment device addresses, C, NL), lock table, shadows, key-to-slot cache, `devmode` |
| flip [new] | taking a map out of deferral when a late pointer writer appears: FLIPPED, hints to SYNC and a fold (slice 1), plus the owner's flip kernel (slice 2) |
| sweep [new] | folding pending re-creations before capacity-changing operations |
| strict mode | the default: one record per slot (S = 1) and one deferring agent per map |
| retired ledger [new] | a ledger whose owner was found dead; still folded by every reader until the map closes |
| `gen` [new] | a map's generation: a segment counter bumped at every map creation, so a reused fd is told apart (R3.8) |
| section | one helper-600 critical section of a group leader, from loading the hint to the commit |
| tool, loader | the user-space eBPF tool (`bpf()` calls, handled by the syscall server in its own process) |
| agent, watcher | the bpftime library inside the traced application; its watcher thread serves GPU host calls |
| `map_info` | the trampoline's `__constant__` table of map descriptors (`MapBasicInfo`), written into each patched module |
| stripe lock | the hash map's per-bucket-group writer lock (`CLAUDE.md`, "Hash Map Concurrency") |
| A, S | number of deferring agents, number of record shards per slot (both 1 in strict mode) |
| LWW | last writer wins (SET/DEL ledgers) |
| D, N | the 52126f33 runtime and slice 1, compared in R10 |
| victim | the CUDA application a benchmark probes |

Map type numbers: 1501 GPU shared hash, 1502 per-thread array, 1503/1504/1513 single-copy GPU arrays, 1512 per-thread array (variant), 1527 GPU ring buffer (bpftime's GPU map types; 1501 is an ordinary HASH on the Orin).

### What changes

| place | change |
|---|---|
| start of `init_attach_ctx_from_handlers`, before loop 1 (`runtime/src/attach/bpf_attach_ctx.cpp:229-245`; loop 2, CUDA links, `:248-259`) | analyzer over all handlers; pointer-writer scan, flip and fold before any link; ledgers |
| `write_trampoline_globals`, `fatbin_record::try_loading_ptxs`, `run_attach_entry_on_gpu` (`nv_attach_impl.cpp:2366-2385`, `nv_attach_fatbin_record.cpp:251-292`, `nv_attach_impl.cpp:1316-1349`) | one helper writes `map_info` with the context rule (R3.2) |
| `MapBasicInfo` (`default_trampoline.cu:84-92`, host copy) | a 32-bit `gen` in the padding at offset 20 and the ledger flag at offset 1, so the entry stays 40 B (`map_info[1024]` is 40 KiB of the 64 KiB constant bank the module shares with the application) |
| `bpf_attach_ctx.cpp:639` | pass the rewritten words |
| `create_map_basic_info` (`bpf_attach_ctx_cuda.cpp:505-554`) | `extra_buffer` of a ledgered HASH = this agent's device block |
| `core.cpp:234-253` | register ids 600-604 |
| each pass | export `ptxpass_site_exec_abi` |
| `default_trampoline.cu` | helpers 600-604 |
| `gpu_helper_protocol.h:22-30` | four opcodes |
| `serve_cuda_request` (`bpf_attach_ctx_cuda.cpp:124-187`) | serve the opcodes; fold before serving |
| `map_handler.cpp/.hpp` | ledger pointer, `gen`, hooks, `map_lookup_copy` |
| `bpftime_hash_map.hpp` | locked fold-then-op variants, in-place atomic add shaped like `with_existing_value` (`:699-709`), a free counter kept only while a ledger is attached (the redesign dropped a shared count for its cost, `CLAUDE.md`, "Hash Map Concurrency") |
| `syscall_context.cpp:615-622`, `bpftime_shm_internal.cpp:295-315`, `handler_manager.cpp:116-123` | `map_lookup_copy`; cached-handler branches to the generic paths; close marks ledgers DEAD |
| `vm/compat/llvm-vm` overlay | `nomerge` and ExitOnError hunks |
| agent | ledger ownership (`flock` file), writer registration, retired-ledger list |
| `tools/bpftimetool/main.cpp:65-117` | `run` runs the scan and flip first |
| benchmark and example loaders, configs | measurement plumbing (R10, item 7) |
| typed HASH helpers (`runtime/src/bpf_helper.cpp:470-622`) | take the generic path when a ledger exists |

**Unchanged:** the flag1/flag2 handshake, lane serialisation, the watcher's sleep policies, GPU map types, the ARRAY path; user loaders need no change.

## Status after three review rounds

- **Core: checked twice, held.** The record protocol (two-half seqlock, fold guard, watermark rule), stragglers, the run-time grouping check and the lane-turn fallback were found sound by two independent rechecks against the code.
- **Lifecycle rules: specified, last revision unreviewed.** Late pointer writers, owner death and several agents broke in every round, each time in the mechanism the previous round had added (FLIPPED and the hint store, liveness locks, writer registration, retired ledgers, the per-helper path after a GPU-writer flip, trylock folds). The fixes from the second recheck (results b6f81768) have not been reviewed.
- **Slice 1 has grown back** with these rules (build list, T1-T23, M1-M30); the 4-week estimate is stale.
- **Candidate simplification, not yet checked:** in slice 1, a second agent, a late pointer writer or an owner death sends the map out of deferral for good (FLIPPED, every unit through UNIT_ADD, exact per group; per-helper after a GPU writer), with the old records still folded. This would drop takeover, re-ledgering and most of the retired-ledger machinery; an agent that started before the ledger existed still needs registration or an equivalent check.

## R1. Reference semantics

### Actions

**Actions.**
- **GPU units** (per thread, in program order across its probe executions).
- **Host operations:**
  - loader `bpf()` map commands, run in the loader process on the segment (`syscall_context.cpp:600-679`);
  - CPU-probe helpers (`bpf_helper.cpp:401-422, 470-622`);
  - watcher-served requests (`bpf_attach_ctx_cuda.cpp:124-187`);
  - close.
- **printk lines.**

**Plain pointer accesses** outside units are data races, as in Linux.

**Precondition PW** [new, enforced in R3.6]. On a ledgered map, no program that runs (attached by any agent, or run by `bpftimetool run`) does any of the following through a lookup pointer, or through a helper that writes through a pointer argument:
- a plain store;
- a non-ADD atomic;
- on the GPU, any atomic.

The only exception is D5.

**Commit point.** A deferred unit commits at the even `ver` store of its record. A `membar.sys` follows before the helper returns.

**hb (happens-before)** is the closure of:
- program order;
- fence-then-flag message passing in either direction;
- kernel completion observed by stream, device or event synchronisation;
- a CPU action before a kernel launch call, to the threads of that kernel [external: launches order earlier host writes before the kernel];
- CPU synchronisation (locks in the segment, between processes too).

Real time between CPU and GPU is only observable through these. The closure is transitive across the GPU and several CPU processes (a GPU commit seen by the watcher, then by a tool through a stripe lock). That needs more than two-party message passing; step 0 tests the three-party shapes (R10). If they fail, (rt) is stated per pair of parties only, and the fold guard (R3.4) keeps a stale read harmless either way.

### SR: the reference

An execution is **exact** if, for every map M, there is a total order ≺ over the units and host operations on M such that:
1. **(po)** each thread's program order holds;
2. **(rt)** if end(a) hb start(b), then a ≺ b;
3. **(seq)** replaying ≺ on Linux's sequential specification gives every value returned to a probe, every host return value, every host read and the final contents.

The specification includes flags, EEXIST, ENOENT and E2BIG (`kernel/bpf/hashtab.c:1084-1113`, L4T 6.8.12). A unit replays as its own instructions, executed atomically.

- **printk:** per agent, (po) and (rt), and every line appears exactly once.
- **Measured against:** this order, not today's bpftime, which no order explains (97 of 128).
- **Scope:** per map, like Linux. Commits are fenced, so a host reader that sees a thread's later commit on any map also sees its earlier ones (message passing). This is not cross-map atomicity.

### Observation points

**Every host access** through the `bpf_map_handler` entry points of R3.5, in any process, reflects every unit whose end hb its start. This needs no kernel completion, flush, launch tracking or live agent.

- `get_next_key` sweeps first when it starts at the first key: with NULL, or with a key not in the map (the kernel starts over then too).
- An iteration in progress may miss later re-creations, like Linux.
- Excluded: ARRAY mmap (`syscall_context.cpp:1036-1048`); batch commands, which fall through to the real `bpf()` (`syscall_context.cpp:852-854`); `run_with_kernel`.

### Per idiom

Linux guarantees atomicity per helper call and per BPF_ATOMIC. Beyond that:
- plain RMW is racy;
- miss followed by NOEXIST loses increments;
- update replaces the element;
- user reads copy one element;
- printk merges by timestamp and can lose lines.

Every SR execution is a Linux-legal serial schedule, so UnitFold refines Linux except D1-D6.

| idiom (sites) | Linux | today | UnitFold | vs Linux |
|---|---|---|---|---|
| A counter: lookup; miss → NOEXIST insert; hit → atomic add (10) | first-touch loss | 97/128 | ADD-or-create; first touch is a host operation | more |
| B non-atomic RMW, EXIST self-write, else NOEXIST (9) | lost updates | ~1 per warp | ADD-or-create, exact Σ | more |
| C leader lookup + update(ANY, `p?*p+d:d`) (4) | racy across launches | exact unless overlapping | ADD-or-create | more |
| E entry SET / exit READ / DEL (9) | per call | per call | SET and DEL deferred; READ via LOOKUP_COPY (READ_LOCAL with C-SB) | same |
| F per-thread record (2) | per call | same | SET; E2BIG decided at a host first touch | same |
| G leader publish (1) | per call | same | SET | same |
| H printk (21) | per-CPU order, may lose | synchronous | per-thread order, no loss, ≤ 1 watcher turn late | same / more / less (D3) |
| J benchmark: update / lookup + `(*v)++` / delete (3) | `++` racy | collapses | SET / ADD-if-present / DEL | same / more / same |
| unrecognised pointer RMW | racy | racy | map not ledgered | same |

In strict mode, host reads are linearizable: one record per key, one seqlock snapshot. The exception is a key whose records still move in a retired ledger as well as the current one (R3.8): D1 then applies.

### Deviations, each counted

- **D1 (shards S > 1 or several deferring agents, both opt-in; and by default while a retired ledger's old owner still commits, R3.8).**
  - Host reads during commits are regular and monotone; they are linearizable only if a double collect of `ver` is stable.
  - A destructive operation splits records at their own reads.
  - Conservation stays exact.
  - Counter: `unstable_reads`.
- **D2.** Commits on a closed map are dropped. bpftime frees maps on close (`runtime/src/handler/handler_manager.cpp:116-123`); in Linux the effect would be unobservable. Counter: `dead_drops`.
- **D3.** printk appears up to one watcher turn late. The backstop is 1,000 us (`runtime/src/attach/cuda_watcher_policy.hpp:27`). Opt-out: a new variable, `BPFTIME_GPU_PRINTK_SYNC=1`, keeps printk synchronous.
- **D4.** A straggler re-creating a host-deleted key gets its E2BIG decision at the first sweep or fold that sees it.
  - Sweeps run before every insert and delete near capacity.
  - What remains: when several stragglers are pending on a nearly full map, which ones get in follows sweep order.
  - Counter: `e2big_sweep`.
- **D5. Late pointer writers.** When a program that writes through lookup pointers of a ledgered map M starts to run late (R3.6):
  - *Stragglers*, sections that loaded a LIVE hint before the hints were set to SYNC, can still commit. For a CPU writer they are folded in place: a straggler that happened before the writer's store but is folded after it lands on top of the store, where SR overwrites it (the writer's own lookups fold first, so only commits that reach it through another path, such as a fold of another map, can do this). For a GPU writer with atomics they are folded by element replacement: an atomic through a pointer to the replaced element goes to the retired element and is lost or, once the element is reused, lands in another key (Linux's update semantics, triggered by the fold).
  - *After a GPU writer's flip*, every later unit on M runs with Linux per-helper semantics, as today: the host finds or creates the element and returns its address, and the leader adds with a GPU atomic after the reply. A delete or update between the reply and the add sends the add to a freed or retired element, as a Linux program's atomic through a lookup pointer can.
  - New sections go to the host from the hint store on, and every later hint write keeps SYNC, so stragglers are limited to sections already running then (one section each, longer only under preemption). Slice 2's flip kernel removes the stragglers; the per-helper semantics stay while the GPU writer is attached.
  - Counters: `late_stragglers`, `post_flip_units`.
- **D6.** Deferred GPU writes after `BPF_MAP_FREEZE` are applied, as Linux allows for programs. Today GPU host calls get EPERM (`runtime/src/handler/map_handler.cpp:445-448, 776-779`). UNIT_* operations use program semantics; unrewritten sites keep EPERM.
- **D7 (inherited).** The base hash map can return E2BIG with free elements left while deletes run concurrently (its per-CPU free lists: the window of the kernel's `pcpu_freelist`, documented in `CLAUDE.md`, "Hash Map Concurrency"). No serial order explains such a return. UnitFold neither adds nor removes it; Lemma 4 holds up to it.

### Proof sketch (strict mode, ADD; SET and DEL analogous)

- **Invariant I.** A ledgered key's element changes only:
  - in a fold;
  - in a host operation run right after a fold under the same stripe lock;
  - by commuting CPU atomic adds through pointers.

  It rests on PW and on full hook coverage.
- **Lemma 1 (snapshot).** The two-half read returns totals held at one instant. It needs H2 and H4, and never waits on an odd `ver`.
- **Lemma 2 (fold).**
  - The deltas s − w met an unchanged element (Invariant I). Adds commute, and the count decides re-creation, so applying them at the fold equals applying them at their commits.
  - Commits whose end hb the fold's start are in s (H2, the three-party shapes of step 0, plus the trailing fence).
  - Later ones are ordered after the operation.
  - A record read older than the watermark (`ver` ≤ the watermark's) counts as no movement: the watermark was written under the stripe lock by a fold that saw newer totals, so nothing is lost and no difference wraps.
- **Lemma 3.** Each lane is in exactly one group, and every lane of a group has the same (fd, site, key bytes). This rests on the run-time agreement check in helper 600 (R3.3), never on the analysis or on `activemask`: compiler merging can put lanes of two sites into one call. The count is the lane count.
- **Lemma 4 (presence and capacity).**
  - Every key insertion is a host operation: a UNIT_* first touch, a host insert, or a fold re-creation.
  - Count-changing operations sweep first near capacity.
  - Pending GPU DELs are folded before an E2BIG.
  - Therefore the base map's accounting linearizes presence and capacity, except D4's order and D7.
- **Theorem.** Per-key objects and the capacity object are linearizable. By locality (Herlihy-Wing), SR holds.

## R2. Difference from gpu_ext and upstream automatic warp execution

From `overlap-survey-20261113/README.md:44-60`:
- **gpu_ext** (ASPLOS '27) claims warp-leader shards merged at kernel completion with "relaxed, eventual consistency". Its own audit found no implementation. Its results come from hand-rewritten tools.
- **Upstream `official/revision/automatic-warp-execution`** is unmerged and opt-in.
  - It decides yes or no per program.
  - It rejects atomics, printk, puts, perf output, exit and per-thread writes.
  - It runs admitted programs in the leader lane, and a non-atomic `*v = *v + 1` counts once per converged group.

| | gpu_ext | auto-warp | UnitFold |
|---|---|---|---|
| reference | eventual | equivalence to today's collapse | SR with an hb clause, oracle-tested |
| unit of decision | tool, by hand | program | call site or idiom, per-site fallback |
| host sees effects | after a completion merge | immediately (nothing deferred) | at every access: the reader folds |
| aggregation | leader election | leader election | reduction: Σ deltas plus lane count |
| host writes against deferred effects | no rule | n/a | fold-then-operate puts every commit on one side |
| atomics, printk, CPU maps | avoided | rejected | supported per site |
| storage | workload-fitted | none | sized by keys; full → exact sync fallback |
| hardware | unstated | `vote.ballot` | message passing plus 8-byte atomicity, litmus-gated |

**Why exact.**
- Effects are cumulative totals composed at fold time with the value they would have met.
- No merge point exists. The hooks do not see completion of graph launches or CUDA 13 handle launches (`nv_attach_impl.cpp:1561-1592`; `probed_launch_activity.cpp:16-24`).
- Aggregation is a reduction over equal key bytes.
- Storage pressure falls back to the exact host path.

**Why per site.** Each unit has its own descriptor, fallback and grouping. A `%s` printk stays synchronous while its program's counters are deferred.

## R3. Dynamic answer to "only GPU probes touch this map"

UnitFold never needs that fact: every host path composes GPU effects at access time.

### R3.1 Hardware facts

Each core fact is litmus-checked at start-up. Deferral is enabled only if H2-H4 pass; H6 decides whether (rt) holds across processes or only per pair (R1).

| id | fact | precedent at 52126f33 |
|---|---|---|
| H1 | GPU atomics within one context on device memory | module lock (`default_trampoline.cu:118-122, 146-148`) |
| H2 | GPU→CPU message passing to a reader **in another process**: stores, `membar.sys`, flag; reader acquire | 1527 tail (`default_trampoline.cu:478-480`) drained by the loader (`nv_gpu_ringbuf_map.cpp:65-68`) |
| H3 | CPU→GPU message passing | `flag2` release (`bpf_attach_ctx_cuda.cpp:317`) → GPU spin + `membar.sys` (`default_trampoline.cu:159-163`); 1527 head (`:461-470`) |
| H4 | 8-byte single-copy atomicity on the segment | 1527 u64 head/tail (`gpu_helper_protocol.h:86-103`) |
| H5 | GPU-scope ordering: `bar.warp.sync`, device-lock acquire/release | standard PTX [external]; `__syncwarp` and CAS locks in use |
| H6 | three-party transitivity: GPU → CPU A → CPU B in another process (B synchronised with A through a segment spinlock), and CPU A → CPU B → GPU | none; nothing at 52126f33 relies on it |

**Never needed:** new GPU atomics on the segment (today's GPU-only `occupy_flag` lock, `default_trampoline.cu:149, 167`, stays and is already exercised), CPU-GPU atomic RMW, store buffering, CPU posting into `CommSharedMem`.

**Capability bits** (measured, optional):
- **C-SB** (CPU-GPU store buffering) → READ_LOCAL.
- **C-XA** (cross-context sysmem atomics) → optional shared single-word records.
- **C-PTX** (`shfl`, `match.any`, `redux`, `vote`, `nanosleep` text accepted after the sm_87 retarget, `nv_attach_utils.cpp:52-65`) → faster grouping.
- **C-SCHED** (an agent kernel gets scheduled while application kernels occupy the GPU) → exact flip.

### R3.2 Layout

**Ledger** [new], in the Boost segment.
- Allocated by the agent, as for its alive-pid set (`runtime/agent/agent.cpp:1195-1199`).
- The segment is `cuMemHostRegister`ed by every agent (`bpftime_shm_internal.cpp:1407-1475`), so the loader reads it without the agent.
- Contents:
  - header (CPU-written): fd, `gen`, sizes, kind (UPSERT_ADD with template c, ADD_IF_PRESENT, LWW), add field (aligned, 4 or 8 bytes), C, S, owner pid, mode hint, flip request and ack, statistics;
  - CPU index: 2C entries; slots created under a ledger mutex;
  - per-slot hint;
  - CPU watermarks;
  - absent list;
  - records per (slot, shard): `ver` plus two halves. ADD halves hold {sum, count}; LWW halves add {wseq, wkind, sum_at, count_at, value}.

**Device block** [new], agent `cuMemAlloc` in the primary context (the agent already uses it: the watcher retains it and makes it current, `bpf_attach_ctx_cuda.cpp:262-276`):
- a header: the ledger's segment device addresses (index, hints, records), C and NL;
- `devmode`, set to DEFER when the agent allocates (and zeroes) the block, written later only by the flip kernel (slice 2);
- a lock table of NL = 2C lock words indexed by **hash(key bytes, shard)**;
- shadows per (slot, shard), protected by that lock;
- the key-to-slot cache. It is only a hint: before using a cached slot s the leader checks s < C, the index entry's occupied bit and its key bytes. The host writes an index entry once at slot creation (occupied bit last, after the key) and never changes it while the ledger lives (no reclamation). A torn, empty (all-zero: key 0, slot 0) or stale cache entry therefore costs a miss (one UNIT_ADD), never a wrong slot. Duplicates are harmless because the host assigns one slot per key. Hint encoding: NONE = 0 (never created), LIVE, ABSENT, SYNC, DEAD.

**Reaching the device block.**
- Modules reach it through `map_info[fd].extra_buffer`. The trampoline uses `extra_buffer` only for ARRAY and GPU map types (`default_trampoline.cu:238-272`).
- `map_info` is a load-time snapshot (`default_trampoline.cu:101`; `nv_attach_impl.cpp:2366-2385`) that carries only this stable pointer.
- **Context rule.** `write_trampoline_globals` writes `map_info` in whatever context is current at load, and uses the primary context only when none is (`with_cuda_context`, `nv_attach_impl.cpp:2306-2310`, used at `:2366-2380`). A device block is valid only in the context it was allocated in [external], unlike today's `extra_buffer` values, which are registered segment addresses valid everywhere (`bpf_attach_ctx_cuda.cpp:528, 559-574`). So the agent records each block's context (and its `cuCtxGetId`), marks ledgered entries in `map_basic_info` (a flag in the new MapBasicInfo fields, with `gen`), and every writer of `map_info` goes through one helper that writes `extra_buffer = 0` for marked entries unless the current context is the block's. There are three writers:
- `write_trampoline_globals` (substituted libraries, `nv_attach_impl.cpp:2366-2385`);
- `fatbin_record::try_loading_ptxs`, the late path used after a refresh (`nv_attach_fatbin_record.cpp:251-292`: `cuModuleLoadDataEx` in the current context, then `cuMemcpyHtoD` of `map_basic_info`);
- `run_attach_entry_on_gpu`, which always creates its own context (`nv_attach_impl.cpp:1316-1349`; used by `bpftimetool`), so it always gets 0.

Helper 600 then takes its B == 0 path (UNIT_ADD per group, R3.3). One deferring context per (ledger, agent) also keeps one writer per record. A device block is never re-created for an existing ledger: if the owner's context id changes (`cudaDeviceReset` in a live process), the owner retires the ledger as on death (R3.8).

**One writer per segment word.**
- In the segment, the GPU writes only records, after the CPU zeroes them at slot creation (the device block is GPU-written device memory).
- The CPU writes everything else.
- The map element is written only by host code.

### R3.3 GPU commit (helper 600, dfr_add) [new]

**Grouping is checked at run time, never trusted** (revised after the red team).
- **Why.** The lanes active at one call of helper 600 need not come from one site. LLVM 15's legacy O3, the pipeline `generate_ptx` runs (`vm/llvm-jit/src/llvm_jit_context.cpp:279-288, 946-948`), merges two rewritten units in sibling branches into one call with a `select`ed site or fd (`redteam/sink.ll` → `sink.O3legacy.ll`: 4 calls become 2). Lanes of both sites then reach the call together, and trusting `popc(activemask)` counts every lane on the leader's key. Under independent thread scheduling, lanes from two call sites might also be active together inside the shared helper body (open).
- **`nomerge`.** An llvmbpf overlay hunk marks every call to ids ≥ 600 `nomerge`. With it the same `opt-15 -O3 -enable-new-pm=0` keeps all 4 calls (`redteam/sink-nomerge.ll` → `sink-nomerge.O3legacy.ll`). This keeps sites apart in the common case, so the constant fast path stays fast. On the red team's sample NVPTX code generation keeps the calls apart too (`llc-15 -march=nvptx64 -mcpu=sm_61`, `recheck/out*.ptx`); nvPTXCompiler is untested, so `nomerge` is not the correctness argument.
- **The check (peeling).** Each round, the lowest remaining lane broadcasts (fd, site, desc) and then its key words with `shfl.sync`; `vote.ballot.sync` over equality of all four gives the group G; G's leader commits; the rest repeat. Every loop bound and branch in a round comes from broadcast values only (the number of key words from `map_info[lfd].key_size`, the immediate bit and the add width from the leader's desc), so all lanes of the warp run the same `shfl.sync` sequence even when a merged call brings lanes of different maps, key sizes or delta kinds; a lane whose key is shorter pads it with zeros, and its fd differs anyway (second recheck). This needs only sm_30-level PTX (`shfl.sync`, `vote.sync`, PTX ISA 6.0 [external]), whose acceptance after the sm_87 retarget step 0 checks (C-PTX). `match.any.sync` (sm_70) is an optional faster form.
- **Without C-PTX** (step 0 rejects the text): lane-turn everywhere. Lanes take turns as in today's 32-step loop (`default_trampoline.cu:134-175`), each lane its own group (n = 1) under its key's lock. Still exact and with no round trip; a warp on one key costs 32 × `c_commit` instead of today's 32 × `c_rt` (about 32 × 11 us). `c_commit` is unmeasured, so no number is claimed.
- **What the analysis decides.** Uniformity only sizes ledgers (keybound, R6) and feeds the cost model. When the site's delta is an immediate, agreement on the site implies equal deltas, so the sum is n·d. Otherwise every lane of the warp runs the same gather loop over G's set bits with `shfl.sync` (or one `redux.sync` when accepted). No staging word is needed, so the path without a device block sums the same way (second recheck).
- **Rounds end with a barrier.** Every lane that entered stays in the loop until all groups are done; each round ends with `__syncwarp(m0)`, as `host_call`'s turn loop does (`default_trampoline.cu:126-175`). Only one leader per warp is inside a lock or a host call at a time, so two groups of one warp never spin on each other (same key on two sites, colliding lock entries, the module comm lock), with or without independent thread scheduling.
- No `%warpid` scratch is used (PTX: unique only within a CTA [external]).

```
_bpf_helper_ext_0600(fd, key, delta, desc, site):
  B = map_info[fd].extra_buffer   // 0: no device block (another context, another agent) -> UNIT_ADD per group
  gen = map_info[fd].gen          // new MapBasicInfo field, written with the rest of map_info
  m0 = activemask(); left = m0
  while left:                                       // uniform over m0
    leader = ffs(left) - 1
    (lfd, lsite, ldesc) = shfl.sync(m0, (fd, site, desc), leader)
    kw = key_words(map_info[lfd].key_size)          // from broadcast values only
    lkey = shfl.sync(m0, own key words 0..kw-1 (zero-padded), leader)
    G = vote.ballot.sync(m0, lane in left && (fd, site, desc) == (lfd, lsite, ldesc) && key == lkey)
    n = popc(G)
    sigma = immediate(ldesc) ? n*d(ldesc) : sum over lanes l in G of shfl.sync(m0, delta, l)
    if lane == leader:
      if B == 0: r = host_call(UNIT_ADD, gen, key, sigma, n, ...); if r.addr: atomicAdd(r.addr, sigma)   // r.addr: after a GPU-writer flip only (D5)
      else:
        L = &B.locktab[hash(key, shard) & (NL-1)]; lock(L)
        s = keycache_find(B, key)                   // hint; s < C and key bytes checked at index[s]
        h = s valid ? ld.volatile slot_state[s] : NONE   // hint (H3); NONE = 0
        DEAD -> drop; IF_PRESENT && ABSENT -> no-op
        devmode != DEFER || h != LIVE -> r = host_call(UNIT_ADD, gen, key, sigma, n, ...); if r.addr: atomicAdd(r.addr, sigma)  // L held; cache slot
        else: sh = shadow[s]; v = sh.ver; sh.sum += sigma; sh.count += n
              st rec.ver = v+1; membar.sys
              st rec.half[((v>>1)+1)&1] = {sh.sum, sh.count}; membar.sys
              st rec.ver = v+2; membar.sys; sh.ver = v+2   // commit, fenced before return
        unlock(L)
    left &= ~G
    syncwarp(m0)                                    // one leader at a time
```

Without `shfl.sync`/`vote.ballot.sync` (C-PTX fails), the same loop runs with G = {leader} each round (lane-turn).

**Properties.**
- The payload is cumulative, with no incarnations.
- A writer that dies leaves the committed half intact.
- A fallback holds the lock across its host call, so later groups cannot return before it.
- One leader per warp at a time, so there is no intra-warp spin, also on pre-Volta GPUs [inferred].
- Cost: the check (two `shfl` per 8 key bytes plus one ballot per distinct key in the warp), the gather (≤ n `shfl` for a varying delta), then device CAS + 1 + ⌈key size / 8⌉ uncached loads (the hint, and the key bytes that verify the cached slot) + 4 uncached stores + 3 `membar.sys`, measured in step 0.
- Optional: a single-word `red.add` record for odd constant deltas.

### R3.4 Host side [new]

**Wait-free record read:**
```
v1 = ld_acq(ver); c = v1>>1; (sum,count) = half[c&1]; fence; v2 = ld(ver)
accept if v2 <= 2c+2   // never waits on odd ver
```

**Fold** of k under k's stripe lock (`bpftime_hash_map.hpp:132-134`).
- **Guard:** if the record's `ver` ≤ the watermark's `ver`, nothing moved; stop. A record read older than the watermark (another process folded a newer one under this lock) is therefore harmless, and Δ is never computed from a stale read. The count must never decrease from one accepted read to the next (asserted, counted).
- Δsum and Δcount = record totals − watermarks.
- If Δcount = 0: nothing to do.
- Present key (UPSERT or IF_PRESENT): in-place 4/8-byte CPU atomic add.
- Absent key, UPSERT: insert c ⊕ Δsum (re-creation; sweep rule; E2BIG → drop, counted).
- Absent key, IF_PRESENT: drop.
- LWW SET: `replace_locked` (`:528-538`) or insert. LWW DEL: delete (`:755-778`).
- Watermarks := snapshot, with watermark `ver` = 2c, the snapshot's own commit (never the second `ver` load, which may already be 2c+2 and would make the guard skip commit c+1).
- Count, not sum, decides re-creation.
- The watermark is published with release after the element changed; fast paths load it with acquire.
- Lock-free lookups (`:112-134`) see an add before or after it, and a replaced element whole.
- Pointer holders see in-place adds and lose writes on SET replacement, as in Linux.

**CPU-probe lookups (fast path).**
- If every record's `ver` is ≤ its watermark's, do today's lock-free lookup, then re-check `ver`.
- Otherwise lock, fold, find.

**Loader lookups.** Today the server does `memcpy` after the lookup, outside any lock (`syscall_context.cpp:615-622`). It becomes `map_lookup_copy`:
- with no movement (the same `ver` test), today's lock-free lookup and copy;
- otherwise lock, fold, copy to a private buffer, unlock, then copy to the user buffer, as `elem_delete` does (`bpftime_hash_map.hpp:770-775`).

A tool killed inside a fold leaves the stripe locked, and the watcher's fold-before-serve then hangs every GPU host call of that stripe. Tool writes have this exposure today (`CLAUDE.md`, "Hash Map Concurrency": a process killed inside a writer leaves its stripe locked); folds extend it to tool reads that find movement. Robust locks would remove it for both; not in this design.

**Host writes.** Update, delete and lookup_and_delete run fold-then-op under one stripe lock, through locked variants (`:715-778`), and set the hint (LIVE, or ABSENT plus the absent list).

**Hint writes respect the flipped state** (second recheck). The ledger header holds a FLIPPED flag (the "mode hint"). Every hint writer (slot creation, UNIT_* step 4, host writes, the fold) reads it under the key's stripe lock and, when it is set, stores SYNC instead of LIVE or ABSENT. A flipped ledger stays attached to its map, so stragglers are still folded.

**UNIT_ADD (watcher).**
1. Check `gen`.
2. Sweep if needed, lock, fold.
3. Run the unit: present → add; absent → UPSERT inserts with Linux's E2BIG, IF_PRESENT no-ops. After a flip caused by a GPU pointer writer, a present key is not added on the host: the reply carries the element's device address and the leader adds sigma with a GPU atomic, in the same atomicity domain as that writer's atomics (R3.6). This is Linux per-helper semantics, not SR (D5).
4. Find or create the slot (NOSLOT if full); set the hint (SYNC if flipped); return the slot.

**With retired ledgers** (R3.8): the fold, the fast paths and the sweep cover the current ledger and every retired one; hint writers, slot creation and UNIT_ADD write only the current ledger.

**Stragglers.** Without store buffering, a commit can land after a host operation's read. It is ordered after that operation:
- after an overwrite: added on top;
- after a delete or lookup_and_delete: UPSERT re-creates, IF_PRESENT drops;
- conservation holds: Σ returned + final = total.

No GPU-local read of ADD keys exists, so no observer saw it earlier.

**Sweep.**
- Before every operation that changes the key count (host insert of an absent key, delete, lookup_and_delete, UNIT_* insert, fold re-creation), and only when the free count (kept while a ledger is attached) ≤ absent-list length + 64, fold each absent-list slot whose count moved.
- One stripe lock at a time, never two. A fold needing re-creation releases its lock, sweeps, and retries.
- Pending GPU DELs are folded before an insert fails with E2BIG (O(C), only on that path).

### R3.5 Hooked entry points

A ledger-pointer test (one load) sits in each of these:
- **HASH branches** of `map_lookup_elem`, `map_update_elem` and `map_delete_elem` (`runtime/src/handler/map_handler.cpp:135, 441, 773`) and their counted wrappers (`:1906, 1914, 1924`).
  - Callers: the loader through `bpftime_shm` (`bpftime_shm_internal.cpp:290-418`), generic CPU helpers (`bpf_helper.cpp:401-422`), and the watcher (`bpf_attach_ctx_cuda.cpp:136, 152, 159`).
- **Typed fast paths** `map_lookup_hash`, `map_update_hash` and `map_delete_hash` (`map_handler.cpp:302, 336, 355`).
  - They are reached by `bpftime_hash_map_*_elem_helper` (`bpf_helper.cpp:470-492, 574-595, 600-621`), which `bpf_attach_ctx.cpp:116-170, 499-555` selects for CPU programs with HASH maps.
  - With a ledger they take the generic path.
- **Other entries:** `map_lookup_and_delete_elem` (`:1797`) and `bpf_map_get_next_key` (`:617`).
- **Diagnostic modes:** `diagnostic_direct_*` (`:391-440`) and the cached-handler branches (`bpftime_shm_internal.cpp:295-315`) route ledgered maps to the generic paths.
- **Loader lookup:** becomes `map_lookup_copy`.
- **Close** (`syscall_context.cpp:273-296` → `close_fd` `bpftime_shm_internal.cpp:852` → `map_free`, `handler_manager.cpp:116-123`): marks the ledger DEAD.

**Not in `bpftime_hash_map`**, which is also per-CPU hash's inner table.

### R3.6 Pointer writers and flip [new]

**Scan before any link.** The scan runs inside `init_attach_ctx_from_handlers`'s attempt loop, after the session-epoch check and immediately before loop 1 (`bpf_attach_ctx.cpp:200-282`, loop 1 `:229-245`), on every attempt; the map-binding mutex held for the whole transaction (`:196-199`) keeps handlers from appearing in between. Loop 1 already attaches CPU links (`instantiate_bpf_link_handler_at`, non-CUDA links at `:656-686`; only CUDA links wait for loop 2). It reads every program handler in the segment and sets two flags:
- `gpu_ptr_writer(M)`: any GPU site **not rewritten** stores, or runs any atomic, through a lookup pointer of M;
- `cpu_ptr_writer(M)`: any CPU program does a store, or an atomic other than ADD, through such a pointer.

A lookup pointer (or one derived from it) passed to a helper that writes through a pointer argument counts as a store: on the CPU `bpf_probe_read*`, `bpf_get_current_comm` and the like; on the GPU helpers 503, 504, 505 and 508, which store through their three arguments (`default_trampoline.cu:511-517, 520-528, 530-539, 552-561`). The scan reads a table of helper argument effects, checked against the trampoline source by a unit test.

Either flag means M gets no ledger. CPU atomic ADD is allowed only next to add-only ledgers.

**Registration across agents** (second recheck). Rewriting is decided per agent (`BPFTIME_GPU_SITE_EXEC`, the start-up gate), so "not rewritten" differs between agents. Every agent of this build, whatever its own setting, registers in the segment each unrewritten GPU site that stores or does atomics through a lookup pointer of M before loop 2 attaches it: a per-map writer count under the ledger mutex, each entry naming the registering agent's own liveness lock, a per-agent `flock` file with the owner lock's rules (R3.8), so a dead agent's entries are dropped. A ledger is created only while the count is zero; registering into a ledgered map flips it first. `bpftimetool run` (`tools/bpftimetool/main.cpp:65-117`), which executes segment programs without `init_attach_ctx_from_handlers`, runs the scan and flip first. The scan reads only handlers in the segment and needs no CUDA context, so every agent of this build runs it, with or without a GPU (mixed builds in one segment are unsupported anyway, `CLAUDE.md`, "Ordinary Array JIT Specialization").

**Late pointer writers are the normal case, not an exception.** Links are re-made while the process runs: `refresh_attach_session` (`runtime/agent/agent.cpp:702-730`, reached from the agent socket and `bpftime_agent_control`) destroys every link and runs `init_attach_ctx_from_handlers` again; the auto-refresh threads call `init_attach_ctx_from_handlers` directly, without destroying links (`agent.cpp:777, 1183`); and another process's agent can start at any time. So "no ledger whenever a late attach is possible" would mean no ledger at all. Instead, at every such entry, before loop 1, whoever finds a new pointer writer of a ledgered map M:
1. sets FLIPPED in M's ledger header under the ledger mutex, then stores SYNC into every slot hint of M. The GPU loads the hint at every commit (R3.3), so from then on new sections send UNIT_* host operations; no kernel of the owner is needed. Every later hint writer keeps SYNC (R3.4), so deferral does not resume;
2. folds every slot of M, re-storing SYNC under each stripe lock (this closes a hint writer's check-then-store race). The stripe locks are taken with bounded trylocks: the flip runs under the map-binding mutex, and a process killed inside a fold would otherwise wedge every agent's init and every tool's handler change. On a timeout FLIPPED and the SYNC hints stay, the fold is retried at the next entry, and the new program is not attached until it has completed;
3. only then runs loop 1, which attaches the new program. The agent's statistics record a sequence number for the FLIPPED store and for each pointer writer's attach (T22 checks their order).

Sections that loaded LIVE before step 1 can still commit (D5; `late_stragglers` counts commits whose `ver` moved after step 2's fold):
- **CPU pointer writer:** stragglers are folded in place with an aligned 4/8-byte CPU atomic add, single-copy atomic against the uprobe's aligned plain stores and atomics, so nothing tears. A straggler that happened before the writer's store but is folded after it lands on top of the store (D5).
- **GPU pointer writer with atomics:** an in-place CPU add would meet the GPU's atomics on the same word, the CPU-GPU atomicity the design otherwise never needs, so stragglers are folded by replacement (D5). From the flip on, M runs with Linux per-helper semantics for every rewritten unit: UNIT_ADD on a present key returns the element's device address and the leader adds sigma with a GPU atomic, in the same domain as the writer's atomics (for a leader in another context this needs C-XA, as today's GPU atomics through host-call pointers do). The result is today's behaviour, not SR, and is counted (`post_flip_units`).

**Slice 2: the flip kernel** removes D5. The owner launches a one-block kernel on a private stream: `devmode = SYNC`, `__threadfence()`, then acquire and release every lock-table entry. Every section that read DEFER has then finished, and later sections see SYNC (H1, H5). The watcher must never block on that kernel: UNIT_* holds a lock-table entry across a round trip only the watcher serves (`bpf_attach_ctx_cuda.cpp:303-330`), so a watcher waiting on the flip kernel while the flip kernel spins on that entry is a deadlock. The watcher polls the kernel's event with `cuEventQuery` between `try_serve` calls. Test: a UNIT_ADD first touch in flight during the flip; mutation: the watcher blocks on the event. The only precedent for an agent-launched kernel, `run_attach_entry_on_gpu` (`nv_attach_impl.cpp:1190`, launch `:1360`, sync `:1364`), uses `cuCtxSynchronize`; whether such a kernel gets scheduled while application kernels hold every SM is C-SCHED, measured in step 0.

**Not flipped.** Loader events (close, freeze, writes) never rely on the flip. A late *recognised* unit joins the ledger if its kind matches; otherwise it gets a host-only descriptor bit and always uses UNIT_*.

### R3.7 GPU-local reads need store buffering

An epoch-validated GPU read (analysis-first's `res_lookup`) admits a cycle under message passing alone:
1. A straggler SET reads the old epoch and lands after a host delete's read, so it is ordered after the delete.
2. A GPU read sees that SET with the old epoch on both sides, so it is ordered before the delete.
3. SET ≺ read ≺ delete ≺ SET.

Only C-SB excludes the cycle.

**Default:** `resident_read` sites use LOOKUP_COPY: fold, then copy into a stack slot, one request per group.

**With C-SB:** READ_LOCAL:
- The host stores an odd epoch, fences, reads records and waits for odd `ver` words to turn even. The wait is bounded by one GPU section; a liveness check covers dead owners.
- A GPU writer stores odd `ver`, executes `membar.sys`, and reads the epoch; it aborts if the epoch is odd.
- A read is valid if epoch-before = epoch-after = write epoch, all even.
- Disabled under memcapture, because that pass moves `r10` by the captured line's rounded length (`ptxpass_kprobe_memcapture/main.cpp:63-127`).

### R3.8 Identity, close, takeover, agents

- **`gen`** [new]: a 64-bit segment counter in the handler. Today `id` is the fd (`map_handler.hpp:89-102, 370`). The ledger header holds it whole; `map_info` and UNIT_* requests carry its low 32 bits; a mismatch drops the request.
- **Close:** sets DEAD and detaches. Ledger and device block stay until the agent exits, because patched images cannot be unloaded (`nv_attach_impl.cpp:2387-2390`).
- **fd reuse:** old rewritten sites see DEAD or a `gen` mismatch. Old unrewritten sites keep the pre-existing hazard.
- **Maps missing from `map_info`** (created later, or fd ≥ 1024) are never rewritten.
- **Owner liveness is a lock the kernel releases** (revised twice). The owner opens a per-ledger file in the segment's directory (`/dev/shm`, named by segment and ledger incarnation id) with `O_CLOEXEC` and holds a `flock` on it for its lifetime; it creates and locks the file before the CAS that publishes the ledger. Another agent declares the owner dead only when it can take that lock (non-blocking), opening the file without `O_CREAT`: a missing file means "not provably dead", so no retire follows. Only the retirer unlinks an old file, after its CAS; removing the segment unlinks the rest, and the measurement clean-up and checks include them. This works across pid namespaces that share `/dev/shm` and across `exec` (the descriptor closes, and the old image's CUDA context is gone with it). A child forked by the owner inherits the lock until it exits or closes the descriptor; deferral for other agents stays off meanwhile, which costs speed, not exactness. The header also records (pid, start time, incarnation id) for diagnostics and for a refresh in the same process to find its own ledger.
- The alive-agent set is not used: a pid enters it only after init (`agent.cpp:1119-1121` then `:1195-1196`; `:726` then `:733` on refresh), leaves it at detach while the owner's GPU work can still run (`:463-473`; the last detach waits at most 2 s and only for noted launches, `nv_attach_impl.cpp:160-167`), stays after SIGKILL, and pids are reused. (pid, start time) read from `/proc` is not enough either: from another pid namespace a live owner looks dead.
- **Ownership ends only at process exit or exec.** The process's CUDA context is torn down then too, but not in a known order with the lock's release (next bullet), so a kernel of the old owner may still commit for a while; retired ledgers stay folded for that reason. Slice 2's takeover seeding assumes the old writer is gone once the lock is free: that premise is open. Detach does not release ownership; a refresh in the same process rejoins its own device block (matched by incarnation id). A changed context id in the owner itself (`cudaDeviceReset`) is handled like death: the owner retires the ledger.
- **Slice 1, owner dead:** the agent that takes the lock folds every slot and installs a new ledger with a CAS on the handler's ledger pointer (no shadow seeding); an agent that loses the CAS uses UNIT_* through the host-only bit. The old ledger is *retired*, not DEAD: its hints and records stay as they are, and it stays on the handler's list of retired ledgers, folded by every reader, until the map closes. The release of the `flock` is not ordered with the teardown of the owner's CUDA context (an exiting process closes its descriptors in no fixed order, and an application may close the descriptor itself), so a kernel of the old owner may still commit after the retire: those commits are folded, not lost. A reader checks the key's record in each retired ledger (one index probe and one `ver` load each); reads of a key whose records move in two ledgers at once are regular, not linearizable (D1), until the old owner's work has ended. After 4 retirements a map gets no new ledger (UNIT_* only: speed, not exactness). DEAD is used only at close.
- **Slice 2, takeover** [new]: instead of retiring, an agent takes ownership with a CPU CAS after confirming death.
  - It seeds its shadows from each record's committed half (`ver` = 2c, totals from half c&1) via `cuMemcpyHtoD` before any commit.
  - If the dead owner left `ver` odd, the new first commit stores 2c+1, then 2c+2; `ver` stays monotone.
  - Watermarks persist, so the restarted application's counts continue exactly.
- **Several agents:** in strict mode, other agents use UNIT_* (host-only bit); opt-in per-agent records give D1.
- **Pre-existing:** one `CommSharedMem`, zeroed by each agent (`bpf_attach_ctx_cuda.cpp:615`; `CLAUDE.md:2316-2319`). A per-agent request slot is a prerequisite for multi-agent use.

### R3.9 The listed facts, answered

| fact | answer |
|---|---|
| loader reads run in another process | the fold runs in the reading process on segment words |
| the request slot is unsafe for CPU posters | the CPU only loads GPU-written words |
| hash pointers are unstable | keys by bytes and slot; in-place add or explicit replace/insert under the stripe lock; loader copies under the lock |
| close and fd reuse | `gen`, DEAD |
| `map_info` is a snapshot | it holds only a stable pointer |
| CPU probes touch the same maps | they use the same hooks; pointer writers are classified |

## R4. Idioms as units

### Recognition

**The analysis.** Abstract interpretation over the stored words. It ports PR #624's worklist (registers, pointer provenance, map fds, stack bytes) without PREVAIL; `ENABLE_EBPF_VERIFIER` is off in dev (`cmake/StandardSettings.cmake:105`). Each value carries:
- **kind:** constant, map fd, stack pointer, map-value pointer, or scalar;
- **scope:** CONST ⊑ PROCESS ⊑ GRID ⊑ BLOCK ⊑ WARP ⊑ THREAD, with a timing flag (502) and a position flag (509/510). Joins are control-dependent;
- **value number.**

**Helper sources:**
- 14 → PROCESS: the module global `(tgid<<32)|tgid` (`default_trampoline.cu:410-422`; `nv_attach_utils.cpp:155-159`);
- 503 → BLOCK;
- 504/508 → GRID;
- 505/511 → THREAD.

**A unit is accepted when all of these hold:**
- **K** (same key);
- **P** (pointer used only by the null test, the field load/store or add, or as the value pointer of its own EXIST update);
- **V** (add-family, with the inserted value = d + c0 proven by value numbers);
- **L** (everything the unit defines is dead at exit);
- **C** (single entry; no foreign call or atomic).

Merged calls are split per incoming flags edge (cuda-counter retprobe pc 76 → lines 76 and 79).

**Rewrite (slice 1)** happens in place: argument setup, `call 60x`, `ja` join, with the rest of the region padded by `ja +0`. No jump fixups are needed.
- **cuda-counter entry A:** pc 22-37 → pc 26-30 `r3=r6; r4=desc; r5=site; call 600; ja 38`, then pc 31-37 nops. pc 22-25 keep the key pointer and the map LDDW.
- **retprobe B:** pc 28-35, plus nops at 36-42 and 69-76. The printk path 43-68 is reached only from pc 16; r1-r5 are redefined at 105-110; slot `r10-0x28` is kept and read at 107 (`read/disasm/cuda-counter.txt`).
- **micro hash_lookup** (`benchmark/gpu/micro/cuda_probe.bpf.c:167-176`): a built object exists, and the unit fits in its 5 slots (pc 70-74) using 4 (`r3`, a one-slot descriptor, `r5`, `call 600`) and falling through to pc 75 (feasibility red team and second recheck, from its disassembly). A descriptor wider than one slot would not fit, here or in A's rewrite.

Non-fitting units stay unchanged. The general patcher arrives in slice 2. A structural self-check runs after rewriting, and T14 does differential validation.

### Shapes

| shape | sites | unit | helper |
|---|---:|---|---|
| A counter (cuda-counter entry @26-37) | 10 | UPSERT_ADD (d = r6, c = 0) | 600 |
| B RMW + EXIST self-write / NOEXIST, merged call @76 | 9 | UPSERT_ADD (d = r7) | 600 |
| C cudagraph `v = p?*p+1:1; update ANY` (`example/gpu/cudagraph/cuda_probe.bpf.c:55-58`) | 4 | UPSERT_ADD | 600 |
| E start_ts SET / READ (@19, @105) / DEL | 9 | SET; READ; DEL | 601; 603; 602 |
| F thread_mapping per-thread SET | 2 | SET | 601 |
| G leader publish | 1 | SET | 601 |
| H printk (constant format, `%lu`/`%u` only) | 21 | OUT record, format interned | 604 |
| J micro update / lookup + `(*v)++` / delete (`cuda_probe.bpf.c:158-184`) | 3 | SET / ADD_IF_PRESENT / DEL | 601 / 600 / 602 |

**A.**
- Today: 32 misses race and 31 inserts are lost (97/128).
- UnitFold: the pid key is the same in every lane, so the run-time check finds one group per warp and Σ = n·d with n = popc(G). The first touch is one UNIT_ADD, then records. Result: 128/128.
- The first-touch race does not exist, because presence is decided by a host operation under the stripe lock.

**B.** Linux loses updates through the racy RMW and through EXIST replacement (`kernel/bpf/hashtab.c:1171-1185`); today about one update per warp lands; UnitFold gives Σd through the `shfl.sync` gather sum.

**C.** One lane per launch (`cudagraph/cuda_probe.bpf.c:44-47`). The gain is the removed round trip, and the result is exact across overlapping launches.

**E.**
- **SET:** the highest lane of the group commits its own value; last writer wins.
- **READ:** the pointer is only loaded from, including after the delete @27. It is served by LOOKUP_COPY or READ_LOCAL into a stack slot below the deepest access (the frame has 64·frames+10 slots, `vm/compat/llvm-vm/local-call-return-block.patch:22-30`).
- **DEL:** deletes `&pid`, a key the entry never wrote. The first UNIT_DEL marks the slot ABSENT; later DELs are free.

**H.** Lines are never merged, unless C-PTX shows identical lanes, in which case one record carries a count. Arguments stay in r3-r5 (`default_trampoline.cu:386-408`).

**J.** Nothing inserts key 0 (the loader only selects programs, `benchmark/gpu/micro/cuda_probe.c:100-107`). One UNIT_ADD marks ABSENT, then each unit is a hint load and a no-op.

**Kind consistency.** A ledger has one add kind. SET and DEL share a ledger with adds of one agent (`sum_at` and `count_at` order them). Conflicting units get the host-only bit.

## R5. Soundness direction

**Original words are kept when:**
- the unit fails K, P, V, L or C, or the map fd is not constant;
- the map is not HASH (1501 counts as HASH on the Orin), or is missing from `map_info`;
- a pointer-writer flag is set, or the budget is exhausted;
- `run_with_kernel` or a counting build (precedent `bpf_attach_ctx_cuda.cpp:478-484`) is in use;
- the rewrite does not fit or fails the self-check;
- the pass lacks `ptxpass_site_exec_abi`;
- `BPFTIME_GPU_SITE_EXEC` is `off` (the default) or `analyze`.

**Dynamic fallbacks** run the unit's semantics in one host operation per group (UNIT_*), on any of:
- no device block, `devmode` SYNC, a hint that is not LIVE (UPSERT);
- no cached slot, NOSLOT;
- a full printk ring (synchronous printk after a drain).

DEAD and `gen` mismatches drop; IF_PRESENT with ABSENT is a no-op.

**The synchronous path stays** (`default_trampoline.cu:134-175`). Its waiting rules change in four ways:
1. Requests on ledgered maps fold first. The thread's own commits precede them through the trailing `membar.sys` plus the `membar.sys` before `flag1` (`:157-158`).
2. UNIT_* holds the key's device lock during its round trip.
3. A synchronous printk drains the ring first (slice 3).
4. Optional warp-batched requests (later): one round trip per warp, lane order kept.

**Unregistered ids** would `exit()` the application through `generate_ptx`'s static `ExitOnError` (`vm/llvm-jit/src/llvm_jit_context.cpp:907, 936-939`; "Ext func not found" `compiler_utils.cpp:366-369`). Prevention:
- ids 600-604 are registered in `core.cpp:234-253`, which memcapture also uses (`ptxpass_kprobe_memcapture/main.cpp:134-137`);
- a containment unit test over one shared id header;
- the `dlsym` capability gate;
- an overlay hunk replacing the static ExitOnError (overlay mechanism: `vm/compat/llvm-vm/CMakeLists.txt:16-34`).

The names `_bpf_helper_ext_06xx` pass the extern filter (`core.cpp:280-283`).

**Uniformity** only sizes ledgers and feeds the cost model; grouping is always checked at run time (R3.3).

## R6. Storage

**Sizing.**
- C = next_pow2(min(`max_entries`, keybound)), clamped to [64, 65,536].
- keybound: 1 for CONST or PROCESS keys; `num_SM` for `%smid`; SM × warps for (smid, warpid); otherwise `max_entries`.
- S = 1 in strict mode.

| structure | where | size | when full | drained by |
|---|---|---|---|---|
| index + header | segment | 256 B + 2C(ks8 + 8) | NOSLOT → UNIT_* (exact) | - |
| hints + watermarks | segment | C(8 + 24·S·A) | - | folds |
| records | segment | ADD: 64 B per (slot, shard); LWW: round64(8 + 2(48 + vs8)) | - | nobody needs to; readers fold; optional 100 ms watcher fold |
| device block | device memory | header + NL·4 + C·S·24 + cache 2C(ks8 + 8) | cache full → UNIT_* | seeded at takeover |
| printk ring (slice 3) | segment, per agent | 32 Ki × 48 B = 1.5 MiB | CAS reservation checks head → synchronous printk after drain | watcher every turn, before any sync printk, at exit (≤ 100 ms) [new]; loader orphan drain if the agent is dead |

**Examples.**
- `call_count`: C = 64, ~10 KiB.
- `sm_histogram`: C = 64.
- `thread_mapping`: ≈ 1.25 MiB of records.

**Budget.** 16 MiB (`BPFTIME_GPU_LEDGER_BUDGET_MB`) of the 128 MiB segment (`bpftime_config.hpp:78-81`), which also holds the maps and `CommSharedMem`'s two 1 MiB buffers (`gpu_helper_protocol.h:12-16`). Overflow → maps ranked by saving, the rest stay synchronous.

**Lifecycle.**
- No slot reclamation yet; it would need a per-slot lock sweep.
- Aggregates need no buffer of their own.
- Nothing is sized by the grid.
- Committed effects survive an exit or SIGKILL in the segment.

## R7. The analyzer

**Where.**
- In the agent's global step, at the start of `init_attach_ctx_from_handlers` (before loop 1, which attaches CPU links), over all handlers.
- Per link at `bpf_attach_ctx.cpp:629-648`: rewritten words at `:639`; device-block addresses through `create_map_basic_info(1024)` at `:643-644`.
- Late links: every entry into `init_attach_ctx_from_handlers` re-runs the step (refresh, auto-refresh, another agent's start; R3.6).
- Offline: `bpftime-site-analyze` over `.bpf.o` with `.BTF.ext`.
- Device blocks are allocated under a `with_cuda_context`-like helper (`nv_attach_impl.cpp:2306`, used at `:2370`) with the primary context [inferred]. With no `cuda_ctx` (`bpf_attach_ctx.cpp:623-628`) there is no ledger.
- Cost: no usable anchor. The auto-warp branch's 2.2-2.5 ms for 4,096 instructions (`device-verifier-phase-attribution-20260904.md`) is the uniformity phase alone, on an x86 host pinned to one CPU, on synthetic programs with no maps and no helper calls. This analyzer also tracks stack bytes, value numbers and def-use on the Orin's cores. It runs at every entry into `init_attach_ctx_from_handlers`, which auto-refresh makes every `auto_refresh_ms` (`agent.cpp:1178-1184`), so results are cached per program handler by an instruction hash, and only the cross-program flags are recomputed when the handler set changed. Measured in slice 1.

**Inputs:**
- program words;
- handlers: type, sizes, `max_entries`, `gen`, ledger, owner;
- attach types;
- `MapBasicInfo`;
- start-up parameters;
- config: `BPFTIME_GPU_SITE_EXEC`, strict or opt-in, printk opt-out, overrides.

**Passes:**
1. CFG.
2. Interpretation.
3. Def-use and liveness.
4. Unit matching.
5. Decision: `defer_unit`, `defer_set`, `defer_del`, `resident_read`, `defer_out`, `unchanged` or `sync`.
6. Dry-run rewrite and self-check, deciding which units are actually rewritten.
7. Global step over pass 6: pointer-writer flags (unrewritten sites count), kinds, budget, ledgers.

The words do not depend on ledger addresses.

**Getting the decision to code generation:**

| obstacle | how it is handled |
|---|---|
| no hook in `generate_ptx` (one `optimizeModule`, `llvm_jit_context.cpp:903-950`) | the decision is in the bytecode |
| IR has no pc | descriptor immediates |
| O3 merging (shown with `opt-15`, `redteam/`) | `nomerge` on calls to ids ≥ 600 (overlay hunk; checked at IR level); correctness from the run-time agreement check (R3.3) |
| memcapture prepends and recompiles (`main.cpp:53-137`) | pc-free; READ_LOCAL off there |
| patch cache keyed by the sha256 of the request including the words (`nv_attach_impl.cpp:994-1027`) | rewritten words get their own entries |
| stale passes | `ptxpass_site_exec_abi` via `dlsym` next to `print_config`/`process_input` (`nv_attach_impl.cpp:601-614`), plus a `site_exec_abi_for(attach_type, func)` query [new] |

**Records.** Survey fields plus `decision`, `exec_mode`, `fallback_reason`, `unit_id`, `site_id`, `pc`, `grouping`, `slice`.
- One record per logical site.
- `cpu_reads_map` is an external input.
- `aggregatable` is three-valued: no (lane-varying), timing (`%globaltimer`), yes.
- `exec_mode` is what the runtime runs (`scripts/exec_mode.py`): GPU records after the first touch; printk ring record; one round trip per group; today's synchronous call. Per configuration: full design default, with C-SB, slice 1.
- Class is derived: unchanged → gpu_local_already; cpu_reads_map = no → eliminate; resident_read with yes → sync; otherwise defer. It is a function of the survey's own `cpu_reads_map`, so the 20/39/0 tally holds by construction and verifies nothing about execution.
- Row filter: 180 deduplicated bpftime records = 109 + 20 + 39 + 12 `cpu_side`; the 12 are dropped, leaving 168 GPU sites.

**Check.** Headline: per-record `decision` and `exec_mode` against (operation, result_use). Then `key_scope` for the 38 map sites and aggregatable record by record. The class tally is reported, not used as a check.

**Simulation now** (hand provenance table, not the analyzer):
- decisions: `defer_unit` 24, `defer_out` 21, `defer_set` 7, `defer_del` 4, `resident_read` 3, with **0 mismatches**;
- `exec_mode`, full design default: 35 GPU records after the first touch, 21 printk ring records, 3 one round trip per group, 0 synchronous per lane; with C-SB the 3 become GPU reads. Survey "eliminate" (20) = 17 GPU records + 3 round trips; survey "defer" (39) = 18 GPU records + 21 ring records (`exec-mode.txt`);
- class 20/39/0 plus 109, 0 mismatches (by construction, above);
- `key_scope` 38/38;
- aggregatable 47 yes / 8 timing / 4 no; yes and no agree with the survey. The timing records are cuda-counter:55/82, rocm-counter:56/83, launchlate:114/117 and launchlate-kernel-gpu-shared-map:119/122.
  - The survey's 51 is **not reproduced**: 51 = 47 + the cuda-counter and rocm-counter four. Its stance on `%globaltimer` is inconsistent (cuda-counter:55 and launchlate:114 are both printk calls with `%globaltimer` arguments, labelled differently), and no consistent rule gives 51 (47, 47, 55). The pushed survey (`callsite-survey-20261113`) keeps its 51 until it is corrected.
- Slice 1 rewrites 20 sites (A 10, B 9, J 1); 14 of them can run on the Orin (rocm-counter's 6 have no device attach here), 10 are measured.

**Real check (slice 1).** The library offline, rocm-counter built with `clang -target bpf -g`, per-edge records; M15 mutates the check itself.

## R8. Discrete GPUs

**Start-up parameters** (cached per device, driver and build):
- `c_rt`;
- `c_commit(k)` at k = 1, 32, 3,136 and 31,264, on one slot and on distinct slots;
- `c_ld_sys`, `c_st_sys`, `c_fence_sys`, `c_atom_dev`, `c_hld`;
- SM count and shape;
- H2-H4 litmus results, capability bits.

**Cost model.**
- Sync: g·`c_rt`, serialized GPU-wide.
- Deferred: ≈ `c_atom_dev` + `c_ld_sys` + 4·`c_st_sys` + 3·`c_fence_sys`; W warps on one slot cost about W/S · T.
- First touch: one `c_rt` per key and agent.
- Readers: S·A·`c_hld` per read.
- Defer when feasible and cheaper. For hot keys, report serialization and suggest opt-in shards (D1); never weaken semantics silently.

**What changes on a discrete GPU.**
- **Placement:** records, index, hints and ring stay in pinned host memory (the registered segment, `bpftime_shm_internal.cpp:1407-1475`). The loader still folds without the agent, and SIGKILL loses nothing committed. The device block stays local. No device-home ledger is offered.
- **Costs:** commits become posted PCIe stores plus fences; hint loads become PCIe reads (optional; drop them except for the ABSENT cache); `c_rt` grows.
- **1501 on CUDA-IPC GPUs** (`nv_gpu_shared_hash_map.cpp:47-62`) is not ledgered until checked.
- **sm_61:** argued, not tested.
- **Primitives:** those the trampoline already emits (`activemask`, `bar.warp.sync`), plus `shfl.sync` and `vote.ballot.sync`, gated by step 0; it is built for sm_61 (`trampoline/Makefile:3, 26`) and retargeted.

## R9. Interactions

- **Several agents:** strict ownership; other agents use UNIT_*; flip via the segment. The shared, zeroed `CommSharedMem` needs a per-agent slot first.
- **CPU probes:** fold on every path, typed ones included; atomic ADD through pointers only next to add-only ledgers; other pointer writes keep M out of the ledger.
- **Graph and CUDA 13 launches** (`probed_launch_activity.cpp:16-24`): irrelevant for correctness. The flip needs no application quiescence: slice 1 stores hints, slice 2 adds the agent's own kernel.
- **Close and fd reuse:** `gen`, DEAD.
- **printk:** per-thread order; reservation order across threads; drained before sync lines; ≤ 1 turn late; no loss.
- **Several probes per kernel:** shared helpers; agent-unique site ids; the pre-existing `__probe_func__<kernel>` clash (`ptxpass_kprobe_entry/main.cpp:53`) is unchanged.
- **Return probes and divergence** (`ptxpass_kretprobe/main.cpp:50-61`; the call is inserted before every `ret` and exit, predicated ones included, so lanes reach the probe from different places): groups are formed by the run-time check over the lanes `activemask` reports.
- **Mixed deferred and sync sites on one map:** sync requests fold first.
- **Detach:** the last detach waits ≤ 2 s and switches probes off (`nv_attach_impl.cpp:160-170`). Ledgers outlive it.
- **SIGKILL mid-section:** the committed half stays valid.
- **Not ledgered:** `run_with_kernel`, 1504, per-CPU, LRU, arrays.

## R10. First slice

### Step 0 (≈ 1 week, standalone; dev unchanged)

1. **Litmus** against a reader in another process mapping a segment registered as bpftime does, with the design's fences, 10^7 iterations:
   - H2, H3;
   - H4 torn reads with 1,024 warps;
   - H6, the three-party shapes: GPU → CPU A → CPU B (B in another process, synchronised with A through a segment spinlock, then reading the record), and CPU A → CPU B → GPU (a hint passed on through the request protocol);
   - C-SB and C-XA reported only;
   - fence-free variants reported, not required to fail.
2. **PTX acceptance** after the sm_87 retarget: `shfl.sync`, `vote.ballot.sync` (needed by the grouping check), `match.any.sync`, `redux.sync`, `nanosleep`, `red.global.add.u64`.
3. **Merging below the IR:** two rewritten units in sibling branches through the real `generate_ptx` path with `nomerge`, then nvPTXCompiler; count the calls in the PTX and the SASS.
4. **Calibration:**
   - a `%globaltimer` breakdown of `host_call` (lock, request, flag1, flag2, release);
   - `c_commit(k)` on one slot and on distinct slots, with the grouping check and in lane-turn;
   - `membar.sys`, `c_ld_sys`;
   - C-SCHED: latency of an agent kernel while a saturating kernel runs (for slice 2's flip kernel).
5. **Gate:** H2-H4 must pass, otherwise only the analyzer and the R7 check ship. Without `shfl.sync`/`vote.ballot.sync`, slice 1 runs lane-turn everywhere. Without H6, (rt) is stated per pair of parties.

### Build (the ≈ 4-week estimate predates the recheck additions; not re-estimated)

Cut after the feasibility red team: slice 1 builds only what its programs exercise, plus the rules that keep it exact when something unexpected happens (a late pointer writer, a dead owner, another context).
1. The analyzer library and offline driver: it decides every idiom (A-J: C, E's SET/READ/DEL, F, G, H too) for the records and T17, which asserts the full-design default `exec_mode`; it rewrites only A, B and the J lookup. Also the pointer-writer scan and writer registration (every entry into init, inside the attempt loop before loop 1), analysis results cached per program handler, the in-place rewrite with self-check.
2. Helper 600: the grouping check (peeling, every bound from broadcast values) or lane-turn, the n·d or `shfl.sync` gather sum, record commit, UNIT_ADD with the device-address reply after a GPU-writer flip. `nomerge` overlay hunk.
3. ADD ledgers in strict mode; device block with the hashed lock table and the verified key-to-slot cache; `extra_buffer = 0` in other contexts.
4. Host: all R3.5 hooks, typed paths included; the fold with its `ver` guard; fast paths for CPU probes and the loader; `map_lookup_copy`; `gen`, DEAD; the UNIT_ADD opcode; fold-before-serve; the absent-list sweep (in `get_next_key` from the first or a missing key, and before count-changing host operations near capacity): a tool can delete ledgered keys at any time (T4, T11 and T12 do), and without the sweep a pending re-creation would be invisible to iteration. A free counter is kept only while a ledger is attached.
5. The flip before loop 1 (FLIPPED flag, hints to SYNC, the fold with bounded trylocks, statistics sequence numbers), for CPU writers (in-place straggler folds) and GPU writers (replacement folds, then per-helper UNIT_ADD); D5 counted. `bpftimetool run` runs the scan and flip first. Owner liveness by `flock` (file created and locked before the ledger is published); on a dead owner, fold all, retire (the retired list stays folded), install a new ledger by CAS.
6. Registration of 600, the `ptxpass_site_exec_abi` gate, the containment test, the ExitOnError hunk, `BPFTIME_GPU_SITE_EXEC`, and the start-up gate: deferral only with a step-0 result for this device, driver and build (cached file; a mismatch leaves every site on today's path).
7. **Measurement plumbing** (none of it exists at 52126f33):
   - micro modes `hash-lookup-present` (the loader inserts key 0, so the lookup + `(*v)++` unit commits) and `hash-counter` (a new program: unit A on the pid key), with config entries in `micro_vec_add_config.json`, where the CPU Hash cases are;
   - an exit-time readout in the micro loader (key 0, the pid key), which today only selects programs and sleeps (`benchmark/gpu/micro/cuda_probe.c:52-127, 179-181`), and in the cuda-counter loader (`total_time_ns`; today it reads only `call_count`, `example/gpu/cuda-counter/cuda_probe.c:37`), plus a sum over the printk durations;
   - a warp total in threadscheduling's loader (today the first 20 warp entries only), and a runner config entry for threadscheduling (`examples_vec_add_config.json:22-62` has none); its dynamic-hook copy probes another kernel symbol (`kprobe/_Z9vectorAddPKfS0_Pfi`) and is left out of the measured set;
   - the statistics dump (commits, UNIT_*, `late_stragglers`, drops);
   - presets with few iterations for 1,024 and 100,352 threads, or a larger victim timeout: today's run would take about 113 s and 1,070 s against the runner's 120 s kill (`run_cuda_bench.py:229-235`);
   - a CPU-side harness: a uprobe lookup loop and a loader lookup loop on a HASH map that a GPU program in the same segment has ledgered, each with and without the ledger;
   - the T15 witness ring in test builds, the oracle for the totals.

**Not in slice 1:** SET, DEL, LOOKUP_COPY, READ_LOCAL, printk, C, the flip kernel, takeover seeding, `match.any`/`redux`, shards, multi-agent records, batching, tier-1, discrete GPUs.

### Expected numbers [inferred]

Anchors: probed floor ≈ 48 us; ≈ 11 us per host call (aa6eb973: (2,877.5 − 48) / 256). D (the 52126f33 runtime) and N (slice 1) run the same new benchmark binaries and configs, built once; D is re-measured. `c_commit` comes from step 0.

| case | today | slice 1 | check |
|---|---|---|---|
| micro CPU Hash lookup (key 0 never present: the absent no-op only, no commit, no fold) | 341.1 us | ≈ 48-50 us after the first launch, plus one UNIT_ADD | 0 requests after the first; reported apart |
| new mode: hash-lookup-present | ≈ 330-340 us | ≈ 48 + `c_commit(1)` | 32 × launches |
| new mode: hash-counter (A on pid) | ≈ 330-340 us | ≈ 48 + `c_commit(1)` | 32 × launches |
| CUDA Counter | 2,877.5 us; 256 requests per timed launch (8 per thread); calls=97 | 160 requests per launch (5 per thread stay synchronous: 2 printk and the `start_ts` update, read and delete) + 2 UNIT_ADD; ≈ 1.8-2.0 ms (48 + 160 × 11 us if a printk request costs what a map request does; more if it costs more) | **calls=128**; total = Σ deltas |
| threadscheduling | 3 host calls/thread | 1 host call per thread (the `thread_mapping` SET stays synchronous) + 2 commits per group (sm, warp) + one UNIT_ADD per new key | Σ sm = Σ warp = threads × launches |
| hash-counter 1,024 / 100,352 threads | ≈ 11 ms / ≈ 1.07 s | no number until contended `c_commit` is measured | exact |
| CPU-probe lookup on a ledgered HASH | ns per lookup | report the overhead | unchanged values |

The CUDA Counter headline is exactness (97 → 128) with a modest speedup: slice 1 removes 3 of the 8 requests per thread (96 of 256 per launch); the `start_ts` sites follow in slice 2 and printk in slice 3.

### Measurement

- **Comparison:** D vs N with the compare flags; upstream `benchmark/gpu` runner with legacy-launch victims; 10 interleaved rounds; Mann-Whitney.
- **Checks before every batch:** cpu5 1728000 and GPU 1020000000; no leftovers, no segment, no root `/dev/shm` debris (segments and per-ledger lock files); timeouts outside `LD_PRELOAD`; containers removed.
- **Request counting:** a smoke round counts requests from the watcher's debug lines.
- **Statistics** dumped at exit.
- **PTX kept**, to confirm one `call _bpf_helper_ext_0600` per unit after O3.

### Exactness tests (oracles from SR)

- **T1** first touch, N·L, 1-1,024 warps, partial warps; a variant with key = lane % 4 (four groups per warp).
- **T2** RMW sum = Σ per-thread deltas, recorded in a 1512 map.
- **T3** two-sided real-time bound with `CLOCK_MONOTONIC` stamps; monotone reads.
- **T4** print-and-clear: Σ returned + final = N·L.
- **T5** mid-run overwrite bound.
- **T6** CPU uprobe in an **all-HASH** program (typed helper): final = N·L + L; its reads are ≥ committed.
- **T7** close and fd reuse.
- **T8** ledger full.
- **T9** (slice 2) exit or SIGKILL, then the application runs again and the new agent takes over with seeded shadows: totals continue. In slice 1, T19 covers SIGKILL.
- **T10** torn reads.
- **T11** d = 0 after a host delete re-creates the key.
- **T12** full map with `max_entries` = 4.
- **T13** (slice 2) late GPU pointer writer with atomics and the flip kernel: the flip completes while application kernels run, and no straggler remains after it; a variant whose flip kernel cannot be scheduled reports `late_stragglers`.
- **T14** llvmbpf CPU JIT differential over random serial schedules, plus near-miss fixtures.
- **T15** 1527 witness ring.
- **T16** divergent return probes.
- **T17** R7 survey check, with `exec_mode` asserted per record.
- **T18** two A units in sibling branches, on different keys and on different maps, 16 lanes on each side: every key gets its own lanes' count. The fixture forces one merged call (built without the `nomerge` hunk, or a hand-written unit whose single call has a `select`ed site and fd, as `redteam/sink.O3legacy.ll`), so the check is what it exercises; a separate check counts the calls in the kept PTX with `nomerge`. A variant merges units with different key sizes and with one immediate and one register delta.
- **T19** owner death: the application SIGKILLed with records pending, then run again; totals continue, and the second run defers (request count) although the dead pid is still in the alive-agent set. Variants: a live owner in another pid namespace sharing `/dev/shm` is not retired; two agents starting at once after the death install one ledger; a test hook closes the owner's lock file while its kernel still commits (the commits are folded from the retired ledger); a first agent running with `BPFTIME_GPU_SITE_EXEC=off` and unrewritten pointer atomics keeps a second agent from creating a ledger.
- **T20** fold guard: a unit test that hands the fold a record older than its watermark, and an accepted read with v1 = 2c, v2 = 2c+2 followed by a read at 2c+2; no change, no wrap, and commit c+1 is folded.
- **T21** a module loaded in an application-created context (`cuCtxCreate`, then `cuLibraryLoadData`), and the late path after a refresh in such a context: no fault, its groups go to UNIT_ADD, exact, with the retprobe unit (a varying delta).
- **T22** a late CPU pointer writer (`*v = 0` in a uprobe) attached through a refresh, through auto-refresh and through init's retry path: the statistics show the FLIPPED store before the writer's attach; after the hint store, a UNIT_ADD on an existing key, a first touch of a new key and a tool update all leave the hints SYNC; no record `ver` moves for sections that start after it; every CPU store survives except where it meets a counted straggler (`late_stragglers` ≤ sections in flight at the hint store).
- **T23** a late GPU pointer writer with atomics (`lock *v += 1` through a lookup pointer) attached by refresh: FLIPPED before its attach; no record moves for sections after the hint store; every later unit is counted in `post_flip_units`; totals equal the sum of all adds up to the counted deviations.

**Slice 1 runs** T1-T8, T10-T12 and T14-T23 with their mutations (T7: `gen`/DEAD and T8: NOSLOT are built in slice 1; T12 covers the sweep; T15 is the oracle; T16 covers the retprobe units). T9 and T13 follow with slice 2.

### Mutations (each must fail its test)

| mutation | failing test |
|---|---|
| M1 typed-path hook missing | T6 |
| M2 fold after the operation | T4/T5 |
| M3 fold without the lock | T4 in a host unit test with a hook that pauses two folders at the record read |
| M4 CPU resets record words | T4/T5 |
| M5 trusting `popc(activemask)` (same code as M17) | T1's key = lane % 4 variant |
| M6 re-creation by Δsum | T11 in a host unit test: delete a key, then a CPU thread runs the commit protocol with count + 1 and sum + 0, then fold |
| M7 no odd `ver` | T10 with a simulated writer paused between the half store and the even `ver` (no fence: reported only, like the litmus negative controls) |
| M8 init ≠ delta accepted | T14 |
| M9 r0 liveness ignored | T14 |
| M10 no `gen` check | T7 |
| M11 600 unregistered | containment test |
| M12 the flip kernel skips the lock-table pass (slice 2) | T13 |
| M13 (merged into M24) | - |
| M14 loader copy outside the fold | T3 in a host unit test with a hook that runs an in-place add during the copy |
| M15 check-script mutation (cudagraph:56 "needed now") | T17 reports an `exec_mode` mismatch at cudagraph:56 |
| M16 no takeover seeding (slice 2; checked on a run where takeover actually happens) | T9 |
| M17 no grouping check (trust `popc(activemask)`) | T18 |
| M18 owner death by alive-set membership | T19 |
| M19 no `ver` guard in the fold | T20 |
| M20 device-block pointer written into a module of another context | T21 |
| M21 no hint store before attaching a late pointer writer | T22 |
| M22 a hint writer ignores FLIPPED | T22 |
| M23 the scan after loop 1 | T22's sequence check (also on the retry and auto-refresh paths) |
| M24 no sweep | T12 in a host unit test: after a delete a CPU thread runs the commit protocol for that key, then `get_next_key` and an insert near capacity |
| M25 owner death by (pid, start time) | T19 (namespace variant) |
| M26 watermark `ver` from the second load | T20 |
| M27 per-lane loop bounds in the peeling loop | T18's mixed-size variant (hangs) |
| M28 no writer registration across agents | T19's `BPFTIME_GPU_SITE_EXEC=off` variant |
| M29 a GPU-writer flip leaves the ledger deferring | T23 (records move after the hint store) |
| M30 retire marks the old ledger DEAD | T19's lock-file variant |

### Exit criteria

- Step 0 passed.
- T17 passes with the real library.
- The slice-1 tests pass and their mutations fail (race windows through the deterministic harnesses named in the table; fence-free variants are reported only).
- calls=128 and 160 requests per launch.
- hash-lookup-present (a commit per group) within 1.2× of CPU Array lookup; the absent-key case reported apart.
- hash-counter ≈ 48 + `c_commit`.
- CPU-probe and loader-lookup overhead reported.

### Later slices

2. SET, DEL, LOOKUP_COPY, C, patcher; the flip kernel (watcher never blocks on it), takeover seeding.
3. printk ring and orphan drain.
4. `match.any`/`redux` grouping, warp-batched sync, tier-1 IR inlining between `optimizeModule` and codegen (`llvm_jit_context.cpp:947-948`).
5. READ_LOCAL under C-SB, shards and multi-agent records, discrete GPUs.

Extension: ATOMIC_LOCAL for lossy plain RMWs on GPU-local maps (mem_trace, ARRAY `(*v)++`).

## Open questions

1. H2, H3, H4 on the registered segment with a reader in another process; H6 (three parties).
2. `c_commit` uncontended and contended, with the grouping check and in lane-turn; whether strict S = 1 is acceptable for hot keys at 100k threads.
3. C-SCHED, for slice 2's flip kernel.
4. C-PTX acceptance, above all `shfl.sync` and `vote.ballot.sync` (without them, lane-turn everywhere).
5. C-SB, which decides READ_LOCAL vs LOOKUP_COPY for the 3 `resident_read` sites.
6. Whether nvPTXCompiler merges helper-600 calls that `nomerge` kept apart (NVPTX code generation did not on the red team's sample, `recheck/`), and whether lanes from two call sites can be active together inside the shared helper body. Neither affects correctness (the grouping check), only how often the fast path applies.
7. Cost of the fold hooks on CPU-probe and loader lookups.
8. The survey's `%globaltimer` inconsistency (51 = 47 + 4); the pushed survey should be corrected.
9. Closed: the micro hash_lookup unit fits in 5 slots (built object).
10. A per-agent request slot.
11. Slot reclamation for churning keys.
12. Acceptability of D6 (frozen maps).
13. Discrete PCIe costs; sm_61 untested.
14. A store-buffering-free exclusion of D4.
15. Takeover seeding after SIGKILL with an odd `ver` (slice 2).
16. Hashed-lock collisions.
17. printk order when a synchronous printk drains the ring "up to the first uncommitted record" (slice 3): the semantics red team showed it can break per-thread order; the drain rule must be settled before slice 3.
18. A tool killed inside a fold leaves a stripe locked (R3.4); robust locks are not in this design. The flip's folds run under the map-binding mutex and use bounded trylocks (R3.6), so a dead holder delays a late writer's attach instead of wedging every agent's init.

## Must-fix coverage (short)

**Correctness review:**
1. Typed hooks → R3.5/T6.
2. Count → R3.4/T11.
3. Seqlock snapshot → R3.3-R3.4/T10.
4. No incarnations; host never waits → R3.4.
5. Core on existing facts only → R3.1.
6. hb clause plus a fence before return; per map → R1.
7. D1 and strict mode → R1, deviations.
8. Fold per kind → R3.4.
9. Loader copy under the lock → R3.4/M14.
10. Flip, bounded → R3.6/T22 (slice 1: hint store), T13 (slice 2: flip kernel).
11. CPU plain stores excluded → R3.6.
12. memcapture → R3.7.
13. Capacity host-linearized → R3.4/Lemma 4/T12.
14. Host-reachable after SIGKILL, discrete too → R6/R8/T19 (slice 1), T9 (slice 2).
15. Per-group fallbacks; ids → R5.
16. Hot keys → R8/step 0.
17. R7 check → R7.
18. `CommSharedMem` → R3.8/R9.

**Feasibility review:**
1. Typed hooks.
2. No `%warpid` scratch: hashed lock and a `shfl.sync` gather.
3. PW as a precondition (READ_LOCAL needs C-SB instead of an epoch).
4. Exact hardware list.
5. Slice A+B+J, in place, T14, conservative CPU scan.
6. Fast path, measured.
7. Host-only slots; sizes and budget.
8. dlsym gate.
9. memcapture off.
10. No quiescence waits.
11. Projections tied to anchors.
12. Real R7.
13. Discrete ledger in host memory.

## Reviewer disagreements and decisions

1. **Slice scope:** A + B + J lookup (feasibility); C moves to slice 2. After the red team, the flip kernel and takeover seeding move to slice 2 as well (the sweep stayed, after recheck R2), and measurement plumbing moves in.
2. **Rewrite:** in place now (all slice-1 regions fit); the patcher later.
3. **Validation:** T14 differential plus a self-check instead of value-number re-analysis.
4. **Hot keys:** strict linearizable default; sharding opt-in with D1 (both reviews satisfied).
5. **Litmus negative controls:** reported, not required to fail.
6. **Late writers:** a scan at every refresh and agent start; whoever finds a late pointer writer stores SYNC into the hints and folds before it attaches (D5 limited to sections already running); slice 2's flip kernel removes D5. Loader events never depend on the flip.
7. **GPU-local reads:** the correctness review called the epoch scheme sound; it is not under message passing alone (the cycle in R3.7), so READ_LOCAL needs C-SB.
8. **Frozen maps:** Linux semantics (D6). Restoring EPERM would make a loader freeze depend on the agent.
9. **printk ring in the segment**, so a dead agent's lines can be drained.
10. **Runners-up:** semantics-first supplies semantics, proof, hardware table and tests; performance-first supplies measurement, eligibility scan, witness ring and later fast paths.

## What was taken from which draft

**analysis-first (base):**
- bytecode units K/P/V/L/C;
- rewrite to registered ids with pc-free immediates;
- the analyzer in the agent's link path;
- the dlsym gate;
- fold-on-access in `bpf_map_handler` under the stripe lock;
- per-(map, agent) single-writer ledgers;
- `gen`/DEAD;
- pointer-writer classification;
- oracle-free decision, derived class, three-valued aggregatable, and the R7 script (extended);
- the 97 explanation;
- T1, T2, T4, T7-T10.

**semantics-first:**
- unit linearizability with po/rt/seq and carve-outs;
- the lemma + locality proof and the host-linearized capacity object;
- the hardware table with litmus gating against another process;
- the full hook list with typed paths;
- the merging copy;
- count with sum and two-half records;
- the ABSENT negative cache;
- host first touch;
- folding deletes before E2BIG;
- the segment printk ring with orphan drain;
- the ExitOnError replacement;
- control-dependent scopes and per-edge records;
- T3, T5, T14, T16 and the check mutation;
- ATOMIC_LOCAL.

**performance-first:**
- the eligibility scan at `init_attach_ctx_from_handlers`;
- step 0 (host-call breakdown, contended costs, PTX acceptance);
- re-measuring at 52126f33 and the geometry series;
- the witness ring;
- `match.any`/`redux` (22/21/21-bit limbs) and warp-batched sync as later slices;
- tier-1 inlining;
- `BPFTIME_GPU_PRINTK_SYNC`;
- the odd-constant single-word idea.

**New here:**
- hashed lock table;
- straggler ordering without incarnations;
- the sweep rule;
- the fast path with a `ver` recheck;
- the flip (hint store, then the flip kernel in slice 2) and replacement folds for GPU pointer writers;
- the C-SB analysis for READ_LOCAL;
- the strict default;
- the host-only bit;
- takeover seeding;
- D4-D6.

**After the red team:**
- the run-time grouping check and `nomerge`;
- owner liveness by a `flock` held for the owner's lifetime ((pid, start time, incarnation) kept for diagnostics), retire-on-death with a retired-ledger list in slice 1;
- the hint store before a late pointer writer attaches;
- the fold's `ver` guard and the three-party litmus tests;
- the context rule for device blocks;
- the verified key-to-slot cache;
- `exec_mode` and the two tallies;
- the slice-1 cut and its measurement plumbing.

## Red-team issues and their status

Two passes on the synthesis (`design-workflow-output.json`, `reds`): semantics (14 issues) and feasibility (10). The O3 merge was reproduced with `opt-15`, and `nomerge` was checked the same way (`redteam/`); the citations about the alive-agent set, `with_cuda_context`, the micro loader's modes and the refresh path were re-read at 52126f33. Nothing else was re-run.

| # | pass, severity | issue | status |
|---|---|---|---|
| 1 | semantics, major | O3 merges two rewritten sites into one call; static grouping counts every lane on the leader's key | fixed: run-time agreement check (R3.3), `nomerge`, Lemma 3 restated, T18/M17 |
| 2 | semantics, major | ownership and death decided by alive-set membership (set late, removed at detach, kept after SIGKILL, pids reused) | fixed: liveness by `flock` (after recheck R7), ownership ends at process exit or exec, retire-on-death in slice 1 (R3.8), T19/M18/M25 |
| 3 | semantics, major | hb needs three-party transitivity, step 0 tested two; the fold had no monotonicity guard (wrapped Δ) | fixed: `ver` guard (R3.4), H6 in step 0, (rt) per pair if H6 fails, T20/M19 |
| 4 | semantics, major | D5 unbounded: on a flip timeout nothing stops the GPU from deferring | fixed: hints to SYNC before attaching, kept SYNC by every later hint writer (R3.6, after recheck R1); D5 limited to sections already running; T22/M21/M22 |
| 5 | semantics, major | device block from the primary context written into modules of another context | fixed: `extra_buffer = 0` on a context mismatch (R3.2), T21/M20 |
| 6 | semantics, minor | flip deadlock if the watcher waits on the flip kernel | slice 2, with the rule: the watcher polls the event, never blocks (R3.6) |
| 7 | semantics, minor | "drain up to the first uncommitted record" breaks printk per-thread order | open for slice 3 (open question 17) |
| 8 | semantics, minor | base map E2BIG with free elements left contradicts SR(seq) and Lemma 4 | accepted as D7 (inherited, documented in `CLAUDE.md`) |
| 9 | semantics, minor | `get_next_key` from a missing key skips the sweep | fixed (R1 observation points) |
| 10 | semantics, minor | loader reads take non-robust spinlocks | mitigated: lock-free when nothing moved (R3.4); risk recorded (open question 18) |
| 11 | semantics, minor | after a flip, UNIT_ADD's CPU in-place add meets late GPU atomics | fixed: replacement after a flip caused by a GPU pointer writer (R3.6) |
| 12 | semantics, minor | hb has no CPU → GPU launch edge | fixed: edge added [external] (R1) |
| 13 | semantics, minor | key-to-slot cache has no publication protocol | fixed: the cache is a hint, key bytes verified against the immutable index (R3.2) |
| 14 | semantics, minor | late pointer writers caught only by agents that run the analyzer | fixed: the scan needs no CUDA context and runs in every agent of the build; mixed builds unsupported (R3.6) |
| 15 | feasibility, major | static grouping vs O3 call sinking | same as 1 |
| 16 | feasibility, major | the flip can hang the application (watcher waits, flip kernel spins on a held lock) | slice 2, with the rule of 6 and a test plus mutation (R3.6) |
| 17 | feasibility, major | slice 1 is not the smallest piece (flip, takeover, sweep, fd reuse, 6 sites that cannot run here) | fixed: cut (R10); flip kernel and takeover seeding moved to slice 2 (the sweep came back after recheck R2); 14 of 20 sites can run on the Orin, 10 measured. Late attach cannot be refused (refresh), so slice 1 keeps the cheap hint store |
| 18 | feasibility, major | micro headline measures only the absent-key no-op; no plumbing for commit-path modes | fixed: plumbing in the build list, 1.2× criterion on hash-lookup-present, absent case reported apart (R10) |
| 19 | feasibility, major | class tally reproduced by construction; resident_read labelled eliminate but runs a round trip | fixed: `exec_mode` per record and both tallies ("The model in one page", R7) |
| 20 | feasibility, minor | the survey's 51 aggregatable not reproduced | recorded (R7); the pushed survey is not changed here |
| 21 | feasibility, minor | CUDA Counter anchor (264 vs 256 requests, printk cost) | fixed: ≈ 11 us per call, ≈ 1.8-2.0 ms (R10) |
| 22 | feasibility, minor | analyzer cost anchor does not transfer | fixed: caveat, measured in slice 1 (R7) |
| 23 | feasibility, minor | micro unit "7 slots, not built" is wrong | fixed: 5 slots, built object; open question 9 closed (R4) |
| 24 | feasibility, minor | garbled D3 sentence | fixed (R1) |

## Independent recheck after the revision

Two fresh readers (mechanisms; consistency and citations) checked the revised text against 52126f33 without the red team's conclusions (`recheck-workflow-output.json`). The mechanisms reader found the record protocol sound: the two-half seqlock read, the fold with its `ver` guard, straggler ordering, the grouping check under independent thread scheduling, nested host calls inside a leader section, and the lane-turn fallback. It also ran `llc-15 -march=nvptx64` on the red team's IR: NVPTX code generation keeps the 4 calls that `nomerge` kept (ptxas not tested). The consistency reader re-ran `exec_mode.py` (identical output) and confirmed the CUDA Counter arithmetic. Issues, all addressed in the text above:

| # | reader, severity | issue | status |
|---|---|---|---|
| R1 | both, blocking | the slice-1 flip was not sticky: UNIT_ADD, tool writes and new slots set hints back to LIVE | fixed: FLIPPED flag in the ledger header, every hint writer keeps SYNC under the stripe lock, the fold re-stores SYNC (R3.4, R3.6); T22 extended, M22 |
| R2 | consistency, blocking | slice 1 dropped the sweep, but tool deletes happen in slice 1: a pending re-creation was invisible to iteration and capacity could deviate | fixed: the sweep is back in slice 1, T12 in slice 1, M24 |
| R3 | both, major | the scan, flip and fold ran after loop 1, which already attaches CPU links; auto-refresh calls init without refresh_attach_session | fixed: before loop 1, at every entry into `init_attach_ctx_from_handlers` (R3.6, R7, "What changes"); citations corrected; M23 |
| R4 | mechanisms, major | after a GPU-writer flip, UNIT_ADD by replacement sent the writer's later atomics to retired elements, unbounded | fixed: UNIT_ADD returns the element's device address and the leader adds with a GPU atomic; replacement only for stragglers (D5, restated: GPU writers only) |
| R5 | mechanisms, major | the path without a device block had no staging word, lock or `gen`, yet slice 1 has a varying-delta unit | fixed: sums by `shfl.sync` gather (no staging word), `gen` in a new MapBasicInfo field (R3.3); T21 uses the retprobe unit |
| R6 | mechanisms, major | the context rule covered only `write_trampoline_globals`; the late path and `run_attach_entry_on_gpu` also write `map_info` | fixed: one helper for all three writers (R3.2, "What changes"); T21 extended |
| R7 | mechanisms, major | (pid, start time) from `/proc` judges a live owner in another pid namespace dead; exec keeps pid and start time | fixed: `flock` on a per-ledger file held for the owner's lifetime, `O_CLOEXEC` (R3.8); T19 variants, M25 |
| R8 | consistency, major | R4 A, R5 and R9 still trusted a static group or `activemask` | fixed |
| R9 | consistency, major | with `nomerge` built in, M17 might survive T18 | fixed: T18 forces a merged call; a separate call count checks `nomerge` |
| R10 | consistency, major | build list and test list disagreed (T7, T8, T15, T16; T22 vs D5; M15's oracle) | fixed: slice-1 run list, M15 via `exec_mode`, D5 restated |
| R11 | consistency, major | the plumbing could not produce several numbers (readouts, warp totals, statistics, runner timeout, CPU-side harness) | fixed: build item 7 |
| R12 | mechanisms, minor | retire without a CAS | fixed: CAS on the handler's ledger pointer |
| R13 | mechanisms, minor | "no intra-warp spin on pre-Volta" did not hold for the peeling loop | fixed: one leader at a time, each round ends with `__syncwarp(m0)` |
| R14 | mechanisms, minor | an all-zero cache entry matches key 0 at slot 0; no bounds check | fixed: occupied bit, NONE = 0, `s < C` |
| R15 | mechanisms, minor | T22's oracle contradicted D5 | fixed: CPU-writer stragglers fold in place; D5 is GPU writers only |
| R16 | mechanisms, minor | a context reset in a live owner leaves records ahead of a fresh device block | fixed: a changed context id retires the ledger |
| R17 | mechanisms, minor | lookup pointers passed to helpers that write through them escaped the scan | fixed: counted as stores (R3.6) |
| R18 | consistency, minor | threadscheduling row understated host calls; 14 vs 10 measured sites; "never needed" GPU atomics; facts vs the gate; key-check loads missing from the cost; watermark `ver` unstated; citation slips | fixed |

## Second independent recheck

Two fresh readers again (`recheck2-workflow-output.json`), told to form their findings before reading the issue tables. Found sound: the record protocol and the watermark rule, nested host calls in a leader section, the path without a device block, the lane-turn fallback, FLIPPED stickiness under stripe locks, the scan against concurrent handler changes (the map-binding mutex is held through init), the three `map_info` writers, fork inheritance of the liveness lock, every re-checked citation, and the tallies. No blocking issue; all of the following are addressed in the text above:

| # | reader, severity | issue | status |
|---|---|---|---|
| S1 | mechanisms, major | in a merged call, per-lane key sizes and delta kinds gave lanes different `shfl.sync` sequences: a hang | fixed: every bound from broadcast values (R3.3); T18 variant, M27 |
| S2 | both, major | after a GPU-writer flip, the GPU's add after the reply can land in a freed or reused element: outside D5 as written, and not in slice 1 | fixed: stated as Linux per-helper semantics for every post-flip unit (D5, counted `post_flip_units`); path built in slice 1; T23, M29 |
| S3 | mechanisms, major | "not rewritten" is a per-agent decision; an agent with the feature off went unseen by the owner | fixed: writer registration across agents (R3.6); T19 variant, M28 |
| S4 | mechanisms, major | releasing the `flock` is not ordered with CUDA context teardown; a retire could drop late commits | fixed: retired ledgers stay folded until close, DEAD only at close (R3.8); T19 variant, M30 |
| S5 | mechanisms, major | a straggler that happened before a CPU writer's store can be folded after it | fixed: CPU writers back in D5 |
| S6 | consistency, major | several mutations needed a race window to fail | fixed: deterministic host-test harnesses (M3, M6, M7, M14, M24) |
| S7 | consistency, major | M23 could not fail T22 | fixed: sequence numbers for the FLIPPED store and the writer's attach |
| S8 | consistency, major | T17 and M15 need decisions for every idiom, the build list named A, B, J only | fixed: the analyzer decides all idioms, rewrites A, B, J |
| S9 | mechanisms, minor | `bpftimetool run` runs segment programs without the scan | fixed: it runs the scan and flip first; PW stated over programs that run |
| S10 | mechanisms, minor | lock file ordering against ledger publication; a missing file | fixed: created and locked before the CAS; missing means not provably dead |
| S11 | mechanisms, minor | GPU helpers 504, 505, 508 also write through pointers | fixed: listed, with a helper-effects table checked by a unit test |
| S12 | mechanisms, minor | scan placement vs init's retry loop | fixed: inside the attempt loop, every attempt |
| S13 | mechanisms, minor | the flip's folds under the map-binding mutex widen the dead-holder exposure | fixed: bounded trylocks; open question 18 |
| S14 | consistency, minor | leftovers (staging, the sweep in slice 2, (pid, start time), T13/M12 wording, duplicate M5/M13), undefined terms, `MapBasicInfo` size, device-block header, free counter cost, What-changes rows, D/N, citations, flock file clean-up, analyzer cost under auto-refresh | fixed |
