rows=100 valid=100 reps=20 payload=None exp=None
| leg | n | median rps | CV% | GMR vs K [95% CI] | x vs B us/req [95% CI] | ev/req | del/exp | lost/req | missing-reported |
|---|---:|---:|---:|---|---|---:|---:|---:|---:|
| O@S1 | 20 | 96320 | 1.63 |  | 1.249 [1.155, 1.344] | 0.0000 | 0.00000 | 0.000000 | 939475..998775 |
| Dnngp@S1 | 20 | 96603 | 0.80 |  | 1.166 [1.113, 1.218] | 0.0000 | 0.00000 | 0.000000 | 967737..996819 |
| B | 20 | 109090 | 0.66 |  |  |  |  |  |  |
| Dn@S1 | 20 | 96564 | 1.65 |  | 1.217 [1.131, 1.303] | 0.0000 | 0.00000 | 0.000000 | 928862..997230 |
| K@S1 | 20 | 70765 | 0.64 |  | 4.982 [4.941, 5.024] | 0.0000 | 0.00000 | 0.000000 | 710257..725280 |
DIFF x(Dn@S1) - x(Dnngp@S1) = 0.051 us/request [-0.037, 0.139] n=20; per event (/2) 0.026 us
DIFF x(Dn@S1) - x(O@S1) = -0.032 us/request [-0.155, 0.091] n=20; per event (/2) -0.016 us
DIFF x(Dnngp@S1) - x(O@S1) = -0.083 us/request [-0.175, 0.008] n=20; per event (/2) -0.042 us
DIFF x(K@S1) - x(Dn@S1) = 3.765 us/request [3.666, 3.864] n=20; per event (/2) 1.883 us
noisy_core rows (non-measured core busy > 0.05): 0 []
