# Step 2 of the multi-thread hash map plan: writers lock stripes (2026-10-30)

Plan: summry `uprobe/hashmap-multithread/hashmap-multithread-research-20261029.md` section 8.
Step 1 (lookups take no lock) is in `../hash-lockfree-lookup-20261030/`.

## Change

- dev `cb3f29d2`: writers lock stripes of buckets (power of two, >= 64 buckets, <= 4096
  stripes) instead of one map lock; per-home-stripe probe bound; element count as tokens in
  per-CPU slots.
- dev `f4fdcbcd` (final): fixes most of cb3f29d2's single-thread cost:
  - tokens per stripe under its lock plus a shared atomic pool (batch 32), not per CPU;
  - one view per operation (shared-memory vectors resolved to plain pointers once);
  - a stripe's lock, bound, tokens and displacement counts in one 64-byte line.
  - Adds a multi-process test (4 children map the segment at their own addresses).
  - Mutation "lock only the home stripe" fails the thread, wrap and process tests.
- Tests: full runtime suite with each case in its own process (193 passed, 3 skipped:
  map usage stats not built in), daemon, frida, text segment transformer and
  syscall trace suites as in the daily config: all passed. Running the whole runtime binary
  in one process crashes after `test_user_to_kernel_tailcall` destroys the global shm
  ("Test ufunc register", then `test_helpers`); both pass alone. Not hash map related;
  not checked on the base commit.

## Setup

- Builds (`../ssl-nginx-2x2-20261023/builds/`): `c98951a4` (before: one map lock),
  `cb3f29d2` (striped), `f4fdcbcd` (final, same binaries as `stripe-line`), kernel.
- Runners unchanged: `../map-threads-20261028/run_churn.py` (insert at entry, lookup + delete
  at return, 4 B key, 8 B value, one key per thread), `../map-table-20261028/run_count.py`
  (shared counters: lookup + atomic add), `run_track.py` (tracking table: one op = lookup +
  delete + insert, 1,000 or 900,000 live entries).
- Threads 1/2/4/6 on CPUs 5,4,3,2,1,0 (4 and 6 cross the clusters), 3 reps, order rotated.
  cpu5 at 1728000 before and after; no daily-job overlap (final ran 05:58-07:09).

## Results: total us per call per thread (medians), `final/summary.md`

| Workload | Threads | kernel | before c98951a4 | striped cb3f29d2 | final f4fdcbcd |
|---|---:|---:|---:|---:|---:|
| churn | 1 | 2.05 | **0.57** | 0.66 | 0.61 |
| churn | 2 | 3.02 | 0.96 | 0.78 | **0.70** |
| churn | 4 | 5.01 | 8.97 | **3.08** | 3.20 |
| churn | 6 | 6.68 | 15.43 | 5.19 | **5.07** |
| tracking 1K | 1 | 3.02 | **0.98** | 1.06 | 1.03 |
| tracking 1K | 6 | 7.49 | 16.72 | **5.21** | 5.26 |
| tracking 900K | 1 | 3.61 | **1.24** | 1.31 | **1.24** |
| tracking 900K | 6 | 8.14 | 17.41 | **5.06** | 5.16 |
| counters, 1 key | 6 | 4.10 | 5.61 | 4.81 | **4.79** |
| counters, 64 keys | 6 | 4.18 | 5.68 | **4.78** | 4.81 |

- With 6 threads bpftime now beats the kernel in total on churn (5.07 vs 6.68) and on both
  tracking tables (5.26 vs 7.49, 5.16 vs 8.14); before it was about 2x slower than the kernel.
- Shared counters still lose at 6 threads (4.79 vs 4.10; they win at 4: 2.26 vs 3.29), with
  a map cost of 0.04 us: what is left there is the probe path (problem 2), not the map.
- final vs striped at 4 threads churn (3.20 vs 3.08) and 6 threads tracking (5.26/5.16 vs
  5.21/5.06) is within run-to-run variation.
- The rest of the 6-thread time is the probe itself (Frida's per-function counter, problem 2),
  not the map: the map cost (object minus control) is 0.09 us for churn.
- With 4 and 6 threads the subtraction goes negative for tracking (-0.7 to -1.7 us): the two
  objects contend differently in the probe path, so the subtraction is not additive there.
  Compare totals.

## Single thread (the plan's gate: must not get slower)

`line-ab2/` (rerun after the daily job; 3 reps each, spread <= 0.02 us):

| | c98951a4 | f4fdcbcd | change |
|---|---:|---:|---:|
| churn total | 0.571 | 0.613 | +7.4% |
| tracking 1K total | 0.982 | 1.005 | +2.3% |
| tracking 900K total | 1.237 | 1.236 | 0 |

- perf stat: churn 2581 -> 2719 instructions per call, tracking 1K 2922 -> 3071 per op: about
  45-50 more per map operation (range computation, range lock loops, token checks). Cycles
  follow the instructions; backend stalls did not grow, although L1D TLB refills per churn
  call went from 34.7 to 52.4 (the stripe line is on a page of its own).
- So the gate is met for the large table and missed by 2% and 7% on the two small ones.
- How we got there (map cost, churn 1 thread, us):
  - cb3f29d2: 0.219;
  - per-stripe tokens: 0.184 (`tokens-ab/`);
  - relaxed bound + `dmb ishst`: no change, dropped (`relaxed-ab/`);
  - view: 0.162 (`view-ab/`);
  - one-line stripe: 0.163; tracking 1K 0.587 -> 0.513 (`line-ab2/`).
  - Profiles: `profile-t1/`.

## Files

- `go.sh`, `*.jsonl`, `summary.md`: first step 2 run (kernel, c98951a4, 9c3e0345, cb3f29d2).
- `final/`: the table above.
- `tokens-ab/`, `relaxed-ab/`, `view-ab/`, `line-ab2/`, `profile-t1/`: single-thread A/B.
- `discarded-daily-overlap/`: measured while the daily timer ran, not used.
