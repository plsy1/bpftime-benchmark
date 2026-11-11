# Engineering-derived watcher policies A, C and B (2026-11-12)

Follows `gpu/watcher-sleep-20261111`: a sleeping watcher capped at 100 us (M100) answers a host
call in about 68 us and costs a fully loaded CPU only 1-2%, but spends about 0.09 CPU s per s
(about 9% of a core) while probed kernels run and still wakes when nothing runs; WFE (W) answers
in 14 us but holds a core. A survey of comparable engineering mechanisms (`survey/`: KVM
halt-polling, NAPI deferral, blk-mq hybrid polling, DPDK power management, spin-then-block
locks) gave candidates A-E (`survey/survey-design.txt`); this round implements A, C and B and
measures them against P, W and M100.

## Policies (`scripts/watcher-policy.patch`, env `BPFTIME_CUDA_WATCHER_POLICY=<letters>`)

- **A, park when nothing runs**: when no probed kernel is in flight (launch note generation
  unchanged and `cudaEventQuery` on the last probed launch done, checked every 1 ms), wait on the
  launch note with a 1 ms backstop instead of sleeping in a loop. While a probed kernel runs:
  sleep 100 us (timer slack 1 us); after a request, busy-poll 200 us (M100's hot window). A
  request found after a full backstop with no new note twice in a run (launches the hooks do not
  see, e.g. CUDA graphs) latches A off for the process.
- **C, adaptive hot window plus deferral** (KVM halt-poll window, NAPI deferral): the busy-poll
  window after a request doubles (from 10 us, at most 300 us) when the gap since the previous
  request is longer than the window but shorter than 300 us, stays for a gap within the window
  and drops to 0 for a gap of 300 us or more; a run of hot polling is capped at 1 ms;
  then 8 sleeps of 20 us before falling back to A's tier; skipped while the measured lateness of
  20 us sleeps exceeds 30 us (EWMA).
- **B, gated WFE** (DPDK monitor with a load gate): wait with WFE instead of sleeping while
  `/proc/loadavg`'s running count (read every 1 ms) shows at least one spare core for three
  checks in a row; a jump of the watcher's run_delay (`/proc/thread-self/schedstat`) by more
  than 200 us between checks, or a WFE return later than 500 us, closes the gate until the next
  noted launch.
- Signal costs (`calib/calib2.log`): loadavg read 1.06 us, schedstat read 0.70 us, cudaEventQuery
  0.3-1.8 us.
- Stats per process (`<run>.stats`, env `BPFTIME_CUDA_WATCHER_STATS`): requests, hot iterations,
  deferral / slow sleeps, WFE waits, parks and backstop timeouts, gate checks / opens, latch
  trips, window histogram, time per tier.

## Setup

- **Build** (not a dev commit): dev 46f5a7cb (`scripts/src.BASE`) plus
  `gpu/watcher-sleep-20261111/scripts/watcher-sleep.patch` plus `scripts/watcher-policy.patch`,
  built with `scripts/build.sh`. W is dev 46f5a7cb's own build; M100 is this build with
  `BPFTIME_CUDA_WATCHER_MAX_SLEEP_US=100` and no policy (the old loop).
- **Victim** `scripts/victim/spin3.cu`: `LAUNCHES SPIN_US JITTER_US THREADS [GAP_US [GRAPH]]`;
  1 x T kernel spinning SPIN_US plus a fixed pseudo-random 0-JITTER_US, an optional sleep after
  each synchronize (low GPU duty, idle agent), optional CUDA graph launches (one kernel node, its
  parameters set per launch; not noted by the launch hooks).
- **Probes** from `gpu/wfe-load-20261111` (kretprobe on spinKernel; `hostspin` one host call per
  lane at exit, `nohost` none). **Load** `scripts/load2.c`: 3 or 6 busy threads, or 6 threads 5 ms
  on / 5 ms off (duty6).
- **Cells** (18, `scripts/rounds.sh`): full / no / partial load, 2 and 20 ms kernels, no host
  call, 32-lane bursts, low GPU duty (18 ms sleep after each launch), idle agent (one launch then
  8 s), two victim processes, short 200 us kernels, duty-cycled load, CUDA graphs.
- 4 rounds, 7 sets each, set order rotating per round (00:32-02:03 JST). All 504 runs valid
  (`verify.csv`: rcs 0, record counts, clocks unchanged before and after; c18 has one record
  more, the warm-up through the graph); highest temperature 54.4 C.
- **Power**: tegrastats every 100 ms in load-off cells, whole seconds inside the victim window;
  jetson_clocks active (all cores at 1,728 MHz, cpu idle state c7 off, WFI still used). c14's
  windows are about 1.3-1.6 s: P has no value there and the others rest on one second.

## Results

Numbers are medians over 4 rounds; "CPU" = victim CPU s per wall s minus P's of the same round;
host-call latency GPU-timed, pooled over runs (p99 / max can come from one run; the > 1 ms share is
the stable tail statistic). Burst cells log one record per launch, so their "cost per call" is
derived: (per-launch mean - P's) / 32. Corrections from the review (`review/`) are folded in.

**One lane, 2 ms kernels** (c02 no load, c07 3 load threads, c01 6 load threads):

| set | CPU, no load | host call median / p90 (us), no load | CPU, on6 | load vs P, on6 | calls > 1 ms, on6 | host call median (us), on3 |
|---|---:|---:|---:|---:|---:|---:|
| W | 1.002 | 13.9 / 14.8 | 0.800 | 0.865 | 11.29% | 13.9 |
| M100 | 0.092 | 63.4 / 106.3 | 0.088 | 0.980 | 3.50% | 64.3 |
| A | 0.094 | 70.0 / 109.4 | 0.089 | 0.980 | 3.69% | 69.5 |
| AC | 0.021 | 69.2 / 108.4 | 0.024 | 0.989 | 1.04% | 67.4 |
| AB | 1.000 | 14.8 / 15.5 | 0.091 | 0.980 | 4.14% | 14.8 |
| ABC | 0.933 | 14.7 / 14.9 | 0.029 | 0.988 | 1.26% | 14.7 |

**Other cells** (full tables in `summary.md`):

| cell | W | M100 | AC | AB | ABC |
|---|---|---|---|---|---|
| c15 short 200 us kernels, on6: load vs P / > 1 ms | 0.892 / 3.93% | 0.942 / 6.61% | 0.980 / 0.45% | 0.946 / 6.97% | 0.973 / 0.47% |
| c14 short kernels, no load: CPU / median (us) | 1.014 / 13.8 | 0.514 / 67.6 | 0.060 / 66.8 | 1.025 / 14.8 | 0.513 / 14.9 |
| c11 idle agent: CPU | 0.002 | 0.011 | 0.002 | 0.002 | 0.002 |
| c09 low GPU duty, no load: CPU / median (us) | 0.156 / 15.4 | 0.021 / 70.0 | 0.006 / 67.5 | 0.170 / 16.1 | 0.160 / 16.2 |
| c10 low GPU duty, on6: > 1 ms (of 1,600 calls) | 14.19% | 0.50% | 1.69% | 1.50% | 2.25% |
| c17 load 5 ms on / 5 ms off: load vs P / > 1 ms | 0.879 / 0.15% | 0.944 / 25.14% | 0.970 / 30.66% | 0.907 / 11.70% | 0.927 / 13.01% |
| c12 two processes, on6: load vs P / > 1 ms | 0.776 / 37.23% | 0.970 / 5.02% | 0.978 / 4.62% | 0.967 / 5.38% | 0.975 / 4.94% |
| c16 two processes, on3: CPU / > 1 ms | 1.988 / 0.92% | 0.113 / 3.84% | 0.065 / 3.86% | 1.700 / 1.52% | 1.692 / 1.50% |
| c18 CUDA graphs, no load: median (us) | 533.3 | 63.9 | 69.7 | 14.9 | 14.8 |
| c05 burst 32 lanes, on6: load vs P / cost per call (us) | 0.870 / 21.7 | 0.970 / 17.2 | 0.974 / 17.1 | 0.967 / 18.5 | 0.981 / 21.6 |

**Power** (VDD_CPU_GPU_CV, c02 no load, mW): P 1,699, W 1,713, AB 1,706, ABC 1,727, AC 1,747,
A 1,760, M100 1,766. Per-round spread 15-40 mW; the WFE sets stay below the sleeping sets in
all rounds but one (ABC r3 1,745 vs AC r2 1,742). In c11 (idle agent) only M100 (+40-50 mW) stands out of the noise. Only cpu idle
state c7 was off (WFI still used, the review checked the usage counters); no run without
jetson_clocks, so how this changes with c7 on is not measured.

### What holds

1. **AC is the cheapest sleeping set and the most robust under full load**: lowest CPU of
   M100 / A / AC in every cell with host calls (AC 0.006-0.065, M100 0.021-0.514; 2 ms kernels
   0.02 vs 0.09); the exception is c04 without host calls (AC 0.021 vs M100 0.018); c01 > 1 ms 1.04% vs 3.50% (every round), c15 0.45% vs 6.61% (p99 120 vs 3,937 us).
   Load vs P in c01 is ahead in 3 of 4 rounds (round 1 a tie).
2. **But C's adaptive window is almost never used** in these cells: with one host call per
   2.5 ms every request resets the window to 0 (win0 = 100% of requests in single-lane cells,
   82% / 99% in c14 / c15). AC's gain there comes from dropping the 200 us busy poll after each
   request and adding 8 deferral sleeps of 20 us; the window only grows in the 32-lane bursts
   (win20). A control (A without the busy poll, AC without deferral) is missing, so the gain
   cannot be credited to the grow/shrink logic. A plausible reason the missing busy poll helps
   under load: the busy poll uses up the watcher's scheduler credit, so its later wake-ups do
   not preempt the load (not tested).
3. **AC's costs**: median latency about 5-6 us above M100 in c02 / c07 / c18 in every round
   (equal in c09 / c14); bursts 12.4 vs 11.2 us per call (c06); low GPU duty under load c10
   1.69% vs 0.50% > 1 ms (27 vs 8 calls of 1,600; A alone 0.19%); duty-cycled load c17 30.7%
   vs 25.1%.
4. **A's park works only when nothing runs**: idle agent CPU 0.002 vs M100's 0.011 (c11), and
   lower in the gap cells c09 / c10; with kernels in flight A equals M100. The ~296 parks per run
   in every no-gap cell are agent start-up (about 300 ms of backstops before the first launch).
5. **B's gate does what it should in steady states**: open with spare cores (c02, c07, c09,
   c14: W's latency, 14.7-16.2 us median, at W's CPU while kernels run), closed under 6 load
   threads (loadavg; gate_opens 0 in c01 / c03 / c05 / c10 / c15). With the gate open ABC spends
   a little less than AB (C's deferral sleeps), in c14 half (0.51 vs 1.03).
6. **B's limits**: with the gate closed AB is slightly worse than A (c10 > 1 ms 1.25-1.75% vs
   0-0.5%, c01 4.0-4.4% vs 3.6-3.8%, every round; likely the two /proc reads per ms). Under a
   changing load the gate flaps (c16: about 1,000-1,250 opens and 80-90 latches per run; c17:
   1,000+ opens) and lands between W and M100. The loadavg thresholds have no hysteresis band;
   only the 3-check streak damps it.
7. **Graphs**: launches the hooks do not see get served: the A family latches after two
   backstop hits and then serves like M100 (c18); W, which waits with WFE only on noted
   launches, falls back to N's 1 ms sleep (533 us median, victim per-launch +546 us).

### Open problems found

- **Bug in A's "unseen" latch** (review): 13 of 256 non-graph A-family runs had one false hit
  (launches the hooks do note). The note comes after the launch returns, and the park wakes only
  on notes, so a short probed kernel (the 1 us warm-up) can post its call before its note; a
  backstop expiring in that gap counts. The counter is never reset and the latch is permanent,
  so a long-lived process would eventually lose parking (idle CPU 0.002 -> about 0.011).
  Fix before any dev commit: count only consecutive hits, or check flag1 against the note order.
- **ABC is not AC plus WFE**: with B on and the gate closed, C's hot window sleeps 10 us instead
  of busy-polling (c05: ABC CPU 0.045 vs AC 0.114, cost per call 21.6 vs 17.1 us).
- **Duty-cycled load (c17) is the worst case for every sleeping set** (25-31% of calls > 1 ms,
  max about 5.0-5.1 ms = the load's on-phase), and P's own victim is slower there than W's
  (3,645 vs 3,105 us per launch), so P is not a clean baseline in c17. Hypothesis: a thread
  waking from a short sleep does not preempt and waits for the slice end (HZ 250, EEVDF).
  Cheap tests: a GPU-free 100 us sleeper under duty vs constant load, vary the on-phase,
  `perf sched timehist`.
- **Two processes (c12 / c13 / c16)**: every call > 1 ms sits in one 2.25-2.75 ms band for every
  set, also without load, and its share scales with the mean wait (W 0.93%, sleeping sets
  3.5-3.9%). Likely a GPU time-slice switch to the other context during the wait. Test: two
  processes with different spins, or MAX_SLEEP_US 50 / 100 / 200.
- **Harness**: the set rotation used 4 of 7 offsets (P never runs first and always follows ABC),
  so deltas under about 1% are not trustworthy; W runs at the default 50 us timer slack, the
  others at 1 us; the `us_*` counters undercount (integer microseconds per ~100 ns iteration).

## Files

- `summary.md`: every cell, set and counter; `verify.csv`: per-run validity.
- `scripts/`: patch, build, harness, victim, load, summarizer, `CLAIMS.md` (the claims the review
  checked).
- `calib/`: signal-cost calibration. `survey/`: the survey workflow outputs and the design.
- `review/`: independent recomputation and critique.
- `logs.tar.xz`: raw runs (`r1`-`r4`), smoke runs, build logs.
