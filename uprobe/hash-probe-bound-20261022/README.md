# Bounded hash probing (1cb20807, 2026-10-22)

Commit `1cb20807` on `codex/uprobe-official-177a1dc465ff` (local, not
pushed): `bpftime_hash_map` (the ordinary BPF hash map) now

- allocates `next_prime(2 * max_entries)` buckets (element limit unchanged,
  load at most about 50%; the map's memory doubles);
- turns a deleted bucket and the deleted run before it back into empty
  buckets when the next bucket is empty;
- tracks the largest distance of a filled bucket from its home bucket and
  stops lookup/update/delete probing there.

Filled buckets never move, so value pointers from lookups stay valid.
Frozen artifacts: incremental dev build of exactly this commit
(`head.txt`, clean tree). Regression filter: 74 cases / 239321 assertions
passed (`tests.log`), including new `[bpftime_hash_map]` tests (randomized
reference comparison under churn, value pointer stability).

## A/B against c3090b59 (same run, with kernel)

`shape6/` (`run-shape-ab.py`, workloads of `../hash-shape-20261022`, 6 rounds,
20000 calls), raw ns per lookup, ratio to same-run kernel:

| Table | Case | Kernel | c3090b59 | 1cb20807 | Change |
|---|---|---:|---:|---:|---:|
| full 1024 | in-order hit | 30.05 | 27.24 (0.91x) | 24.83 (0.83x) | -8.8% |
| full 1024 | random hit | 31.78 | 37.09 (1.17x) | 24.55 (0.77x) | -33.8% |
| full 1024 | random miss | 35.18 | 676.70 (19.23x) | 33.01 (0.94x) | -95.1% |
| 1000 keys | in-order hit | 29.39 | 27.12 (0.92x) | 23.95 (0.81x) | -11.7% |
| 1000 keys | random hit | 33.86 | 36.55 (1.08x) | 25.24 (0.75x) | -30.9% |
| 1000 keys | random miss | 34.94 | 83.14 (2.38x) | 33.31 (0.95x) | -59.9% |
| half 512 | in-order hit | 25.16 | 23.98 (0.95x) | 24.43 (0.97x) | +1.9% |
| half 512 | random hit | 25.84 | 28.53 (1.10x) | 26.10 (1.01x) | -8.5% |
| half 512 | random miss | 28.78 | 38.64 (1.34x) | 29.07 (1.01x) | -24.8% |
| half 512 after churn | in-order hit | 25.14 | 23.98 (0.95x) | 24.98 (0.99x) | +4.2% |
| half 512 after churn | random hit | 25.95 | 28.55 (1.10x) | 24.52 (0.94x) | -14.1% |
| half 512 after churn | random miss | 28.71 | 2250.52 (78.38x) | 46.64 (1.62x) | -97.9% |

`gap6/` (`run-gap-ab.py`, the three program shapes of
`../hash-lookup-helper-gap-20261021`, 6 rounds, 50000 calls):

| Program | Case | Kernel | c3090b59 | 1cb20807 | Change |
|---|---|---:|---:|---:|---:|
| hash only | all hit | 31.68 | 37.54 (1.19x) | 24.54 (0.77x) | -34.6% |
| hash only | about half missing | 41.78 | 359.49 (8.61x) | 33.61 (0.80x) | -90.7% |
| hash + global data | all hit | 32.23 | 37.92 (1.18x) | 25.94 (0.80x) | -31.6% |
| hash + global data | about half missing | 41.95 | 359.00 (8.56x) | 34.20 (0.82x) | -90.5% |
| hash + array (bound) | all hit | 32.12 | 44.23 (1.38x) | 30.98 (0.96x) | -29.9% |
| hash + array (bound) | about half missing | 41.82 | 365.61 (8.74x) | 41.58 (0.99x) | -88.6% |

Notes: in-order hits on a half-full table got 2-4% slower (the table is
twice as large, so consecutive keys spread over more cache lines); still
<= 0.99x kernel. After churn, misses stay at 1.62x: deleted buckets inside
probe runs remain until reused; probing is bounded by the largest
displacement instead of scanning the table.
