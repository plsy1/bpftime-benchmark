# H7b-20261126-try1 — E7b: HAProxy with 4 threads (x86 NUC), summary

Eval plan E7b (Q1's main answer; "H7b" because E7/E7c are older experiment names). Session ended 15:53:20, exit 0;
checks before/after PASS. 180 measured rows (+ 9 warm-up) in rounds 0–1, all valid; round 2: 24 perf rows. Image
bpftime-ssl-haproxy:x86-20261126 (frozen nginx image + Ubuntu's haproxy 2.8.30-0ubuntu0.24.04.2, libssl unchanged;
R/docker-haproxy/Dockerfile). Policy H4 (harness/config-x86.json): `nbthread 4`, threads cpu-mapped to CPUs 2, 3, 6, 7
(2 P cores, both SMT threads each; haproxy-t4.cfg), consumer on 4, wrk on 0, 1, 8–11 with 6 threads and 96 connections
(wrk splits 100 into 6 × 16; the generator was widened from 8–11 after the smoke saturated 4 E-core wrk threads at B);
response from `http-request return` (16 B body). Legs B, K, Kb (= K + SSLSNIFF_PB_WAKEUP=32), Ol (upstream head
70f7bc1f), OlL3, OlL3x730, Dn (Tern c71a9987), Dn730f (Tern + #730 lock, PREREG-2b), Dnpc; S4 counting loader; 10 s
runs; 2 rounds × 10 paired reps, all legs shuffled per rep; round 2 perf stat (sc) on the haproxy process, 3 reps ×
20 s, 8 legs (no B). Tables: summary-h7b.md (analyze_paper.py), cpu-per-event.txt, perf-sc.txt.

Harness changes for E7 (run_paper.py vs .pre-e7, session.sh vs .pre-e7): a server interface (start_haproxy, readiness
by curl, thread-count check), per-thread run time of the server, "show info" from the stats socket, request count
from CumReq (see flags); the nginx path is unchanged.

## Throughput (GMR of rps, rounds 0–1, n = 20, 95 % CI; rounds agree within 0.006, the Q1 pairs within 0.002)

| ratio | value |
|---|---|
| **Dn730f / OlL3 (Q1)** | **1.0526 [1.0494, 1.0557]** (round 0 1.0532, round 1 1.0520) |
| Dn / OlL3x730 | 1.0528 [1.0494, 1.0561] |
| Dn / Ol | 1.1684 [1.1651, 1.1717] |
| Dnpc / Ol | 1.0605 [1.0565, 1.0645] |
| Dnpc / Kb | 1.2672 [1.2633, 1.2710] |
| Dn / Kb | 1.3960 [1.3928, 1.3992] |
| Dn / K | 1.5349 [1.5311, 1.5387] |
| Ol / Kb | 1.1948 [1.1919, 1.1978] |
| Kb / K | 1.0995 [1.0968, 1.1021] |
| OlL3 / Ol | 1.0886 [1.0839, 1.0932] |
| OlL3x730 / OlL3 (lock removed, upstream) | 1.0195 [1.0157, 1.0234] |
| Dn730f / Dn (lock added, Tern) | 0.9807 [0.9780, 0.9834] |

Median rps: B 324k, Dn 260k, Dn730f 255k, OlL3x730 247k, OlL3 242k, Dnpc 236k, Ol 223k, Kb 186k, K 169k.

## Per-event cost, two units

The server is saturated in every row, so per-event cost follows from throughput. Two units are given; they differ by
the thread count (4) and must not be mixed:
- **Wall-time x (PREREG-2 §3 unit, = (1/rps − 1/rps_B)·1e6/2):** Dn730f − OlL3 −0.103 us/event [−0.110, −0.097],
  Dn − OlL3x730 −0.101 [−0.108, −0.095] (summary-h7b.md). Single-thread values (H7a, C1 nginx) are ≈ −0.20.
- **Server thread CPU (4 threads' run time ÷ events, ≈ 4 × wall-time x under saturation; cpu-per-event.txt):** B 6.144
  (median; mean 6.148); x Dn 1.513, Dn730f 1.665, OlL3x730 1.919, OlL3 2.074, Dnpc 2.290, Ol 2.805, Kb 4.552, K 5.614
  (paired means, ± 0.015–0.029). Paired: Dn730f − OlL3 −0.408 ± 0.023; Dn − OlL3x730 −0.406 ± 0.026; OlL3 −
  OlL3x730 +0.155 ± 0.029; Dn730f − Dn +0.152 ± 0.022; Dn − Ol −1.292 ± 0.025; Dnpc − Ol −0.515 ± 0.033.
- Consumer CPU per event (sniff_cpu_ns ÷ events, not in the figures above): Tern legs 0.79, upstream legs 0.72–0.73,
  Kb 0.72, K 1.56 us.

Absolute x charges to the tracer everything in HAProxy's own per-request cost that changes between B's rate and the
leg's rate; only leg-vs-leg differences cancel B. B's thread CPU per event is 6.14 here vs 4.21 single-thread (H7a),
×1.46 for the combined change in thread count, SMT sharing, connection count and generator placement; the tracing
costs do not scale by one factor (H7b x / H7a x: K 1.36 … OlL3x730 1.59), so no single-thread comparison of
thread-CPU figures is made.

## System calls in the haproxy process (perf sc, per request, median of 3; perf-sc.txt)

| leg | all | getuid | getgid | getpid | process_vm_readv | all − these |
|---|---:|---:|---:|---:|---:|---:|
| K / Kb | 9.56 / 9.44 | 0 | 0 | 1.00 | 0 | 8.56 / 8.44 |
| Ol | 20.37 | 5.99 | 5.99 | 3.00 | 0 | 5.39 |
| OlL3 / OlL3x730 | 8.45 / 8.41 | 0 | 0 | 3.00 | 0 | 5.46 / 5.41 |
| Dn / Dn730f | 6.47 / 6.42 | 0 | 0 | 1.00 | 0 | 5.47 / 5.42 |
| Dnpc | 8.46 | 0 | 0 | 1.00 | 2.00 | 5.46 |

Upstream adds one getpid per event (2 per request; the 1.00 seen with K and Tern is presumably HAProxy's own, B was
not measured) and Ol in addition 6 getuid + 6 getgid per request, as on nginx (C1, E18); OlL3 removes exactly the 12
credential calls; Dnpc's two process_vm_readv per request are its checked reads. The remainder is 5.4–5.5 per request
on every bpftime leg (194k–249k rps in the perf runs) but ~3 higher with the kernel tracer; the likely cause is the x86-64 uretprobe
return path (sys_uretprobe exists on this 6.12 kernel and raw_syscalls counts it), not verified. Denominators are
access-log lines over the perf window (log drops bias the rates up by ≤ 0.1 %).

## Delivery and loss (16 B; percentages of events = 2 × requests)

Unreported events = 2·req − delivered − lost: K and Kb +2 in every run (the readiness request, sent before the kernel
tracer attaches, as on nginx C1), every Tern leg −3 in every run (HAProxy-specific: Tern's offset is 0 on nginx C1;
start-up events seen only by the in-process agent, cause not traced). Neither offset lies inside the measurement
window. Ol, OlL3, OlL3x730 report 0 lost; median −3 like Tern, but 18 of their 60 runs miss more, up to 8,136 events
(0.18 %) unreported. Reported loss: K/Kb in 15/40 runs (≤ 0.07 %), Dn 10/20 (≤ 0.18 %), Dn730f 2/20 (≤ 0.15 %), Dnpc
6/20 (≤ 0.11 %).

## Reading (corrected after the independent check)

1. **Q1 (eval plan E7b criterion: lower 95 % bound > 1.05 and point ≥ 1.10): not met.** Dn730f / OlL3 = 1.0526 [1.0494,
   1.0557]: the point is about half the required lead and the lower bound misses 1.05 (Holm adjustment would only
   widen the CI). Per the plan (L462) the sentence "The rest of \sys's design matters under threads" goes (the plan
   points to intro.tex:39; it now sits at eval.tex:100) and the paper turns analytic, as PREREG-2 §8 already implies.
2. With 4 threads the like-for-like gaps are 5.3 % for both pairs (with the lock and without), vs 3.8 % on single-thread
   HAProxy (H7a) and 3.6 % on single-worker nginx (C1). Only the ratio is a fair cross-session comparison; it is
   confounded by SMT sharing, 96 vs 100 connections and the generator move. In wall-time units the 4-thread gap is
   0.10 us/event (half the single-thread 0.20) because 4 threads serve in parallel; per core or per thread-CPU it is
   0.20 or 0.41. These units disagree, so no "gap per event grows with threads" claim is made.
3. The #730 lock costs about the same on both bases here: +0.155 (upstream) vs +0.152 (Tern) us/event of thread CPU
   (difference of differences not tested), 2 % of throughput. Per probe hit (≥ 2 per event: entry and return) that is
   ≤ ~77 ns, the size of E4's microbenchmark at T = 4 (median ns/call: Ol − Ol730x +82; Dn730f − Dn +59 in E4f); it is
   diluted to 2 % by ~8 us of thread CPU per event. (E4's paired wall-time ratios at T = 4: upstream 1.24, Tern 1.18 in E4f.)
4. Against the kernel the threaded server keeps the single-thread picture: Dnpc / Kb 1.267, Dn / Kb 1.396 (H7a: 1.2715,
   1.404; nginx C1 Dn / Kb 1.350). Kb / K is 1.10 here vs 1.14 single-thread.
5. Upstream head's credential calls cost 0.73 us/event of thread CPU (Ol − OlL3; 0.18 in wall-time units), the largest
   item in Tern's lead over upstream head (Dn − Ol −1.29 thread CPU).

## Flags

- Generator headroom (eval plan): CPUs 2, 3, 6, 7 busy 3.97–3.99 of 4 in every row (each ≥ 0.99; every thread ran
  9.98–10.10 s per 10 s); busiest wrk CPU ≤ 0.78 (B), ≤ 0.67 elsewhere; CPU 4 (consumer) median 0.27–0.56 on traced legs
  (max 0.574, K), 0.06–0.14 at B (no consumer).
- **Deviation from PREREG-2 §6:** its non-measured set {0, 1, 3, 5, 7, 15} includes CPUs H4 uses (wrk 0, 1; haproxy 3,
  7), so the analyzer flags all 180 rows and the pre-registered sensitivity analysis cannot be run. Post hoc
  substitute: on the remaining {5, 15}, 1 of 180 rows exceeds 5 % (5.08 %; next 5.00 %).
- Request count: HAProxy (`log stdout format raw`) drops access-log lines with several threads (up to 2,096 per 10 s
  run, 2,869 in the 20 s perf runs), so the count is CumReq − 1 (run_paper.py subtracts the non-request log lines,
  which were exactly 1 in every row). The extra 1 in CumReq is more likely the stats-socket "show info" stream than the
  connection-error line (CumConns − CumSslConns = 1; not confirmed in the HAProxy source); the result is the same while
  both are 1, and with one thread (H7a) the log equals CumReq − 1 exactly. wrk counts 63–93 fewer (in-flight at stop).
- server_thread_cpu_ns sums every task of the haproxy process, including bpftime's 2 agent threads, which ran 0 ns in
  every window.
- The session's pin-show and frequency logs list only CPUs 2, 4, 6, 8–11 (hard-coded in session.sh), so the pinning of
  0, 1, 3, 7 is not in the session record; check-before/after (CPUs 0, 1, 3, 7 at 3518–3526 MHz) and current sysfs (all
  restored) cover it. session.sh now derives these lists from the pin list.
- O was not run at 4 threads (not in the plan). Q1 used an unadjusted CI (it fails anyway).
- Not yet run: E7b's tool part (memleak and funclatency on haproxy), which waits for the x86 tool builds of E6.
- Independent check (3 subagents with their own scripts: numbers, H7a cross-checks, harness review): every number
  reproduces; corrections applied above.
