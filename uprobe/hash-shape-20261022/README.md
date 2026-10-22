# Hash lookup shape: key order, fill, value use and misses (2026-10-22)

Purpose: (1) split why the official benchmark shows ordinary hash lookup at
about 0.95x kernel while the consuming hash test shows about 1.2x, and (2)
measure miss cost for a full table, a half-full table and a half-full table
after insert/delete churn. Runtime: `c3090b59` (same frozen artifacts as
`../hash-helper-fixes-20261022/full10-c3090b59`), unchanged hash map.

Workload (`source/`, experiment-only, not in the bpftime tree; built by
`build-workloads.sh`): one 1024-entry u32->u64 hash data map and one hash
result map (single map type, so no JIT binding and the typed hash helper is
used). Loader inserts keys 0..fill-1 (value key*17+5); `churn512` then runs
64 cycles of inserting and deleting 512 other keys. Each call does 1000
lookups:

| Case | Keys | Value used |
|---|---|---|
| control | random, no map access | - |
| seq_unused | 0..999 in order (wrapping at fill) | no, like the official benchmark |
| seq | same | yes |
| rand_hit | random present keys | yes |
| rand_miss | random keys never inserted | yes |

Workloads: `full1024` (fill 1024), `official1000` (fill 1000, like the
official benchmark), `half512` (fill 512), `churn512` (fill 512 after churn).

`run-shape.py`: kernel and c3090b59 alternating, 6 rounds, 20000 calls per
case, CPU 5 at 1.728 GHz, checksums/hit/miss verified (except seq_unused).
Exit 0. A CPU-bound simulation ran on another core during rounds 1-2 by
mistake; bpftime medians of rounds 1-2 vs 3-6 differ by at most 1.2%.

Raw ns per lookup (median), ratio to same-run kernel:

| Workload | Case | Kernel | bpftime | Ratio |
|---|---|---:|---:|---:|
| full1024 | seq_unused | 28.94 | 26.87 | 0.93x |
| full1024 | seq | 29.70 | 27.21 | 0.92x |
| full1024 | rand_hit | 31.88 | 37.11 | 1.16x |
| full1024 | rand_miss | 35.33 | 676.83 | **19.2x** |
| official1000 | seq_unused | 29.11 | 27.02 | 0.93x |
| official1000 | seq | 29.19 | 27.16 | 0.93x |
| official1000 | rand_hit | 34.02 | 36.87 | 1.08x |
| official1000 | rand_miss | 34.90 | 83.22 | 2.38x |
| half512 | seq | 25.22 | 23.77 | 0.94x |
| half512 | rand_hit | 26.06 | 29.55 | 1.13x |
| half512 | rand_miss | 28.89 | 38.88 | 1.35x |
| churn512 | seq | 25.22 | 23.88 | 0.95x |
| churn512 | rand_hit | 26.12 | 29.05 | 1.11x |
| churn512 | rand_miss | 28.74 | 2254.07 | **78.4x** |

Control: bpftime 1.61-2.14, kernel 3.78-5.55 (official1000 has a modulo by
1000 in the key computation).

Findings:

1. Using the returned value does not matter (seq_unused vs seq equal).
2. Key order is the main reason for 0.93x vs ~1.15x on hits: in-order keys
   give 27 ns, random keys 37 ns on the same full table; kernel changes by
   only ~2-5 ns. Table fill (1000 vs 1024) does not change bpftime hits.
3. Misses depend on free buckets: 19x on a full table (7 empty buckets of
   1031), 2.4x with 31 empty buckets, 1.35x half full, but 78x on a half-full
   table after churn: deleted buckets are never turned back into empty ones,
   so a miss scans most of the table.
