# Hash maps in the GPU map model: design, stage one (2026-11-26)

User 11-26: start designing the hash map, after `gpu/hash-current-20261126` (today every GPU hash operation is a host call: 10-13 us back to back, 18-21 us sparse, one at a time for the whole process).

How this was made: a design panel (`appendix/`) of four independent designs, three judges (correctness, cost at llama.cpp scale, fit with the code) and one synthesis.
- The four starting points: one shared device table; per-(SM, lane) shard tables; classifying the key and routing it to array storage; a CPU-owned table with a device cache.
- Judges' scores, lens by lens:

  | design | cost | fit | correctness |
  |---|---:|---:|---:|
  | shards | 7 | 7 | 5 |
  | device table | 4 | 6 | 7 |
  | key classes | 6 | 5 | 5 |
  | CPU-owned with a cache | 2 | 3 | 7 |

- The synthesis (`appendix/synthesis.md`) is model output. What is taken from it below was checked:
  - every PTX ISA and CUDA guide quote it rests on was found verbatim in the local copies (`bpftime-gpu-work/step0b/c5-guarantees/docs/text/`);
  - cost anchors were traced to our results, or are marked unverified;
  - the target-architecture claim was checked in the code.

  Nothing was built or run.

## What it is

A **device hash map**: a hash map that GPU probes use without host calls. It has two parts.

1. **Key directory** (from the device-table design).
   - Open addressing in device memory (cuMemAlloc, zeroed as 1531's copy is). Each key claims a slot for good, with one 64-bit compare-and-swap inside trampoline code.
   - The table has next_pow2(2 x max_entries) slots. At most max_entries are ever claimed, so at least half stay empty and every probe sequence ends.
   - A claimed slot never changes its key. Hence a key never sits in two slots, there is no ABA, and the CPU's iteration order never changes.
2. **Values that GPU programs only add to** (stage one; from the shards design, as its "directory variant").
   - Each claimed key gets a row numbered in claim order. A row holds 1531's (SM, lane) shards: (smid & 7) x 32 + laneid, 256 on the Orin.
   - Rows beyond a per-map limit R are one copy each (proposed default R = min(max_entries, 512)).
   - The delivery thread sums the shards, as for 1531.

**One cost difference from 1531:** the device hash map must read a key's slot (one dependent load) before it can form the shard address. 1531 computes the address from the key alone.

## What stage one serves, and what it does not

- **Served:** counters and histograms keyed by anything with u32 keys and u64 values, changed only by 64-bit atomic adds. Inserts use `BPF_NOEXIST`; there are no GPU deletes. That covers the examples' counter idiom (cuda-counter's call_count, threadscheduling's sm/warp histograms, th-hostcall's counter), keyed by a constant, pid, SM, warp, block or bin.
- **Not served:**
  - **A hash keyed per thread** (a start time by thread index, inserted at entry and deleted at exit). Slots are permanent, so every distinct thread key needs one: 720,896-4,096,000 threads per llama.cpp launch. Those tools get E2BIG (counted). Their storage in the model is 1530 (thread-local, by value), declared by the tool, as `lat` does.
  - **Stores, multi-word values, deletes and in-place reads:** the store layout (below), not decided.
- **Coverage by default is small.** Today only 1501 maps would become device hash maps (plain HASH maps may be shared with CPU programs). That is 4 maps and 6 of 38 hash call sites in bpftime's examples, in two programs never run on this board.
  - So hash tools declare the device hash type, as `lat` declares 1530 and 1531 (decided, below).

## Operations (stage one)

All header and value accesses go through the `.global` address (`cvta.to.global`), as 1531's do. PTX ISA 8.6 says "since using distinct virtual addresses behaves as if using different proxies, they require a proxy fence".

- **lookup(K):**
  - Walk from K's home slot with `ld.relaxed.gpu.global.b64` until K's header or an empty slot.
  - A live K returns the calling thread's shard of K's row. A missing K, or one whose claim is in flight, returns NULL. That is bpf's answer for a key not yet inserted.
  - The lookup never waits.
  - Constant map fds can take an inline hit path (hash, one header load, compare, row to address). Whether that pays is check C2.
- **Through the returned pointer:**
  - null tests and 64-bit atomic adds, which become `red.global.add.u64` as for 1531;
  - "adds only" is a contract, as for 1531 today: the tool declares the type.
- **update(K, V, BPF_NOEXIST):**
  - A live K: -EEXIST, counted.
  - At an empty slot: reserve capacity with an atomic add on a `bound` counter (-E2BIG, counted, past max_entries), then CAS the slot from empty to {K, claiming}.
    - Won: take a row with an atomic add on `next_row`, add V into the caller's shard, then publish {K, row, live} with `st.release.gpu`.
    - Lost to K: give the reservation back, wait for live (bounded: below), then -EEXIST.
    - Lost to another key: keep walking.
- **Waits are bounded.** The wait in "lost to K" exists so that a thread that got EEXIST does not then get NULL from its own lookup. It polls with `nanosleep` up to a fixed bound, then returns -EEXIST and counts an unstable claim.
  - An unbounded spin would rely on forward progress that is only partly documented (CUDA guide: "aims to provide parallel forward progress"), and a stuck spin hangs the application's kernel.
- **Not from the GPU:**
  - get_next_key, lookup-and-delete and BPF_F_LOCK return -EINVAL, as today;
  - BPF_ANY or BPF_EXIST updates and deletes are store-layout operations; a map that uses them stays a host-call map.
- **No CAS in tool code, no warp collectives.**
  - llvmbpf's CMPXCHG returns a load taken before the exchange (step0 e5: 102 of 122,880 claims counted).
  - Agreement on an `activemask` mask is not documented, and step0b measured violations.
  - Every lane takes its own path.

## Basis (quotes found in the local PTX ISA 9.5 and CUDA guide copies)

- **One slot per key:**
  - Every header access is a 64-bit strong `.gpu` operation on one `.global` address, so they are morally strong and totally ordered: "Two overlapping write operations are related in coherence order if they are morally strong".
  - "When an atomic operation A and a write W overlap and are morally strong, then the following two communications cannot both exist ...: A reads any byte from a write W' that precedes W in coherence order. A follows W in coherence order."
  - A claimed key never changes, and a walk CASes only the first slot it read as empty. So K cannot get a second slot. (The derivation is the panel's; the correctness judge checked it.)
- **Adds without acquire:**
  - Rows are zeroed before any probe runs: "Start of a CUDA API call to enqueue a task synchronizes with start of the task".
  - Rows are then changed only by atomic adds, so every add lands whenever the adder saw the key.
- **Delivery sees what was published:** "a strong read on M followed by an acquire memory fence" is an acquire pattern, paired with the inserter's release. The snapshot then sums the shards, as 1531's does.
- **Sizes:** only 64-bit strong accesses touch shared cells. "The axioms in the memory consistency model do not apply if a PTX program contains one or more mixed-size data-races", so a map whose program touches values with other widths is not admitted.
- **Target:** `.relaxed`/`.acquire` and `nanosleep` "require sm_70 or higher". The trampoline is built for sm_61, but patched modules are rewritten to the device's architecture before compiling (`rewrite_ptx_target`, sm_87 here). Check C3 confirms it on the real modules.
- **Inference, not stated by NVIDIA:** the claim/publish protocol as a whole, and the bounded wait's "EEXIST then not NULL" property. Check C4 tests both.

## CPU view

- **Before the agent makes the device copy:** the map is today's CPU hash table (bpf semantics), so a loader can prefill it.
- **At the copy:** the agent writes the live keys into the zeroed block (each into shard 0 of its row), then publishes the address.
- **After the copy:**
  - The tool reads delivered snapshots: 1531's delivery thread, schedule and owner rule.
  - get_next_key goes in slot order, which never changes.
  - CPU update and delete return EPERM, as 1531 and 1532 do.
- **Loss counts** (E2BIG, EEXIST, unstable claims) need a channel: reserved keys cannot work in a hash key space. They go to a companion status array (decided, below).

## Expected costs (estimates, not measured)

| use, rate | today (measured, `hash-current`) | stage one (estimate) | anchors |
|---|---|---|---|
| counter, every thread, constant key (tg128, mul_mat_vec_q) | one host call per thread: about 9.6 hours per run (computed) | 0.02-0.06 ns per thread, about -3% to -9% | measured: 1531's add to a shard, 0.010-0.015 ns; unmeasured: the dependent header load |
| counter, every thread, rms_norm (32-token prefill) | 7.92-8.15 s per prefill, 341-351 times | close to 1531's cost on that kernel | as above |
| counter, one thread per block | 13.1-13.7 us per call, +44% (rms_norm, pp32) | well under 1 us per call | inferred |
| counter, one thread per launch | 17.9-20.9 us per call, +10 points on tg128 | well under 1 us per call | inferred |
| a hot key without shards (rejected) | | 1.09-1.10 ns per thread | measured (prototype merge cost) |

Panel figures not traced to our results, and so not used here: 59.8 ns for registered atomics on a shared word, 0.275 ns for "32 lanes on one word" (step0's row is a per-lane own word), and a 41 ns CAS loop (derived).

## Designed, not decided: the store layout

For maps that store, read or delete (cudagraph's per-launch start times, thread_mapping's 80-byte records, BPF_ANY updates), the panel designed:
- one value per key behind the same directory, with versioned multi-word writes;
- deletes that keep the slot;
- a private copy in the 1530 area for plain reads, with a write-back rule;
- a seqlock mirror for the CPU.

It rests on more inference than stage one: versioned reads, waits, the mirror seqlock, and copies that force a kernel's 1530 area back into the address form (+0.025 ns per thread, measured). It is kept in `appendix/synthesis.md` (sections 5-9) for a later decision.

## The bytecode check: not in stage one (proposed)

The panel proposed a second analyzer over eBPF bytecode, to choose each map's layout and refuse unsafe uses.
- **For stage one it is not needed** if the tool declares the type, as with 1531, where "adds only" is a documented contract.
- **The routing pass's PTX allowlist still guards what it lowers inline:** constant-fd lookups, and adds through their results. Anything it does not recognise keeps the helper call.
- **Why not add it:** a second checker that must agree with the PTX allowlist is the kind of code that failed open in each review round so far.

## Decisions (user, 11-26, as proposed)

1. **Declaration: yes.** A hash tool declares the device hash type (a new type number, as `lat` declares 1530 and 1531). Plain HASH maps keep today's host path; 1501 is not changed.
2. **First touch: bpf's semantics.** An insert that loses to the same key returns EEXIST, and the idiom's increment is lost, as in bpf; losses are counted. A fused "lookup or insert" may come later as an opt-in.
3. **Loss counts: a companion status array** named after the map, read with the ordinary map API (no dlsym entry).
4. **R: min(max_entries, 512) by default,** not a tool parameter for now.

## First checks (round one), with decision rules written now

Copied into a rules file before any run; nothing has run.

- **C1, compile only.**
  - Run llvmbpf's PTX over the hash examples (cuda-counter, cudagraph, gpu_shared_map, threadscheduling, threadscheduling_dynamic_hook, micro), th-hostcall and the pt_hash variants.
  - Report per map:
    - which of its accesses through lookup results are 64-bit atomics;
    - whether `__atomic_add_fetch` and `__sync_fetch_and_add` produce a fetch whose result is used;
    - whether plain u64 accesses come out as two 32-bit accesses.
  - Rule: a map whose accesses are not all 64-bit atomic adds with unused results is not stage-one material. List it, with the reason.
- **C2, the hit path by hand** on mul_mat_vec_q (tg128, mul_mat_vec_q probed). Variants are regenerated from the current by-value build's dump (81ea6349) and compared within the same pass. The 11-18 forms are keyed to the address-form build and do not apply.
  - Variants:
    - `addmin`, today's 1531 add, regenerated as the reference;
    - `hh`, a runtime key: one dependent `ld.relaxed.gpu.global` header load at the home slot, compare, row, shard, `red.global`;
    - `hh2`, the hit at the second slot;
    - `hpre`, a constant key whose row is fixed at patch time;
    - `hcall`, the walk in a helper.
  - Rules:
    - hh - addmin <= 0.02 ns in both passes: build the inline path.
    - 0.02-0.05 ns: build it inline, plus `hpre` for constant keys if `hpre` saves at least 0.006 against `hh`.
    - Above 0.05 ns: report that hash counters cost that much more than arrays.
    - Inline only if hcall - hh >= 0.006 ns in both passes.
    - A third pass if a difference moves more than 0.006 between passes.
- **C3, compile only.** The new PTX forms (`atom.cas.b64` relaxed and acquire, `ld.relaxed`, `st.release`, `fence.acq_rel.gpu`, `nanosleep`, `red.global`) merged into the patched llama.cpp modules.
  - Rules:
    - the SASS shows the `.global` strong forms;
    - a kernel that gains registers or loses warps per SM with the inline path keeps the helper call.
- **C4, the claim protocol by hand,** in a standalone CUDA test with the helper bodies as inline PTX.
  - The test: 10^6 threads x 10 launches insert with BPF_NOEXIST from 64 duplicated keys while lookups and adds race; then every header is scanned. Sums are checked for keys inserted beforehand and for keys inserted first by the GPU.
  - Controls must fail: a claim without the reservation, a publish without release.
  - Rules:
    - any key in two slots, or any sum different from the expected count minus the counted EEXIST, stops the build;
    - zero violations reads as "consistent with the model", not as proof.

## Files

- `DESIGN.md`: this file.
- `appendix/designs.json`: the four designs.
- `appendix/judgements.json`: the three judgements.
- `appendix/synthesis.md`: the synthesis as it came back.
- `appendix/panel-workflow.js`: the panel's script.
