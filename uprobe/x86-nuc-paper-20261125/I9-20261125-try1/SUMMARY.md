# I9-20261125-try1 — co-location policy C (x86 NUC), summary

Session 01:09:41–01:44:33, exit 0. 147 rows (7 warm-up), 140 measured, all valid; checks before/after PASS; noisy_core
rows 0 (analyze_paper.py, from busy_all; busy CPUs are 2 and 8–11 only). Policy C: one nginx worker and the consumer
share CPU 2 (nginx 0 migrations; wrk 8–11, 4 threads); 16 B payload; cnt loader; 2 rounds × 10 paired reps (round 0
after one warm-up rep). Kb#C = K#C+SSLSNIFF_PB_WAKEUP=32; "nap0 WB32" = Dnpc#C with BPFTIME_CONSUMER_NAP_US=0 and
SSLSNIFF_PB_WAKEUP=32 (the only Tern leg with the kernel's batch-32 policy; Dn#C and Dnpc#C run Tern's default nap).
Tables: `summary-i9.md` (analyze_paper.py per round).

Question (eval plan I9, method review C3): policy M gives the consumer its own core and hides its cost; does Tern's lead
survive when the consumer shares the traced worker's core?

## Ratios (GMR of rps, both rounds pooled, n = 20, 95 % CI; the two rounds differ by ≤ 0.007)

| ratio | value | policy |
|---|---|---|
| Dnpc nap0 WB32 / Kb#C | 1.192 [1.187, 1.197] | same (batch 32, no nap) |
| Dnpc#C / Kb#C | 1.182 [1.177, 1.186] | Tern default nap vs kernel batch 32 |
| Dn#C / Kb#C | 1.285 [1.281, 1.289] | Tern default nap vs kernel batch 32 |
| Ol#C / Kb#C | 1.176 [1.171, 1.181] | upstream 1 ms nap vs kernel batch 32 |
| Dn#C / Ol#C | 1.093 [1.089, 1.097] | |
| Dnpc#C / Ol#C | 1.005 [1.002, 1.007] | |
| Dnpc nap0 WB32 / Dnpc#C | 1.009 [1.004, 1.014] | |
| Kb#C / K#C | 1.402 (round 0 1.400, round 1 1.404) | |
| K#C / B#C | 0.388 | |

## CPU and loss (medians over 20 runs)

| leg | rps | consumer us/event | nginx us/request | both us/request | lost % of events, median (max) |
|---|---:|---:|---:|---:|---:|
| B#C | 109.1k | – | 9.1 | – | – |
| K#C | 42.3k | 2.60 | 18.5 | 23.5 | 3.52 (3.90) |
| Kb#C | 59.3k | 0.77 | 15.2 | 16.8 | 1.15 (1.35) |
| Ol#C | 69.8k | 0.72 | 12.8 | 14.2 | 0 reported; 0.49 unreported |
| Dn#C | 76.2k | 0.87 | 11.3 | 13.0 | 0.47 (0.64) |
| Dnpc#C | 70.1k | 0.89 | 12.4 | 14.2 | 0.26 (0.47) |
| Dnpc nap0 WB32 | 70.7k | 0.84 | 12.4 | 14.0 | 0.61 (0.87) |

Reading:
1. With the consumer on the worker's core, checked Tern still beats the kernel at the same notification policy
   (nap0 WB32 / Kb#C 1.19), and Tern's defaults beat the kernel's batch-32 setting (checked 1.18, unchecked 1.29).
   Under policy M the nearest numbers are E3's (S4lat loader, another session): Dn / K WB32 1.369, Dnpc / K WB32 1.240;
   C2 will give like-for-like policy-M numbers. The lead comes from the traced side: Tern's consumer costs a little
   more per event than Kb's (0.87–0.89 vs 0.77 us) but nginx spends less per request (11.3–12.4 vs 15.2 us).
2. Against upstream head, checked Tern's lead shrinks to 0.5 % (Dnpc#C / Ol#C 1.005 [1.002, 1.007]; 0.7 % after
   normalizing by delivery, since Ol drops 0.49 % unreported vs Dnpc's 0.26 %); unchecked keeps 9 %.
3. K#C runs at 0.39× baseline, and batching helps the kernel far more here (Kb#C / K#C 1.40) than in E3 under policy M
   (1.125, different loader and session, so indicative only). A plausible mechanism is that each per-event wakeup
   preempts the traced worker on the shared core; this run does not measure context switches.
4. Every leg loses some events in this configuration (K#C 3.5 %, Kb#C 1.1 %, Tern 0.3–0.6 %, upstream ~0.5 % unreported).
   Normalizing by delivery (events or bytes give the same within 0.03 pt) moves the Tern / K#C ratios by 3.0–3.4 % and
   Kb#C / K#C by 2.5 %, and changes no conclusion; the paper
   should say "with event loss under 4 % on every leg", not "lossless". Co-location as the cause of the loss is inferred:
   I9 has no policy-M control with the cnt loader in the same session.
5. Scope: 16 B only (I3 shows ratios shrink with payload; co-location at larger payloads was not tested).

## Flags

- Lost counter: in every K, Kb and Tern row cnt.lost equals 2·req − delivered exactly (offset −2 on K rows, 0 on Tern),
  so reported loss is complete. missing_ev at 16 B undercounts write misses (16 KiB units; harness comment "C1 audit"),
  so missing_ev − lost is not meaningful here. Ol#C's unreported loss is unreported_ev = 3,296–11,357 events per run
  (median 0.49 %). (An earlier draft quoted missing_ev − lost and a counter-window explanation; both were wrong.)
- Limitation for the paper (eval plan §721): policy M gives the consumer its own core; this run is the sensitivity check.
- Independent check (3 subagents, own scripts, 2026-11-26 ~02:00): all ratios, CPU and loss numbers reproduce; the rps
  column was corrected from round-0 to pooled medians and the readings above incorporate its objections (policy match,
  "disappears", E3 comparison, mechanism, loss cause). A second pass on the corrected text fixed one CI rounding
  (Dnpc#C / Ol#C), one CPU cell (Ol#C 14.2) and the normalization range.
