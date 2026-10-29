# Step 1 of the multi-thread hash map plan: lookups take no lock (2026-10-30)

Plan: summry `uprobe/hashmap-multithread/hashmap-multithread-research-20261029.md` section 8
(decided 2026-10-30). Problem 1 of `../dimension-plan.md`: every hash map operation, lookups
included, took the map's single spinlock.

## Change (dev `9c3e0345`, test fixed in `33bd3d13`)

- Each bucket's 4-byte state word also holds a generation that every delete increments. Lookups
  read the word, compare the key and accept the match only if the word is unchanged (a
  seqlock per bucket). They take no lock and write nothing shared.
- `fix_size_hash_map_impl` locks its own writers with one spinlock on a cache line of its own.
  Callers no longer take the handler's map lock, which shared a cache line with `map_impl_ptr`.
- Test `test_hash_map_lockfree_lookup.cpp`: with the re-check removed, lookups matched a key
  that was never inserted (a half-overwritten key) 18k-291k times in 1.5 s, every run.

## Setup

- Builds (`../ssl-nginx-2x2-20261023/builds/`):
  - `c98951a4`: before; its map code is the same as dev `1aa74c4c`;
  - `9c3e0345`: after.
  - Same daily config for both: GCC 13, LLVM 15, RelWithDebInfo, LTO.
- Runners reused unchanged:
  - `../map-table-20261028/run_count.py`: shared counters, hash map with lookup + atomic add; NKEYS 1 and 64;
  - `../map-threads-20261028/run_churn.py`: churn, 4 B key, 8 B value, one key per thread.
- Threads 1/2/4/6 on CPUs 5,4,3,2,1,0, so 4 and 6 threads cross the two clusters. 3 reps, backend order rotated.
- jetson_clocks: cpu5 at 1728000 before and after. 216 runs, all checked: hits = calls, sums exact.
- Files: `go.sh`, `count.jsonl`, `churn.jsonl`, `summarize.py` -> `summary.md`.

## Results (us per call per thread, medians; map = object minus control)

| Benchmark | Threads | kernel map | before map | after map | kernel total | before total | after total |
|---|---:|---:|---:|---:|---:|---:|---:|
| counters, 1 key | 1 | 0.02 | 0.02 | 0.02 | 1.26 | 0.23 | 0.24 |
| | 4 | 0.04 | 1.07 | 0.00 | 3.33 | 3.37 | 2.25 |
| | 6 | 0.14 | 0.85 | 0.00 | 4.19 | 5.58 | 4.67 |
| counters, 64 keys | 6 | 0.06 | 0.97 | 0.02 | 4.13 | 5.72 | 4.74 |
| churn | 1 | 0.22 | 0.12 | 0.11 | 2.04 | 0.57 | 0.56 |
| | 2 | 0.40 | 0.42 | 0.20 | 3.03 | 0.96 | 0.74 |
| | 4 | 0.16 | 6.18 | 3.77 | 5.02 | 8.96 | 6.52 |
| | 6 | 0.24 | 10.36 | 6.49 | 6.74 | 15.38 | 11.51 |

- **Counters**: the hash map cost is gone at every thread count (0.85-1.07 -> 0.00-0.03 us).
  - What is left of the total is the probe path: Frida's shared usage counter, problem 2.
  - At 6 threads that path is 4.67 us, against the kernel's 4.19.
- **Churn**: only the lookup got cheaper. Inserts and deletes still take one lock per map, and
  they write the shared element count. As planned, this step is not the result for churn (see step 2).
- **One thread**: no change (counters 0.02 -> 0.02, churn 0.12 -> 0.11).
