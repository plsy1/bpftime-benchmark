# v2 of the A/C policies: window cap, latch rule, unlatch (2026-11-12 13:28-14:05 JST)

The controls (`../controls/README.md`) left open problems in AC's design: C's window never shrinks
(it grows to 300 us while request gaps stay below 300 us), the 1 ms episode cap does not stop it,
and A's unseen latch could miss processes alternating graph and noted launches and could not come
off again. v2 changes both and adds the cells that should expose them.

## What changed (`scripts/watcher-policy-v2.patch`, against the controls' source)

`BPFTIME_CUDA_WATCHER_V2=1` (set AC2; set AC is the same v2 build with it unset, so the AC of
these rounds runs the controls' code plus the diagnostics, from a rebuilt library; compare AC with
AC2 within these rounds):

- C's window: on a gap longer than the window, the window becomes max(2 x gap, 10 us), at most
  50 us (`BPFTIME_CUDA_WATCHER_WIN_MAX`); any gap of 50 us or more closes it (AC: doubling from 10
  to 300 us, closed at 300 us). The 1 ms episode cap is off. The gap is the watcher's: from the end
  of the previous service to the next detection, so it includes the watcher's own sleep delay.
- A's unseen latch: two hits (a request found when a 1 ms park ends with no launch note) within
  1 s latch it; notes do not reset the count (AC: two hits with no park ended by a note between).
  Latched, the watcher still tracks noted launches, and the latch comes off after 1 s in which no
  request was served while no noted kernel was in flight.
- Diagnostics for every set: for the first 4 hits, how long after the watcher last saw a note the
  park began (`h*_note`, 0 = no note seen yet), how long after the event query that found nothing
  running (`h*_idle`), and whether `any_running()` was true at the hit (`h*_any`).

Victim (`scripts/victim/spin3.cu`, `spin3-graph-modes.patch`): GRAPH 2 alternates graph and plain
launches, GRAPH 3 runs graphs for the first half, then plain launches. Built as before (`-O3
-D__NV_LEGACY_LAUNCH`, shared cudart). A first rebuild with nvcc's defaults launched through CUDA 13
kernel handles, which the launch hooks do not see: every A-family run latched in the smoke test
(`logs.tar.xz`, v2smoke). To the watcher a default nvcc 13 build looks like a graph application.

## Setup

- Sets P, M100, AC, AC2, order shuffled per round and cell (seeded); 19 cells, 4 rounds, 304 runs,
  all valid by the summarizer's checks (`verify.csv`); highest temperature 54.1 C in the
  before/after snapshots, 54.7 C in tegrastats during runs.
- New cells: c21 / c22 request gaps about 155-295 us (spin 130 + 0-140 us), no load / full load;
  c23 / c24 gaps about 55-115 us (spin 30 + 0-60 us); c25 / c26 / c27 2 / 4 / 8 calls per launch,
  no load; c28 4 calls, full load; c29 graph and plain launches alternating; c30 graphs, then
  plain. Repeated: c01, c02, c05, c06, c10, c11, c15, c17, c18.
- Summary: `SETS="P M100 AC AC2" ROUNDS_FILE=rounds-v2-cells.txt summarize.py logs/v2`. It drops
  only the first latency value, so graph cells (c18, c29, c30), which have two warm-up calls, keep
  one warm-up call in their latency sets; it pools calls over 1 ms over the rounds, so one
  disturbed run sets a pooled share and maximum.
- Review: an independent recomputation and a critic (`review/`); their corrections are in the text.
- Disturbed: round 2 had outside CPU load from 13:40:47 to 13:42:15 (tegrastats: up to all six
  cores busy and RAM +450 MB at 13:42:12-15.5, during c02 AC2; background load during c18 P and
  M100; c01 P's load rate 15% below the other rounds, so c01's "load vs P" range in round 2 is
  1.14-1.17 for every set). Medians are not moved; pooled tails of c02 AC2 are (below). Load cells
  have no tegrastats, so a disturbance there shows only through P.

## Results (`summary.md`)

CPU = victim CPU seconds per wall-clock second minus P's of the same round (1.0 = one full core;
under load about +-0.03 of noise). Cost per call in multi-call cells = (per-launch mean - P's) /
calls per launch.

| cell | measure | M100 | AC | AC2 |
|---|---|---:|---:|---:|
| c23 gaps 55-115 us, no load | CPU (cores) | 1.032 | 1.026 | 0.072 |
| c23 | host call median (us) | 14.8 | 14.7 | 25.9 |
| c24 gaps 55-115 us, full load | calls > 1 ms | 2.63% | 2.72% | 0.27% |
| c24 | load vs P | 0.949 | 0.962 | 0.977 |
| c24 | host call median (us) | 14.5 | 14.9 | 26.3 |
| c21 gaps 155-295 us, no load | CPU (cores) | 0.695 | 0.091 | 0.048 |
| c25 / c26 / c27 2 / 4 / 8 calls | cost per call (us) | 42.9 / 25.7 / 17.2 | 52.7 / 31.3 / 20.5 | 51.4 / 30.6 / 20.1 |
| c25 / c26 / c27 | CPU (cores) | 0.095 / 0.101 / 0.112 | 0.025 / 0.032 / 0.046 | 0.039 / 0.046 / 0.059 |
| c28 4 calls, full load | cost per call (us) | 50.8 | 46.7 | 48.4 |
| c05 32 calls, full load | cost per call (us) | 16.6 | 16.9 | 22.1 |
| c06 32 calls, no load | cost per call (us) | 11.1 | 12.3 | 12.3 |

1. **The 50 us limit removes the continuous busy poll.** With request gaps of 55-115 us AC polled
   all the time (window 300 us for 3996 of 4001 requests, the cap firing 432-451 times without
   effect) and cost a full core, as M100; AC2 never opened its window there (0.06-0.08 cores, every
   round). Under full load (c24) that cut calls over 1 ms tenfold and raised the load's throughput
   (AC2 higher than AC in 4 of 4 rounds). The price is latency: host-call median +11 us (14.7 ->
   25.9) and per-launch median +15 us under load, every round; a request now waits for a 20 us
   deferral sleep instead of a poll. The predicted cliff at 170-300 us gaps did not happen: in c21
   AC's window was closed for about half the requests (win0 1996-2101 of 4001); probably the gap
   the watcher measures includes its own sleep delay and often reaches 300 us (not tested). AC2
   halves CPU there (0.091 -> 0.048) for a host-call median 1-4 us higher in every round.
2. **Sizing the window from the gap does not help small bursts.** AC2's window sits at 21-50 us
   where AC's stays at 20; cost per call is the same within 1.8 us (c28 under load 1.8 us worse, in
   every round), CPU is 0.012-0.014 cores higher, and the per-launch median and p90 are about 35 us
   higher in every round (the means agree). Latency inside a burst is not timed (the probe records
   one lane's call per launch only in single-lane cells).
3. **Under full load AC2 is worse in 32-call bursts (c05)**: per-launch mean 3,282-3,296 vs
   3,115-3,135 us (cost per call 22.1 vs 16.9 us) and p90 +800 us in every round, though the
   per-launch median is 55 us lower. AC2 closed its window about 4,600 times more often (win0
   7,050 vs 2,416) and skipped its deferral sleeps about 4,100 times more (guard_skips 4,754 vs
   616): under preemption the measured gap passes 50 us and closes the window, where AC closes
   only at 300 us. So the 50 us close threshold, not the sizing, is the likely cause (inferred from
   the counters, not tested). c06 (no load): equal mean.
4. **Latch**: every AC and AC2 run of c18 (graphs), c29 (alternating) and c30 (graphs, then
   plain) latched once after two hits; AC2 came off the latch once in every c30 run. AC latched
   in c29 too: with launches back to back the plain launch's note arrives while the watcher is in
   its deferral sleeps, not in a park, so it never resets AC's count. The hole remains for
   alternation with idle time before plain launches (not tested). Not exercised: AC2's 1 s hit
   window (the hits were 2-5 ms apart) and any benefit of unlatching (in c30 launches stay back to
   back, CPU is equal; no timestamp of the unlatch is recorded). A graph application that goes idle
   for more than 1 s unlatches and pays one more 1 ms backstop wait to latch again (not tested).
5. **Isolated hits come from the first probed launch.** No non-graph run latched; 4 runs had one
   hit each (r2 c05 AC2, r2 c10 AC2, r4 c11 AC2, r2 c17 AC), all with `h0_note=0`: the watcher had
   not seen any launch note yet. In 3 of the 12 AC graph runs one of the two latching hits was this
   start-up hit. Likely mechanism: the first launch's note comes after the first-time lookup in
   `note_launch_if_probed`, after its kernel posted. One run caught it: r4 c18 AC's start-up hit
   has `h0_any=1` with no note seen; with no event recorded yet `any_running()` is true only when
   its try-lock fails, that is while `note()` held the event mutex: the first note was being made
   at the moment of the hit.
6. **c02 AC2's tail is the disturbance**: 14 of its 16 calls over 1 ms (pooled 0.12%, max
   5,299 us) are in round 2, 12 of them inside the outside burst; without round 2 AC2 has 2 in
   9,600 (0.02%), as AC.
7. c01, c10, c15, c17: AC2 equals AC within noise. c11 (one call per run): CPU and park counts
   equal; its latency (n = 1 per run) cannot be compared.

## What it means for the design

The problem v2 was for is fixed by the 50 us limit (c23/c24), but the 50 us close threshold
costs bursts under full load (c05), and the gap-based sizing adds CPU without benefit. Not tested
yet: AC's doubling with a 50 us maximum, a close threshold that tolerates single long gaps (two
gaps in a row, or a higher threshold than the maximum), and the uncapped case v2 created (a steady
stream of gaps below 50 us, which now polls without any limit; no cell has it).
