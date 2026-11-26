rows=120 valid=120 reps=20 payload=16 exp=M4
| leg | n | median rps | CV% | GMR vs K [95% CI] | x vs B us/req [95% CI] | ev/req | del/exp | lost/req | missing-reported |
|---|---:|---:|---:|---|---|---:|---:|---:|---:|
| Dviewpc+BPFTIME_CAREFUL=1+BPFTIME_VIEW=1 | 20 | 81218 | 0.74 |  | 3.123 [3.058, 3.189] | 2.0000 | 1.00000 | 0.000000 | -356..0 |
| Dviewpc+BPFTIME_CAREFUL=1 | 20 | 81661 | 0.71 |  | 3.102 [3.044, 3.160] | 2.0000 | 1.00000 | 0.000000 | -246..0 |
| Dnextpc | 20 | 81320 | 1.18 |  | 3.200 [3.135, 3.265] | 2.0000 | 1.00000 | 0.000000 | -308..0 |
| Dviewpc | 20 | 81504 | 0.87 |  | 3.109 [3.030, 3.189] | 1.9999 | 0.99997 | 0.000055 | -160..0 |
| B | 20 | 109513 | 1.04 |  |  |  |  |  |  |
| K+SSLSNIFF_PB_WAKEUP=32 | 20 | 65832 | 0.72 |  | 6.025 [5.957, 6.093] | 2.0000 | 1.00000 | 0.000000 | -301..2 |
TOST Dviewpc+BPFTIME_CAREFUL=1+BPFTIME_VIEW=1 vs Dviewpc+BPFTIME_CAREFUL=1 margin ±0.015: ratio 0.9983 90% CI [0.9942, 1.0024] n=20 -> EQUIVALENT
TOST Dviewpc vs Dnextpc margin ±0.015: ratio 1.0074 90% CI [1.0012, 1.0135] n=20 -> EQUIVALENT
RATIO Dviewpc+BPFTIME_CAREFUL=1+BPFTIME_VIEW=1 / K+SSLSNIFF_PB_WAKEUP=32 = 1.2361 [1.2297, 1.2426] n=20
RATIO Dviewpc+BPFTIME_CAREFUL=1 / K+SSLSNIFF_PB_WAKEUP=32 = 1.2382 [1.2328, 1.2437] n=20
RATIO Dnextpc / K+SSLSNIFF_PB_WAKEUP=32 = 1.2285 [1.2198, 1.2373] n=20
TOSTUS x(Dviewpc+BPFTIME_CAREFUL=1+BPFTIME_VIEW=1) - x(Dviewpc+BPFTIME_CAREFUL=1) = 0.011 us/event 90% CI [-0.015, 0.036] 95% CI [-0.020, 0.041] n=20 margin ±0.084 -> EQUIVALENT, no difference shown
TOSTUS x(Dviewpc) - x(Dnextpc) = -0.045 us/event 90% CI [-0.083, -0.008] 95% CI [-0.091, 0.000] n=20 margin ±0.084 -> EQUIVALENT, no difference shown
noisy_core rows (non-measured core busy > 0.05): 0 []
