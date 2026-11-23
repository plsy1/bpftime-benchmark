# x86 E6x: real-patch ladder O -> L1 -> L2 -> L3port next to the Odiag switches (policy M, 16 B, 2 x 10 reps)

Added for symmetry with the Jetson, whose gate (b) failed: L1 = f14b7c0d (no per-event pinning), L2 = be93dd87 (+ uid/gid cache), L3port = 8ac1ec70 (+ pid/tid cache); same cmake options as official. Session E6x-ladder-20261123-try1, 200 rows, all valid.

rows=200 valid=200 reps=20 payload=16 exp=E6x
| leg | n | median rps | CV% | GMR vs K [95% CI] | x vs B us/req [95% CI] | ev/req | del/exp | lost/req | missing-reported |
|---|---:|---:|---:|---|---|---:|---:|---:|---:|
| L2 | 20 | 84807 | 0.54 | 1.4601 [1.4546, 1.4656] | 2.599 [2.540, 2.659] | 1.9994 | 0.99969 | 0.000000 | 0..469 |
| B | 20 | 109408 | 1.30 | 1.8730 [1.8597, 1.8865] |  |  |  |  |  |
| Odiag+NO_PIN+UIDGID | 20 | 84148 | 0.94 | 1.4475 [1.4403, 1.4548] | 2.702 [2.626, 2.777] | 1.9998 | 0.99992 | 0.000000 | 0..633 |
| K | 20 | 58203 | 0.50 |  | 8.025 [7.947, 8.102] | 2.0000 | 1.00000 | 0.000000 | -334..2 |
| Odiag+ALL3 | 20 | 87651 | 0.94 | 1.5058 [1.4973, 1.5143] | 2.242 [2.147, 2.336] | 1.9998 | 0.99989 | 0.000000 | 0..904 |
| L3port | 20 | 88093 | 1.32 | 1.5089 [1.4978, 1.5201] | 2.218 [2.123, 2.314] | 1.9996 | 0.99981 | 0.000000 | 0..646 |
| Odiag+NO_PIN | 20 | 77326 | 0.83 | 1.3294 [1.3234, 1.3354] | 3.759 [3.685, 3.833] | 2.0000 | 0.99998 | 0.000000 | 0..498 |
| Odiag | 20 | 60594 | 0.57 | 1.0438 [1.0403, 1.0472] | 7.303 [7.238, 7.368] | 2.0000 | 1.00000 | 0.000000 | 0..276 |
| O | 20 | 60340 | 0.71 | 1.0396 [1.0359, 1.0433] | 7.369 [7.278, 7.460] | 2.0000 | 0.99999 | 0.000000 | 0..256 |
| L1 | 20 | 77497 | 0.77 | 1.3331 [1.3272, 1.3390] | 3.723 [3.640, 3.806] | 1.9996 | 0.99979 | 0.000000 | 0..567 |
TOST L3port vs Odiag+ALL3 margin ±0.015: ratio 1.0021 90% CI [0.9960, 1.0082] n=20 -> EQUIVALENT
TOST L1 vs Odiag+NO_PIN margin ±0.015: ratio 1.0028 90% CI [0.9980, 1.0076] n=20 -> EQUIVALENT
TOST L2 vs Odiag+NO_PIN+UIDGID margin ±0.015: ratio 1.0087 90% CI [1.0050, 1.0124] n=20 -> EQUIVALENT
DIFF x(O) - x(L1) = 3.646 us/request [3.566, 3.726] n=20; per event (/2) 1.823 us
DIFF x(L1) - x(L2) = 1.124 us/request [1.068, 1.179] n=20; per event (/2) 0.562 us
DIFF x(L2) - x(L3port) = 0.381 us/request [0.294, 0.467] n=20; per event (/2) 0.190 us
DIFF x(O) - x(L3port) = 5.150 us/request [5.063, 5.237] n=20; per event (/2) 2.575 us
DIFF x(Odiag) - x(Odiag+NO_PIN) = 3.544 us/request [3.481, 3.608] n=20; per event (/2) 1.772 us
DIFF x(Odiag+NO_PIN) - x(Odiag+NO_PIN+UIDGID) = 1.057 us/request [0.982, 1.132] n=20; per event (/2) 0.528 us
DIFF x(Odiag+NO_PIN+UIDGID) - x(Odiag+ALL3) = 0.460 us/request [0.378, 0.542] n=20; per event (/2) 0.230 us
DIFF x(Odiag+ALL3) - x(L3port) = 0.023 us/request [-0.062, 0.108] n=20; per event (/2) 0.012 us
DIFF x(Odiag) - x(O) = -0.066 us/request [-0.123, -0.009] n=20; per event (/2) -0.033 us

Real patches and switches agree on every step (per event): pinning 1.823 vs 1.772, uid/gid 0.562 vs 0.528, pid/tid 0.190 vs 0.230, total re-entry 2.575 (real) vs 2.523 (E6 switches) us.
