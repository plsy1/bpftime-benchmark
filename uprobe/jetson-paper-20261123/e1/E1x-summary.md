# E1 TOST extension to 20 reps (Jetson), session J-E1x-20261123, 16:30–17:01

Only the legs of gates (a)/(b), reps 10–19 (rotation differs from reps 0–9 because the leg set differs). All 130 rows valid.

| gate | n | ratio | 90 % CI | margin | verdict |
|---|---:|---|---|---|---|
| (a) 16 B K cnt / stock | 20 | 0.9986 | [0.9924, 1.0048] | ±1 % | equivalent |
| (a) 16 B O cnt / stock | 20 | 1.0003 | [0.9888, 1.0120] | ±1 % | not shown |
| (a) 16 B D cnt / stock | 20 | 1.0009 | [0.9862, 1.0157] | ±1 % | not shown |
| (a) 256 KB K | 20 | 1.0009 | [0.9943, 1.0076] | ±1 % | equivalent |
| (a) 256 KB O | 20 | 1.0010 | [0.9927, 1.0094] | ±1 % | equivalent |
| (a) 256 KB D | 20 | 1.0076 | [0.9964, 1.0189] | ±1 % | not shown |
| (b) Odiag (all off) / O | 20 | 0.9864 | [0.9731, 1.0000] | ±1.5 % | not shown |

Audit, 256 KB reps 10–19: K missing − reported = exactly the readiness request; D missing = reported; O delivers 36.9–38.5 % of bytes with 0 reported lost.
