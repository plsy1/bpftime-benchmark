# v3: window rules for C (2026-11-12 14:35-15:22 JST)

v2 (`../v2/README.md`) removed the continuous busy poll with a 50 us window limit, but its
window closed on any single gap of 50 us or more, and under full load 32-call bursts got worse
(16.9 -> 22.1 us per call). v3 keeps the 50 us limit and tries other rules for when the window
grows, shrinks and closes; the A part (park, latch) is v2's in every v3 set and is not exercised
here (no graph cells; no run latched).

## Sets (`scripts/watcher-policy-v3.patch`, on top of `../v2/scripts/watcher-policy-v2.patch`)

All are AC (park while no probed kernel is in flight, adaptive window, then up to 8 deferral sleeps of
20 us; the deferral sleeps are skipped while sleeps return more than 30 us late, the late-sleep guard).
The gap is measured from the end of the previous service to the detection of the request.

| set | window size | window closes | env |
|---|---|---|---|
| AC | doubles from 10 to 300 us | on one gap >= 300 us (1 ms episode cap, ineffective) | V2 unset |
| AC2 | max(2 x gap, 10), at most 50 us | on one gap >= 50 us | V2=1 |
| AC3 | doubles from 10, at most 50 us | on two gaps >= 50 us in a row; a caught or shorter gap resets the count | V2=1 WIN_MODE=3 |
| AC4 | doubles from 10, at most 50 us | halved by each gap >= 50 us, closed below 10 us (KVM's shrink) | V2=1 WIN_MODE=4 |
| AC5 | as AC3 | as AC3, gap measured to the last check that found no request | V2=1 WIN_MODE=3 GAP_LB=1 |
| ACn | none | (window off, WIN_MAX=0) | V2=1 WIN_MAX=0 |

AC5 was added after the smoke test: with 1 us kernels AC2 stayed in a polling regime (51.7 us per
launch) and AC3 in a sleeping one (71.0 us): the measured gap includes the watcher's own sleep (and
preemption), so polling keeps gaps short and sleeping keeps them long. Its implementation is flawed
(review): the last empty check is taken before each sleep, so a request the window missed and the
first deferral sleep found has a measured gap equal to the window and counts as caught; the window
then never adapts down. AC5 is not a clean test of measuring gaps without the watcher's delays.

## Setup

- 14 cells, 8 sets, 4 rounds, 448 runs, all valid by the summarizer's checks (`verify.csv`); highest
  temperature 54.9 C (before/after snapshots). Order shuffled per round and cell (seeded). One build.
- New cells c31 / c32: 1 us kernels, 30,000 launches, no load / full load (P 27 us per launch).
  Their latency covers the first 4,095 calls only (the probe keeps 4,096 records), about 14% of the
  run, and the share of fast calls swings within a run (512-call blocks 0-99%); window counters,
  CPU and per-launch times cover the whole run. "CPU minus P" is negative there for sleeping sets
  (P's own host thread uses 0.5-0.8 core, its share depends on the launch time): compare sets by
  CPU per launch (CPU seconds / launches), not with P. c32's P drifts over the rounds (per-launch
  mean 37.9 -> 55.9 us), so its "load vs P" depends on the round.
- Repeated cells: c23 / c24 (gaps about 65-125 us under bpftime, 55-115 us at P's launch rate), c21,
  c25-c28 (2 / 4 / 8 calls per launch; 4 under load), c05 / c06 (32 calls), c01, c02, c15.
- Summary: `SETS="P M100 AC AC2 AC3 AC4 AC5 ACn" ROUNDS_FILE=rounds-v3-cells.txt summarize.py logs/v3`.
- Review: independent recomputation and a critic (`review/review.json`); corrections are in the text.
  No outside disturbance found in the no-load tegrastats; the check is weak in the short cells
  (c23: 1-4 samples).

## Results (`summary.md`)

Cost per call = (per-launch mean - P's of the same round) / calls per launch (includes the
per-launch hook cost). Medians over 4 rounds; "every round" means the same sign in all 4 (with 4
rounds, at best p = 0.125 per cell).

| cell | M100 | AC | AC2 | AC3 | AC4 | AC5 | ACn |
|---|---:|---:|---:|---:|---:|---:|---:|
| c05 32 calls, full load: cost per call (us) | 16.9 | 16.6 | 22.1 | 15.7 | 20.7 | 15.6 | 32.6 |
| c06 32 calls, no load | 11.3 | 12.5 | 12.2 | 11.8 | 12.2 | 11.7 | 27.0 |
| c25 2 calls, no load | 43.2 | 54.4 | 51.8 | 45.2 | 52.5 | 44.8 | 53.1 |
| c26 4 calls, no load | 26.0 | 31.3 | 31.2 | 27.4 | 31.4 | 27.5 | 39.5 |
| c27 8 calls, no load | 17.4 | 20.3 | 19.9 | 18.4 | 20.7 | 18.3 | 31.9 |
| c28 4 calls, full load | 52.0 | 49.1 | 49.2 | 40.6 | 46.6 | 41.4 | 55.2 |
| c23 gaps 55-115 us, no load: CPU (cores) | 1.039 | 1.028 | 0.066 | 0.104 | 0.069 | 0.110 | 0.105 |
| c24 same, full load: calls > 1 ms | 2.74% | 2.80% | 0.28% | 0.26% | 0.26% | 0.23% | 0.27% |
| c31 1 us kernels, no load: per-launch mean (us) | 50.9 | 53.1 | 52.1 | 57.0 | 57.4 | 54.6 | 64.1 |
| c31: CPU per launch (us) | 72.8-88.7 | 80.7-90.6 | 67.7-73.5 | 36.4-41.6 | 35.3-39.5 | 79.5-85.7 | 34.2-40.2 |
| c32 same, full load: calls > 1 ms per run (of 4,095) | 41-55 | 65-72 | 4-8 | 6-8 | 5-8 | 28-59 | 7-9 |
| c32: per-launch mean (us) | 149.2 | 165.3 | 127.6 | 124.3 | 126.7 | 164.7 | 128.0 |

1. **Closing the window on one long gap caused v2's burst regression.** In c05, AC2 (one gap
   closes) and AC4 (one gap halves) cost 20.7-22.6 us per call in every round, AC3 and AC5 (two
   gaps in a row) 15.1-16.2, AC 16.3-17.3. AC3 and AC4 share the doubling and differ only in the
   reaction to one gap of 50 us or more, so this is a controlled contrast. AC2's window closed
   about 6,300-6,900 times per run, AC3's 1-25. The regression runs through the late-sleep guard:
   AC2 / AC4 skipped their deferral sleeps about 4,000-4,600 times per run (AC3 / AC5 350-440, AC
   600-660) and fell into 100 us sleeps; without load (c06, no guard skips) the same rules cost
   only 0.4-0.5 us per call more. What creates the mid-burst gaps of 50 us or more (presumably the
   watcher's preemption) is not measured.
2. **AC3 is cheaper per call than AC, AC2, AC4 and ACn in all 24 burst cell-rounds** (margin over
   the best of them: 0.4-0.5 us in c06, 0.6-1.5 in c05, 1.1-2.0 in c27, 2.3-7.9 in c25 / c26 /
   c28); AC5 is within 2.8 us of it (lower in 12-13 of 24). In small bursts AC3 keeps its window
   size across launches (c25: window 20 us or less for 4,001 of 4,002 requests; AC2 closes at every
   launch), so the second call of a launch is caught; the watcher still stops polling at most
   20 us after the last call (hot iterations about half of AC2's). Price: +0.002 to +0.013 cores
   over AC in every round (medians +0.005 to +0.008 in c25-c28, +0.009 / +0.011 in c05 / c06). M100
   is lower than AC3 in c06 and c25-c27 by median (not in c25 round 4), higher in c05 and c28, at
   0.1-0.18 cores.
3. **At 55-115 us gaps (c23 / c24) AC2, AC3, AC4 and ACn never open the window** (0 hot iterations
   in every run); their CPU differences (+0.03 to +0.14 per round) are noise between identical
   behaviour (noise floor about 0.05 core). AC: +1.03 cores without load, +0.26 and 2.80% of calls
   over 1 ms under load. AC5 opens slightly under load.
4. **Dense requests (1 us kernels).** Without load (c31) AC2 is faster than AC3 in every round
   (per-launch 47.5-52.8 vs 55.5-62.4 us) for about 30-37 us more CPU per launch (between-set
   CPU differences match the hot iterations at about 105-112 ns each); AC3, AC4 and ACn sleep and
   their per-launch ranges overlap. AC5 polls on nearly every request (12.9-13.4 M hot iterations)
   at 80-86 us of CPU per launch without being faster than AC3 or ACn in a way the ranges support.
   Under full load (c32) AC2, AC3, AC4 and ACn barely poll and have 4-9 calls over 1 ms per 4,095;
   AC5 keeps polling (9.3-11.8 M hot iterations) and has 28-59 and a per-launch mean of 154-182 vs
   121-128 us. AC3's whole-run per-launch mean in c32 is still about 50 us above its median
   (121-128 vs 74.5 us; P 10-24 us apart): a tail as large as ACn's, not none.
5. c01, c02, c15: all sets equal to AC within noise. c21 (gaps 155-295 us): AC's 300 us threshold
   opens windows there, and its host-call mean is about 4 us lower than every v2/v3 set in every
   round (63.8-66.1 vs 66.7-71.4 us) for 0.01-0.04 more cores; not equal, as v2 claimed.

## What it means for the design

AC3 (window doubling from 10 us up to 50 us, closed by two gaps of 50 us or more in a row) is the
best compromise tested, not a dominant rule: it removes the continuous poll at 55-115 us gaps,
costs less than AC in every burst cell, and adds no tail beyond ACn's under dense requests with
full load. It gives up throughput with dense requests and no load to AC2 (and latency to M100,
which pays up to a core), about 4 us of latency at 155-295 us gaps to AC, and 0.002-0.013 cores in
bursts. Not tested: gap patterns alternating short and long (each short gap resets the count, so
the window never closes; polling stays bounded by 50 us per request), the 50-65 us edge,
several processes, graph launches with the v3 rules, and a correct gap measurement that excludes
the watcher's own delays (AC5 is not one). The latch rule shared by AC2-ACn was tested in v2 only.
