# Hash update/delete in three program shapes (2026-10-22)

Question: the two helper fixes (`baf0cc85` dispatch, `c3090b59` selection)
also changed update/delete paths, which so far only had unit tests. How do
hash update/delete perform in hash-only, hash + global data and hash + array
(JIT-bound) programs, before and after the fixes and the hash map change?

Workload (`source/`, experiment-only; `build-workloads.sh`): 1024-entry u32->u64
hash data map with keys 0..511 inserted; result map hash (or array for
`hash-array`). 1000 iterations per call over random keys 0..511:
`control` (no map access), `update` (rewrite a present key, BPF_EXIST),
`delete_insert` (delete a present key and insert it again, BPF_NOEXIST; two
map operations per iteration). The loader checks after each run that keys
0..511 with their values, and nothing else, remain.

`run-mutate.py`: kernel + bdd9835 + c3090b59 + 1cb20807 alternating, 6
rounds, 20000 calls, CPU 5 at 1.728 GHz, checksums/success counts/final table
verified. Exit 0. Helper selection and binding per build in
`gap6/helper-evidence.json` (hash-array is bound in every build; typed hash
helpers for hash-globals/hash-array only from c3090b59 on).

Raw ns per iteration (median), ratio to same-run kernel:

| Program | Case | Kernel | bdd9835 | c3090b59 | 1cb20807 |
|---|---|---:|---:|---:|---:|
| hash only | update | 90.83 | 36.04 (0.40x) | 38.09 (0.42x) | 31.46 (0.35x) |
| hash only | delete + insert | 211.42 | 92.91 (0.44x) | 90.89 (0.43x) | 75.07 (0.36x) |
| hash + global data | update | 91.24 | 53.77 (0.59x) | 35.81 (0.39x) | 28.77 (0.32x) |
| hash + global data | delete + insert | 212.21 | 112.32 (0.53x) | 91.56 (0.43x) | 74.80 (0.35x) |
| hash + array | update | 91.18 | 75.86 (0.83x) | 42.42 (0.47x) | 35.58 (0.39x) |
| hash + array | delete + insert | 211.63 | 153.67 (0.73x) | 107.50 (0.51x) | 89.21 (0.42x) |

Control: kernel 3.35-3.38, bpftime 1.41-1.44 in every build.

Findings: bpftime hash update/delete are well below kernel in every shape.
The helper fixes removed most of the global-data and bound-program overhead
(hash + array update 75.9 -> 42.4 ns), the bounded hash map another 6-7 ns.
Bound programs still pay about 4-7 ns over hash-only (dispatcher), the target
of the next change.
