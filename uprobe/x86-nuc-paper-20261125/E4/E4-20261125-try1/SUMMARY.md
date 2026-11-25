# E4-20261125-try1 — per-CPU lost updates and the #730 lock price (x86 NUC), summary

21:05:51–22:59:22, loss=0 timing=0. 400 rows, all valid. Workers on CPUs 2–7 (6 CPUs), T threads, legs K / Ol
(upstream head, with the #730 per-CPU exec lock) / Ol730x (lock removed) / Dn (Tern) / Dn730 (Tern with the lock
restored), 5 reps per cell, leg order shuffled. Kernel preempt mode: voluntary (kernel uprobe programs run under
migrate_disable). Table: `summary.txt` (summarize_e4.py).

Variants (e4/src/pcpu_lost.bpf.c): a = PERCPU_ARRAY `*v += 1` (JITs to one `incq` on x86); b1 = PERCPU_ARRAY
load, one `bpf_ktime_get_ns()`, store (a helper call inside the read-modify-write window); c = PERCPU_HASH `*v += 1`.

## Loss (count mode, exact per-run counter)

| cell | T | K | Ol (lock) | Ol730x | Dn | Dn730 |
|---|---:|---|---|---|---|---|
| a | 1–64 | 0 | 0 | 0 | 0 | 0 |
| b1 | 1, 4 | 0 | 0 | 0 | 0 | 0 |
| b1 | 16 | 0 | 0 | 11.0 % [8.3, 12.4] | 14.6 % [13.6, 15.4] | 1–3 lost, 5/5 reps |
| b1 | 64 | 0 | 0 | 42.6 % [38.5, 44.0] | 49.4 % [45.9, 50.8] | 28–54 lost, 5/5 reps |
| c | 16 | 0 | 0 | 1–5 lost, 5/5 reps | 1 lost in 1/5 reps | 0 |
| c | 64 | 0 | 0 | 1–5 lost, 5/5 reps | 0 | 0 |

(median [min, max] of lost/expected; expected ≈ 96–99 M updates per run.)

Reading:
1. Once there are more threads than CPUs (T = 16, 64 on 6 CPUs) and a helper call sits between load and store, a
   userspace per-CPU map without the lock loses a large share of updates (11–49 %); the kernel and the locked upstream
   lose none. This is kind 2 (non-preemptible per-CPU context) in practice.
2. Without the lock even a single-instruction update loses a few (variant c, Ol730x, every rep). Likely mechanism (not traced): the thread reads its
   CPU, is migrated before the non-atomic `incq`, and races a thread now running on that CPU. Rare (~1e-8) but nonzero.
3. Dn730 (Tern with the lock restored) still loses in b1 at T ≥ 16 (1–54 per run): Tern's inlined per-CPU lookup
   takes the CPU from `::sched_getcpu` rather than from the lock's CPU (bpf_helper.cpp ~1587), so lock and slot can
   disagree after a migration. Reported to the Jetson session (record, not fix). Dn730 is not a faithful "Tern + #730".

## #730 lock price (timing mode, variant a, no counter; paired ratio of wall time, 95 % CI)

| T | Ol / Ol730x | Dn730 / Dn | Ol730x / Dn | K / Dn |
|---:|---|---|---|---|
| 1 | 1.085 [1.078, 1.091] | 1.089 [1.054, 1.125] | 1.010 | 8.16 |
| 4 | 1.243 [1.178, 1.312] | 1.165 [1.083, 1.254] | 1.080 | 6.35 |
| 16 | 1.285 [1.235, 1.336] | 1.294 [1.275, 1.314] | 1.188 | 7.45 |
| 64 | 1.280 [1.223, 1.339] | 1.297 [1.270, 1.324] | 1.204 | 7.49 |

The lock costs ~9 % of a whole probe call uncontended and ~25–30 % with 4+ threads on 6 CPUs. (Wall time includes
the target loop; per-call ns per busy CPU in summary.txt: Ol 148 vs Ol730x 137 ns at T=1, 473 vs 375 at T=64.)
K/Dn ≈ 6–8 is the uprobe trap cost (~1.1 us per call on this kernel vs ~0.14 us for the in-process hook).

## Flags

- Timing-mode rows have no counter, so their "lost" column (Ol730x 1 in 3 rows) is unverified and not used.
- Smoke (earlier, different N) said the lock costs ~19 % per call; the full run gives 8.5 % at T=1 and 24–30 % at T≥4.
  Use the full-run numbers.
- K = 0 reflects migrate_disable on this kernel, not general kernel non-preemption.
- Independent check: all loss fractions and timing ratios reproduce; lost is real (loader sums map and slots after
  the target exits; runs = expected, nulls = 0, missed = 0). Dn730 bug confirmed in source (e7a5b79f:
  `bpftime_resolve_array_cpu_query_v1` sets `hooks->read_cpu = ::sched_getcpu`, bypassing `current_bpf_cpu`); Dn730
  loses 0 in c because the helper path uses `my_sched_getcpu()`. Timing-mode a rows show Ol730x losing 1 in 3 of 10
  T ≥ 16 rows: variant a is not strictly loss-free without the lock either (counting mode did not catch it).
