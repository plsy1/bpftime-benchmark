# Per-CPU hash fixes: shared counters before and after (2026-10-28)

Re-run of the per-CPU hash part of the shared-counter benchmark (`../map-table-20261028`,
problem 4 of `../dimension-problems.md`) after two dev-branch commits:

- `cdc05f67` per-cpu hash: run every operation under the map lock;
- `c98951a4` per-cpu hash: kernel update flags and size limit on the program path
  (`BPF_NOEXIST` on an existing key fails with `EEXIST` instead of overwriting, `BPF_EXIST`
  on a missing key with `ENOENT`, a full map with `E2BIG`).

Backends: kernel (BTF 6.8.12), `09421773` (before), `c98951a4` (after; frozen build
`../ssl-nginx-2x2-20261023/builds/c98951a4`). Objects `count-percpu-hash.bpf.o` (lookup + add,
`BPF_NOEXIST` insert on a miss, retry the lookup if the insert fails) and the control; 1 and
64 keys shared by all threads; threads 1/2/4/6; 3 reps of 6 s. Checks: probe hits = calls, no
failed update, sum of the map over keys and CPUs = calls. jetson_clocks on, cpu5 at 1728000
before and after. Runner: `../map-table-20261028/run_count.py`; `run1-count.jsonl`,
`summary-run1.md`.

## Correctness

| Build | Valid runs | Failed |
|---|---:|---|
| kernel | 48 / 48 | - |
| 09421773 (before) | 40 / 48 | 64 keys, 2-6 threads: 7 of 9 runs (4 SIGSEGV, 3 hung until the timeout); 1 key, 2 threads: one run lost one count |
| c98951a4 (after) | 48 / 48 | - |

The lost count is the flags bug: both threads missed the new key and inserted it with
`BPF_NOEXIST`; the second insert overwrote the first thread's value instead of failing, so the
program did not retry the lookup and one increment disappeared (`map_sum` 33,962,099 of
33,962,100 calls).

## Cost of the lock

Per-call time per thread (us), whole probe:

| Keys | Threads | kernel | before | after |
|---:|---:|---:|---:|---:|
| 1 | 1 | 1.265 | 0.245 | 0.252 |
| 1 | 2 | 1.842 | 0.355 | 0.464 |
| 1 | 4 | 3.504 | 2.275 | 2.687 |
| 1 | 6 | 4.048 | 4.593 | 4.889 |
| 64 | 1 | 1.266 | 0.246 | 0.256 |
| 64 | 2 | 1.840 | 0.348 | 0.457 |
| 64 | 4 | 3.491 | crash | 2.460 |
| 64 | 6 | 4.096 | crash | 5.220 |

Map cost (per-CPU hash minus control): after the fix 0.04-0.05 us at one thread (before
0.04), 0.18-0.19 at two, 0.15-0.39 at four and 0.27-0.56 at six; the kernel stays within
+-0.2 of zero. Single-thread cost is unchanged; with threads the per-CPU hash now pays the
whole-map spinlock like the ordinary hash map (problem 1, deferred), less than the hash counter
(1.0-1.4 us at 4-6 threads) because only lookups take it after the first inserts.
