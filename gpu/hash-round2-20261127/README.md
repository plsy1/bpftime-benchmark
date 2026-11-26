# Device hash map, round-two checks (2026-11-26/27)

Round one (`gpu/hash-round1-20261126`) left two open items:
1. **E2BIG with the map half full.** Threads reserved capacity right before their CAS, so every thread that lost the CAS to the same key held a reservation for a while.
2. **The release publish was not observable** by the stress test.

The user then took the four design decisions (`gpu/hash-design-20261126/DESIGN.md`, "Decisions"), and round two checks the changed insert path. The rules were written before any run (`RULES-round2.md`). Nothing is built in bpftime yet.

## The change: claim, then reserve (v2)

**Insert.** A thread that finds an empty slot first reads the reservation count with `ld.acquire.gpu`, then CASes the slot from empty to claiming(K).
- **The winner adds 1 to the count** with `atom.acq_rel.gpu`.
  - If the old value is below max_entries, the row is that old value: the thread adds into its shard and publishes live(K, row) with `st.release.gpu`.
  - Otherwise it publishes refused(K) and returns E2BIG.
- **If the precheck finds the count at max_entries,** the thread reads the slot again and returns E2BIG only if the slot is still empty. Otherwise it handles that slot again.
- **A thread that meets claiming(K)** waits, bounded, for live (EEXIST) or refused (E2BIG).
- **A walk that passes all slots** waits, bounded, for the count to reach max_entries, then returns E2BIG.

**Lookup:** claiming or refused returns NULL.

**Properties and dependency:**
- The count never goes down, and refused slots are permanent.
- E2BIG is exact only while capacity never comes back. Stage one has no GPU deletes, and CPU writes are refused after the device copy.

**Why the precheck is exact** (PTX ISA 9.5, 8.8-8.10; confirmed independently by the memory-model reviewer below):
- Every winner's add is a release pattern on the count.
- A precheck that reads a value at or above max_entries reads, through the chain of atomic adds, after all max_entries successful adds, so it synchronizes with each one.
- So the slot re-read sees every successful winner's claim, and every slot that winner walked past.

## C5: the v2 protocol by hand (`c5/`)

A standalone CUDA test, built from round one's C4: 10^6 threads x 10 launches. Clocks were not checked before these runs, so the kernel times below are indicative. C6's environment record, taken 10 minutes later the same night, shows cpu5 at 1,728,000 and the GPU at 1,020 MHz. Each thread looks its key up; a hit adds 1 to its (SM, lane) shard, and a miss inserts with `BPF_NOEXIST` under the v2 protocol. After each trial every header is scanned and the checks of `RULES-round2.md` run. Every run is a fresh process under the GPU lock, with a timeout.

| run | keys | max_entries | trials | live (expected) | EEXIST per trial | E2BIG per trial | refused slots (max per trial) | violations |
|---|---|---:|---:|---|---|---|---:|---|
| a | 64 | 128 | 1 + 1,000 | 64 (64) | 12,184-12,224 | **0** | 0 | none |
| b | 64 | 64 | 1 + 1,000 | 64 (64) | 11,994-12,224 | **0** | 0 | none |
| c | 64 | 48 | 1 + 1,000 | 48 (48) | 9,159-9,176 | 2,499,920-2,500,070 | 16 | none |
| d | 64, 16 inserted beforehand | 128 | 1 + 200 | 64 (64) | 9,128-9,171 | 0 | 0 | none |
| e | one per thread | 1,024 | 1 + 20 | 1,024 (1,024) | 0 | 9,989,760 | 1,023 | none |
| f | one per thread | 48 | 1 + 20 | 48 (48) | 0 | 9,999,520 | 80 | none |
| g | 64, wait bound 0 | 128 | 1 + 300 | 64 (64) | 12,186-12,257 | 0 | 0 | none |
| h | 64, concurrent reader | 128 | 1 + 300 | 64 (64) | 10,174-10,176 | 0 | 0 | none |

- **The round-one problem is gone.** E2BIG is 0 in every trial where the keys fit (runs a, b, d, g, h: 2,805 trials). Round one's protocol had 7,061-11,695 per trial at max 128.
- **Every key with E2BIG ended outside the map, and every key with EEXIST (not unstable) ended in it.** Live keys were always min(keys, max_entries), and the count always equalled live + refused slots.
- **Run g (wait bound 0):** 469,891 EEXIST came out unstable, and 73,485 of them were followed by NULL from the thread's own lookup. That is reported, not a violation. No EEXIST that was not unstable was followed by NULL.
- **Run h (concurrent reader):** no live header was seen with a row sum of 0. As in round one, the reader is too slow to test the publish: the smallest row sum it saw when a header turned live was 13.

**Controls.** Each control ran 1 rule run + 200 trials, and each failed the check the rules name:

| control | what is broken | trials with violations | the check that fired |
|---|---|---:|---|
| 1 | the claim is a plain store instead of the CAS | 201/201 | sums and winners (no key ended live) |
| 2 | winners never check the count, max 48 | 190/201 | live > max_entries (up to 64), rows >= max_entries |
| 5 | the precheck returns E2BIG without reading the slot again, max 64 | 44/201 | E2BIG on a key that ends live (122 key-trials) |
| 6 | round one's order (reserve before the CAS) | 201/201 | E2BIG with keys <= max_entries (7,876-10,789 per trial) |

Control 5 shows the slot re-read is needed: without it, a thread can get E2BIG for its key while another thread of the same key takes the last place.

**A finding: refused slots, up to a full directory.**
- **How many.** A winner refused at capacity keeps its slot. There are at most (distinct keys tried) - max_entries refused slots: run c had 0-16 with 64 keys at max 48. The directory fills once the distinct keys tried reach the slot count (2-4 times max_entries).
- **Who hits it.** This is about the number of keys, not the kind. An undersized map keyed by block or warp hits it too, and the refused slots last for the life of the device copy.
- **One key per thread.** Every thread that won a CAS before the count reached max_entries tried to reserve. At max 1,024, 994-1,023 winners per trial were refused, so 2,018-2,047 of 2,048 slots ended non-empty.
- At max 48 the directory (128 slots) filled completely, and every later insert took the full-directory path.
- The kernels then ran far longer: 10 launches took 1,573 ms at max 1,024 and 433 ms at max 48, against 6.4-10.8 ms in runs a-d. Lookups and inserts of absent keys walk until an empty slot, and almost none are left. The split of that time was not measured.
- Stage one does not serve per-thread keys (they get E2BIG, and their storage is 1530). But any map given far more distinct keys than max_entries pays this cost, so it is a limit to document and a problem for the build plan. Round one's order would not have it, by construction (not measured with per-thread keys): there a thread claims a slot only after it holds a reservation, so claimed slots never exceed max_entries. It had the spurious E2BIG instead, which hits the counter idiom that stage one is for.
## C6: the publish order (`c6/`)

**Reading first.** These conclusions come from reading the code.
- `RULES-round2.md` stated them before any run.
- `c6/C6-report.md` wrote its own version after its shakedown runs, which had already shown variant D's weak outcomes, and before its rule runs. It corrected four passages after the runs, keeping the original wording.
- **Kernel code.** Lookups take the row from the same 64-bit word as the state, and no GPU code reads a row's values. So nothing in a kernel depends on the publish order.
- **The delivery snapshot is the one reader that pairs a header with row contents.** It is a kernel on the delivery stream, run on a schedule while application kernels run (`nv_gpu_model_maps.cpp`, `deliver_locked`). Today it loads with `ld.relaxed` and no fence.
  - For the hash map it must read each header with an acquire pattern before it sums that row.
  - A violation would be a key delivered with sum 0 (the winner's first add missing) in a snapshot taken mid-run.
- **The guarantee** (PTX ISA 9.5, 8.8 and 8.9.4, quoted in `c6/C6-report.md`): the winner's `st.release` on the header is a release pattern. It synchronizes with the snapshot's acquire pattern on the header when the snapshot reads that value.

**Test.** Message passing at scale:
- A writer adds 1 to its data word with `red.global.add.u64`, then sets its flag in the publish form under test.
- A reader in another block polls the flag, applies the acquire form under test, then loads the data word.
- A weak outcome is flag 1 with data 0.
- Each launch randomly pairs 128 writer warps with 128 reader warps (lane 0 only), and 32 more blocks stress memory. Data and flags sit in separate page-aligned regions, each instance on freshly placed 128-byte cells, and the SM of every writer and reader is recorded.
- Each variant ran 10^8 instances under the GPU lock, and every run reached the instance limit before the time limit. Every instance saw its flag, with no timeouts.

| variant | publish | reader | weak outcomes | on the same SM | on different SMs |
|---|---|---|---:|---:|---:|
| A (the design) | `st.release.gpu` | `fence.acq_rel.gpu` | **0** | 0 of 11,151,702 | 0 of 88,848,298 |
| B | `st.relaxed.gpu` | fence | 0 | 0 | 0 |
| C | `st.release.gpu` | no fence | 0 | 0 | 0 |
| D | `st.relaxed.gpu` | no fence | **78,461** | 0 | 78,461 of 88,848,298 |

- **Harness checks** (10^7 instances each) show the counting can see a weak outcome:
  - flag written before the add, with a wait between: 5,991,449 weak;
  - the same with no wait: 10,935 weak.
- **SASS of each variant:** the writer's release is `MEMBAR.ALL.GPU; ERRBAR` before `STG.E.64.STRONG.GPU`, and the reader's fence is `MEMBAR.ALL.GPU; ERRBAR; CCTL.IVALL`. Each variant's PTX differs from A only in the publish line, the fence line, or both.

**What this shows:**
- The tested form of the design (A: `st.release` publish; the reader loads with `ld.relaxed`, then `fence.acq_rel`) gave no weak outcome in 10^8 instances. The build's snapshot will read headers with `ld.acquire.gpu` instead ("An acquire operation on M", the first acquire pattern in 8.8). That form rests on the document, not on this test.
- This test can see a publish that has neither the release nor the acquire (D: 78,461 times, every one across SMs).
- Leaving out only one side (B or C) gave no weak outcome on this device. The PTX ISA guarantees the order only with both a release pattern and an acquire pattern, so the design keeps both: the release on the publish, and an acquire pattern in the snapshot.
- **The rules missed this case.** They named an outcome only for "B, C and D all clean" and for "A weak". "D weak, B and C clean" is reported here as measured, with no further rule applied.

## R2: an independent reading of the v2 code (`review/`)

**Setup.**
- Three reviewers got only the code (`c5-device-part.cu`), a statement of what it must provide (`CONTRACT.md`) and PTX ISA 9.5 chapter 8. They did not get the argument above.
- Each read with one lens: the memory model; bpf semantics; progress and capacity.
- Each finding then went to a separate reviewer told to refute it.
- 11 findings came back and none was refuted (`review/review.json`).
- Finding 1 was checked step by step against the code and the kernel source. The other ten were read in full, with their refuters' corrections.

| # | finding | lens | outcome |
|---|---|---|---|
| 1 | Thread A gets E2BIG because K1's winner took the last place but has not yet published. A then looks up K1 and gets NULL. No one-at-a-time order explains that. | bpf semantics | **Confirmed. bpf has the same window**; see below. Documented, not fixed, against the rules (below). |
| 2 | The full-directory wait can run out with the count below max_entries, and still returns E2BIG. | bpf semantics, progress | Confirmed. The case is counted (unstable) and goes into the loss counts. It never happened in C5 (0 in every run). |
| 3 | Chapter 8 has no clause that makes a store visible within bounded time. Inside one launch, "a lookup after a completed insert finds the key" therefore rests on the hardware. Across launches the document guarantees it (stream order). | memory model, bpf semantics | Confirmed. It is documented; every GPU spin-wait rests on the same thing. |
| 4 | The stress test's reader is a GPU kernel; the real reader would be the CPU. | memory model | The real reader is the delivery snapshot, which is a GPU kernel at `.gpu` scope; the CPU reads its output after the stream completes. Noted for the build. |
| 5 | The `red` adds carry no `.sem` and no scope. | memory model | The defaults are `.relaxed` and `.gpu` (the `red` description), so the adds are strong. The build writes them out. |
| 6 | The acquire-pattern text names `fence.acquire`, while the reader uses `fence.acq_rel`. | memory model | The fence description says `fence.acq_rel` synchronizes "as described in acquire and release patterns", but the pattern text does not name it. The snapshot will use `ld.acquire.gpu` on headers (pattern case 1), which avoids the question. |
| 7 | Refused slots are permanent, can fill the whole directory, and then every absent key walks all slots. | progress | Confirmed and measured in C5 (runs e, f). |
| 8 | A wait can run out on a claim that ends refused: EEXIST (unstable) for a key that never gets in. | progress | Confirmed. Unstable EEXIST can mean either outcome; documented. |
| 9 | The code relies on host preconditions it does not check: rows_cap >= max_entries, and S is a power of two. | progress | Confirmed. Map creation must enforce both. |
| 10, 11 | Restatements of 3 and 2 from another lens. | | as 3 and 2 |

**Finding 1 against bpf.** The kernel's own htab has the same window (`kernel/bpf/hashtab.c` in the board's L4T r39.2.1 source, Ubuntu noble 6.8).
- `htab_map_update_elem` takes only the new key's bucket lock (`htab_lock_bucket`).
- It then takes capacity in `alloc_htab_elem`: `__pcpu_freelist_pop`, which returns `-E2BIG` when nothing is left, or `is_map_full`/`inc_elem_count` for maps that are not preallocated.
- Only after that does it link the element (`hlist_nulls_add_head_rcu`).
- Lookups (`__htab_map_lookup_elem`) take no lock.

So an insert into another bucket can get E2BIG while the key that took the last element is not yet linked, and a lookup of that key then misses. The contract written for the review ("as if one at a time") was stricter than bpf. The v2 window has the same shape: the add, the shard add, then the publish.

**Its length differs.** The kernel keeps the window open under its bucket lock, with preemption and interrupts off (`htab_lock_bucket`: `preempt_disable`, `local_irq_save`, `raw_spin_lock`). Nothing documented bounds how long a GPU thread can sit between its add and its publish.

**The rules are set aside here.** `RULES-round2.md` says a confirmed protocol fault means a fix and C5 again.
- **Why it is documented instead:** the behaviour matches bpf's own htab, and a fix costs on every insert.
- **The reviewer's fix:** a second counter, bumped after each live publish, gates every E2BIG path with a bounded wait until it reaches max_entries. That is one more atomic per successful insert, and a wait before E2BIG.
- **The user's decision (11-27): document it,** as bpf's htab has it. No second counter.

## The build, mapped (`code-map-stage1.json`)

Five readers mapped where a device hash map type would go in bpftime (prototype branch `proto/map-model-70f7bc1f`). The map is agent output. The facts DESIGN.md now uses were checked in the code: 1531's `elem_update` returns EPERM and its `elem_delete` EINVAL (`nv_gpu_model_maps.cpp`), and `MapBasicInfo` has one `extra_buffer` pointer (`nv_attach_impl.hpp`). Besides the touch points, they found items the design has to settle before building:

**Design statements to fix:**
- **Walk termination.** DESIGN.md says "at most max_entries are claimed, so at least half stay empty and every probe sequence ends". That no longer holds with refused slots. Walks are bounded by the slot count instead, as C5 does.
- **CPU delete.** DESIGN.md says CPU delete returns EPERM "as 1531 and 1532 do"; 1531's delete returns EINVAL. The device hash returns EPERM, and the text is corrected.

**Loss counts:**
- On first touch with a constant key, nearly every thread gets EEXIST. One device word for that count is a hot spot (one word took 1.09-1.10 ns per thread in the prototype), so the counts need shards, as 1531's values have.
- The tool reads them through the companion status array.

**The CPU side:**
- **Before the device copy** the map takes CPU updates with bpf's hash semantics, which 1531 never does. The copy must seal the map against updates still in flight.
- **After the copy,** the CPU should not walk the delivered directory by open addressing. The snapshot reads slots one by one, so a slot between a key's home and its slot can be delivered empty while the key is delivered live. The CPU builds its own index from the delivered live headers instead.
- **The snapshot** reads each header with an acquire pattern before summing its row (C6).

**Layout and failures:**
- **One layout definition.** The hash, the header encoding and the row layout are needed in the trampoline, the routing pass, the snapshot and the agent's seeding. 1531's shard formula is already written three times. One shared header (`gpu_helper_protocol.h`) holds them.
- `MapBasicInfo` carries one device pointer per map, so the geometry goes into a header at the start of the device block.
- **No device copy** (another process owns the map): lookups return NULL and inserts return an error, as 1531 does. A host call there would be wrong.
- **Owner takeover:** a process that takes over the map from a dead owner gets a fresh block, so it must seed the directory again from the delivered keys.

## What this decides

- **v2 replaces round one's order.** E2BIG no longer appears while keys fit: 0 in 2,805 trials, against 7,061-11,695 per trial before. The claim, the precheck re-read and the reservation held in every run, and each control failed as required.
- **The publish keeps the release, and the snapshot gets an acquire pattern.**
  - The tested pair (release, then `ld.relaxed` + `fence.acq_rel`) gave 0 weak outcomes in 10^8. The test sees the reordering when both halves are missing (78,461 in 10^8).
  - The build's `ld.acquire.gpu` rests on the document.
- **Documented, not changed:**
  - the E2BIG/lookup window, which bpf's htab has too (against the rules' "fix and C5 again"; the user's decision, 11-27);
  - unstable outcomes, counted, for waits that run out;
  - visibility within one launch rests on the hardware.
- **A limit to document, and a problem for the build plan:** refused slots number up to (distinct keys tried) - max_entries and fill the directory once the distinct keys reach the slot count. Misses then walk all of it (C5 e, f: 1,573 ms and 433 ms for 10 launches, against 6.4-10.8 ms). Any undersized map hits this, not only per-thread keys (which belong in 1530).
- **Next:** build stage one in bpftime on `proto/map-model-70f7bc1f`, with the items in "The build, mapped" settled in the plan first.

## Files

- `RULES-round2.md`: the rules.
- `c5/`:
  - `src/c5.cu`, `build.sh`, `run_rules.sh`;
  - `logs/` (one log per run, `run_rules.out` the summary);
  - `build/` (PTX, SASS and ptxas output of each build; binaries left on the board).
- `c6/`:
  - `C6-report.md`, with the reading first and the corrections made after the runs;
  - `src/c6.cu`, scripts, `logs/`, `build/` (PTX and SASS).
- `review/`: what the reviewers got (`CONTRACT.md`, `c5-device-part.cu`, the chapter 8 text), and every reviewer's and refuter's full answer (`review.json`).
- `code-map-stage1.json`: the five code readers' answers.
