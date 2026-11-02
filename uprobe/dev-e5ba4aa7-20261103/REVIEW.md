# Review of hash map step 2 (dev e5ba4aa7), 2026-11-03

One Fable 5.1 subagent (max effort), read-only: the table, its callers, the tests, DESIGN.md,
CLAUDE.md, kernel v6.8 `hashtab.c` / `percpu_freelist.c`. Nine findings; each checked against
the code (and the L4T kernel-noble 6.8 source for 3) by the main session. Nothing built or run.

| # | Finding | Verdict | Since |
|---|---|---|---|
| 1 | `elem_delete` clears USED (relaxed) before unlinking; a writer killed between the two stores leaves a !USED element linked, and every lookup reaching it restarts forever. Also live: a deleter preempted there makes lookups of that bucket spin until it runs again. | **Real** (hang, death or preemption window). Fix: unlink (release) first, then bump the generation; a death there leaks one element instead. | step 2 |
| 2 | `take_element` pass 1 takes other lists' locks blocking, while the stripe lock is held: a list whose holder died hangs every inserter that reaches it, and their stripes. Docs say it "blocks only its own users". | **Real** (hang after a process death only; the kernel also blocks, but cannot die). Fix or document. | step 2 |
| 3 | Updating an existing key copies the value in place; kernel 6.8 prealloc htab links a fresh element (per-CPU `extra_elems`) and unlinks the old one, so readers see old or new whole. CLAUDE.md says "as the kernel without BPF_F_LOCK". | **Real** doc mismatch; torn multi-word values possible. In place since before the redesign (keeps value pointers stable). Decision: replace-on-update or document. | all versions |
| 4 | DESIGN.md requires a per-step generation re-check; the code dropped it. | Code right (stale `next` leads to a free element, a terminator, or the head of the reader's own chain). Update DESIGN.md. | doc |
| 5 | E2BIG exact only without concurrent deletes (relaxed count hint, lists passed are not re-checked). | Real, same as the kernel's `___pcpu_freelist_pop`. Document. | step 2 |
| 6 | Unseeded multiplicative hash: colliding keys are trivial to construct (`k_j = j * phi^-1`). | Real, low for tracing keys (the reviewer's `i << 56` example does not collide: 256 keys, 256 buckets). A per-map random seed would cost ~nothing. | step 2 |
| 7 | 31-bit generation: a reader stalled across 2^31 reuses of one element could match. | Not practical; note only. | step 2 |
| 8 | Release fence before writing a reused element's key/value is redundant on ARMv8 (list lock orders it). | Keep: it is what makes the ordering hold in the C++ model; one barrier per insert. | step 2 |
| 9 | Stale text: CLAUDE.md claims of 1, 2, 3, 5; `next_key` also hangs on a dead stripe; test headers of `test_hash_map_lockfree_lookup.cpp` (base-31 hasher) and `test_hash_map_striped_writers.cpp` (probe runs, token pool). | Real. | doc |

Test gaps named by the review: writer death mid-delete (1), dead list holder with stealing (2),
torn in-place update (3), `next_key` during modification, mixed insert/delete at capacity (5),
`lookup_read` with 8/16 B keys, max_entries below the list count, a process killed mid-operation.

Checked and cleared by the review (and spot-checked): lookup vs insert ordering, ABA on reuse,
lookup termination and terminator encoding, torn keys, value alignment for 4/8/12/16/64 B keys,
stripe/list arithmetic, free-list ordering, lock order (no live deadlock), update flags,
delete-with-value, next_key order, per-CPU hash slot handling, spin lock protocol.

## Reproduced (`review-repro/`, 07:15-07:30, built against the e5ba4aa7 table header)

`repro.cpp`, one case per argument; logs `run1.log`, `run2.log`, `run3.log`. Not measurements.

- **1 reproduced.** The state a half-done delete leaves (USED cleared, element still linked) set by
  hand: a lookup of the key behind it and a miss in the bucket both did not return in 2 s; both
  finished correctly once the word was restored.
- **2 reproduced.** One list held (as by a dead process), the inserter's own list drained: the
  inserter blocked after 500 inserts with the map at 501 of 600, another writer of its stripe
  blocked too, a lookup in that stripe returned. After the release all inserts finished, E2BIG
  at exactly 600.
- **3 reproduced.** 16 B values written as {x, x} by two updaters, three readers for 5 s: 391,877
  of 364 million pointer reads and 995,658 `lookup_read` copies mixed old and new halves.
- **5 not reproduced.** Six threads each deleting then inserting (1-28 free elements at every
  moment, 22-40 million inserts) and three insert-only against three delete-only threads (1-16
  free, ~3.5 million inserts each): no E2BIG. The window (an element moves into a list the
  inserter already passed while another inserter takes the one it was heading for) is real by
  reading but narrow; the kernel's freelist has the same one.
- **6 reproduced.** 1000 keys `j * phi^-1`: all in one chain, 2245 ns per insert.
