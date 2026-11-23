# x86 E6: Figure 1 breakdown (policy M, 16 B, 2 blocks x 10 reps, 24 legs rotated together)

Session E6-fig1-20261123-try1; 480 rows, all valid. x = extra time per request over B (paired, us); per event = x / 2 (E_work = 2.000 from nginx). 95% t-CI half-widths.

| segment (per event, us) | kernel | official (Odiag) | dev |
|---|---:|---:|---:|
| hook (S1 - B) | 2.487 ±0.035 | 0.575 ±0.051 | 0.630 ±0.040 |
| helpers (S2h - S1) | 0.062 ±0.022 | 0.675 ±0.049 | 0.141 ±0.035 |
| maps (S2hm - S2h) | 0.273 ±0.017 | 0.309 ±0.040 | 0.139 ±0.031 |
| probe read (S3 - S2hm) | 0.059 ±0.032 | -0.028 ±0.036 | 0.019 ±0.030 |
| output (S4 - S3) | 1.135 ±0.044 | 2.113 ±0.043 | 0.115 ±0.059 |
| **total (S4 - B)** | **4.017 ±0.040** | **3.644 ±0.043** | **1.045 ±0.042** |

Official's re-entry into the kernel (same binary, switches at S4; cumulative order):
- per-event CPU pinning: 1.771 us/event (inside output)
- bpf_get_current_uid_gid syscalls: 0.546 us/event (inside helpers; 3 calls per event)
- getpid/gettid in perf output: 0.206 us/event (inside output)
- total re-entry 2.523 us/event, more than official's whole hook saving over the kernel (2.487 - 0.575 = 1.912 us/event). Net, official is only 0.373 us/event cheaper than the kernel.

Closure and additivity checks (PREREG 3.5):
1. Identity: segments sum to S4 - B by construction.
2. Cross-session: E6 x(S4) vs E4 (16 B): K 8.033 vs 8.020, O 7.287 vs 7.343, D 2.089 vs 2.038 us/request (within 2.5%).
3. Single knockouts vs ALL3: pin 3.542 + UIDGID-only 1.079 + PIDTID-only 0.482 = 5.103 vs ALL3 5.046 us/request (residual 1.1%, threshold 15%). Cumulative vs single: uid_gid 1.091 vs 1.079; pid/tid 0.413 vs 0.482.
4. NOOP_OUTPUT vs Odiag@S3: +0.025 ±0.059 us/request (helper dispatch of perf output costs ~0 when it returns at once).
5. ALL3 vs L3port (real patches): +0.036 ±0.061 us/request; equivalent within ±10% of Δ(Odiag@S4 - ALL3) = ±0.50.
6. O (stock build) vs Odiag@S4: +0.042 ±0.095 us/request.
Cross-method closure (E7 counters) not yet measured.
