# E3-20261125-try1 — latency / notification-policy factorial (x86 NUC), summary

Session 19:19:31–21:02:46, exit 0. 287 rows (17 warm-up), 270 measured, all valid; sha guard PASS before/after;
only tailscale-watchdog active among timers; noisy_core rows 0. Loader: H2 `sslsniff-s4lat2` (sha e3669180…).
Latency = uprobe hit → consumer read of that event, from the SIGUSR2..SIGUSR1 window; quantiles are the `*_inf`
variants (lost and unreported events count as +inf). Full tables: `summary-e3.md` (harness/summarize_e3.py).

Legs: K = kernel sslsniff (immediate wakeup); K WBn = SSLSNIFF_PB_WAKEUP=n; Dn/Dnpc/Dxn = Tern unchecked / checked /
exact (BPFTIME_DIAG_EXACT=1); nap0 = BPFTIME_CONSUMER_NAP_US=0 (no nap: the consumer is woken per event); default
nap = Tern's default; nap1000 = official-like 1 ms nap; O/Ol = official / upstream head (1 ms nap by default).

## Round 0, closed loop (n = 10 paired reps)

| policy | leg | rps / K (95% CI) | p50 us | p99 us | consumer ns/event |
|---|---|---|---:|---:|---:|
| immediate | K | 1 | 2.5 | 2.8 | 2,211 |
| immediate | Dn nap0 | 1.433 [1.424, 1.441] | 2.6 | 3.1 | 3,032 |
| immediate | Dnpc nap0 | 1.293 [1.284, 1.302] | 3.1 | 3.6 | 3,424 |
| immediate | Dxn exact nap0 | 1.027 [1.022, 1.031] | 3.5 | 3.9 | 3,936 |
| batch 32 | K WB32 | 1.125 [1.115, 1.135] | 116 | 262 | 814 |
| batch 32 | Dxn exact nap0 WB32 | 1.115 (= 0.991 × K WB32, TOST ±2 % equivalent) | 113 | 227 | 896 |
| batch 32 | Dnpc nap0 WB32 | 1.394 (1.240 × K WB32) | 82.5 | 204 | 943 |
| batch 32 | Dn nap0 WB32 | 1.530 (1.360 × K WB32) | 74.8 | 159 | 919 |
| default nap | Dn | 1.540 (1.369 × K WB32) | 76.3 | 154 | 903 |
| default nap | Dnpc | 1.395 | 77.0 | 154 | 918 |
| default nap | Dxn exact | 1.117 | 77.5 | 154 | 952 |
| 1 ms nap | Dnpc nap1000 | 1.391 | 457 | 976 | 939 |
| 1 ms nap | Ol | 1.334 | 546 | 1,088 | 915 |
| 1 ms nap | O | 1.042 | 546 | 1,085 | 964 |
| batch 8 / 128 | K WB8 / K WB128 | 1.114 / 1.130 | 30.7 / 351 | 63 / 923 | 954 / 1,025 |

Nap effect at fixed build: default nap / nap0 = 1.075 (Dn), 1.079 (Dnpc), 1.089 (Dxn).

Reading:
1. At the kernel's own policy (wake per event) Tern keeps a 43 % (unchecked) / 29 % (checked) / 2.7 % (exact) lead
   at kernel-like latency (p50 2.6–3.5 us vs 2.5 us). It pays more consumer CPU per event (3.0–3.9 vs 2.2 us) but
   less nginx CPU per request (12.0 vs 17.1 us), so total CPU per request is lower (≈18.0 vs ≈21.6 us for Dn nap0).
2. At matched batching (WB32), the exact build equals the kernel (0.991, equivalent at ±2 %), with the same
   latency (113 vs 116 us p50). This matches E18's Dxn+EXACT / K WB32 = 0.9886 under the S4 loader.
3. Tern's default (nap) costs ~7.5–9 % throughput against nap0 and moves p50 from ~3 us to ~77 us; the 1 ms nap of
   official/upstream gives ~0.5 ms p50 and ~1.1 ms p99.
4. The throughput-only headline (Dn / K WB32 = 1.37) is at roughly half K WB32's latency, not at the kernel's
   default latency.

## Rounds 1–2, open loop (n = 5; achieved rate 999.6 / 24,991 req/s)

| leg | 1,000 req/s p50 / p99 us | 25,000 req/s p50 / p99 us |
|---|---|---|
| K | 11.1 / 21.4 | 2.5 / 13.0 |
| K WB32 | 6,505 / 15,950 | 143 / 1,230 |
| K WB128 | 19,550 / 660,603 (p99.9 3.8 s) | 1,018 / 2,685 |
| Dnpc nap0 | 11.2 / 32.7 | 3.2 / 28.0 |
| Dxn exact nap0 | 5.7 / 34.5 | 3.6 / 25.4 |
| Dnpc (default nap) | 78.0 / 164 | 77.6 / 154 |
| Dxn exact (default nap) | 79.7 / 164 | 77.9 / 154 |
| Dnpc nap0 WB32 | 384 / 1,062 | 89 / 1,079 |
| Dnpc nap1000 | 507 / 1,070 | 586 / 1,092 |
| Ol | 510 / 1,071 | 581 / 1,090 |

Reading: a wakeup threshold (kernel WBn, or Tern's WB32) makes latency inversely proportional to the event rate
(at 2,000 events/s, 32 events take ~16 ms), while a nap bounds it by the nap length independent of rate. Tern's
default nap gives a rate-independent ~78 us p50 / ~160 us p99. Consumer CPU per event at 1,000 req/s is 11.7 us (K)
vs 15.6 (Dnpc nap0) and 17–18.5 us (nap legs): at low rate the per-wakeup cost dominates.

## Flags (no row dropped)

- **The S4lat loader loses events in closed loop on every leg** (reported lost, up to 1,460 per 10 s run; K 1,180;
  O/Ol report 0 lost but have 275/656 unreported). It timestamps every event, so the consumer is slower than the S4
  loader's. Throughput headlines stay with C2 (S4 loader), as the plan says; these rows are for latency and same-policy
  ratios. Lost events are included as +inf in all quantiles.
- **Open loop 25,000 req/s: the wrk Lua pacer fell behind** by > 1 ms on 3–5 % of requests (lag_gt1 19k–35k of
  ~750k), with lag_max 16 ms typical and 51–101 ms in 11 of 50 rows. Event latency is measured per event (uprobe →
  consumer), so it has no coordinated omission, but arrivals were burstier than the target rate. Reported losses at
  25k (K WB32/WB128, Dnpc nap1000, Dxn, up to 100 per run) do not line up with the 50+ ms lag rows (most are in 16 ms
  rows). pending_max = 25 in every row (connection cap per wrk thread).
- round 1 K WB128 p99/p99.9 (0.66 s / 3.8 s): events wait in the buffer until 128 accumulate or the poll timeout
  fires; this is the policy, not a fault.
- lat_n < events: the latency window is the SIGUSR2..SIGUSR1 interval, shorter than the 10 s load.
- Independent check (subagent, own scripts, 2026-11-25 23:3x): every number above reproduces; histogram buckets
  re-derive lat_q exactly. Notes from it: validity does not test cnt.lost (≤ ~0.1 % of events lost, rps unaffected,
  but no "lossless" wording); n_inf counts only losses inside the latency window (e.g. Dn nap0 rep 8: 1,368 lost in
  the run, 578 in the window); O/Ol p*_inf are upper bounds where loss_report_complete is false; **p99.9 varies
  widely between reps (K r2: 20–344 us) and should not be quoted**; p99 is stable.
