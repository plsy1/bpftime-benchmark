# I3-20261125-try1 — payload sweep (x86 NUC), summary

Session 23:19:43–01:05:27, exit 0. 448 rows (8 warm-up), 440 measured, all valid; checks before/after PASS; noisy_core
rows 0 (analyze_paper.py, from busy_all). Policy M (nginx {2,6}, sniff 4, wrk 8–11), cnt loader; payloads in the order
16K, 1K, 256K, 4K, 64K, one round of n = 10 paired reps each (CIs are within-round). 256 KiB adds stock-loader legs.
Deviation: Jetson ssh at 00:27:05–12 during round 2 rep 9 leg K (DEVIATIONS.md). Excluding that row moves the ratios by
≤ 0.0051 (Dn / K 1.261 → 1.256) and K's 256 KiB cost per event by 0.018 us (summary-i3-exclK29.md); nothing changes
in the readings. Tables: `summary-i3.md` (harness/summarize_i3.py). Kb = K+SSLSNIFF_PB_WAKEUP=32.

Eval-plan wording applies: "ratios under another configuration" (not the C2 loader/payload), never placed beside C2.

## Throughput ratios (GMR of rps, 95 % CI) and delivery

del/exp = delivered / expected **write bytes**; at 1 KiB the row field is an event ratio, so the byte ratio is computed
here as write_bytes / bytes_nginx.

| payload | Dn / Kb | Dnpc / Kb | Ol / Kb | Dn / K | Dnpc / Ol | bytes del/exp O, Ol, L3port | bytes del/exp K, Kb, Dn, Dnpc (median) |
|---:|---|---|---|---|---|---|---|
| 1 KiB | 1.353 [1.342, 1.363] | 1.236 [1.229, 1.242] | 1.178 [1.172, 1.184] | 1.526 | 1.049 | 0.71, 0.58, 0.52 | ≥ 0.999 |
| 4 KiB | 1.316 [1.308, 1.324] | 1.208 [1.200, 1.216] | 1.180 [1.171, 1.189] | 1.464 | 1.024 | 0.28, 0.23, 0.21 | ≥ 0.9995 |
| 16 KiB | 1.260 [1.252, 1.267] | 1.169 [1.163, 1.175] | 1.162 [1.156, 1.169] | 1.381 | 1.006 [1.000, 1.012] | 0.23, 0.20, 0.18 | ≥ 0.9992 |
| 64 KiB | 1.206 [1.198, 1.215] | 1.131 [1.124, 1.139] | 1.173 [1.167, 1.179] | 1.304 | 0.965 [0.959, 0.970] | 0.15, 0.13, 0.13 | ≥ 0.9982 |
| 256 KiB | 1.181 [1.166, 1.197] | 1.118 [1.108, 1.128] | 1.163 [1.151, 1.174] | 1.261 | 0.961 [0.949, 0.974] | 0.12, 0.10, 0.10 | ≥ 0.9977 |

Events per request (expected): 2, 2, 3, 6, 18 (one event per read, one per ≤ 16 KiB TLS record written).

## Cost per event (extra time per request vs B ÷ events per request; us, mean [95 % CI])

Legs that deliver (≥ 99.5 % in every run): cost per delivered event.

| payload | K | Kb | Dn | Dnpc |
|---:|---|---|---|---|
| 1 KiB | 4.08 [4.06, 4.11] | 3.08 [3.06, 3.10] | 1.04 [0.99, 1.09] | 1.59 [1.55, 1.63] |
| 4 KiB | 4.20 | 3.24 | 1.19 | 1.77 |
| 16 KiB | 3.94 | 3.09 | 1.25 | 1.80 |
| 64 KiB | 3.83 | 3.00 | 1.25 | 1.81 |
| 256 KiB | 3.83 [3.71, 3.95] | 3.07 [2.93, 3.22] | 1.36 [1.20, 1.53] | 1.90 [1.79, 2.00] |

Lossy legs: the true cost lies between cost per attempted (expected) event, which undercharges them (a dropped event
skips the copy-out and the consumer), and cost per delivered event, which overcharges them.

| payload | Ol per attempted / per delivered | O | L3port |
|---:|---|---|---|
| 1 KiB | 1.90 / 3.01 | 3.68 / 4.76 | 1.14 / 2.00 |
| 4 KiB | 1.93 / 5.81 | 3.74 / 9.31 | 1.12 / 3.68 |
| 16 KiB | 1.84 / 5.52 | 3.60 / 9.05 | 1.15 / 3.72 |
| 64 KiB | 1.49 / 4.00 | 3.53 / 8.15 | 0.97 / 2.74 |
| 256 KiB | 1.51 / 7.42 | 3.36 / 15.77 | 0.97 / 4.83 |

Stock loader at 256 KiB (X-stock / X): K 0.995 [0.989, 1.000], O 1.001 [0.990, 1.013], Dn 0.995 [0.979, 1.010],
Dnpc 0.988 [0.974, 1.003]: no effect of the counting loader detected; the CIs allow up to −2.6 %. Tested at 256 KiB only.

Reading:
1. Tern stays ahead of the kernel at every payload, unchecked and checked, against K and Kb.
2. The ratios shrink with payload (Dn / Kb 1.35 → 1.18). About three quarters of that is dilution: the request costs more
   (B 106k → 6.9k req/s) while per-event costs stay in a narrow band; holding the 1 KiB per-event costs fixed predicts
   Dn / Kb 1.22 at 256 KiB (observed 1.18). The rest is a real drift: Tern's per-event cost rises (Dn 1.04 → 1.36,
   Dnpc 1.59 → 1.90; 1 KiB and 256 KiB CIs do not overlap) while Kb's stays flat (3.08 → 3.07), so Tern's per-event gap
   to Kb shrinks from 2.04 to 1.71 us (unchecked) and from 1.49 to 1.18 us (checked); K's own cost falls slightly
   (4.08 → 3.83), which also narrows the gap to K.
3. Per event vs per byte cannot be separated here: events per request grow with payload (one per 16 KiB record) and
   sslsniff copies at most 8 KiB per event. The one constant-event contrast (1 → 4 KiB, 2 events/request) shows a
   per-byte component of about 0.1 us/KiB on the legs that deliver (Kb 0.105, Dn 0.10, Dnpc 0.12, K 0.08 ± 0.05);
   the lossy legs show ~0 (O 0.04, Ol 0.03, L3port −0.01).
4. Official, upstream and L3port drop data silently: they deliver 52–71 % of write bytes at 1 KiB and 10–28 % at ≥ 4 KiB,
   and report 0 lost. At ≥ 64 KiB the loss is mostly the 16 KiB records (reads 82–100 % delivered). Their consumers use
   5–11 % of a core (sniff_cpu_ns / wall; 4–13 % busy per run on core 4), so it is not consumer CPU; likely the producer's buffer filling between the consumer's 1 ms polls
   (not traced). Their throughput is therefore not comparable at ≥ 1 KiB, and neither normalization is fair (bounds
   above): per attempted event upstream (1.5–1.9 us) is cheaper than Kb and close to Dnpc, L3port (≈ 1.0–1.15) close to
   Dn, while doing less work. The paper should report their delivery, not their ratios, at these payloads.
5. Tern and the kernel: median byte delivery ≥ 99.77 % per payload, ≥ 99.52 % in every run; reported loss ≤ 0.43 % of
   events per run (K, 256 KiB).

## Flags (no row dropped)

- missing_ev − lost is negative for K, Kb, Dn, Dnpc at every payload (down to −1,363 at 4 KiB; −15 to −149 at
  ≥ 64 KiB, where the small header writes are undercounted; only Kb at 256 KiB stays ≥ +4). This is a property of missing_ev, not
  of the lost counter: run_paper.py counts the write shortfall in 16 KiB units (ceil(bytes/16384)), which undercounts
  missing writes at small payloads; at 1 KiB, where unreported_ev = 2·req − delivered − lost exists, it is 0 in every Dn
  and Dnpc run and a constant 2 in every K and Kb run, so lost accounts for the shortfall. (An earlier draft of this summary blamed a counter window; that was wrong.)
- The nginx core is often not saturated: leg means of its busy fraction range 0.92–0.98 at many points (K 4 KiB 0.92,
  Dn 64 KiB 0.94, K 64 KiB 0.945, Dn 256 KiB 0.95, B 4/256 KiB ~0.96, L3port 16 KiB 0.96 with one run at 0.69, most
  legs at 64/256 KiB 0.97–0.98), while the busiest points reach ~0.995. Part of the extra 1/rps is then idle or
  stall time, adding noise to per-event costs; Dn / K at 64 KiB compares two legs both near 0.94.
- O's 256 KiB per-delivered cost has a per-rep range of 14.2–17.3 us (CI 15.1–16.5); Ol and L3port there are as wide.
- Independent check (3 subagents, own scripts, 2026-11-26 ~02:00): every table number reproduced; the readings above are
  the corrected versions (per-event flatness, "only dilution", O/Ol per-event comparison, 1 KiB byte delivery, the
  missing_ev explanation, the exclusion bound and the stock-loader wording were wrong or overstated in the first draft).
  A second pass on the corrected text (2 subagents) found only the wording fixes now applied (busy list, per-byte
  scope, missing_ev scope, offset 2, consumer metric, one O cell).
