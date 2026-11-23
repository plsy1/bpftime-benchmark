# E2 affinity gate + pinbench (Jetson), 2026-11-23

Session J-E2-20261123 (15:35–16:24), 16 B, 10 reps, 21 legs rotated together in every rep; all 210 rows valid, guards ok.
Policies: M nginx {4,5} / sslsniff {3} / wrk {0,1}; S nginx {5}; U all six CPUs; C nginx {4} / sslsniff {5} (same cluster).
x = 1/rps − 1/rps(B) per request (paired, 95 % t-CI); per event = /2 (E_work = 2.000 at 16 B).

| policy | K rps | Odiag/K GMR | Odiag+NO_PIN/K | D/K | Δx_pin = x(Odiag) − x(+NO_PIN) |
|---|---:|---|---|---|---|
| M | 13,148 | 0.892 [0.886, 0.898] | 1.079 [1.063, 1.096] | 1.204 [1.187, 1.222] | 14.87 µs/req [13.77, 15.98] = 7.44 µs/event |
| S | 13,022 | 0.981 [0.965, 0.997] | 1.073 [1.054, 1.093] | 1.225 [1.205, 1.244] | 6.73 µs/req [5.07, 8.39] = 3.36 µs/event |
| U | 13,408 | 0.879 [0.871, 0.887] | 1.071 [1.058, 1.083] | 1.187 [1.163, 1.211] | 15.12 µs/req [14.12, 16.12] = 7.56 µs/event |
| C | 13,433 | 0.953 [0.934, 0.972] | 1.048 [1.031, 1.067] | 1.180 [1.165, 1.194] | 7.12 µs/req [6.09, 8.14] = 3.56 µs/event |

- Interactions (same-rep paired): Δx_pin(M) − Δx_pin(S) = 8.14 µs/req [6.09, 10.20]; U − S = 8.39 [6.74, 10.05]; M − U = −0.25 [−1.75, 1.25]; C − S = 0.39 [−1.73, 2.50].
- O#M vs Odiag#M: x differs by −0.53 µs/req [−1.29, 0.24] (O 0.897 vs Odiag 0.892).
- C vs S (same cluster vs cross cluster consumer): x(K#C) − x(K#S) = −2.52 µs/req [−3.49, −1.55]; Odiag −0.33 [−2.03, 1.36]; D 0.22 [−1.11, 1.56].
- B: 19,717 (M) / 19,731 (S) / 19,772 (U) / 19,702 (C) rps; nginx busy ≈ 0.98 on its core(s).

## pinbench, idle (J-E2-pinbench-idle-20261123), cpu3 at 1.728 GHz, 10 runs × 1e6

| case | ns/op (median) | cycles |
|---|---:|---:|
| pin sequence, orig mask {3} (equal-mask fast path) | 1,570 | 2,713 |
| pin sequence, {2,3} / {3,4} / {0-3} / {0-5} | 3,123–3,129 | ~5,400 |
| getpid / gettid / getppid | 291 / 286 / 298 | ~500 |
| getuid + getgid | 595 | 1,028 |
| readlinkat(/proc/self/exe) | 1,690 | 2,920 |
| sched_getcpu (vDSO) | 3.6 | 6 |

End-to-end pin cost per event vs pinbench: S 3.36 / 1.57 = 2.1×, M 7.44 / 3.12 = 2.4× (x86: about 1.1× in both).

## Reading against the checkpoints
- G2: under S official still trails K on the Jetson (0.981, CI upper 0.997 < 1), but by 2 % instead of 11–12 % under M/U.
- Removing the pinning alone puts official ahead of K under every policy (+5 to +8 %).
- G2b: the C − S difference for Odiag is −0.17 µs/event (CI spans 0) → not triggered; the kernel consumer is 1.26 µs/event cheaper when on the same cluster.
