# Jetson results, 2026-11-24 morning (E2s, E3 O/D + Opc/Dpc first half, E4, E5-R2, E6, E7, E8, loaded pinbench)

Policy M unless noted, 16 B unless noted. GMR vs K with 95 % t-CI; x = extra µs per request vs B; per event = x / 2.000.
Full tables: E2s.md, E4.md, E5.md, E6.md, E7.md, E8.md in this directory (analyze_paper.py 6dcb26dd, analyze_e7.py 8f2f9b0e).

## E2s (policy S, n = 10)
O#S/K 0.998 [0.982, 1.014], Odiag#S/K 1.000 [0.990, 1.011]. Under M O/K is 0.892, so on the Jetson the official deficit
vs K is almost entirely a policy-M (two-core nginx mask) effect. G2: decided (O measured under S).

## E4 headline (n = 30 per payload)
- 16 B: O 0.892 [0.885, 0.898], D 1.199 [1.185, 1.214], Dpc 1.085, Opc 0.825. Stock legs equal to counting legs
  (O-stock 0.893, D-stock 1.198, Opc-stock 0.827, Dpc-stock 1.075). Every tracing leg ev/req 2.000, del/exp 1.0000.
- 256 KB: O 0.922 but delivers 37.4 % of the response bytes (7.92 vs 17.87 ev/req) with 0 reported lost (silent);
  Opc 40.2 % of bytes, also silent. D 1.136 (D-stock 1.131), Dpc 1.021; K, D, Dpc deliver ≥ 99.87 % and missing = reported.
- x86 for comparison: O 1.041, D 1.532 (16 B).

## E5-R2 (n = 10): O 0.887, D 1.201, Dpc 1.089 — within 0.5 pp of E4.

## E6 Figure 1 (n = 20), µs per event
| segment | K | Odiag | D |
|---|---:|---:|---:|
| hook (S1−B) | 4.36 | 2.70 | 2.92 |
| helpers (S2h−S1) | 0.95 | 3.04 | 0.97 |
| maps (S2hm−S2h) | 1.36 | 2.10 | 1.31 |
| probe read (S3−S2hm) | 0.42 | 0.06 | 0.25 |
| output (S4−S3) | 5.60 | 9.70 | 0.95 |
| total x(S4)/2 | 12.69 | 17.59 | 6.39 |
(differences of means; paired CIs to be computed with the figure script.)

Official "re-entering the kernel", real-patch ladder (main on the Jetson), µs per event:
O→L1 (no pinning) 7.72, L1→L2 (uid/gid cache) 2.32, L2→L3port (pid/tid cache) 0.75, total 10.79.
Odiag switches, cumulative: 7.70 / 2.10 / 0.99 (total 10.79 incl. Odiag→ALL3 path). Hook saving of the official vs K:
4.36 − 2.70 = 1.66 µs/event. So re-entering costs 6.5× the hook saving; net the official is 4.54 µs/event dearer than K
(x86: 2.52 vs 1.91, net 0.37 cheaper). Same structure as x86, much stronger.
Checks: E6 vs E4 at S4 within 1.4 % (O 34.45 vs 34.93, K 25.37 vs 25.61, D 12.78 vs 12.94 µs/req);
single knock-outs 15.41 + 3.47 + 2.36 = 21.23 vs ALL3 21.58 µs/req (residual 1.6 %); NOOP_OUTPUT − S3 = +0.48 µs/req;
ALL3 − L3port = +0.74 µs/req; O − Odiag = −0.74 µs/req (gate b direction, Odiag slightly slower).

## E7 counters (n = 3 per mode)
- Syscalls per request (nginx worker): B 8.03; K 8.02 at S1/S3/S4 (no extra, unlike x86's +3); D 8.02; Odiag+ALL3 8.02;
  L3port 8.02; O and Odiag 29.98 (+22: setaffinity 4, getaffinity 2, getuid 6, getgid 6, getpid 2, gettid 2);
  L1 24.02 (+16), L2 12.02 (+4); Dpc 10.02 (+2 process_vm_readv). Same counts as x86.
- Pinning in cycles (Odiag@S4 − NO_PIN): user +4,343, kernel +20,634 cycles/request = 2 pin sequences, i.e. ~12,500
  cycles per sequence (10,300 kernel). Idle pinbench pair mask: 5,397 cycles (3.12 µs). So the in-nginx kernel side is
  ~2× the idle unit cost, plus ~2,200 user cycles of indirect cost per sequence.
- Cross-method closure (Δcycles/f vs E6 Δx): K@S1−B 0.90, Odiag@S1−B 0.97, Odiag@S2h−S1 0.98, D@S1−B 0.95,
  pin 14.45 vs 15.41 (0.94); K@S3−K@S1 1.32 (outside ±25 %: kernel program time not all on the worker's wall clock?).
- Kernel BPF stats: 6.001 hits/request at S1, S3, S4 (invariant). Program time 0.43 / 3.8 / 9.0 µs per request.
  K hook x(S1)−x(B) 8.72 µs/req, of which ~8.3 µs is trap/return = ~1.38 µs per hit (~2,390 cycles at 1.728 GHz).

## Loaded pinbench (60 s B leg, pinbench on cpu3 from t = 10 s)
All unit costs unchanged vs idle (ratio 0.97–1.004, including the all-CPU mask, which doubled on x86).
So on the Jetson the 2.4× in-nginx pinning cost is not explained by load on other cores; E7 says it is kernel cycles
in the worker (~2× idle) plus user-side indirect cycles. Candidate (not tested): nginx runs inside the container's
cpuset cgroup, pinbench on the host.

## E8 payload sweep (n = 5): O silent loss starts at 16 KB on the Jetson (del/exp by events up to 1 KB, by write bytes above, as x86)
O del/exp: 1 KB 1.0000, 2 KB 0.9998, 4 KB 0.9995 (missing − reported 16..131, inside τ), 16 KB 0.916,
128 KB 0.412, 256 KB 0.376, with 0 reported lost. K and D ≥ 0.998 everywhere. D/K 1.19 (16 B) → 1.14 (256 KB);
O/K 0.88–0.93. (x86: O loses from 1 KB, 78 %.)

## E3 microbenchmark (O/D: ABBA complete; Opc/Dpc: first half only), ns per call, median of 10 or 20 samples
- D/O probe read 1.002, write 1.003: no dev regression on the Jetson (x86: D 2,533 vs O 1,694). uprobe 1.07,
  uretprobe 1.13, uprobe+uretprobe 1.08 (D slower by 14–35 ns).
- Maps: D/O 0.06–0.74 (array lookup 1,185 vs 20,023), array/per-cpu-array delete equal.
- Probe check on: Opc read 859,971 / write 864,819 ns per call of 1,000 operations → ~855 ns per checked access
  (one process_vm_readv), 188× O. x86 ~270 ns. End-to-end the checks cost Opc 0.825 vs O 0.892 at 16 B.

## Follow-ups for the NUC session (11-24 07:00)
- E6 ratios vs K: L1 1.0908 [1.0843, 1.0973], L2 1.1683 [1.1606, 1.1761], L3port 1.1962 [1.1807, 1.2118],
  Odiag+NO_PIN 1.0791 [1.0709, 1.0874]; L3port per event 6.44 µs (D 6.39): official + three patches ≈ dev.
- E3 absolute (ns / cycles at 1.728 GHz): K uprobe 1242/2145, uretprobe 1674/2893, both 1788/3090 (bookends 1240-1242);
  O 197/340, 268/463, 320/553; D 211/364, 303/524, 346/598. The single bpftime hook is cheap; the e2e hook per event
  (4,670 cycles) is 4 probes plus indirect cost.
- K@S3−K@S1 closure 1.32: BPF program time +3.39 µs/req; inside E7 Δ(1/rps) 7.18 vs Δcycles 7.22 (1.00). The gap is E7's
  K@S3 rps 2 % below E6 (n = 3, -d 20, perf attached), not the counters.
- K +3 syscalls on x86 = sys_uretprobe (Linux 6.11+ x86-64, NUC verified); arm64 6.8 has no such path, hence +0 here.
- cpuset control (pinbench host vs container on cpu4, ABAB x 3) queued after E10-E14.
- E8 D / K del/exp per payload: 16 B 1.00000/0.99999, 1 KB 1.00000/0.99999 (events); 2 KB 0.99986/0.99993,
  4 KB 0.99931/0.99942, 16 KB 0.99982/0.99981, 128 KB 0.99914/0.99921, 256 KB 0.99886/0.99847 (bytes). D minimum 0.99886.
