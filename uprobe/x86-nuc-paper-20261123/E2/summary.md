# x86 E2 affinity gate (S vs M in the same reps) + pinbench

Session E2-affinity-20261123. Reps 0-4 overlapped the Jetson artifact copy to the NUC (14:29:30-14:39:30); reps 0-4 vs 5-9: Odiag/K M 1.0480 vs 1.0495, S 1.1753 vs 1.1731; pin M 3.554 vs 3.424, S 1.701 vs 1.753 us/req — no effect.

rows=110 valid=110 reps=10 payload=16 exp=E2
| leg | n | median rps | CV% | GMR vs K [95% CI] | x vs B us/req [95% CI] | ev/req | del/exp | lost/req | missing-reported |
|---|---:|---:|---:|---|---|---:|---:|---:|---:|
| O#M | 10 | 60475 | 1.05 | 1.0426 [1.0322, 1.0531] | 7.443 [7.300, 7.586] | 1.9998 | 0.99992 | 0.000000 | 0..188 |
| D#S | 10 | 88525 | 0.62 | 1.5352 [1.5266, 1.5438] | 2.090 [2.031, 2.149] | 2.0000 | 1.00000 | 0.000000 | -335..0 |
| B#S | 10 | 108791 | 0.34 | 1.8844 [1.8768, 1.8920] |  |  |  |  |  |
| K#M | 10 | 57863 | 0.67 |  | 8.149 [8.071, 8.226] | 2.0000 | 1.00000 | 0.000000 | -182..2 |
| K#S | 10 | 57648 | 0.55 |  | 8.126 [8.059, 8.193] | 2.0000 | 1.00000 | 0.000000 | -272..2 |
| Odiag#S | 10 | 67740 | 0.61 | 1.1742 [1.1684, 1.1800] | 5.557 [5.498, 5.617] | 1.9998 | 0.99989 | 0.000000 | 0..276 |
| D#M | 10 | 88689 | 0.57 | 1.5345 [1.5244, 1.5447] | 2.124 [2.053, 2.195] | 1.9998 | 0.99990 | 0.000198 | -1123..0 |
| Odiag#S+NO_PIN | 10 | 76857 | 0.62 | 1.3299 [1.3214, 1.3385] | 3.831 [3.775, 3.886] | 2.0000 | 0.99999 | 0.000000 | 0..663 |
| Odiag#M+NO_PIN | 10 | 77453 | 1.29 | 1.3302 [1.3192, 1.3413] | 3.856 [3.729, 3.983] | 2.0000 | 0.99998 | 0.000000 | 0..439 |
| Odiag#M | 10 | 60563 | 0.71 | 1.0487 [1.0439, 1.0536] | 7.345 [7.257, 7.433] | 1.9997 | 0.99983 | 0.000000 | 0..307 |
| B#M | 10 | 109455 | 0.48 | 1.8907 [1.8820, 1.8995] |  |  |  |  |  |
DIFF x(Odiag#M) - x(Odiag#M+NO_PIN) = 3.489 us/request [3.377, 3.601] n=10; per event (/2) 1.745 us
DIFF x(Odiag#S) - x(Odiag#S+NO_PIN) = 1.727 us/request [1.657, 1.796] n=10; per event (/2) 0.863 us
DIFF x(O#M) - x(Odiag#M) = 0.098 us/request [-0.048, 0.244] n=10; per event (/2) 0.049 us

Interaction Δx_pin(M) − Δx_pin(S) = 1.762 us/request, 95% t-CI [1.598, 1.926], n=10.

## pinbench (idle, CPU 4 at 3.5 GHz, 10 runs × 1e6)

| case | starting mask | median ns | cycles @3.5 GHz |
|---|---|---:|---:|
| pin_sequence | single | 801.1 | 2804 |
| pin_sequence | pair | 1612.1 | 5643 |
| pin_sequence | pcores | 1604.4 | 5615 |
| pin_sequence | all | 1522.0 | 5327 |
| sched_getcpu | - | 1.1 | 4 |
| getppid | - | 86.7 | 304 |
| getpid | - | 87.1 | 305 |
| gettid | - | 83.7 | 293 |
| getuid+getgid | - | 164.9 | 577 |
| readlinkat_proc_self_exe | - | 695.8 | 2435 |

G1: fired. Official's x86 lead over the kernel is +17.4% when nginx is pinned to one core (S, the historical method) and +4.9% when it may run on two (M). The pin costs 0.86 us/event under S and 1.75 us/event under M, as pinbench predicts (same-mask fast path 0.80 us; a mask with more cores 1.5-1.6 us). Without the pin, official is +33% under both. K, D and B are unaffected by the policy.
