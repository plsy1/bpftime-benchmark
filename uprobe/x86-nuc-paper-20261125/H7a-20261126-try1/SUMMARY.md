# H7a-20261126-try1 — E7a: HAProxy with 1 thread (x86 NUC), summary

Eval plan E7a ("H7a" because E7/E7c are older experiment names). Session ended 14:51:36, exit 0; checks before/after
PASS. 250 measured rows (+ 13 warm-up), all valid. Image bpftime-ssl-haproxy:x86-20261126 (haproxy 2.8.30, libssl as in
the frozen nginx image). Policy H1: `nbthread 1` (haproxy-t1.cfg), the thread under the mask {2,6} like nginx's policy
M, consumer on 4, wrk on 8–11 with 4 threads and 100 connections; `http-request return` body of 16 B (round 0) or
4 KiB (round 1); S4 counting loader; 10 s runs; 10 paired reps per round, all legs shuffled per rep. Legs (12 of the
plan): B, K, Kb (SSLSNIFF_PB_WAKEUP=32), L3port, Ol, Olpc, OlL3x730, OlL3x730pc, Dn, Dnpc, Dxn+EXACT,
Dnpc/NAP0-WB32 (Dnpc + BPFTIME_CONSUMER_NAP_US=0 + SSLSNIFF_PB_WAKEUP=32); O (official 4708f409) at 16 B only, for
delivery. Tables: summary-h7a.md (analyze_paper.py), ../H7b-20261126-try1/cpu-per-event.txt (H7a part).

## Throughput at 16 B (GMR of rps, n = 10, 95 % CI)

| ratio | value |
|---|---|
| Dnpc / Kb (M1 analogue) | 1.2715 [1.2609, 1.2822] |
| Dxn+EXACT / Kb (M2 analogue) | 0.9946 [0.9877, 1.0015] |
| Dnpc/NAP0-WB32 / Kb (M3 analogue) | 1.2676 [1.2598, 1.2755] |
| Dn / Kb | 1.4035 [1.3935, 1.4137] |
| Dn / K | 1.5975 [1.5896, 1.6054] |
| Kb / K | 1.1382 [1.1309, 1.1455] |
| Dn / OlL3x730 | 1.0384 [1.0307, 1.0462] |
| Dnpc / OlL3x730pc | 1.0311 [1.0197, 1.0426] |
| Dn / Ol | 1.1639 [1.1550, 1.1728] |
| Dnpc / Olpc | 1.1559 [1.1429, 1.1691] |
| L3port / Dn | 0.9826 [0.9740, 0.9914] |
| Ol / Kb | 1.2059 [1.1965, 1.2153] |

Median rps: B 118.0k, Dn 95.5k, L3port 93.7k, OlL3x730 91.9k, Dnpc 86.4k, Dnpc/NAP0-WB32 86.2k, OlL3x730pc 84.0k,
Ol 82.1k, Olpc 74.9k, Kb 68.1k, Dxn 67.6k, O 62.7k, K 59.7k.

Server CPU per event (thread run time ÷ events, x = leg − B paired means, us; B 4.211 median, mean 4.213; with one
thread this equals the PREREG-2 wall-time x within 0.02): Dn 1.007, L3port 1.099, OlL3x730
1.206, Dnpc 1.549, Dnpc/NAP0-WB32 1.565, OlL3x730pc 1.726, Ol 1.863, Olpc 2.447, Kb 3.107, Dxn 3.147, O 3.735, K 4.118
(± 0.02–0.08). Paired: Dn − OlL3x730 −0.199 ± 0.042; Dnpc − OlL3x730pc −0.177 ± 0.062; Dn − Ol −0.856 ± 0.042.

## 4 KiB

| ratio | value |
|---|---|
| Dnpc / Kb | 1.2318 [1.2233, 1.2403] |
| Dxn+EXACT / Kb | 0.9970 [0.9918, 1.0023] |
| Dnpc/NAP0-WB32 / Kb | 1.2380 [1.2260, 1.2500] |
| Dn / Kb | 1.3456 [1.3343, 1.3571] |
| Kb / K | 1.1087 [1.1025, 1.1149] |

Ol, Olpc, OlL3x730, OlL3x730pc and L3port deliver 18–25 % of the write bytes per run (per-leg medians 21–25 %; 0.47–1.0
events per request instead of 2, medians 0.51–0.81; read events are dropped too) and report no loss, as on nginx at
≥ 4 KiB (I3: 10–28 %). They do less work, so their 4 KiB ratios (e.g.
Dn / OlL3x730 1.017, L3port / Dn 1.011) are not like-for-like and are not reported. K, Kb and the Tern legs deliver
≥ 99.44 % of the logged write bytes in every run (per-leg medians 99.62–99.79 %; ~0.17 % of the byte gap is structural:
the log counts 4,184 B per response, SSL_write sees 4,177 B) and ≥ 99.7 % of events, and report their loss (median
0.03–0.14 % of events).

## Delivery at 16 B

Request counts from the access log equal CumReq − 1 (one non-request line per run) in every run. Unreported events
(2·req − delivered − lost): K/Kb +2, all Tern legs −3 in every run (constant offsets, as in H7b); Ol, Olpc, OlL3x730,
OlL3x730pc, L3port and O report 0 lost and miss up to 852 events per run (0.052 % of events) unreported (their best
runs show the same −3 as Tern). Reported loss ≤ 0.04 % of events in any run. K's +2 is the readiness request, sent
before the kernel tracer attaches (as on nginx C1); Tern's −3 is HAProxy-specific start-up events (0 on nginx C1).

## Reading (corrected after the independent check)

1. On single-thread HAProxy the nginx picture holds, with ratios 3–5 points higher (cross-session): Dnpc / Kb 1.2715
   (nginx DX1 1.240, E18 1.2375), Dn / Kb 1.40 (nginx 1.35), the same-policy Dnpc/NAP0-WB32 / Kb 1.268, and exact +
   checked within ±1.5 % of Kb (0.995 [0.988, 1.002]; TOST not run; nginx E18 0.989 [0.982, 0.996], ~1 % below). At
   4 KiB the checked ratios are 3–4 points lower (Dnpc / Kb 1.23, NAP0-WB32 / Kb 1.24), Dn / Kb ~6 points lower (1.35),
   and exact + checked stays at parity (0.997).
2. Tern vs upstream with three patches: Dn / OlL3x730 1.038, x −0.20 us/event on both the CPU and the PREREG-2 wall-time
   measure (−0.201 [−0.241, −0.162]; nginx C1: 1.036, −0.203); checked pair 1.031, −0.18. Outside the ±0.084 margin;
   the size is consistent with DX1's split (getpid ~0.10 + hash map ~0.10, x86-facts §19.2), not tested here (no perf
   round in H7a).
3. HAProxy's base cost per event (4.21 us of server CPU at 16 B) is ~7 % below nginx's (~4.5 us: N7 4.54, E18 4.59),
   so its ratios come out slightly larger, as the arithmetic predicts. The eval plan had allowed for smaller ratios on
   a server with a higher base cost; that case did not occur.
4. Upstream at 4 KiB silently drops ~3/4 of the data on HAProxy too; the defect is not nginx-specific.

## Flags

- Generator headroom: server thread run time 0.986–0.998 of wall time in every row (CPU 2 + 6 busy 0.99–1.02); busiest
  wrk CPU ≤ 0.52; CPU 4 0.002 (B, idle) to 0.28 (K, 4 KiB). The thread moved between CPUs 2 and 6 in 115 of 250 rows
  (≤ 7 migrations; within the mask, like nginx policy M; 2 and 6 are different P cores, so no SMT sharing).
- noisy_core (PREREG-2 §6 set {0, 1, 3, 5, 7, 15}): no row above 5 % (max 4.5 %).
- n = 10 per payload, one session.
- missing_ev (byte-based) is not used at 16 B (it undercounts writes there; x86-facts §19.8); at 4 KiB delivery is the
  byte ratio del/exp.
- Independent check (subagents with their own scripts): every number reproduces; corrections applied above.
