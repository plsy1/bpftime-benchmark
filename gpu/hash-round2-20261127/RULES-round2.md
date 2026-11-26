# Device hash map, round-two checks (rules written before any run, 2026-11-26)

Round one (results gpu/hash-round1-20261126) found two open items:
1. **E2BIG with the map half full.** Threads reserve capacity at the empty slot, right before their CAS, so every thread that loses the CAS to the same key held a reservation for a while.
2. **The release publish was not observable** by the stress test.

The user's decisions are in DESIGN.md (11-26). Nothing is built in bpftime; these checks decide the insert path stage one builds.

## The protocol change (v2): claim, then reserve

**insert(K, BPF_NOEXIST).** Walk from K's home slot with `ld.relaxed.gpu`. At each slot:
- **live K:** EEXIST.
- **claiming K:** a bounded wait until the slot turns live or refused.
  - live: EEXIST;
  - refused: E2BIG;
  - the bound runs out: EEXIST, counted unstable.
- **refused K:** E2BIG.
- **another key** (live, claiming or refused): the next slot.
- **empty slot:**
  1. Precheck: `ld.acquire.gpu` of the reservation count. If it is at least max_entries, read the slot again; if it is still empty, E2BIG; otherwise handle the slot again.
  2. CAS empty -> claiming(K).
     - **Won:** `atom.acq_rel.gpu.add` 1 to the count.
       - The old value below max_entries: row = old value, add into the shard, `st.release.gpu` live(K, row).
       - Otherwise: `st.release.gpu` refused(K), E2BIG.
     - **Lost to K:** wait, as for "claiming K".
     - **Lost to another key:** the next slot.
- **All S slots walked** (no empty slot, no K): a bounded wait until the count is at least max_entries, then E2BIG; if the bound runs out, E2BIG, counted unstable.

**lookup(K):** a claiming or refused K returns NULL.

**Properties:**
- The count only grows (no give-backs).
- Refused slots are permanent.
- Rows are the count's old values, so they are dense below max_entries.

**Dependency (written down here):** E2BIG is exact only while capacity never comes back. Stage one has no GPU deletes, and CPU update and delete return EPERM after the device copy. Deletes would need another rule.

## C5: the v2 protocol by hand (standalone CUDA, as C4)

**Source:** `r2/src/c5.cu`, from `c4.cu`. It has the v2 insert, a runtime key count (64 keys as before, or one key per thread), and builds chosen by -DFAULT.

**How runs are made:**
- Each run is a fresh process. One GPU process runs at a time, under a lock.
- Every run has a total timeout.
- Each trial uses fresh zeroed memory and a new seed.
- Thread count and launches are as in C4: 10^6 x 10.

**Runs:**

| run | keys | max_entries | other | rule run + trials |
|---|---|---:|---|---|
| a | 64 | 128 | | 1 + 1,000 |
| b | 64 | 64 | | 1 + 1,000 |
| c | 64 | 48 | | 1 + 1,000 |
| d | 64 | 128 | 16 keys inserted beforehand | 1 + 200 |
| e | one per thread | 1,024 | | 1 + 20 |
| f | one per thread | 48 | | 1 + 20 |
| g | 64 | 128 | wait bound 0 | 1 + 300 |
| h | 64 | 128 | concurrent reader | 1 + 300 |

**Checks (violations).** These are round one's checks, adjusted for v2:
- No key in two slots, and no key with both a live and a refused slot.
- Live slots <= max_entries, and live = min(keys, max_entries).
- Count = live + refused slots (every winner adds once).
- Rows of live headers are unique and below max_entries.
- Per key, delivered = attempts - EEXIST - E2BIG (+ the inserted value).
- **E2BIG = 0 whenever keys <= max_entries** (runs a, b, d, g, h). This is the round-one bug.
- **Per key: E2BIG > 0 implies the key is not live at the end.**
- **Per key: EEXIST not counted unstable > 0 implies the key is live at the end.**
- EEXIST (not unstable) followed by NULL from the thread's own lookup = 0.
- No slot is left claiming.
- No walk passes S slots, except through the full-directory path (which is counted).

**Reported, not violations:** refused slots per trial (and the largest), E2BIG from the full-directory path, unstable counts, wait times, and EEXIST then NULL in run g.

**Controls.** Each must fail the check named, or be reported as not seen by this test:

| control | build | run | must fail |
|---|---|---|---|
| 1 | the claim is a plain store instead of the CAS | 64 keys, max 128 | sums or winners |
| 2 | winners never check the count | 64 keys, max 48 | live > max_entries |
| 5 | the precheck returns E2BIG without reading the slot again | 64 keys, max 64 | E2BIG on a key that ends live |
| 6 | round one's order: reserve at the empty slot before the CAS, give back on a loss | 64 keys, max 128 | E2BIG with keys <= max_entries |

Each control gets 1 rule run + 200 trials.

**Rule:**
- Any violation in the protocol build stops stage one's build until it is explained.
- Zero violations read as "consistent with the model", not as proof.

## C6: the publish order

**Reading first** (recorded in the write-up before the test runs):
- **Kernel paths.** Lookups take the row from the same 64-bit word as the state, and no GPU code reads a row's values. So the kernel paths do not depend on the publish order.
- **The delivery snapshot.** The reader that pairs a header with row contents is the delivery snapshot: a kernel on the delivery stream that runs while application kernels run. Today it uses `ld.relaxed` with no fence (`nv_gpu_model_maps.cpp`).
  - For the hash map, it must read each header with an acquire pattern before summing that header's row.
  - A violation would show as a key with sum 0 in a mid-run snapshot (the winner's first add missing).
- **The guarantee,** quoted from PTX ISA 9.5, sections 8.8 and 8.9.4: the winner's `st.release` on the header is a release pattern, and the snapshot's acquire pattern on the header synchronizes with it when it reads that value.

**Test:** a message-passing test, not the protocol.
- **Writer threads:** `red.global.add.u64` 1 to data[i] (only this writer writes it), then flag[i] = 1 in the publish form under test.
- **Reader threads,** in other blocks: spin on flag[i] with `ld.relaxed.gpu`, then the acquire form under test, then `ld.relaxed.gpu` of data[i].
- **A weak outcome:** flag 1 and data 0.
- **Layout:** data and flag lie in different pages. Extra blocks stress memory, as GPU litmus tests do. The SM of each writer and reader is recorded.

**Variants:**

| | publish | reader | weak outcomes |
|---|---|---|---|
| A | `st.release.gpu` | `fence.acq_rel.gpu` | must be 0 |
| B | `st.relaxed.gpu` | `fence.acq_rel.gpu` | control |
| C | `st.release.gpu` | no fence | control |
| D | `st.relaxed.gpu` | no fence | control |

**Stop rule and outcome:**
- Each variant runs 10^8 instances or 10 minutes, whichever comes first.
- If B, C and D show no weak outcome: "not observable on this device by this test", and the design rests on the PTX ISA text.
- A weak outcome in A stops the build.

## R2: an independent reading of the v2 insert path

**Who and what:**
- Three reviewers get the source (c5.cu's protocol part) and the PTX ISA memory-model sections, not the argument above.
- Each reads with one lens:
  1. the PTX memory model: does every visibility the code relies on follow from the document;
  2. bpf semantics: can every result of lookup and insert (NULL, EEXIST, E2BIG) be explained by some order of the operations;
  3. progress and capacity: walk termination, refused slots, a full directory, the waits.

**Rule:**
- Every finding is checked against the code.
- A confirmed fault in the protocol means a fix and C5 again.
