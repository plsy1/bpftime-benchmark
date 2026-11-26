rows=160 valid=160 reps=20 payload=16 exp=DN1
| leg | n | median rps | CV% | GMR vs K [95% CI] | x vs B us/req [95% CI] | ev/req | del/exp | lost/req | missing-reported |
|---|---:|---:|---:|---|---|---:|---:|---:|---:|
| Dn | 20 | 89255 | 1.15 |  | 2.087 [2.023, 2.152] | 1.9999 | 0.99995 | 0.000098 | -400..0 |
| Dnextpc | 20 | 80997 | 0.68 |  | 3.186 [3.137, 3.234] | 2.0000 | 1.00000 | 0.000000 | -301..0 |
| Dnext+BPFTIME_CAREFUL=1 | 20 | 89210 | 0.60 |  | 2.082 [2.041, 2.123] | 1.9999 | 0.99993 | 0.000139 | -583..0 |
| Dnext | 20 | 88927 | 0.69 |  | 2.090 [2.042, 2.139] | 2.0000 | 0.99998 | 0.000032 | -645..0 |
| K+SSLSNIFF_PB_WAKEUP=32 | 20 | 65660 | 0.63 |  | 6.091 [6.037, 6.146] | 2.0000 | 0.99999 | 0.000014 | -363..2 |
| Dnextpc+BPFTIME_CAREFUL=1 | 20 | 80841 | 0.56 |  | 3.217 [3.184, 3.250] | 2.0000 | 1.00000 | 0.000000 | -273..0 |
| Dnpc | 20 | 81003 | 1.25 |  | 3.213 [3.136, 3.290] | 2.0000 | 0.99999 | 0.000014 | -453..0 |
| B | 20 | 109358 | 0.51 |  |  |  |  |  |  |
TOST Dnext vs Dn margin ±0.015: ratio 0.9997 90% CI [0.9941, 1.0053] n=20 -> EQUIVALENT
TOST Dnextpc vs Dnpc margin ±0.015: ratio 1.0022 90% CI [0.9972, 1.0072] n=20 -> EQUIVALENT
TOST Dnext+BPFTIME_CAREFUL=1 vs Dnext margin ±0.015: ratio 1.0007 90% CI [0.9975, 1.0040] n=20 -> EQUIVALENT
TOST Dnextpc+BPFTIME_CAREFUL=1 vs Dnextpc margin ±0.015: ratio 0.9974 90% CI [0.9936, 1.0013] n=20 -> EQUIVALENT
RATIO Dnpc / K+SSLSNIFF_PB_WAKEUP=32 = 1.2328 [1.2248, 1.2409] n=20
RATIO Dnextpc / K+SSLSNIFF_PB_WAKEUP=32 = 1.2355 [1.2308, 1.2402] n=20
RATIO Dnextpc+BPFTIME_CAREFUL=1 / K+SSLSNIFF_PB_WAKEUP=32 = 1.2323 [1.2272, 1.2375] n=20
RATIO Dn / K+SSLSNIFF_PB_WAKEUP=32 = 1.3562 [1.3482, 1.3644] n=20
RATIO Dnext+BPFTIME_CAREFUL=1 / K+SSLSNIFF_PB_WAKEUP=32 = 1.3568 [1.3501, 1.3635] n=20
TOSTUS x(Dnext+BPFTIME_CAREFUL=1) - x(Dnext) = -0.004 us/event 90% CI [-0.022, 0.014] 95% CI [-0.026, 0.018] n=20 margin ±0.084 -> EQUIVALENT, no difference shown
TOSTUS x(Dnextpc+BPFTIME_CAREFUL=1) - x(Dnextpc) = 0.016 us/event 90% CI [-0.008, 0.040] 95% CI [-0.013, 0.045] n=20 margin ±0.084 -> EQUIVALENT, no difference shown
TOSTUS x(Dnext) - x(Dn) = 0.001 us/event 90% CI [-0.030, 0.033] 95% CI [-0.037, 0.040] n=20 margin ±0.084 -> EQUIVALENT, no difference shown
TOSTUS x(Dnextpc) - x(Dnpc) = -0.014 us/event 90% CI [-0.045, 0.018] 95% CI [-0.052, 0.024] n=20 margin ±0.084 -> EQUIVALENT, no difference shown
noisy_core rows (non-measured core busy > 0.05): 0 []
