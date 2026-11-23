# x86 E5-R2: reproduction session (policy M, 16 B, 10 reps)

rows=50 valid=50 reps=10 payload=16 exp=E5
| leg | n | median rps | CV% | GMR vs K [95% CI] | x vs B us/req [95% CI] | ev/req | del/exp | lost/req | missing-reported |
|---|---:|---:|---:|---|---|---:|---:|---:|---:|
| D | 10 | 88965 | 0.58 | 1.5320 [1.5207, 1.5433] | 2.096 [2.024, 2.168] | 2.0000 | 1.00000 | 0.000000 | -50..0 |
| Dpc | 10 | 81084 | 0.71 | 1.3981 [1.3870, 1.4094] | 3.173 [3.101, 3.245] | 2.0000 | 1.00000 | 0.000000 | -369..0 |
| B | 10 | 109256 | 0.51 | 1.8827 [1.8711, 1.8943] |  |  |  |  |  |
| O | 10 | 60901 | 0.71 | 1.0472 [1.0393, 1.0551] | 7.304 [7.221, 7.387] | 2.0000 | 1.00000 | 0.000000 | 0..347 |
| K | 10 | 58083 | 0.74 |  | 8.081 [7.985, 8.178] | 2.0000 | 0.99999 | 0.000008 | -215..2 |

## G9: 16 B GMR vs K across x86 sessions (same boot, 2026-11-23)

| session | O/K (Odiag/K for E6) | D/K | n |
|---|---:|---:|---:|
| E1 pilot | 1.0406 | 1.5350 | 10 |
| E2 (policy M) | 1.0426 | 1.5345 | 10 |
| E4 headline | 1.0409 | 1.5316 | 30 |
| E6 @S4 | 1.0453 | 1.5268 | 20 |
| E5-R2 | 1.0472 | 1.5320 | 10 |

Range across five sessions: O/K 0.66 pp, D/K 0.82 pp (G9 threshold 3 pp) — stable. All sessions are in one boot of the NUC (M#21).
