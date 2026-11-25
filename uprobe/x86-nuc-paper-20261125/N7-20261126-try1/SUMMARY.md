# N7-20261126-try1 — Fig. 1 stage ladder with real builds (x86 NUC), summary

Eval plan N7 (reviewer R1-a/b: the old Fig. 1 used a diagnostic build). Session ended 07:03:22, exit 0; checks
before/after PASS. 210 measured rows (+ 21 warm-up), all valid. Policy M, 16 B, cnt loader stages S1 (hook only), S2h
(+ helpers), S2hm (+ maps), S3 (+ probe read), S4 (stock program sections, output). Legs B + {K, O (official 4708f409),
Ol (upstream head 70f7bc1f), Dn (Tern c71a9987)} × 5 stages, one round of 10 paired reps, all 21 legs shuffled per rep.
Per event = per request / 2. Tables: summary-n7.md (analyze_paper.py), stages.txt. Only stage differences go into the
figure; S4 totals are not quoted in the paper (C2 owns totals).

## Stage differences (us per event, 95 % CI half-width, n = 10)

| segment | kernel | official | upstream head | Tern |
|---|---:|---:|---:|---:|
| hook (S1 − B) | 2.508 ± 0.039 | 0.619 ± 0.063 | 0.716 ± 0.071 | 0.641 ± 0.053 |
| helpers (S2h − S1) | 0.034 ± 0.042 | 0.638 ± 0.065 | 0.630 ± 0.050 | 0.089 ± 0.049 |
| maps (S2hm − S2h) | 0.301 ± 0.056 | 0.284 ± 0.036 | 0.284 ± 0.036 | 0.223 ± 0.049 |
| probe read (S3 − S2hm) | 0.033 ± 0.050 | 0.041 ± 0.051 | 0.049 ± 0.051 | −0.035 ± 0.059 |
| output (S4 − S3) | 1.183 ± 0.045 | 2.069 ± 0.059 | 0.185 ± 0.053 | 0.102 ± 0.033 |
| (sum = S4 − B, for checking only) | 4.059 | 3.651 | 1.865 | 1.019 |

## Reading (corrected after the independent check)

1. The real-build ladder reproduces E6's (diagnostic-build) kernel and official segments: hook 2.51 vs 2.49 and 0.62 vs
   0.58, official helpers 0.64 vs 0.68 and output 2.07 vs 2.11, kernel output 1.18 vs 1.14, sums 4.06 vs 4.02 and 3.65
   vs 3.64. The figure can drop "diagnostic build". Tern's map segment does not reproduce E6's (old Tern D 0.139 vs Dn
   0.223), so E6's derived "Tern's maps save ~0.17 us/event vs official" does not carry over (here 0.061 [0.012, 0.110]).
2. Upstream head (the baseline the motivation section should use, per R1) keeps official's helper cost (0.63; E7 shows
   6 getuid + 6 getgid per request at S2h and no getpid/gettid there) but its output stage is 0.19 instead of 2.07 us:
   upstream head no longer pins the CPU around each perf output (official bpf_helper.cpp:487–552 does a
   sched_getaffinity and two sched_setaffinity per output; olatest's output path has none) and dropped the per-output
   gettid; O − Ol output 1.88 [1.81, 1.96] vs E6x pinning 1.82. Its hook is 0.10 [0.01, 0.19] dearer than official's
   (E17 attributed +0.106 to the #730 lock without isolating it).
3. Tern vs the others, paired: hook vs official +0.02 [−0.05, +0.10] (no detectable difference); helpers 0.09 and output
   0.10 are small; maps + probe read together (S3 − S2h) 0.19 vs ~0.33 for every other leg (Dn − O −0.137 [−0.207,
   −0.068], Dn − Ol −0.146 [−0.205, −0.086], Dn − K −0.146 [−0.227, −0.064]). Split into the two segments it is unstable
   (they share the Dn@S2hm leg, r = −0.80): maps vs Ol −0.061 [−0.123, +0.001], probe read vs every other leg about
   −0.07 (significant), so quote the combined S3 − S2h figure.
4. Kernel: hook (trap) 2.51 dominates; output 1.18 (perf buffer + per-event wakeup).
5. Re-entry pricing for official's bars stays with the real patch ladder (E6x: pinning 1.823, uid/gid 0.562, pid/tid
   0.190 us/event), as the plan says.

## Flags

- noisy_core: 30 of 210 rows have core 0 above 5 % (max 11.7 %); core-0 busy depends on the leg (median 5.6–5.9 % for
  K@S4, O@S4, Dn@S4 vs 1.1–3.7 % for the other legs), so it could touch the output segment. Output on unflagged pairs only
  agrees (K 1.187 n = 4, O 2.120 n = 3, Ol 0.180 n = 9, Dn 0.098 n = 2; wide CIs). Main results include all rows.
- Deviation found after the session: I2churn (04:40–05:14) pinned CPUs 0–7 to 3.5 GHz and disabled C-states ≥ 2; its
  restore failed (run-i2.sh listed SMT siblings, so pin-state.txt had duplicate idle entries and pin-cpus.sh wrote two
  values: "Invalid argument"), and later sessions restore only their own CPUs. So during I2N and N7 CPUs 0, 1, 3, 5, 7
  stayed at a fixed 3.5 GHz with deep C-states off (measured CPUs 2, 4, 6, 8–11 were set by session.sh as always). This
  is the likely trigger of the core-0 activity first seen in I2N (core 0 had been quiet in every earlier session). Fixed
  2026-11-26 ~07:15 (restore from a de-duplicated state file; pin-cpus.sh now takes the first entry); idle core 0 then
  0.65 % busy (1.35 % before).
- One round, n = 10 (as planned); stage differences are paired within reps.
