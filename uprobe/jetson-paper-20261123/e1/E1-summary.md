# E1 pilot gate (Jetson), session J-E1-20261123, 2026-11-23 14:55–15:32

Policy M (nginx {4,5}, sslsniff {3}, wrk {0,1} -t2), 10 reps, every leg-rep valid; start/end guards ok.
A first start at 14:54 was refused by the load guard (0.44 ≥ 0.3; no data), kept as `raw/J-E1-20261123.refused-load-0.44`.

## 16 B (round 0)

| leg | median rps | GMR vs K [95% CI] |
|---|---:|---|
| B | 19,624 | 1.496 [1.483, 1.509] |
| K | 13,038 | 1 |
| O | 11,750 | 0.892 [0.886, 0.899] |
| Odiag | 11,645 | 0.883 [0.872, 0.895] |
| D | 16,031 | 1.215 [1.192, 1.239] |
| D+AOT0 | 15,832 | 1.187 [1.143, 1.232] |
| K-stock / O-stock / D-stock | 13,234 / 11,689 / 15,827 | |
| B#Mt3 | 19,661 | 1.497 |

## 256 KB (round 1)

| leg | median rps | GMR vs K | bytes delivered | reported lost |
|---|---:|---|---:|---:|
| K | 1,238 | 1 | 99.77–99.93 % | 127–449 |
| O | 1,139 | 0.921 [0.913, 0.929] | 37.0–37.9 % | 0 |
| D | 1,420 | 1.141 [1.128, 1.154] | 99.78–99.93 % | 157–509 |

## Gates (PREREG 2.0.3 TOST, 90 % CI)

| gate | result | verdict |
|---|---|---|
| (a) cnt vs stock loader, ±1 % | 16 B: K 0.992 [0.984, 1.000], O 1.006 [0.993, 1.019], D 1.008 [0.985, 1.031]; 256 KB: K 1.002 [0.990, 1.014], O 1.003 [0.990, 1.016], D 1.004 [0.990, 1.018] | points inside, CIs wider → pre-registered extension to 20 reps (same as x86) |
| (b) Odiag (all off) vs O, ±1.5 % | 0.990 [0.981, 0.998] | point inside, CI lower bound outside → extend to 20 reps |
| (c) D vs D+AOT0, ±2 % | 1.024 [0.995, 1.054] | point outside margin → not shown; per plan AOT0 stays a sensitivity row, headline unchanged |
| (e) nginx busy ≥ 0.95; B#Mt3 not > 2 % above B | busy {4}+{5} min 0.975; Mt3/B 1.001 [0.998, 1.004] | pass: wrk -t2 saturates nginx |
| (f) O del_over_exp at 16 B ≥ 0.9975 | 1.00000 in all 10 reps | pass |
| (g) τ calibration with K | K missing − reported ≤ 1 after removing the readiness request (README) at both payloads; τ = 200 | pass |

Silent loss: O at 256 KB delivers 37 % of the bytes and reports 0 lost (all 10 reps). D: missing = reported exactly in every rep.
