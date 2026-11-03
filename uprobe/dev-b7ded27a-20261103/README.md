# Review fixes for hash map step 2 (dev 199e23ec, 48767d88, b7ded27a), 2026-11-03 16:20-17:00

Fixes for `../dev-e5ba4aa7-20261103/REVIEW.md`:

| commit | finding | change | test, mutation |
|---|---|---|---|
| 199e23ec | 1 | delete unlinks (release), then sets the new generation (release) | "hash map lookups pass a delete stopped halfway": swapped stores fail the chain check (2 == 1); without it the child's lookups do not return in 2 s |
| 48767d88 | 6 | per-map random seed: XORed into 4/8 B keys, start of the mix for longer keys | "hash map chains stay short for keys made to collide": longest chain 3 (8 B and 16 B constructed keys); seed 0: 1000 and 1000; no seed in the long-key start: 16 B 1000 |
| b7ded27a | 2, 3, 4, 5, 9 | comments, CLAUDE.md, test headers corrected; DESIGN.md "As built" section | gap tests: 2 entries / 6 lists (mutation: no stealing, second insert fails), `lookup_read` with 8/16 B keys |

Findings 2 (dead free-list holder hangs inserts) and 3 (in-place update tears values) are
documented, not changed. Candidate gate on b7ded27a: green, same results as on e5ba4aa7
(`gate-b7ded27a.log`). The `collide` repro rebuilt on the fixed header: chain of 2, 45 ns per
insert (was 1000, 2245 ns; timed during a compile, the chain is the point).

## Suite hash cases (cpu5, 5 reps of 50000, ns per call as `run_hash.py` reports)

`suite.jsonl` (`suite-summary.txt`): e5ba4aa7 vs b7ded27a. `suite-diag.jsonl`: the same two plus
`exp-noseed` (b7ded27a's tree with 199e23ec's header: delete fix, no seed) and `exp-seed-noxor`
(the `_seed` member, no XOR for 4/8 B keys). Builds in `../ssl-nginx-2x2-20261023/builds/`.

| case (diag run) | e5ba4aa7 | b7ded27a | exp-noseed | exp-seed-noxor |
|---|---:|---:|---:|---:|
| hash_map_lookup | 16072 | 18110 (+12.7%) | 16075 (+0.0%) | 16628 (+3.5%) |
| hash_map_update | 26449 | 25182 | 27022 | 26517 |
| hash_map_delete | 36342 | 34354 | 36312 (-0.1%) | 35356 |
| per_cpu_hash_map_lookup | 23519 | 24572 (+4.5%) | 23198 | 23464 |
| per_cpu_hash_map_delete | 65364 | 64231 | 65598 | 65777 |

- The delete fix costs nothing (exp-noseed equals e5ba4aa7 within 0.1%).
- The seed costs ~2 ns per lookup (+12.7% suite hash lookup, +4.5% per-CPU hash lookup). The
  XOR is one instruction, but the seed is loaded from the map object, whose pointer arrives late:
  before, the key's multiply ran while that pointer was loaded; now load seed -> XOR -> multiply
  -> shift is on the path to the head load. The member alone (no XOR) moves lookup +3.5% (layout).
- Update and delete are 4-5% faster in b7ded27a in both runs; not explained (layout), not claimed.
- `read`/`write` bimodal as before.

Decision (11-03): the seed stays, at ~2 ns per lookup (CLAUDE.md, dev 603c81f7).
