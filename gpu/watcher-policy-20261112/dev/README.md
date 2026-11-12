# AC3 on the dev branch (2026-11-12)

The window rule chosen in `../v3/` (AC3: park while no probed kernel may run, 100 us sleeps while
one may, a poll window doubling from 10 to 50 us and closed only by two gaps of 50 us or more in
a row, up to 8 deferral sleeps of 20 us with a late guard, the unseen-launch latch of v2) as the
dev branch's host-call watcher, replacing 46f5a7cb's WFE wait (now opt-in,
`BPFTIME_CUDA_WATCHER_WFE=1`). Commit 52126f33 on `dev/uprobe-official-4708f4092d1e` (see its message);
the rules are in `runtime/src/attach/cuda_watcher_policy.hpp`, documented in CLAUDE.md "CUDA
Attach: Host Calls And GPU Ringbuf".

## Tests and checks

- `[cuda-watcher-policy]` (3 cases, no GPU): window sizes and closing, deferral sleeps and the
  late guard, latching and unlatching. Passes on build-cuda and on build-probecheck (CUDA off,
  LTO).
- `[cuda-host-call]` (13 cases on the GPU), new or changed:
  - "The host-call watcher answers spaced calls within a short sleep while a probed kernel runs":
    calls 2 ms apart median about 65 us (threshold 150), 361 sleeps of 100 us in 40 ms and at
    most a park or two (it neither polls nor parks while a noted kernel runs);
  - "The host-call watcher parks while idle and a noted launch ends the park": parks while idle,
    first call after 5 ms idle about 50 us (threshold 250; a park ending on its own up to 1 ms);
  - "The host-call watcher's poll window catches calls back to back": 61 of 64 calls caught
    (threshold 48);
  - "Requests from unnoted launches keep the watcher from parking until they stop": latches,
    0 parks during a second unnoted kernel, calls about 63 us, unlatches within 3 s, parks again;
  - "With WFE the host-call watcher answers at once while a probed kernel runs"
    (`BPFTIME_CUDA_WATCHER_WFE=1`): about 15 us per call, leaves the flag about 90 us after a
    kernel ends (threshold 400), and a latched watcher does not wait on the flag.
- Mutations (`mutations/`, each must fail a test; script `scripts/mutate.py`): 13, all caught
  (`mutations/summary-final.txt`). M8 (WFE on by default) first survived: the default-policy tests
  had pinned `BPFTIME_CUDA_WATCHER_WFE=0` after the review; they now unset it. The first 8 were
  also run before the review fixes (`mutations/summary-first.txt`).
- Regression (`regression/`): every runtime test case as root in a process of its own on
  build-cuda (`each.sh`), on the committed code: 303 cases, 4 failures, all present before (the
  same 4 of 302 before the review fixes, `each-before-review-fixes.log`): three map-usage counter cases
  (fail in every build) and "Test cuda compile" ("VM name cannot be empty"; fails the same way on
  46f5a7cb's build, noted unrelated in `gpu/fixes-20261109/protocol`).
- Review (`review/review.json`): three lenses (equivalence with the measured AC3, tests, docs),
  each finding checked by a refuter. Confirmed and fixed: with `BPFTIME_CUDA_WATCHER_WFE=1` the
  kernels' end was queried every 1 ms instead of 100 us, and a latched watcher waited with WFE
  (a core held for the life of a graph application, idle or not); the latch test's latency bound
  could not tell a latched watcher from a parking one (phase-locked calls: about 200 us either
  way), and the loop's window and sleeps were not tested (now counters: parks, sleeps, window
  catches); the docs stated untested causes (preemption inside bursts, isolated hits only at the
  first launch: 2 of 31 v3 hits came under load 4-5 ms after a note) and mixed up which set some
  numbers belong to. Refuted: the default-policy tests depending on an exported
  `BPFTIME_CUDA_WATCHER_WFE` (pinned to 0 anyway).

## Measurement (`summary.md`, `verify.csv`; 2026-11-12 16:16-16:48 JST)

Sets: P (no bpftime), W (dev 46f5a7cb's build: WFE while a noted launch may run), D (dev with AC3,
the build before the review fixes: those touched only the WFE opt-in path and added three relaxed
counters), AC3 (the experimental build of v3, `V2=1 WIN_MODE=3`). Same build flags for all three
bpftime builds. 17 cells, 4 rounds, order shuffled per round and cell (seeded), 272 runs, all
valid; highest temperature 55.3 C. CPU = victim CPU seconds per wall-clock second minus P's (1.0 =
one core); cost per call = (per-launch mean - P's) / calls per launch.

| cell | measure | W | D | AC3 |
|---|---|---:|---:|---:|
| c01 full load | CPU (cores) | 0.822 | 0.023 | 0.023 |
| c01 | load vs P | 0.863 | 0.988 | 0.990 |
| c01 | host calls > 1 ms (per round) | 8.3-10.1% | 0.9-1.1% | 0.9-1.1% |
| c01 | host call median (us) | 14.6 | 70.7 | 70.6 |
| c02 no load | CPU (cores) | 1.004 | 0.021 | 0.022 |
| c02 | host call median / p90 (us) | 13.9 / 14.8 | 69.1 / 108.7 | 69.6 / 108.8 |
| c05 32 calls, full load | cost per call (us) | 22.1 | 15.8 | 15.8 |
| c05 | load vs P | 0.868 | 0.975 | 0.974 |
| c06 32 calls, no load | cost per call (us) | 9.2 | 11.6 | 11.8 |
| c11 idle | CPU (cores) | 0.002 | 0.002 | 0.002 |
| c15 short kernels, full load | load vs P / calls > 1 ms | 0.887 / 2.98% | 0.969 / 0.42% | 0.981 / 0.44% |
| c17 duty-cycled load | calls > 1 ms (per round) | 0.09-0.34% | 29.0-32.0% | 29.7-31.8% |
| c17 | load vs P | 0.886 | 0.969 | 0.974 |
| c18 graphs | host call median (us) / > 1 ms | 534 / 3.80% | 70 / 0.02% | 69 / 0.05% |
| c21 gaps 155-295 us | CPU (cores) / median (us) | 1.016 / 14.3 | 0.059 / 72.9 | 0.058 / 72.0 |
| c23 gaps 55-115 us | CPU (cores) / median (us) | 1.014 / 13.9 | 0.090 / 25.4 | 0.052 / 26.1 |
| c24 same, full load | calls > 1 ms / load vs P | 1.30% / 0.911 | 0.28% / 0.985 | 0.29% / 0.979 |
| c25 2 calls per launch | cost per call (us) / CPU | 18.0 / 1.000 | 45.3 / 0.030 | 45.4 / 0.031 |
| c28 4 calls, full load | cost per call (us) / load vs P | 113.1 / 0.873 | 37.3 / 0.988 | 39.0 / 0.989 |
| c29 graph / plain alternating | median / p90 (us), > 1 ms | 15.5 / 876, 3.64% | 69.5 / 108, 0.00% | 70.3 / 109, 0.00% |
| c31 1 us kernels, no load | per-launch mean (us) / CPU per launch (us) | 50.4 / 81-90 | 63-72 / 41-44 | 56-72 / 38-44 |
| c32 same, full load | calls > 1 ms | 1.20% | 0.14% | 0.17% |

1. **D behaves as the measured AC3** in every cell within the round-to-round spread (c23 CPU
   0.090 vs 0.052 lies within v3's noise floor of about 0.05 core between identical behaviour;
   c31's per-launch ranges overlap, 63-72 vs 56-72 us, with equal CPU per launch).
2. **Against 46f5a7cb (W)**: W answers in about 14 us wherever a core is free, but holds a whole
   core while any noted probed kernel runs (c02, c06, c21, c23, c25: +1.0 core; D 0.02-0.13), and
   under full load costs the load 11-14% of its throughput (c01 0.863, c05 0.868, c28 0.873; D
   0.975-0.988) with 8-10% of host calls over 1 ms in c01 (D 1%). D is better for bursts under
   load (c05 15.8 vs 22.1, c28 37.3 vs 113.1 us per call) and for unnoted launches (c18 graphs
   70 vs 534 us; c29 no call over 1 ms vs 3.6%). D gives up latency with a free core (c02
   median 69 vs 14 us, c06 11.6 vs 9.2, c25 45.3 vs 18.0 us per call, c31 dense requests 63-72 vs
   50 us per launch).
3. **Duty-cycled load (c17)** stays the sleeping policy's weak case: 29-32% of calls over 1 ms
   (W 0.1-0.3%), as in every earlier round; not addressed.
4. Idle (c11): equal (0.002 core): W parks too once no noted launch runs.

## Probe costs outside host calls: the upstream GPU benchmark subset (`bench/`)

The 24-case subset (memory note: light probes Empty tiny/xlarge and Global timer xlarge, the
non-light set, the 4 examples), the ready-waiting runner, W (46f5a7cb's build, view `wake`) and D
(52126f33's build, view `candidate`) alternating, 10 rounds (17:20-17:29 JST), 20 batches, all
case sections valid (`bench/summary.md`, `bench/rounds.log`, raw `bench/runs/`). 46f5a7cb's own
commit had found some light and CPU-array cases faster than before it (Empty probe tiny 34.2 ->
28.0, CPU Array map update 51.0 -> 41.1 us), possibly from the WFE watcher keeping a core awake.
With AC3 they stay where 46f5a7cb put them: Empty probe tiny 29.0 (W) vs 29.6 (D), CPU Array
update 43.6 vs 42.5, lookup 46.6 vs 47.2 us. 22 of 23 cases differ by 0.98-1.05x with p >= 0.023
(Baseline large 0.99x, p 0.023, has no probe); CUDA Counter 2,915.7 -> 2,967.5 us (1.02x, p
0.014; one of 23 comparisons, not attributed).
