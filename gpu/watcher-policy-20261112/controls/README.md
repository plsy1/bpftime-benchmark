# Attribution controls for AC's gain (2026-11-12 02:42-03:11 JST)

The main round (`../README.md`) found AC the cheapest sleeping set and the most robust under
full load, but C's adaptive window never opened in single-lane cells. Two controls separate the
parts of AC:

- **A0**: A with `BPFTIME_CUDA_WATCHER_HOT_US=0`: no busy poll after a request (A keeps 200 us).
- **AC0**: AC with `BPFTIME_CUDA_WATCHER_K_DEFER=0`: the adaptive window without the 8 deferral
  sleeps of 20 us.

## Setup

- Build: `scripts/watcher-policy-ctl.patch` replaces `../scripts/watcher-policy.patch` (same
  base). Changes: the two knobs above, and A's unseen latch now needs two hits in a row (a park
  ended by a noted launch resets the count) instead of two hits in the whole run. A and AC here
  run with that change.
- Sets P, M100, A, A0, AC, AC0; order shuffled per round and cell (seeded Python shuffle,
  `scripts/rounds2.sh`, `scripts/rounds3.sh`).
- Cells from the main round: c01 (full load), c02 (no load), c10 (low GPU duty, full load), c15
  (200 us kernels, full load), c17 (load 5 ms on / 5 ms off); the burst cells c05 / c06
  (32 calls per launch, full / no load) ran as a second block (03:03-03:11).
- 4 rounds, 168 runs, all valid (`verify.csv`); highest temperature 53.6 C.
- Summary: `SETS="P M100 A A0 AC AC0" ROUNDS_FILE=rounds-ctl-cells.txt summarize.py`.

## Results (`summary.md`; review in `review/`, per-round values in `review/recompute.md`)

| cell | measure | M100 | A | A0 | AC | AC0 |
|---|---|---:|---:|---:|---:|---:|
| c01 full load | calls > 1 ms | 3.55% | 3.48% | 0.84% | 1.00% | 0.90% |
| c01 full load | load vs P | 0.980 | 0.979 | 0.991 | 0.988 | 0.990 |
| c02 no load | CPU (cores) | 0.092 | 0.093 | 0.019 | 0.021 | 0.019 |
| c02 no load | host call median / p90 (us) | 63.8 / 107.3 | 70.0 / 109.3 | 68.1 / 108.8 | 69.3 / 108.9 | 67.8 / 108.7 |
| c15 short kernels, full load | calls > 1 ms | 6.91% | 6.91% | 0.21% | 0.46% | 0.22% |
| c15 short kernels, full load | load vs P | 0.946 | 0.948 | 0.989 | 0.979 | 0.982 |
| c17 duty-cycled load | calls > 1 ms | 25.69% | 26.79% | 28.05% | 30.74% | 28.86% |
| c05 burst, full load | cost per call (us) | 17.0 | 18.3 | 109.1 | 17.1 | 19.3 |
| c06 burst, no load | cost per call (us) | 10.7 | 11.7 | 104.6 | 12.4 | 14.9 |

CPU = victim CPU seconds per wall-clock second minus P's (1.0 = one full core). Cost per call in
burst cells = (per-launch mean - P's) / 32 (the probe logs one lane only).

1. **With one call per launch, AC's gain is entirely the dropped 200 us busy poll.** A0 equals
   AC in c01 / c02 and is better in c15; the split against A / M100 holds in every round.
2. **The busy poll's load cost is plain CPU displacement**: (A - A0) CPU spread over 6 load
   threads predicts the load loss (c01 0.012 predicted, 0.012 seen; c15 0.044 vs 0.041). Its
   effect on the > 1 ms tail (c01 3.5% -> 0.8%, c15 6.9% -> 0.2%) has no tested mechanism
   (hypothesis: after burning 200 us the watcher's wake-ups no longer preempt the load), and
   c10 points the other way (A 0.25-1.0% vs A0 1.25-2.25% in every round; 400 calls per run).
3. **Without any busy poll bursts collapse**: A0 serializes 32 calls behind 100 us sleeps (cost
   per call 105-110 us, launch 2.3x longer). C's window opens there (win20 for about 74,400 of
   76,832 calls) and keeps AC at M100's level under load (17.1 vs 17.0 us); without load AC is
   16% above M100 (12.4 vs 10.7 us, every round).
4. **The deferral sleeps help only in bursts** (AC vs AC0: 17.1 vs 19.3 and 12.4 vs 14.9 us per
   call, every round; about one 100 us sleep saved per launch) and **cost something with short
   kernels under load** (c15 AC vs AC0: > 1 ms 0.35-0.57% vs 0.18-0.27%, load 0.974-0.982 vs
   0.977-0.991, from about 31,000 extra wake-ups per run).
5. **Duty-cycled load (c17) stays bad for every sleeping set**; dropping the busy poll lowers CPU
   and raises load throughput but does not shorten the tail.
6. **Latch after the fix**: 0 latches in 112 A-family runs; 5 isolated hits (r3 c01 A, r3 c02
   AC0, r1 c05 A, r2 c05 A0, r3 c05 A). The c02 hit came with no load and 2-3 ms kernels, so the
   request arrived during a park while a noted kernel was running: the cause is not only "a
   short kernel posts before its note" as assumed. Graph latching (c18) was not rerun.

## Problems the review found in the design (not exercised by any cell yet)

- **C's window never shrinks**: it doubles (10 -> 300 us) while gaps are below 300 us and resets
  only on a gap of 300 us or more. A steady stream with gaps of 20-300 us would busy-poll
  nearly all the time, more than M100 for gaps of 200-300 us. The 1 ms episode cap is
  ineffective: it inserts one 20 us sleep and starts a new episode. Cells to expose it: one
  call per launch with gaps of about 170-300 us; 2, 4 or 8 calls per launch.
- **Small bursts rely on the deferral sleeps**: the first call of a launch always sees window 0
  and the second is caught by a deferral sleep; the window catches calls only from the third or
  fourth on.
- **The latch fix opens a hole**: a process alternating graph launches and noted launches resets
  the count at every noted launch, so it never latches and each graph call can wait up to the
  1 ms backstop. Conversely, spurious hits are reset only by a park ended by a note, so two
  hits hours apart can still latch a long-lived process for good (no way back).
- **Shuffle not counterbalanced**: 28 seeded orders, AC0 directly before AC 10 times; the burst
  block ran later as a separate block. Within-cell comparisons are fine.
- The `us_*` counters undercount (integer microseconds per ~100 ns iteration); use victim CPU.
