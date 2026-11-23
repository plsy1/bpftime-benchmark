# x86 E9: same-frequency control (P cores of nginx and sslsniff at 1.7 GHz, turbo on, min=max; E cores 2.5 GHz)

Question (G14): does x86 behave like the Jetson (1.728 GHz) when clocked alike? Sessions E9-samefreq-20261123-try1 (e2e, policy M, 16 B, 10 reps) and E9-full10-1.7ghz-20261123-try1 (CPU 2 at 1.7 GHz, O and D).

## End to end

rows=60 valid=60 reps=10 payload=16 exp=E9
| leg | n | median rps | CV% | GMR vs K [95% CI] | x vs B us/req [95% CI] | ev/req | del/exp | lost/req | missing-reported |
|---|---:|---:|---:|---|---|---:|---:|---:|---:|
| D | 10 | 43968 | 0.99 | 1.5382 [1.5227, 1.5538] | 4.354 [4.176, 4.533] | 2.0000 | 1.00000 | 0.000000 | -122..0 |
| B | 10 | 54378 | 0.91 | 1.9031 [1.8862, 1.9201] |  |  |  |  |  |
| Odiag | 10 | 29743 | 0.84 | 1.0356 [1.0277, 1.0434] | 15.377 [15.158, 15.595] | 2.0000 | 1.00000 | 0.000000 | 0..34 |
| K | 10 | 28646 | 0.58 |  | 16.575 [16.356, 16.794] | 2.0000 | 1.00000 | 0.000000 | -20..2 |
| Odiag+NO_PIN | 10 | 38308 | 0.85 | 1.3402 [1.3296, 1.3509] | 7.708 [7.619, 7.798] | 2.0000 | 1.00000 | 0.000000 | 0..190 |
| O | 10 | 29722 | 0.19 | 1.0378 [1.0331, 1.0426] | 15.302 [15.196, 15.408] | 2.0000 | 1.00000 | 0.000000 | 0..1 |
DIFF x(Odiag) - x(Odiag+NO_PIN) = 7.668 us/request [7.447, 7.889] n=10; per event (/2) 3.834 us

| vs kernel | x86 3.5 GHz (E4/E2) | x86 1.7 GHz (E9) | Jetson 1.728 GHz (Jetson E1) |
|---|---:|---:|---:|
| official | 1.041 | 1.038 | 0.892 |
| official without pinning | 1.330 | 1.340 | 1.071-1.079 |
| dev | 1.532 | 1.538 | 1.215 |

Per-event pinning cost: 1.77 us at 3.5 GHz and 3.83 us at 1.7 GHz, i.e. about 6,200-6,500 cycles either way; Jetson policy M: 7.44 us at 1.728 GHz, about 12,900 cycles.

## Microbenchmarks at 1.7 GHz vs 4.4 GHz

Every kernel row costs the same number of cycles at both clocks (e.g. uprobe 4,884 vs 4,896 cycles), and every O/K and D/K ratio is unchanged within 0.02 except per-CPU hash lookup (4.15 vs 3.86, 0.66 vs 0.59).

G14: not fired. At the Jetson's clock x86 keeps official ahead (+3.8%) and dev at +54%; the platform difference is not the clock.

Correction (2026-11-24, from the consolidated fact sheet): the factor of about 2 in pin cycles is an end-to-end (in-situ) effect. Idle, the pin sequence costs the same cycles on both machines (pinbench: x86 2,804 / ~5,600 cycles for single / multi-core masks; Jetson 2,713 / ~5,400). In nginx the per-event pin costs 2.1-2.4x its idle unit price on the Jetson but 1.08x on x86. Simple syscalls (getpid) cost about 1.6-1.8x the cycles on the Orin (~500 vs ~300).
