# E4f-20261126-try1 — Dn730f gate (PREREG-2b) on the x86 NUC, summary

02:15:00–02:33:59, loss=0 timing=0. 120 rows, all valid. Runner and CPUs as E4-20261125 (e4/run_e4.py, workers on CPUs
2–7, threads unpinned); legs Dn (Tern), Dn730 (Tern + #730 lock, defective), Dn730f (Dn730 + inlined lookup reads the
lock's CPU, ladder 1a2d42ec, builds/dn730f); script scripts/run-e4f.sh. PREREG-2b frozen 01:50:36, before this run.

## Gate: loss (count mode, exact per-run counter; 5 reps per cell)

| cell | T | Dn | Dn730 | Dn730f |
|---|---:|---|---|---|
| b1 | 16 | 15.6 % [14.2, 16.7] | 1–4 lost, 5/5 reps | **0, 5/5** |
| b1 | 64 | 48.0 % [46.6, 49.2] | 19–67 lost, 5/5 reps | **0, 5/5** |
| c | 16 | 0 | 0 | **0, 5/5** |
| c | 64 | 1 lost in 2/5 reps | 0 | **0, 5/5** |

All 20 Dn730f counting runs: lost 0, runs = expected, nulls 0, missed 0. **PREREG-2b gate passed**: Dn730f replaces Dn730
in M6 (Dn730f vs OlL3), in P4 and in the C2 leg list. Positive control in the same session: Dn730 lost in 10/10 b1 runs
(reproducing E4-20261125), and Dn (no lock) loses 16–48 % in b1 and occasionally 1 update in c.

## Lock price (timing mode, variant a, paired ratio of wall time, 95 % CI, n = 5)

| T | Dn730f / Dn | Dn730 / Dn | Dn730f / Dn730 |
|---:|---|---|---|
| 1 | 1.096 [1.078, 1.113] | 1.074 [1.071, 1.077] | 1.020 [1.001, 1.039] |
| 4 | 1.179 [1.121, 1.241] | 1.126 [1.051, 1.205] | 1.048 [0.947, 1.159] |
| 16 | 1.297 [1.245, 1.351] | 1.314 [1.277, 1.354] | 0.987 [0.967, 1.007] |
| 64 | 1.314 [1.257, 1.373] | 1.326 [1.289, 1.364] | 0.991 [0.973, 1.009] |

- As PREREG-2b expected, the fix costs a little uncontended (+2.0 % at T = 1, ≈ +2.3 ns per call: `my_sched_getcpu`
  reads a thread_local in the dlmopen-ed agent instead of the vDSO); none measurable at T ≥ 16.
- P4 (lock price independent of the base): Dn730f / Dn 1.096 vs E4-20261125 Ol / Ol730x 1.085 [1.078, 1.091] at T = 1;
  1.30–1.31 vs 1.28–1.29 at T ≥ 16. Different sessions; consistent, not a formal test.

## Flags

- Power: only the 10 b1 runs discriminate (variant c goes through the per-CPU hash helper, which already used
  `my_sched_getcpu`; Dn730 lost 0 there too). Fisher 10/10 vs 0/10: p = 1.1e-5. The test rules out a residual defect
  losing more than ~2 % of Dn730's rate (at 1/100 of it, all-zero has probability 0.15).
- No JIT dump of Dn730f; that its lookup is inlined is inferred from the code (1a2d42ec keeps `read_cpu` non-null) and
  from the Dn-b1 dump of the same source/llvm-jit. Either way (inline or helper) the lookup now uses the lock's CPU.
- PREREG-2b's timestamp is self-recorded (file birth 01:50:27, sha file 01:50:36; not in git). No other file mentions
  dn730f before E4f.
- Independent check (subagent, own scripts): gate, builds (sha256 per run = run_e4.py's), counters and every timing ratio
  reproduce.
- Code review (subagent, plus the Jetson session's own grep of its port 19efeb16): PASS. The guard is line-for-line
  upstream's; the only raw CPU read left is the guard's own; every per-CPU access (array, hash, shared array,
  get_smp_processor_id, perf output) uses `my_sched_getcpu`; upstream head has no JIT inlining at all, so Dn730f now
  matches it; `current_bpf_cpu` is one LTO-private initial-exec TLS variable in the agent; the AOT key needs no change
  (the CPU symbol is bound at link time). Remaining Tern-vs-upstream differences are Tern features (trap path refuses
  bound-array programs, extra checks before the lock, no epoll signal taking, no OlL3 credential hooks), not the lock
  port. The ladder's unit test test_array_jit_lifetime.cpp:240-258 (raw −1 CPU expects exec 0) should now fail
  (guard returns −EAGAIN); ladder-only, not run. M6 must use OlL3 (checks off), not OlL3pc.
- Timing rows carry no counter (lost unverified), as in E4.
- Dn's c/T=64 single lost updates (2/5 reps) match E4-20261125's rare migration race for unlocked builds.
