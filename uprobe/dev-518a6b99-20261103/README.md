# Hash map: updates of existing keys take a spare element (dev 518a6b99), 2026-11-03 17:30-18:35

Review finding 3 of `../dev-e5ba4aa7-20261103/REVIEW.md`: an update of an existing key wrote the
value in place and concurrent readers mixed halves of two values. Now, as the kernel's
preallocated htab (`alloc_htab_elem` with the per-CPU `extra_elems`), the update writes the new
key and value into a spare element, links it at the head (in the old element's place with one
head store when the old element is the head), unlinks and retires the old element (new
generation) and keeps it as the spare. One spare per stripe, under the stripe lock.

Checked first: the per-CPU hash only inserts with `no_exist` (its table values are slot numbers,
never updated); the JIT does not inline hash updates; `get_index_of_value` has no callers.

## Tests (unit tests in `test_hash_map_chains.cpp`)

- "hash map copies of a value being updated are whole": 16 B `{x, x}`, two updaters, three
  readers, 1.5 s: 0 torn of 55 million `lookup_read` copies, 75 torn pointer reads (an old
  element reused by a later update while the reader holds it; the kernel has the same window).
  In place (`../dev-e5ba4aa7-20261103/review-repro`, 5 s): 995,658 torn copies, 391,877 torn
  pointer reads.
- "hash map updates of existing keys replace their element": a key alone in its chain stays
  found across 1000 updates, a full map still takes updates, capacity stays exact.
- Mutations: in place -> torn copies, pointer unchanged; the old head kept as the new element's
  next -> lookups never return (timeout); lookup index bound left at max_entries -> lookups of a
  replaced key never return (timeout).
- Candidate gate green (`gate-518a6b99.log`; `gate-f9b92cee.log` is the first version's).

## Cost (suite hash cases, cpu5, 5 reps of 50000; ns per probe of 1000 operations)

`suite.jsonl` (`suite-summary.txt`): e5ba4aa7, b7ded27a, f9b92cee (first version: one spare per
free list, taken with an atomic exchange). `suite-stripe.jsonl`: b7ded27a, f9b92cee,
`exp-spare-stripe` (binaries identical to dev-518a6b99).

| case | b7ded27a (in place) | f9b92cee (spare per list) | 518a6b99 (spare per stripe) | kernel (full10, 11-03) |
|---|---:|---:|---:|---:|
| hash_map_update (existing keys) | 24987 | 47229 | 34649 (+38.7%) | 95844 |
| hash_map_lookup | 17488 | 18024 | 18052 (+3.2%) | 28619 |
| hash_map_delete | 35051 | 35626 | 35560 (+1.5%) | 107785 |
| per_cpu_hash_map_update | 34874 | 34860 | 34444 | |
| per_cpu_hash_map_lookup | 24834 | 25189 | 24460 | |

- Why the first version cost 22 ns: `perf record -e cycles:u` on the victim (`profile-logs`, 20000
  iters; `profile-symbols.txt`): against b7ded27a, `fix_size_hash_map_impl::elem_update` +980
  samples and the exchange's LSE helper `__aarch64_swp4_acq_rel` +1055 new, the spin lock's
  `__aarch64_swp4_acq` +292. The stripe's spare needs no atomic (the update holds the stripe lock).
- What remains (+9.7 ns): the release fence before writing the spare, the release stores of the
  word, the head, the unlink and the retirement, and a second element's lines.
- Memory: spares are heads / 16 elements (at most 4096): up to 1/8 of max_entries (10240 entries:
  1024 spares), 1/16 at a power of two (1024: 64), 0.4% at 1M entries.

## Semantics against the in-place table (the kernel's)

- A value pointer from a lookup follows the element: valid until the key is updated or deleted;
  a BPF `*v += 1` racing an update of that key goes to the old element and is lost.
- An updated key moves to the head of its chain: `next_key` during updates may return it again.

## Rerun on 518a6b99 (19:24-20:21): key-pattern matrix and full ten-round suite

`run-rerun.sh` (`run-rerun.log`), cpu5 1728000 before and after, no leftovers.

Full ten rounds (`full10-518a6b99/comparison.md`, against e5ba4aa7's of this morning, kernel
re-run in the same suite): hash update +28.4% (0.36x kernel), lookup +11.6% (0.63x), delete
-4.6% (0.32x); per-CPU hash update +2.9%, lookup +5.8%, delete -1.5%; every other case within 2%;
embed 155.6 ns (-0.2%). Exact-artifact activation audit passed, 17 cases. Same benchmark
binaries as the morning run (artifact-sha256.txt).

Key-pattern matrix (`keys/summary.md`, 306 runs, all exact; kernel, e5ba4aa7, 518a6b99; one op =
lookup + delete of the oldest key + insert of a new one, no update of an existing key). One
thread, map cost against e5ba4aa7:

- rand32 0.99-1.01x, seq64 1.02-1.05x, shl12 0.96-1.07x;
- **addr (8 B scattered addresses) 1.11-1.28x**: 0.184 -> 0.204 us at 10240 / 90%, 0.245 -> 0.313
  us at 102400 / 90%;
- against the kernel 0.28-0.68x (was 0.27-0.62x).

Six threads: totals 4.90-5.20 us for both builds (the probe path; the map cost is below the
control's noise, as this morning).

The seed, not the spare (this workload never updates an existing key): without a seed the
Fibonacci multiply spreads keys built from an arithmetic progression better than at random;
XORed with a seed they spread like random keys, as with the kernel's jhash and hashrnd.
`chains/chains.cpp` (the matrix's live sets, mean chain length at a hit, seed 0 vs seeded):
addr at 10240 1.00 / 1.15 -> 1.31 / 1.59 (50% / 90%; rand32 1.31 / 1.54); at 102400 the chains
change little (1.46 / 1.58 -> 1.37-1.40 / 1.69) but the cost rises most, so locality between the
deleted and the inserted key (ids a fixed distance apart) probably mattered too (not checked).

## Seed switch (dev 7670cee5)

After this rerun: hash and per-CPU hash maps keep seed 0 with the kernel's `BPF_F_ZERO_SEED` or with
`BPFTIME_HASH_ZERO_SEED` set (not 0) in the creating process; seeded by default, as the kernel.
Gate green (`gate-7670cee5.log`).
