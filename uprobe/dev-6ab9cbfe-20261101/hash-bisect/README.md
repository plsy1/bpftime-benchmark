# Which commits slowed the hash maps in the uprobe suite (2026-11-01 05:18-05:40 JST)

Question from `../README.md`: in the official uprobe suite, dev 6ab9cbfe's hash maps are
slower single-threaded than a7d54d7e (10/26). Answer: three steps, all from 10-28..10-30;
nothing after f4fdcbcd changed them, so the four review rounds did not.

## Method

- `run_hash.py`: the suite's userspace leg as `benchmark.py` runs it (loader
  `benchmark/uprobe/uprobe` under the syscall server, `benchmark/test 1 50000` under the agent,
  `BPFTIME_ARRAY_JIT_INLINE=0`), same runner binaries as `../full10-6ab9cbfe`. One loader per
  build per rep, 3 reps, build order rotated, `taskset -c 5`, root.
- 11 frozen builds (`../../ssl-nginx-2x2-20261023/builds/`), one per step since a7d54d7e.
- cpu5 at 1728000 before and after; nothing else running. `steps.jsonl`, `steps.log`,
  `steps-logs/`, table in `summary.md`. dev-6ab9cbfe reproduces the full suite's values
  (hash update 42.8 vs 42.9 ns, per-CPU hash lookup 34.9 vs 35.0).

## Result (ns per map operation, medians)

| Step | Build | hash update | hash lookup | hash delete | per-CPU update | per-CPU lookup | per-CPU delete |
|---|---|---:|---:|---:|---:|---:|---:|
| 10/26 dev | a7d54d7e | 27.2 | 23.0 | 27.9 | 44.5 | 27.7 | 99.6 |
| daily build config (GCC 13, LLVM 15, LTO) | 09421773 | 27.2 | 22.9 | 27.6 | 44.6 | 27.7 | 99.6 |
| per-CPU hash: every op under the map lock + kernel flags | c98951a4 | 27.2 | 22.9 | 27.5 | **56.4** | **35.0** | **105.7** |
| hash: lookups take no lock | 9c3e0345 | 26.1 | **27.2** | **31.5** | 55.4 | 34.7 | 105.8 |
| hash: writers lock stripes | cb3f29d2 | **50.2** | 27.5 | **65.6** | 56.8 | 34.9 | 105.6 |
| hash: per-stripe tokens, one-line stripe, views | f4fdcbcd | 43.3 | 27.6 | 44.2 | 55.8 | 34.7 | 106.2 |
| review fixes (5 builds) | dev-b9c849fb .. dev-6ab9cbfe | 42.8-44.0 | 27.4-27.6 | 43.0-44.2 | 56.6-58.1 | 34.9-35.2 | 105.5-108.6 |
| kernel (full suite, 11/01) | | 95.3 | 28.5 | 108.3 | 68.2 | 30.4 | 151.1 |

Arrays and the probe path are flat across all builds (array lookup 1.2, per-CPU array lookup
4.1, uprobe 0.20 us).

1. **Per-CPU hash** (+12 / +7 / +6 ns): the step 09421773 -> c98951a4, which holds cdc05f67
   (every operation under the map lock: the 10/28 fix for SIGSEGVs, hangs and a lost count
   with several threads) and c98951a4 (kernel update flags). No frozen build separates the
   two. The profile shows the lock: `__aarch64_swp4_acq` (the spinlock's acquire, an
   out-of-line atomic) goes from 8.0% to 14.4% of the victim's cycles. A correctness cost;
   it is what puts per-CPU hash lookup at 1.15x the kernel.
2. **Hash lookup** (+4.3 ns) and delete (+4 ns): 9c3e0345, lookups without a lock. With
   one thread the generation protocol (acquire loads of the bound and bucket words, the
   re-check) costs more than the uncontended spinlock it replaced.
3. **Hash update and delete** (+24 and +34 ns, then -7 and -21): cb3f29d2 (striped writers),
   partly recovered by f4fdcbcd. Net from 9c3e0345 to f4fdcbcd: update +17 ns, delete
   +13 ns. In f4fdcbcd the cost moved from the lock function into `elem_update` itself
   (4.3% -> 10.9% of cycles; `profile-*.txt`). Its hottest samples are just after the
   load-acquire of the home stripe's bound (`load_bound`, `bpftime_hash_map.hpp`, update's
   first read) and around the hash modulo and range lock
   (`annotate-f4fdcbcd-elem_update.txt`). Which instruction costs what was not tested.

## Relation to the 10/30 gate

`../../hash-striped-writers-20261030` accepted +7.4% on single-thread churn (0.571 ->
0.613 us per call, three map operations), about 14 ns per operation. That matches steps 2 and 3
here (+4, +17, +13 ns). Same cost, not a new regression; this suite's loop of 1,000 map
operations per call with no probe work between them shows it at full size, while churn
spreads it over a probe call.

## Profiles

`run_hash.py --perf`, `perf record -e cycles:u` on the victim only (1 rep, 20,000
iterations; for locating cost, not timing): `profile-<build>.txt` for 09421773, c98951a4,
9c3e0345, f4fdcbcd; `annotate-f4fdcbcd-elem_update.txt`. The perf.data files stay on the host
(`profile-logs/`).
