# Claims to verify (v3 rounds, 2026-11-12 14:35-15:22 JST)

Raw logs: logs/v3/r1..r4 (rounds5.sh; 14 cells x sets P M100 AC AC2 AC3 AC4 AC5 ACn, shuffled per round
and cell); summary logs/v3/summary.md (SETS="P M100 AC AC2 AC3 AC4 AC5 ACn" ROUNDS_FILE=rounds-v3-cells.txt
summarize.py logs/v3). One build (watcher-policy-v3.patch on top of v2). Sets (all AC: park + adaptive
window + 8 deferral sleeps of 20 us; AC2-ACn with v2's latch rule):
- AC: window doubles from 10 to 300 us, closed by a gap >= 300 us.
- AC2 (v2): window max(2 x gap, 10), at most 50 us, closed by any gap >= 50 us.
- AC3: window doubles from 10 up to 50 us, closed only by two gaps >= 50 us in a row.
- AC4: window doubles up to 50 us, halved by each gap >= 50 us, closed below 10 us.
- AC5: AC3, with the gap measured to the last check that found no request (GAP_LB) instead of to the
  detection.
- ACn: window off (WIN_MAX=0): deferral sleeps only.
"Gap" = from the end of the previous service to the detection of the request (AC5: to the last empty
check). New cells c31/c32: 1 us kernels, 30000 launches, no load / full load (P 27 us per launch);
latency there covers the first 4095 calls only (the probe keeps 4096 records). "CPU" = victim CPU s per
wall s minus P's of the same round (1.0 = one core). Cost per call = (per-launch mean - P's) / calls.

1. All 448 runs valid; max temperature 54.9 C (.env snapshots).
2. Bursts: AC3 has the lowest cost per call of the AC family in every round of c05, c06, c25, c26,
   c27, c28 (medians: c05 15.7 vs AC 16.6 / AC2 22.1 / AC4 20.7; c06 11.8 vs 12.5 / 12.2 / 12.2;
   c25 45.2 vs 54.4 / 51.8 / 52.5; c26 27.4 vs 31.3 / 31.2 / 31.4; c27 18.4 vs 20.3 / 19.9 / 20.7;
   c28 40.6 vs 49.1 / 49.2 / 46.6), equal to AC5 within 2.3 us per round; M100 is lower in c25-c27
   (43.2 / 26.0 / 17.4) and c06 (11.3), higher in c28 (52.0). ACn (no window) 32.6 in c05, 27.0 in c06.
3. AC3's extra CPU over AC in bursts: +0.004 to +0.008 cores (c25 0.030 vs 0.026 ... c28 0.040 vs
   0.034, every round); AC2 +0.012 to +0.015.
4. The c05 regression of v2 comes from closing the window on one long gap: AC2 (one gap closes) and
   AC4 (one gap halves) are 20.7-22.6 us per call in every round, AC3 and AC5 (two gaps in a row)
   15.1-16.2; AC2's win0 about 6,300-6,900 per run vs AC3's 1-25.
5. AC3 keeps the window open across launches in small bursts (c25: win20 for all 4001 requests but
   one, every round; AC2 closes at every launch: win0 2001).
6. c23/c24 (gaps 55-115 us): AC2, AC3, AC4, AC5 and ACn are equal (window closed for all requests in
   AC2/AC3; CPU +0.05 to +0.14 per round, overlapping; c24 calls > 1 ms 0.23-0.28%); AC +1.03 cores
   and 2.80% > 1 ms under load, as in v2.
7. c31 (dense requests, no load): AC5 polls (window 21-50 us for nearly every request, about 13 M hot
   iterations) and costs 79-86 us of CPU per launch vs AC3 36-42, ACn 34-40, AC2 68-74, P 20-21;
   per-launch mean AC5 53.4-64.3 vs AC3 55.5-62.4, AC2 47.5-52.8, ACn 54.3-69.1 us. AC3 opens its window
   for about 40-63% of the requests.
8. c32 (dense requests, full load): AC5 has 28-59 calls > 1 ms per run (of 4095 recorded) vs AC3 6-8,
   AC2 4-8, ACn 7-9; per-launch mean 153.6-181.7 vs AC3 121.1-128.1 us; AC5 keeps polling under load
   (9-13 M hot iterations; AC3 under 20,000). Measuring the gap to the last empty check removes the
   lengthening by the watcher's own preemption, which is what made AC2/AC3 stop polling under load.
9. c01, c02, c15, c21: all v2/v3 sets equal to AC within noise (c21 CPU: AC +0.08, the others
   +0.04-0.08).
10. Choice: AC3 is the best rule tested: no continuous polling at 55-115 us gaps (as AC2), better
    than AC in every burst cell, no tail under dense requests with full load.
