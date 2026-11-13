# UnitFold: exact per-call-site execution of GPU eBPF helper calls

Final design for direction 2 (GPU-to-CPU host calls). It merges three drafts (analysis-first, semantics-first, performance-first) after two reviews; both reviews chose analysis-first as the base. A longer version with the same decisions and more detail (full test and mutation tables, must-fix coverage tables) is at `/tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/dir2-design/final/design.md`. The R7 simulation is `.../dir2-design/final/r7_check.py`, `r7_output.txt` and `r7_records.json`.

**Conventions.**
- Every `path:line` is in `/home/jetson/src/bpftime-daily-state/worktrees/4708f4092d1e/candidate` at 52126f33, re-read for this document. llvmbpf lines refer to submodule `vm/llvm-jit` at 87bac39a, before the overlay.
- **[new]** marks a mechanism invented here. **[inferred]** marks reasoning over code that was never run. **[external]** marks CUDA/PTX documentation not re-read.
- Nothing was built and nothing ran on the GPU. Only Python ran, over `records.json`.

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
| CUDA Counter | 2,877.5 us | 264 requests per launch (8 per thread + first-touch inserts) |

- This gives about 10.7 us per host call.
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
| hint | CPU-written slot state (LIVE, ABSENT, DEAD) that the GPU reads |
| device block [new] | per (ledger, agent) device memory: lock table, shadows, key-to-slot cache, `devmode` |
| flip [new] | agent-local drain when a late pointer writer appears |
| sweep [new] | folding pending re-creations before capacity-changing operations |
| strict mode | the default: one record per slot (S = 1) and one deferring agent per map |

### What changes

| place | change |
|---|---|
| between the two loops of `init_attach_ctx_from_handlers` (`runtime/src/attach/bpf_attach_ctx.cpp:229-245` / `:248-259`) | analyzer over all handlers; ledgers |
| `bpf_attach_ctx.cpp:639` | pass the rewritten words |
| `create_map_basic_info` (`bpf_attach_ctx_cuda.cpp:505-554`) | `extra_buffer` of a ledgered HASH = this agent's device block |
| `core.cpp:234-253` | register ids 600-604 |
| each pass | export `ptxpass_site_exec_abi` |
| `default_trampoline.cu` | helpers 600-604 |
| `gpu_helper_protocol.h:22-30` | four opcodes |
| `serve_cuda_request` (`bpf_attach_ctx_cuda.cpp:124-187`) | serve the opcodes; fold before serving |
| `map_handler.cpp/.hpp` | ledger pointer, `gen`, hooks, `map_lookup_copy` |
| `bpftime_hash_map.hpp` | locked fold-then-op variants, in-place atomic add shaped like `with_existing_value` (`:699-709`), a relaxed free counter |
| typed HASH helpers (`runtime/src/bpf_helper.cpp:470-622`) | take the generic path when a ledger exists |

**Unchanged:** the request protocol, lane serialisation, the watcher's sleep policies, GPU map types, the ARRAY path, loaders and tools.

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

**Precondition PW** [new, enforced in R3.6]. On a ledgered map, no attached program does any of the following through a lookup pointer:
- a plain store;
- a non-ADD atomic;
- on the GPU, any atomic.

The only exception is D5.

**Commit point.** A deferred unit commits at the even `ver` store of its record. A `membar.sys` follows before the helper returns.

**hb (happens-before)** is the closure of:
- program order;
- fence-then-flag message passing in either direction;
- kernel completion observed by stream, device or event synchronisation;
- CPU synchronisation.

Real time between CPU and GPU is only observable through these.

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

- `get_next_key(NULL)` sweeps first.
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

In strict mode, host reads are linearizable: one record per key, one seqlock snapshot.

### Deviations, each counted

- **D1 (opt-in only: shards S > 1 or several deferring agents).**
  - Host reads during commits are regular and monotone; they are linearizable only if a double collect of `ver` is stable.
  - A destructive operation splits records at their own reads.
  - Conservation stays exact.
  - Counter: `unstable_reads`.
- **D2.** Commits on a closed map are dropped. bpftime frees maps on close (`runtime/src/handler/handler_manager.cpp:116-123`); in Linux the effect would be unobservable. Counter: `dead_drops`.
- **D3.** printk appears up to one watcher turn late. The backstop is 1,000 us (`runtime/src/attach/cuda_watcher_policy.hpp:27`). Opt-out: `BPFTIME_GPU_PRINTK_SYSNC` is not a typo-free name, so use `BPFTIME_GPU_PRINTK_SYNC=1`.
- **D4.** A straggler re-creating a host-deleted key gets its E2BIG decision at the first sweep or fold that sees it.
  - Sweeps run before every insert and delete near capacity.
  - What remains: when several stragglers are pending on a nearly full map, which ones get in follows sweep order.
  - Counter: `e2big_sweep`.
- **D5.** If a flip times out (`T_flip` = 1 s):
  - late commits are applied by element replacement, never in place;
  - a concurrent write through an old pointer can be lost (Linux's update semantics, triggered by the fold).
  - Counter: `late_stragglers`.
- **D6.** Deferred GPU writes after `BPF_MAP_FREEZE` are applied, as Linux allows for programs. Today GPU host calls get EPERM (`runtime/src/handler/map_handler.cpp:445-448, 776-779`). UNIT_* operations use program semantics; unrewritten sites keep EPERM.

### Proof sketch (strict mode, ADD; SET and DEL analogous)

- **Invariant I.** A ledgered key's element changes only:
  - in a fold;
  - in a host operation run right after a fold under the same stripe lock;
  - by commuting CPU atomic adds through pointers.

  It rests on PW and on full hook coverage.
- **Lemma 1 (snapshot).** The two-half read returns totals held at one instant. It needs H2 and H4, and never waits on an odd `ver`.
- **Lemma 2 (fold).**
  - The deltas s − w met an unchanged element (Invariant I). Adds commute, and the count decides re-creation, so applying them at the fold equals applying them at their commits.
  - Commits whose end hb the fold's start are in s (H2 plus the trailing fence).
  - Later ones are ordered after the operation.
- **Lemma 3.** Each lane is in exactly one equal-key group; the count is the lane count.
- **Lemma 4 (presence and capacity).**
  - Every key insertion is a host operation: a UNIT_* first touch, a host insert, or a fold re-creation.
  - Count-changing operations sweep first near capacity.
  - Pending GPU DELs are folded before an E2BIG.
  - Therefore the base map's accounting linearizes presence and capacity, except D4's order.
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

Each core fact is litmus-checked at start-up. Deferral is enabled only if H2-H4 pass.

| id | fact | precedent at 52126f33 |
|---|---|---|
| H1 | GPU atomics within one context on device memory | module lock (`default_trampoline.cu:118-122, 146-148`) |
| H2 | GPU→CPU message passing to a reader **in another process**: stores, `membar.sys`, flag; reader acquire | 1527 tail (`default_trampoline.cu:478-480`) drained by the loader (`nv_gpu_ringbuf_map.cpp:65-68`) |
| H3 | CPU→GPU message passing | `flag2` release (`bpf_attach_ctx_cuda.cpp:317`) → GPU spin + `membar.sys` (`default_trampoline.cu:159-163`); 1527 head (`:461-470`) |
| H4 | 8-byte single-copy atomicity on the segment | 1527 u64 head/tail (`gpu_helper_protocol.h:86-103`) |
| H5 | GPU-scope ordering: `bar.warp.sync`, device-lock acquire/release | standard PTX [external]; `__syncwarp` and CAS locks in use |

**Never needed:** GPU atomics on the segment, CPU-GPU atomic RMW, store buffering, CPU posting into `CommSharedMem` (`occupy_flag` is GPU-only, `default_trampoline.cu:149`).

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

**Device block** [new], agent `cuMemAlloc` in the primary context (`bpf_attach_ctx_cuda.cpp:262-276`):
- `devmode`, written only by the agent's flip kernel;
- a lock table of NL = 2C entries {lock, staging sum} indexed by **hash(key bytes, shard)**: every lane of a group computes the same entry;
- shadows per (slot, shard), protected by that lock;
- the key-to-slot cache (duplicates are harmless because the host assigns one slot per key).

**Reaching the device block.**
- Modules reach it through `map_info[fd].extra_buffer`. The trampoline uses `extra_buffer` only for ARRAY and GPU map types (`default_trampoline.cu:238-272`).
- `map_info` is a load-time snapshot (`default_trampoline.cu:101`; `nv_attach_impl.cpp:2366-2385`) that carries only this stable pointer.

**One writer per word.**
- The GPU writes only records, after the CPU zeroes them at slot creation.
- The CPU writes everything else.
- The map element is written only by host code.

### R3.3 GPU commit (helper 600, dfr_add) [new]

**Grouping.**
- **Static:** the key (and optionally the delta) is invariant within a block, derived only from constants, helper 14 and 503/504/508. This is a single-thread dataflow fact; all active lanes form one group.
- **Dynamic:** otherwise, including `%smid`, `%warpid` and `%globaltimer` keys [external: they can change under preemption]. In slice 1 each lane is its own group (lane-turn); with C-PTX, `match.any.sync` groups.
- No `%warpid` scratch is used (PTX: unique only within a CTA [external]).

**Sum.** n·d for a constant delta. Otherwise lanes `atomicAdd` into the hashed entry's staging word while the leader holds that lock; no broadcast primitive is needed.

```
_bpf_helper_ext_0600(fd, key, delta, desc, site):
  B = map_info[fd].extra_buffer
  for each group G: leader = lowest lane; n = popc(G)
    L = &B.locktab[hash(key, shard) & (NL-1)]       // all lanes
    leader: lock(L); s = keycache_find(B, key)
    syncwarp(G); if delta varies: atomicAdd(&staging(L), delta); syncwarp(G)
    leader: sigma = const ? n*d : take(staging(L))
      h = slot ? ld.volatile slot_state[s] : NONE      // hint (H3)
      DEAD -> drop; IF_PRESENT && ABSENT -> no-op
      devmode != DEFER || h != LIVE -> host_call(UNIT_ADD, gen, key, sigma, n, ...)  // L held; cache slot
      else: sh = shadow[s]; v = sh.ver; sh.sum += sigma; sh.count += n
            st rec.ver = v+1; membar.sys
            st rec.half[((v>>1)+1)&1] = {sh.sum, sh.count}; membar.sys
            st rec.ver = v+2; membar.sys; sh.ver = v+2   // commit, fenced before return
      unlock(L)
    syncwarp(G)
```

**Properties.**
- The payload is cumulative, with no incarnations.
- A writer that dies leaves the committed half intact.
- A fallback holds the lock across its host call, so later groups cannot return before it.
- One lane per group locks, so there is no intra-warp spin on pre-Volta GPUs [inferred].
- Cost: device CAS + 1 uncached load + 4 uncached stores + 3 `membar.sys`, measured in step 0.
- Optional: a single-word `red.add` record for odd constant deltas.

### R3.4 Host side [new]

**Wait-free record read:**
```
v1 = ld_acq(ver); c = v1>>1; (sum,count) = half[c&1]; fence; v2 = ld(ver)
accept if v2 <= 2c+2   // never waits on odd ver
```

**Fold** of k under k's stripe lock (`bpftime_hash_map.hpp:132-134`).
- Δsum and Δcount = record totals − watermarks.
- If Δcount = 0: nothing to do.
- Present key (UPSERT or IF_PRESENT): in-place 4/8-byte CPU atomic add.
- Absent key, UPSERT: insert c ⊕ Δsum (re-creation; sweep rule; E2BIG → drop, counted).
- Absent key, IF_PRESENT: drop.
- LWW SET: `replace_locked` (`:528-538`) or insert. LWW DEL: delete (`:755-778`).
- Watermarks := snapshot.
- Count, not sum, decides re-creation.
- Lock-free lookups (`:112-134`) see an add before or after it, and a replaced element whole.
- Pointer holders see in-place adds and lose writes on SET replacement, as in Linux.

**CPU-probe lookups (fast path).**
- If every record's `ver` equals its watermark, do today's lock-free lookup, then re-check `ver`.
- Otherwise lock, fold, find.

**Loader lookups.** Today the server does `memcpy` after the lookup, outside any lock (`syscall_context.cpp:615-622`). It becomes `map_lookup_copy`: lock, fold, copy to a private buffer, unlock, then copy to the user buffer, as `elem_delete` does (`bpftime_hash_map.hpp:770-775`).

**Host writes.** Update, delete and lookup_and_delete run fold-then-op under one stripe lock, through locked variants (`:715-778`), and set the hint (LIVE, or ABSENT plus the absent list).

**UNIT_ADD (watcher).**
1. Check `gen`.
2. Sweep if needed, lock, fold.
3. Run the unit: present → add; absent → UPSERT inserts with Linux's E2BIG, IF_PRESENT no-ops.
4. Find or create the slot (NOSLOT if full); set the hint; return the slot.

**Stragglers.** Without store buffering, a commit can land after a host operation's read. It is ordered after that operation:
- after an overwrite: added on top;
- after a delete or lookup_and_delete: UPSERT re-creates, IF_PRESENT drops;
- conservation holds: Σ returned + final = total.

No GPU-local read of ADD keys exists, so no observer saw it earlier.

**Sweep.**
- Before every operation that changes the key count (host insert of an absent key, delete, lookup_and_delete, UNIT_* insert, fold re-creation), and only when the relaxed free count ≤ absent-list length + 64, fold each absent-list slot whose count moved.
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

**Init-time scan.** Between the two instantiation loops, the scan reads every program handler in the segment and sets two flags:
- `gpu_ptr_writer(M)`: any GPU site **not rewritten** stores, or runs any atomic, through a lookup pointer of M.
- `cpu_ptr_writer(M)`: any CPU program does a store, or an atomic other than ADD, through such a pointer.

Either flag means M gets no ledger. CPU atomic ADD is allowed only next to add-only ledgers. The scan runs before any patched module exists.

**Late pointer writers** (from an auto-refresh or another agent) trigger a flip:
1. The requester writes `flip_req` in the ledger header. The owner's watcher polls it every turn (≤ 1 ms). A dead owner (pid not in the alive set) means no GPU writer is left.
2. The owner launches a one-block kernel on a private stream: `devmode = SYNC`, `__threadfence()`, then acquire and release every lock-table entry. Every section that read DEFER has then finished, and later sections see SYNC (H1, H5). The owner waits on an event, not on `cuCtxSynchronize`; the only precedent, `run_attach_entry_on_gpu` (`nv_attach_impl.cpp:1190`, launch `:1360`, sync `:1364`), uses `cuCtxSynchronize`.
3. After the ack, the requester folds all slots, sets the hint to SYNC and attaches. Old rewritten sites now use UNIT_*.
4. If there is no ack within `T_flip` (1 s), the late program attaches anyway (bounded time). Later movement is folded by replacement only and counted (D5).

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

- **`gen`** [new]: a 64-bit segment counter in the handler. Today `id` is the fd (`map_handler.hpp:89-102, 370`). It is carried in the header and in UNIT_* requests; a mismatch drops the request.
- **Close:** sets DEAD and detaches. Ledger and device block stay until the agent exits, because patched images cannot be unloaded (`nv_attach_impl.cpp:2387-2390`).
- **fd reuse:** old rewritten sites see DEAD or a `gen` mismatch. Old unrewritten sites keep the pre-existing hazard.
- **Maps missing from `map_info`** (created later, or fd ≥ 1024) are never rewritten.
- **Takeover** [new]: an agent finding the owner dead takes ownership with a CPU CAS.
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
- **retprobe B:** pc 28-35, plus nops at 36-42 and 69-76. The printk path 43-68 is reached only from pc 16; r1-r5 are redefined at 105-110; slot `r10-0x28` is kept and read at 107 (`dir2-read/disasm/cuda-counter.txt`).
- **micro hash_lookup** (`benchmark/gpu/micro/cuda_probe.bpf.c:167-176`): fits in 7 slots per the feasibility review [inferred; the object is not built in dev].

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
- UnitFold: the pid key is PROCESS-scoped, so the group is static and Σ = popc. The first touch is one UNIT_ADD, then records. Result: 128/128.
- The first-touch race does not exist, because presence is decided by a host operation under the stripe lock.

**B.** Linux loses updates through the racy RMW and through EXIST replacement (`kernel/bpf/hashtab.c:1171-1185`); today about one update per warp lands; UnitFold gives Σd through the staging sum.

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

**Uniformity** selects only the static group path, which rests on single-thread value invariance. Flagged values take the dynamic path.

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
| device block | device memory | NL(4 + 8) + C·S·24 + cache 2C(ks8 + 8) | cache full → UNIT_* | seeded at takeover |
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
- In the agent's global step, between the loops of `init_attach_ctx_from_handlers`, over all handlers.
- Per link at `bpf_attach_ctx.cpp:629-648`: rewritten words at `:639`; device-block addresses through `create_map_basic_info(1024)` at `:643-644`.
- Late links: a refresh re-runs the step.
- Offline: `bpftime-site-analyze` over `.bpf.o` with `.BTF.ext`.
- Device blocks are allocated under a `with_cuda_context`-like helper (`nv_attach_impl.cpp:2306`, used at `:2370`) with the primary context [inferred]. With no `cuda_ctx` (`bpf_attach_ctx.cpp:623-628`) there is no ledger.
- Cost: 2.2-2.5 ms for 4,096 instructions (auto-warp branch's `device-verifier-phase-attribution-20260904.md`).

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
| O3 merging | phi'd descriptors are read at run time [inferred] |
| memcapture prepends and recompiles (`main.cpp:53-137`) | pc-free; READ_LOCAL off there |
| patch cache keyed by the sha256 of the request including the words (`nv_attach_impl.cpp:994-1027`) | rewritten words get their own entries |
| stale passes | `ptxpass_site_exec_abi` via `dlsym` next to `print_config`/`process_input` (`nv_attach_impl.cpp:601-614`), plus a `site_exec_abi_for(attach_type, func)` query [new] |

**Records.** Survey fields plus `decision`, `fallback_reason`, `unit_id`, `site_id`, `pc`, `grouping`, `slice`.
- One record per logical site.
- `cpu_reads_map` is an external input.
- `aggregatable` is three-valued: no (lane-varying), timing (`%globaltimer`), yes.
- Class is derived: unchanged → gpu_local_already; cpu_reads_map = no → eliminate; resident_read with yes → sync; otherwise defer.
- Row filter: 180 deduplicated bpftime records = 109 + 20 + 39 + 12 `cpu_side`; the 12 are dropped, leaving 168 GPU sites.

**Check.** Headline: per-record `decision` against (operation, result_use). Then `key_scope` for the 38 map sites, aggregatable record by record, and the class tally (derived, nearly vacuous).

**Simulation now** (hand provenance table, not the analyzer):
- decisions: `defer_unit` 24, `defer_out` 21, `defer_set` 7, `defer_del` 4, `resident_read` 3, with **0 mismatches**;
- class 20/39/0 plus 109, 0 mismatches;
- `key_scope` 38/38;
- aggregatable 47 yes / 8 timing / 4 no; yes and no agree with the survey. The timing records are cuda-counter:55/82, rocm-counter:56/83, launchlate:114/117 and launchlate-kernel-gpu-shared-map:119/122.
  - The survey's 51 = 47 + the first four. Its stance on `%globaltimer` is inconsistent: no consistent rule gives 51 (47, 47, 55).
- Slice 1 rewrites 20 sites (A 10, B 9, J 1).

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
- **Primitives:** only those the trampoline already emits; it is built for sm_61 (`trampoline/Makefile:3, 26`) and retargeted.

## R9. Interactions

- **Several agents:** strict ownership; other agents use UNIT_*; flip via the segment. The shared, zeroed `CommSharedMem` needs a per-agent slot first.
- **CPU probes:** fold on every path, typed ones included; atomic ADD through pointers only next to add-only ledgers; other pointer writes keep M out of the ledger.
- **Graph and CUDA 13 launches** (`probed_launch_activity.cpp:16-24`): irrelevant for correctness. The flip uses the agent's own kernel, not application quiescence.
- **Close and fd reuse:** `gen`, DEAD.
- **printk:** per-thread order; reservation order across threads; drained before sync lines; ≤ 1 turn late; no loss.
- **Several probes per kernel:** shared helpers; agent-unique site ids; the pre-existing `__probe_func__<kernel>` clash (`ptxpass_kprobe_entry/main.cpp:53`) is unchanged.
- **Return probes and divergence** (`ptxpass_kretprobe/main.cpp:50-61`): groups come from `activemask`.
- **Mixed deferred and sync sites on one map:** sync requests fold first.
- **Detach:** the last detach waits ≤ 2 s and switches probes off (`nv_attach_impl.cpp:160-170`). Ledgers outlive it.
- **SIGKILL mid-section:** the committed half stays valid.
- **Not ledgered:** `run_with_kernel`, 1504, per-CPU, LRU, arrays.

## R10. First slice

### Step 0 (≈ 1 week, standalone; dev unchanged)

1. **Litmus** against a reader in another process mapping a segment registered as bpftime does, with the design's fences, 10^7 iterations:
   - H2, H3;
   - H4 torn reads with 1,024 warps;
   - C-SB and C-XA reported only;
   - fence-free variants reported, not required to fail.
2. **PTX acceptance** after the sm_87 retarget: `shfl.sync`, `vote.ballot.sync`, `match.any.sync`, `redux.sync`, `nanosleep`, `red.global.add.u64`.
3. **Calibration:**
   - a `%globaltimer` breakdown of `host_call` (lock, request, flag1, flag2, release);
   - `c_commit(k)` on one slot and on distinct slots;
   - `membar.sys`, `c_ld_sys`;
   - C-SCHED: latency of an agent kernel while a saturating kernel runs.
4. **Gate:** H2-H4 must pass, otherwise only the analyzer and the R7 check ship.

### Build (≈ 4 weeks)

1. The analyzer library and offline driver: A, B, J-lookup units, records for all sites, the pointer-writer scan, the in-place rewrite with self-check.
2. Helper 600: static path (n·d or staging sum), lane-turn dynamic path, record commit, UNIT_ADD.
3. ADD ledgers in strict mode; device block with the hashed lock table; takeover seeding; flip kernel and request (`T_flip` = 1 s).
4. Host: all R3.5 hooks; fast path; `map_lookup_copy`; fold, sweep, free counter; `gen`, DEAD; the UNIT_ADD opcode; fold-before-serve.
5. Registration of 600, the `ptxpass_site_exec_abi` gate, the containment test, the ExitOnError hunk, `BPFTIME_GPU_SITE_EXEC`.

**Not in slice 1:** SET, DEL, LOOKUP_COPY, READ_LOCAL, printk, C, `match.any`/`redux`, shards, multi-agent records, batching, tier-1, discrete GPUs.

### Expected numbers [inferred]

Anchors: probed floor ≈ 48 us; ≈ 10.7 us per host call (aa6eb973). D is re-measured at 52126f33. `c_commit` comes from step 0.

| case | today | slice 1 | check |
|---|---|---|---|
| micro CPU Hash lookup (key 0 never present) | 341.1 us | ≈ 48-50 us after the first launch, plus one UNIT_ADD | 0 requests after the first |
| new mode: hash-lookup, key present | ≈ 330-340 us | ≈ 48 + `c_commit(1)` | 32 × launches |
| new mode: hash-counter (A on pid) | ≈ 330-340 us | ≈ 48 + `c_commit(1)` | 32 × launches |
| CUDA Counter | 2,877.5 us; 264 req/launch; calls=97 | 160 req/launch + 2 UNIT_ADD; ≈ 1.76 ms + 2·`c_commit` | **calls=128**; total = Σ deltas |
| threadscheduling | 3 host calls/thread | 1 host call + 2 per-lane commits | Σ sm = Σ warp = threads × launches |
| hash-counter 1,024 / 100,352 threads | ≈ 11 ms / ≈ 1.07 s | no number until contended `c_commit` is measured | exact |
| CPU-probe lookup on a ledgered HASH | ns per lookup | report the overhead | unchanged values |

### Measurement

- **Comparison:** D vs N with the compare flags; upstream `benchmark/gpu` runner with legacy-launch victims; 10 interleaved rounds; Mann-Whitney.
- **Checks before every batch:** cpu5 1728000 and GPU 1020000000; no leftovers, no segment, no root `/dev/shm` debris; timeouts outside `LD_PRELOAD`; containers removed.
- **Request counting:** a smoke round counts requests from the watcher's debug lines.
- **Statistics** dumped at exit.
- **PTX kept**, to confirm one `call _bpf_helper_ext_0600` per unit after O3.

### Exactness tests (oracles from SR)

- **T1** first touch, N·L, 1-1,024 warps, partial warps.
- **T2** RMW sum = Σ per-thread deltas, recorded in a 1512 map.
- **T3** two-sided real-time bound with `CLOCK_MONOTONIC` stamps; monotone reads.
- **T4** print-and-clear: Σ returned + final = N·L.
- **T5** mid-run overwrite bound.
- **T6** CPU uprobe in an **all-HASH** program (typed helper): final = N·L + L; its reads are ≥ committed.
- **T7** close and fd reuse.
- **T8** ledger full.
- **T9** exit or SIGKILL, then the application runs again (takeover): totals continue.
- **T10** torn reads.
- **T11** d = 0 after a host delete re-creates the key.
- **T12** full map with `max_entries` = 4.
- **T13** late CPU pointer writer: flip within `T_flip`; forced-timeout variant reports D5.
- **T14** llvmbpf CPU JIT differential over random serial schedules, plus near-miss fixtures.
- **T15** 1527 witness ring.
- **T16** divergent return probes.
- **T17** R7 survey check.

### Mutations (each must fail its test)

| mutation | failing test |
|---|---|
| M1 typed-path hook missing | T6 |
| M2 fold after the operation | T4/T5 |
| M3 fold without the lock | T4 |
| M4 CPU resets record words | T4/T5 |
| M5 `popc(activemask)` for per-lane groups | histograms, T1 with lane % 4 |
| M6 re-creation by Δsum | T11 |
| M7 no odd `ver` or fence | T10/T15 |
| M8 init ≠ delta accepted | T14 |
| M9 r0 liveness ignored | T14 |
| M10 no `gen` check | T7 |
| M11 600 unregistered | containment test |
| M12 flip skips the sweep | T13 |
| M13 no sweep | T12 |
| M14 loader copy outside the fold | T3 |
| M15 check-script mutation (cudagraph:56 "needed now") | T17 shows 20/38/1 |
| M16 no takeover seeding | T9 |

### Exit criteria

- Step 0 passed.
- T17 passes with the real library.
- T1-T16 pass and M1-M16 fail.
- calls=128 and 160 requests per launch.
- CPU Hash lookup within 1.2× of CPU Array lookup.
- hash-counter ≈ 48 + `c_commit`.
- CPU-probe overhead reported.

### Later slices

2. SET, DEL, LOOKUP_COPY, C, patcher.
3. printk ring and orphan drain.
4. C-PTX grouping, warp-batched sync, tier-1 IR inlining between `optimizeModule` and codegen (`llvm_jit_context.cpp:947-948`).
5. READ_LOCAL under C-SB, shards and multi-agent records, discrete GPUs.

Extension: ATOMIC_LOCAL for lossy plain RMWs on GPU-local maps (mem_trace, ARRAY `(*v)++`).

## Open questions

1. H2, H3, H4 on the registered segment with a reader in another process.
2. `c_commit` uncontended and contended; whether strict S = 1 is acceptable for hot keys at 100k threads.
3. C-SCHED and `T_flip`.
4. C-PTX acceptance.
5. C-SB, which decides READ_LOCAL vs LOOKUP_COPY for the 3 `resident_read` sites.
6. O3 sinking of helper-600 calls (harmless, but it disables the constant fast path).
7. Cost of the fold hooks on CPU-probe and loader lookups.
8. The survey's `%globaltimer` inconsistency (51 = 47 + 4); correct the survey.
9. Whether the micro hash_lookup unit fits in place (object not built).
10. A per-agent request slot.
11. Slot reclamation for churning keys.
12. Acceptability of D6 (frozen maps).
13. Discrete PCIe costs; sm_61 untested.
14. A store-buffering-free exclusion of D4.
15. Takeover seeding after SIGKILL with an odd `ver`.
16. Hashed-lock collisions.

## Must-fix coverage (short)

**Correctness review:**
1. Typed hooks → R3.5/T6.
2. Count → R3.4/T11.
3. Seqlock snapshot → R3.3-R3.4/T10.
4. No incarnations; host never waits → R3.4.
5. Core on existing facts only → R3.1.
6. hb clause plus a fence before return; per map → R1.
7. D1 and strict mode → R1.5.
8. Fold per kind → R3.4.
9. Loader copy under the lock → R3.4/M14.
10. Flip, bounded → R3.6/T13.
11. CPU plain stores excluded → R3.6.
12. memcapture → R3.7.
13. Capacity host-linearized → R3.4/Lemma 4/T12.
14. Host-reachable after SIGKILL, discrete too → R6/R8/T9.
15. Per-group fallbacks; ids → R5.
16. Hot keys → R8/step 0.
17. R7 check → R7.
18. `CommSharedMem` → R3.8/R9.

**Feasibility review:**
1. Typed hooks.
2. No `%warpid` scratch: hashed lock and staging.
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

1. **Slice scope:** A + B + J lookup (feasibility). It exercises every mechanism; C moves to slice 2.
2. **Rewrite:** in place now (all slice-1 regions fit); the patcher later.
3. **Validation:** T14 differential plus a self-check instead of value-number re-analysis.
4. **Hot keys:** strict linearizable default; sharding opt-in with D1 (both reviews satisfied).
5. **Litmus negative controls:** reported, not required to fail.
6. **Late writers:** an init-time scan plus an agent-local flip bounded by `T_flip`, with D5 on timeout. Loader events never depend on the flip.
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
- hashed lock and staging table;
- straggler ordering without incarnations;
- the sweep rule;
- the fast path with a `ver` recheck;
- flip kernel, segment request, `T_flip` and replacement folds;
- the C-SB analysis for READ_LOCAL;
- the strict default;
- the host-only bit;
- takeover seeding;
- D4-D6.
