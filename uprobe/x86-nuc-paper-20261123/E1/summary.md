# x86 E1 pilot (method gates), policy M

Sessions: E1-pilot-20261123 (reps 0-9), E1-extend-20261123-try1 (reps 10-19, TOST extension; only the legs under test, so the rotation differs from reps 0-9). Raw rows: /home/y/src/bpftime-x86-runs/paper-20261124/raw/<session>/rows.jsonl

## 16 B

rows=130 valid=130 reps=20 payload=16 exp=E1
| leg | n | median rps | CV% | GMR vs K [95% CI] | x vs B us/req [95% CI] | ev/req | del/exp | lost/req | missing-reported |
|---|---:|---:|---:|---|---|---:|---:|---:|---:|
| B | 10 | 108842 | 2.19 | 1.8625 [1.8287, 1.8970] |  |  |  |  |  |
| D-stock | 10 | 89152 | 0.66 | 1.5354 [1.5288, 1.5419] | 1.969 [1.782, 2.156] |  |  |  |  |
| O-stock | 20 | 60309 | 1.10 | 1.0404 [1.0339, 1.0470] | 7.348 [7.114, 7.581] |  |  |  |  |
| D | 10 | 89173 | 0.28 | 1.5350 [1.5293, 1.5407] | 1.972 [1.801, 2.142] | 2.0000 | 1.00000 | 0.000000 | -123..0 |
| O | 20 | 60552 | 0.90 | 1.0434 [1.0389, 1.0479] | 7.305 [7.152, 7.458] | 2.0000 | 1.00000 | 0.000000 | 0..170 |
| Odiag | 10 | 60777 | 0.70 | 1.0457 [1.0403, 1.0510] | 7.225 [7.044, 7.406] | 2.0000 | 1.00000 | 0.000000 | 0..85 |
| K-stock | 10 | 57989 | 0.48 | 0.9977 [0.9924, 1.0031] | 8.017 [7.838, 8.195] |  |  |  |  |
| D+AOT0 | 10 | 88582 | 1.15 | 1.5206 [1.5044, 1.5369] | 2.079 [1.881, 2.277] | 1.9999 | 0.99997 | 0.000051 | -822..0 |
| K | 20 | 57989 | 0.56 |  | 7.977 [7.789, 8.165] | 2.0000 | 1.00000 | 0.000000 | -371..2 |
| O177frozen | 10 | 60448 | 0.51 | 1.0421 [1.0359, 1.0484] | 7.281 [7.084, 7.477] | 1.9998 | 0.99989 | 0.000000 | 0..202 |
TOST K-stock vs K margin ±0.010: ratio 0.9977 90% CI [0.9934, 1.0021] n=10 -> EQUIVALENT
TOST O-stock vs O margin ±0.010: ratio 0.9972 90% CI [0.9909, 1.0035] n=20 -> EQUIVALENT
TOST D-stock vs D margin ±0.010: ratio 1.0002 90% CI [0.9954, 1.0052] n=10 -> EQUIVALENT
TOST Odiag vs O margin ±0.015: ratio 1.0049 90% CI [1.0009, 1.0088] n=10 -> EQUIVALENT
TOST D+AOT0 vs D margin ±0.020: ratio 0.9906 90% CI [0.9836, 0.9976] n=10 -> EQUIVALENT
TOST O177frozen vs O margin ±0.015: ratio 1.0015 90% CI [0.9956, 1.0074] n=10 -> EQUIVALENT

## 256 KB

rows=120 valid=120 reps=20 payload=262144 exp=E1
| leg | n | median rps | CV% | GMR vs K [95% CI] | x vs B us/req [95% CI] | ev/req | del/exp | lost/req | missing-reported |
|---|---:|---:|---:|---|---|---:|---:|---:|---:|
| O | 20 | 4889 | 1.19 | 1.0363 [1.0297, 1.0429] |  | 3.8387 | 0.11609 | 0.000000 | 671313..700652 |
| O-stock | 20 | 4879 | 1.02 | 1.0383 [1.0297, 1.0471] |  |  |  |  |  |
| K | 20 | 4694 | 1.22 |  |  | 17.9625 | 0.99935 | 0.010084 | 2..18 |
| K-stock | 20 | 4722 | 1.16 | 1.0035 [0.9944, 1.0127] |  |  |  |  |  |
| D-stock | 20 | 5812 | 1.67 | 1.2507 [1.2367, 1.2647] |  |  |  |  |  |
| D | 20 | 5806 | 1.35 | 1.2451 [1.2358, 1.2544] |  | 17.9702 | 0.99954 | 0.007423 | -18..0 |
TOST K-stock vs K margin ±0.010: ratio 1.0035 90% CI [0.9960, 1.0111] n=20 -> NOT shown
TOST O-stock vs O margin ±0.010: ratio 1.0020 90% CI [0.9969, 1.0071] n=20 -> EQUIVALENT
TOST D-stock vs D margin ±0.010: ratio 1.0045 90% CI [0.9944, 1.0147] n=20 -> NOT shown

## Gate outcomes (G0)
- (a) 16 B: K, D (n=10) and O (n=20) cnt loader equivalent to stock within ±1%. 256 KB: O equivalent; K (1.0035 [0.9960, 1.0111]) and D (1.0045 [0.9944, 1.0147]) not shown at n=20 → G0(a) fallback at 256 KB only: 256 KB throughput is reported from the stock-loader legs (K-stock, O-stock, D-stock) run in the same session, and the cnt legs provide the audit.
- (b) Odiag (all off) ≡ O within ±1.5%. (c) D+AOT0 vs D within ±2% (−0.9%). (d) O177frozen ≡ O within ±1.5%.
- (e) nginx busy (sum over {2,6}) ≈ 0.996 for B. (f) O del/exp at 16 B ≥ 0.9975 (median 1.00000). (g) K missing − reported ≤ 2 (one request is the readiness curl), τ = max(1e-4·exp_ev, 200) holds.
- 29 rows of reps 0-1 ran before this Claude session was moved to CPUs 12-14 (see README deviations).
