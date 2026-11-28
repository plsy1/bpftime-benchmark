# PREREG-2c — P5a VIEW leg under C2 conditions (x86; written 2026-11-27 before any V-n1/V-n2 measurement)

Written after the user's P5a GO (2026-11-27) and after one functional smoke (V-smoke-20261127-try1, 1 rep, 4 legs, not
analysed for cost). PREREG-2, -2a, -2b and every C2 rule are unchanged. This is a separate family; it does not enter
C2's Holm correction.

## 1. Why
- PREREG-2 left the VIEW leg out of C2, because its hooks are machine-wide and would change every leg.
- P5a's spike (raw/P5a-20261126) met its criteria: M4 measured CAREFUL+VIEW − CAREFUL at +0.011 µs/event, 90 % CI
  [−0.015, +0.036], in one exploratory session with the hooks on for all legs.
- This addendum measures the VIEW leg the way it would be deployed: hooks only while that leg runs, C2's
  configuration, two sessions on two days.

## 2. Setup
- Harness: harness/run_paper_view.py, a copy of run_paper.py (sha 3616a97d) with the leg switch VIEWD (tern_viewd
  start before the leg's loader, stop after the leg; binary = the M4 binary, sha a6fb6fa2) and `--server-stderr`
  (nginx stderr kept per leg for the agent's exit report). Session script: harness/session-view.sh (session.sh with
  the runner path changed). C2 itself keeps run_paper.py and session.sh unchanged.
- Builds:
  - Dnextpc = Tern careful build, 6b6b247e, frozen for C2.
  - Dviewpc = Dnextpc + the view read side, 3499d664.
  - Checks on (pc), as C2's Dnextpc legs.
- Policy M, 16 B, C2's load (closed loop, 100 connections, 10 s).
- Legs: B, Kb (K + SSLSNIFF_PB_WAKEUP=32), Dnextpc+CAREFUL, Dviewpc+CAREFUL, Dviewpc+CAREFUL+VIEW+VIEWD.
- V-n1: round 0, 1 warm-up + 10 reps, on 2026-11-27.
- V-n2: round 1, 1 warm-up + 10 reps, after C2 night 2 has finished on 2026-11-28.
- n = 20 per leg; warm-ups excluded.

## 3. Tests (x86 margins of PREREG-2: ±0.084 µs/event, ±1.5 % rps; TOST with 90 % CIs of paired per-rep differences)
- **V1 (primary): the cost of the view.** Δx = x(Dviewpc+CAREFUL+VIEW+VIEWD) − x(Dviewpc+CAREFUL): same binary, so
  this is the hooks plus the read path. Equivalent if the 90 % CI lies within ±0.084 µs/event.
- **V2: inertness of the read side.** Dviewpc+CAREFUL vs Dnextpc+CAREFUL (different binaries; B5 showed layout effects
  up to 0.87 %). Equivalent if within ±1.5 % rps and ±0.084 µs/event; otherwise reported as a layout difference, not as
  a cost of the view.
- **V3 (descriptive, no test): the headline.** rps ratio Dviewpc+CAREFUL+VIEW+VIEWD / Kb with a 95 % CI, next to
  Dnextpc+CAREFUL / Kb.
- **Session effect (descriptive).** V1's Δx in V-n1 and in V-n2 separately.

## 4. Validity and correctness checks
- A row is invalid if viewd is not ready before the leg, cannot be stopped, or leaves its pin (harness reason
  'viewd'), or for any of run_paper.py's usual reasons.
- The VIEW leg must log viewd hook-counter growth.
- Miss criterion (P5a G3): the agent's exit reports in server-stderr.txt must show view misses ≤ 0.1 % of the leg's
  events, summed over the nginx processes. Reads are not counted (BPFTIME_VIEW_COUNT is off: counting would add an
  atomic per read).
- noisy_core rule as in PREREG-2.

## 5. Prediction
- V1 equivalent; Δx ≈ +0.01 µs/event, as in M4.
- V2 equivalent or a small layout difference below 1 %.
- Misses 0 (V-smoke: 0 in both nginx processes).
