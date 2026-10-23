# Shrinking hash probe bound (af10949b, 2026-10-22)

Commit `af10949b` (local, not pushed) on top of `f26cc335`.

Why the "half-full table after many inserts/deletes, pure misses" case was
still 1.62x kernel after `1cb20807`: a simulation of the exact algorithm
(`/tmp` scratch, not kept) for the churn512 workload (keys 0..511, then 64
cycles of inserting and deleting 512 other keys) left no empty bucket at all
(2053 buckets: 512 filled, 1541 deleted). Misses therefore stop only at the
probe bound, which was 4 although every live element sat at distance 0: the
bound only grew, and elements placed far during the churn kept it high after
they were deleted. Each miss probed 5 buckets.

Change: `bpftime_hash_map` counts filled buckets per exact distance from the
home bucket (64 tracked distances, 256 bytes per map; farther elements are
counted separately and keep the bound conservative while any remain) and
lowers the bound when the farthest elements are deleted. New test in
`[bpftime_hash_map]` checks the bound across the tracked and far ranges.
Regression filter: 75 cases / 239572 assertions (`tests.log`).

## A/B against f26cc335 (`shape6/`, same run, with kernel)

`run-shape-ab.py` on the `../hash-shape-20261022` workloads, 6 rounds, 20000
calls, raw ns per lookup:

| Table | Case | Kernel | f26cc335 | af10949b | Change |
|---|---|---:|---:|---:|---:|
| half 512 after churn | random miss | 28.61 | 46.90 (1.64x) | 29.60 (1.03x) | **-36.9%** |
| half 512 after churn | random hit | 26.22 | 25.23 (0.96x) | 26.07 (0.99x) | +3.3% |
| half 512 | random miss | 28.77 | 28.75 (1.00x) | 29.05 (1.01x) | +1.0% |
| full 1024 | random miss | 35.11 | 32.94 (0.94x) | 32.80 (0.93x) | -0.4% |
| full 1024 | random hit | 31.63 | 24.50 (0.77x) | 24.52 (0.78x) | +0.1% |
| 1000 keys | random hit | 34.12 | 25.37 (0.74x) | 25.98 (0.76x) | +2.4% |
| 1000 keys | in-order hit | 29.53 | 24.10 (0.82x) | 24.88 (0.84x) | +3.2% |

Lookup hits do not execute changed code (only delete and insert do); their
+-3% moves are run-to-run or layout variation.

## Full ten rounds and the hash delete side effect

`full10-af10949b/` (userspace only): all map rows within +-1% of f26cc335
except ordinary hash delete 36.86 -> 39.23 ns (+6.4%). AB/BA screen
(`full10-af10949b/abba-screen/`, 20 rounds): hash delete median paired ratio
1.064, slower in 20/20 pairs.

Two follow-ups on top of af10949b (local commits):
- `dffb2fce` returns from the count update unless the last element at the
  current largest distance is deleted. Three-way screen (`fastpath/`, 24
  rounds): hash delete f26cc335 37.01, af10949b 38.48 (1.048), dffb2fce 38.85
  (1.050): no gain.
- `ccb0ade1` skipped counting for distance 0. Logically less work, but the
  three-way screen (`skip-home/`) showed hash delete 43.44 ns (1.172).
  Dropped (branch reset to dffb2fce before it was pushed).

Machine code: the hash delete path is inlined into
`bpftime_map_delete_elem_helper`, a ~9.7 KB function holding every map
type's delete; small source changes re-allocate registers and reorder blocks
across the whole function, so delete speed moved by +3% to +17% without a
change in logic. Addressed by the typed hash delete helper in
`../hash-typed-delete-20261023`.
