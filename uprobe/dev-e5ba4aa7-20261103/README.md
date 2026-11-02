# Hash map step 2 (dev e5ba4aa7): elements in u32 chains, per-CPU free lists (2026-11-03 04:12-06:07)

Step 2 of `../hash-redesign-survey-20261102/PLAN.md` (design: `DESIGN.md` there). The open-addressing
table is replaced by the kernel htab layout in offsets: preallocated elements `{gen|used, next, key,
value}`, u32 chain heads (power of two >= max_entries), nulls terminators, one stripe lock per
write, per-CPU free lists as the capacity. Step 1 (dev 42595149, shared atomic count) is the
baseline without the token cliff; dev a4ac0d31 is the version before both steps.

`run-all.sh` ran after the daily job, one stage at a time, cpu5 1728000 before and after, no
leftover workloads. Frozen builds: `../ssl-nginx-2x2-20261023/builds/dev-{42595149,e5ba4aa7}`.

## Key pattern matrix through BPF programs (`keys/`, 408 runs, all counts exact)

`../hash-keypattern-20261103/run_keys.py`: one op = lookup + delete of the oldest key + insert of
a new one, 24 B values. Map cost = object minus control, us per op, medians of 3.

| capacity, load | kernel | a4ac0d31 | step 1 | step 2 | step 2 / kernel |
|---|---:|---:|---:|---:|---:|
| 10240, 50% | 0.27-0.29 | 0.15-0.24 | 0.15-0.22 | 0.13-0.17 | 0.48-0.61x |
| 10240, 90% | 0.29-0.30 | 2.9-3.4 | 0.17-0.29 | 0.14-0.19 | 0.48-0.62x |
| 102400, 50% | 0.45-0.46 | 0.18-0.39 | 0.19-0.39 | 0.15-0.21 | 0.34-0.47x |
| 102400, 90% | 0.62-0.64 | 32.7-34.5 | 0.42-0.53; shl12 **5.36** | 0.17-0.28 | 0.27-0.43x |

Step 2 is the fastest backend in all 16 single-thread configurations, and the clustered `shl12`
keys (id << 12) at 102400 / 90% go from 5.36 us (step 1) to 0.165 us. At six threads the probe
path itself (~5 us, contended) hides the map: totals per op per thread are 4.94-5.20 us for step 2
and 4.97-5.32 us for step 1 except shl12 at 102400 / 90% (12.87 us); kernel 7.6-8.0 us,
a4ac0d31 up to 240 us. Tables: `keys/summary.md`.

## Suite hash cases (`suite.jsonl`, `suite-summary.txt`, cpu5, 5 reps of 50000)

| case | a7d54d7e | a4ac0d31 | step 1 | step 2 | step 2 vs a4ac0d31 |
|---|---:|---:|---:|---:|---:|
| hash_map_update | 26524 | 29112 | 28996 | 26426 | -9.2% |
| hash_map_lookup | 22906 | 22249 | 22272 | 16060 | **-27.8%** |
| hash_map_delete | 27260 | 33875 | 40607 | 36623 | **+8.1%** |
| per_cpu_hash_map_update | 44474 | 36371 | 34599 | 33961 | -6.6% |
| per_cpu_hash_map_lookup | 27692 | 25314 | 24984 | 23176 | -8.4% |
| per_cpu_hash_map_delete | 99421 | 58301 | 63890 | 65148 | **+11.7%** |

Array, probe and per-CPU array cases within 1%. `write` (and `read` in other builds) is bimodal,
~4280 or ~4850, independent of the map (seen in a4ac0d31 too).

## Full ten-round suite (`full10-e5ba4aa7/`, compare to dev-a4ac0d31-20261102 full10)

Same picture: hash lookup -27.9%, update -9.2%, delete +7.9%; per-CPU hash lookup -7.6%, update
-6.4%, delete +12.1%; everything else within 1%; embed 155.9 ns (+0.2%). Against the kernel run
of the same morning: hash lookup 0.56x (was 0.75x), delete 0.34x (was 0.31x). Exact-artifact
activation audit passed, 17 cases. `full10-e5ba4aa7/comparison.md`.

## Structure harness, 100000 entries (`structure/`, `structure/table.txt`)

Step 1 (`structure_step1`), step 1 with the multiply mixer (`structure_mix`), step 2
(`structure2`), 12 key patterns, loads 10-100%, filled and after churn; no check failures.

- Misses: step 2 2-4x faster than both (no probe run, one short chain): e.g. u64 random 90% after
  churn 42 vs 130/120 ns, 64 B path strings 102 vs 340/284 ns.
- Clustering gone: u32 id << 12 at 90% hit/miss 81/16 ns vs step 1 1686/6910 ns.
- No tombstones: after churn, hit and miss change little (step 1 rises 1.3-2x).
- Memory: 32 B per entry for 4 B key / 8 B value (step 1 34), 32 vs 50 for 8 B keys, 40 vs 66 for
  16 B keys, 88 vs 162 for 64 B keys (layout, not RSS).
- Slower than the mixer in some small-key hits and churns, two cache lines (head + element)
  against one bucket: u32 random 90% filled hit 60 vs 47 ns; u32 id << 8 at 90% churn 498 vs 249
  ns, id << 12 487 vs 249 ns. (The `mix` disp column is not meaningful: the harness measures
  displacement from the unmixed home.)

## Counters, churn, tracking table (`gate/summary.md`, us per op per thread)

- counters (count-hash, count-percpu-hash, 1/64 keys, 1-6 threads): step 2 within -14% to +5% of
  step 1 (0.86-1.05x), all exact.
- churn k4/v8: 1 thread 0.599 vs 0.583 us (+0.019 us map cost), 6 threads equal.
- tracking table (memleak-like): 1 thread 0.736 vs 1.014 us (1000 live), 0.978 vs 1.227 us
  (900000 live); 6 threads equal.

## Findings (measure all, then plan)

1. Deletes are slower than a4ac0d31 (+8% hash, +12% per-CPU hash in the suite), though faster than
   step 1 (-10%). A delete takes the stripe lock and then a free-list lock (step 1: a lock plus
   the shared atomic; a4ac0d31: a lock and a parked token). Kernel deletes are 3x slower still.
2. Single-thread churn of small sequential-ish 4 B keys in the structure harness is up to 2x the
   mixer variant (the two-line cost foreseen in the plan); not visible through BPF programs.
3. Everything else is equal or faster, the token cliff and the clustering are gone, memory per
   entry is 6-46% lower.

Candidate gate on this tree: `gate-e5ba4aa7.log`.
