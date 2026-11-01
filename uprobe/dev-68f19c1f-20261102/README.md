# uprobe suite and ssl-nginx on dev 68f19c1f (2026-11-02 04:15-05:34 JST)

Dev `68f19c1f`: 6ab9cbfe plus phase 1 of the hash map single-thread plan
(`../hash-single-thread-20261102`): home-stripe fast paths, inline spin locks, four-byte key
lookups without the fence. Same methods and runners as `../dev-6ab9cbfe-20261101`; BTF kernel,
cpu5 at 1728000 before and after, daily job finished at 03:33. `run-all.sh`, `run-all.log`
(first the gate re-check in `../hash-single-thread-20261102/gate2`). Containers removed.

## ssl-nginx (`ssl-nginx/`, 2 rounds x 10 reps; 6ab9cbfe measured again in the same run)

| Leg | 16 B req/s | vs kernel | 256 KB req/s | vs kernel |
|---|---:|---:|---:|---:|
| baseline (no tracing) | 17,541 | +45.0% | 1,749.0 | +39.0% |
| kernel | 12,097 | 0 | 1,258.1 | 0 |
| official 177a1dc | 10,826 | -10.5% | 1,128.2 | -10.3% |
| dev 6ab9cbfe | 14,106 | +16.6% | 1,386.3 | +10.2% |
| dev 68f19c1f | 14,186 | +17.3% | 1,391.0 | +10.6% |

68f19c1f vs 6ab9cbfe, paired: +0.9% (16 B, 14/20) and +0.5% (256 KB, 13/20): within noise
(sslsniff's maps are a per-CPU array and a perf event array, not hashes). Audit: 2.000 events
per request at 16 B for every tracing leg; 256 KB kernel 18.01, 6ab9cbfe 17.95, 68f19c1f
17.73, official 9.49.

## Complete ten-round uprobe suite (`full10-68f19c1f/full10/comparison.md`)

ns per operation (value per call / 1000 for the map cases); 6ab9cbfe from 11/01.

| Operation | kernel | 6ab9cbfe | 68f19c1f | change | 68f19c1f / kernel |
|---|---:|---:|---:|---:|---:|
| uprobe | 1240.4 (per call) | 201.5 | 199.2 | -1.1% | 0.16x |
| hash update | 95.8 | 42.9 | 29.5 | -31.2% | 0.31x |
| hash lookup | 29.0 | 27.6 | 22.3 | -18.9% | 0.77x |
| hash delete | 108.4 | 43.8 | 37.5 | -14.4% | 0.35x |
| per-CPU hash update | 67.9 | 58.1 | 54.3 | -6.4% | 0.80x |
| per-CPU hash lookup | 29.4 | 35.0 | 33.5 | -4.5% | **1.14x** |
| per-CPU hash delete | 150.5 | 105.7 | 104.6 | -1.0% | 0.70x |

Everything else (probe paths, read/write helpers, arrays, per-CPU arrays, embed) within
±1.2% of 6ab9cbfe; the kernel arm within ±1.4% of 11/01 except array delete (+4.9%) and
per-CPU hash lookup (-3.2%). Activation audit and sample validation passed.

Against a7d54d7e (10/26): hash update 28.2, lookup 23.0, delete 28.5, per-CPU update 44.5,
lookup 27.7. Hash update and lookup are back; delete stays +9 ns (token and displacement
bookkeeping of the striped table); the per-CPU hash keeps its map lock (phase 2 of the plan,
not done), so its lookup is still above the kernel.
