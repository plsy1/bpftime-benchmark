# DX2-20261125-try1 summary (exploratory C1 follow-up; 16:55:40–17:40:03, exit 0, 172 rows valid, noisy_core 0)

Purpose: PREREG-2 §8 branch 2 (Dn faster than OlL3x730 by > 0.1 µs/event in C1): attribute the gap with counters.
Stage ladder on both builds, n = 10 paired reps (1 warm-up dropped), per-event x vs B (µs/event):

| stage | adds | OlL3x730 | Dn | gap OlL3x730 − Dn [95% CI] |
|---|---|---|---|---|
| S1 | hook only | 0.597 | 0.646 | −0.048 [−0.148, 0.052] |
| S2h | + helpers (no maps) | 0.730 | 0.766 | −0.035 [−0.106, 0.036] |
| S2m | + maps (no uid/meta helpers) | 0.885 | 0.829 | +0.056 [−0.000, 0.112] |
| S2hm | + helpers + maps | 0.977 | 0.933 | +0.044 [−0.001, 0.088] |
| S3 | + probe read | 0.986 | 0.899 | +0.088 [0.040, 0.135] |
| S4 | + perf output (full) | 1.236 | 1.048 | +0.188 [0.116, 0.260] |

- OlL3x730 − OlL3x730P (perf output without getpid per sample, 81ef1d41) = 0.099 [0.018, 0.180]: upstream's remaining per-event
  getpid costs ≈ 0.10 µs/event in situ (OlL3x730P makes 0 getpid, 8.0 syscalls/request, perf sc round 1).
- OlL3x730P − Dn = 0.089 [0.011, 0.167]: the rest, which appears with the maps (S2m − S1: upstream +0.288, Dn +0.183 µs/event).
  sslsniff does hash updates/lookups/deletes on bufs and start_ns per event; upstream's hash map vs Tern's lock-free table.
- Tern's hook is not cheaper (S1 gap −0.048, CI spans 0).
- perf cyc (worker, per request): S2hm user instr OlL3x730 46,633 vs Dn 44,392 (+2,241), user cycles +379; S3 +2,224 instr, +31 cycles;
  OlL3x730P S4 user 21,760 cycles / 47,845 instr. Kernel cycles equal at S2hm/S3 (≈17.2–17.3k).
- Reading for C1: Dn − OlL3x730 −0.203 = per-event getpid (≈0.10, an execution-context kind-1 cost) + map implementation (≈0.10,
  not a kernel-environment cost) − hook (≈0.05, not significant).
- Upstream legs lose events silently (unreported up to 963/run at S4); Dn 0.
- nginx worker core recorded (new field): cpu2 71 rows, cpu6 69.
