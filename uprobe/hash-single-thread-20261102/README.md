# Hash maps' single-thread cost, phase 1 (2026-11-02)

Plan and research: `PLAN.md` (problem 8 / step 7 of `../dimension-plan.md`). Phase 1 is done
on dev, five commits on top of 6ab9cbfe (not pushed at writing):

| Commit | Change | Test (mutation) |
|---|---|---|
| 0ffe368b | Shared-segment spin locks taken inline (glibc's protocol on the pthread word) | "bpftime spin locks exclude each other and pthread spin locks" (no exchange: fails) |
| d16fcbf5 | Four-byte key lookups: acquire load of the key instead of the fence | torn-key test; the mutation (relaxed, no fence) is NOT caught on the Orin |
| a57591f5 | Update and delete inside the home stripe: one lock, no range machinery; inline 4/8-byte copies | "hash map writers lock every stripe they touch beyond the home stripe" (three mutations of the fast-path bounds: each fails) |
| 5b28dae5 | CLAUDE.md | |
| 68f19c1f | The handler's map lock calls the spin lock out of line (forced inline cost per-CPU hash 30 instructions) | spin lock and per-CPU tests |

Candidate gate green at 5b28dae5 (runtime focused 153 cases, transformer 46 + 1 expected
failure, syscall trace 7); map test tags green at 68f19c1f.

## Results

Official suite, userspace leg (`gate2/`, ns per operation, medians of 3):

| | a7d54d7e | 6ab9cbfe | 68f19c1f | kernel (full suite) |
|---|---:|---:|---:|---:|
| hash update | 28.2 | 42.9 | **29.5** | 95.8 |
| hash lookup | 23.0 | 27.6 | **22.3** | 29.0 |
| hash delete | 28.5 | 43.6 | **37.5** | 108.4 |
| per-CPU hash update | 44.5 | 58.0 | **54.4** | 67.9 |
| per-CPU hash lookup | 27.7 | 35.0 | **32.9** | 29.4 |
| per-CPU hash delete | 99.6 | 105.9 | **104.8** | 150.5 |

Full ten-round suite and ssl-nginx: `../dev-68f19c1f-20261102` (per-CPU hash lookup 33.5 ns
there, 1.14x the kernel; ssl-nginx unchanged within noise).

Multi-thread (`gate/summary.md`, 5b28dae5 vs 6ab9cbfe, total us per call): churn -7.9% (1
thread) / -0.4% (6); tracking table -1.6 to -2.0%; hash counters -2.7 to -3.7%; per-CPU hash
counters 0 (1 thread) / -2.7 to -3.6% (6); `gate2/` per-CPU counters on 68f19c1f 0 / -2.1 to
-5.0%. No configuration got slower; 144 + 24 runs, all checked ok.

## Left

- **Hash delete** +9 ns over a7d54d7e: the striped table's token and displacement
  bookkeeping, spread over the function (no hot spot). Not pursued.
- **Per-CPU hash**: the map lock itself (the Boost index rehashes). Phase 2 of `PLAN.md`
  (storage on the open-addressing table, lock-free lookups) would remove it; not started.

## Files

`exp/` (experiment diffs and A/B), `stat/` (perf stat of focused loops; perf.data host only),
`focus-runner/` and `focus-src/` (hash-only victim), `gate/`, `gate2/`. Builds frozen in
`../ssl-nginx-2x2-20261023/builds/` (exp-*, wip-*, dev-5b28dae5, dev-68f19c1f).
