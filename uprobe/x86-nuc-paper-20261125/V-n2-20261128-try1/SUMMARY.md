# VIEW leg (P5a post-go), PREREG-2c — V-n1 + V-n2 (analysis 2026-11-28)

## Data

**Sessions.**
- V-n1-20261127-try1: round 0, 2026-11-27 15:3x.
- V-n2-20261128-try1: round 1, 2026-11-28 01:25–01:43, after C2 night 2.

**Rows.**
- 5 legs × 10 measured reps per session, 1 warm-up each, excluded.
- 100 measured rows, all valid; noisy_core 0; check-after PASS on both.

**Harness.**
- harness/run_paper_view.py, a copy of run_paper.py with the VIEWD switch and `--server-stderr`; session-view.sh.
- viewd binary sha a6fb6fa2 (the M4 binary), run per leg.
- C2 kept run_paper.py and session.sh unchanged.

**Analysis.**
- harness/analyze_c2.py view (v2); output analysis-view.txt.
- An independent re-implementation and an audit agree with every number.

## Pre-registered tests (±0.084 µs/event, ±1.5 % rps; 90 % CIs; n = 20)

**V1, cost of the view** (Dviewpc+CAREFUL+VIEW+VIEWD − Dviewpc+CAREFUL; same binary; hooks only for that leg):
**Δx +0.011 µs/event, 90 % CI [−0.018, +0.040] → equivalent.**
- GMR 0.9982, 90 % [0.9936, 1.0028]; p = 0.52.
- Per session: +0.027 [−0.002, +0.057] and −0.005 [−0.074, +0.064] (95 %).
- Without the first formal rep: +0.006, 90 % [−0.025, +0.037], equivalent.
- As predicted (≈ +0.01, M4 +0.011).

**V2, read side inert** (Dviewpc+CAREFUL vs Dnextpc+CAREFUL; different binaries): **not equivalent → reported as a
layout difference**, not as a cost of the view.
- rps GMR 1.0095, 90 % [1.0042, 1.0147]: within ±1.5 %.
- Δx −0.058, 90 % [−0.090, −0.026]: lower bound past −0.084.
- So Dviewpc is about 0.95 % faster than Dnextpc. This is the order of B5's layout effect (0.87 %); the prediction was
  "< 1 %".
- The decision is unstable. Without the first formal rep (n = 18) the Δx 90 % CI is [−0.0839, −0.0165], 0.00012 µs
  inside the margin, so equivalent; with PREREG-2a's unrounded 0.0837 it is still not equivalent.
- The two criteria disagree because 0.084 µs was folded at 89 614 rps; at about 81k rps it equals about 1.36 %.

**V3 (descriptive):**
- VIEW / Kb **1.2437** [1.2377, 1.2498];
- Dnextpc+CAREFUL / Kb 1.2343 [1.2278, 1.2409];
- Dviewpc+CAREFUL / Kb 1.2460 (from the re-implementation).

## Checks (PREREG-2c §4)

- **Miss criterion (≤ 0.1 % of events):** 0 misses over 32 667 537 events (V-n1 0 / 16.35 M, V-n2 0 / 16.32 M).
  Every VIEW row has 2 exit reports (master and worker), all 0; checked against server-stderr.txt.
- **viewd:**
  - every VIEW leg: start rc 0, "ready mode=fexit nslots=262144", stop rc 0;
  - no pin before or after;
  - hook counters grow in every row (minimum deltas: newtask 40, creds 29, rename 36, exec 26, exit 30);
  - lock_fail, collision and dead_write all 0.
- **Carry-over:** no leg with p < 0.01.

## Reading

- With its hooks attached only while it runs, the kernel view costs nothing measurable on nginx: V1 is equivalent at
  ±0.084 µs/event, two sessions, the same as M4 with the hooks on all session. It produced no misses.
- With the view, the checked careful Tern build is 24.4 % ahead of Kb (V3), while giving the kernel's values in every
  E5 scenario except raw clone children (I8v).
- V2's layout difference (Dviewpc about 1 % faster than Dnextpc) is a binary-layout effect, not a speed-up from the
  view.
