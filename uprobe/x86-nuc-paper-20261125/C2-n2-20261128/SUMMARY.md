# C2 (x86) — confirmatory session pair, PREREG-2 / 2a / 2b (analysis 2026-11-28)

## Data

**Sessions.**
- Night 1: C2-n1-20261126, round 0. It ran 2026-11-27 00:46–01:45 JST; its id carries the date the runner started.
- Night 2: C2-n2-20261128, round 1, 2026-11-28 00:15–01:25, plus round 2: perf sc, 3 × 20 s on 9 legs.

**Rows.**
- 22 legs × 10 measured reps per night; 1 warm-up rep per session, excluded.
- 440 measured rows, all valid; noisy_core 0 (largest non-measured-core busy 0.0435); check-before/after PASS on both
  nights.
- Builds frozen as of 11-26: Dnext 6b6b247e; Dn730f per PREREG-2b (gate E4f passed).
- Careful mode active on its leg: checked separately (raw/C2-carefulchk-20261127).

**Analysis.**
- harness/analyze_c2.py (v2 after the audit; v1 kept as analyze_c2.py.v1). Output: analysis-c2.txt.
- Two independent re-implementations from the PREREG text (blind to the script and its output) and a line-by-line
  audit agree with every number. Their scripts are in the session scratchpad.
- Audit changes from v1:
  - M5's Holm p now uses the log ratio, like the other five (9.51e-12; v1 used Δx's p, 9.24e-12; ranking unchanged);
  - M2's label states the direction;
  - B's carry-over test added;
  - TOST count, the §8 rule and the full sensitivity output added.

## Confirmatory family

Paired over (round, rep), n = 20. GMR with 95 % t-CI; Δx in µs/event. Superiority: paired t on the log ratio, Holm
over 6. TOST: 90 % CI against ±1.5 % and ±0.084 µs/event (M5: µs only).

| | contrast | GMR [95 % CI] | Δx µs/event [95 % CI] | p Holm | result |
|---|---|---|---|---|---|
| M1 | Dnpc / Kb | **1.2405** [1.2346, 1.2464] | −1.479 [−1.511, −1.447] | 3.4e-26 | Dnpc faster |
| M2 | Dxn+EXACT / Kb | **0.9868** [0.9828, 0.9907] | +0.102 [+0.071, +0.133] | 1.3e-06 | **Dxn+EXACT slower** (−1.3 %); not equivalent (90 % CI [0.9835, 0.9900], Δx [+0.077, +0.128]) |
| M3 | Dnpc/NAP0-WB32 / Kb | **1.2263** [1.2199, 1.2328] | −1.408 [−1.441, −1.374] | 4.8e-25 | faster |
| M4 | Dnpc/NAP0 / K | **1.2896** [1.2838, 1.2953] | −1.935 [−1.968, −1.902] | 5.0e-28 | faster |
| M5 | Dn vs OlL3x730 | 1.0393 [1.0339, 1.0448] | **−0.221** [−0.251, −0.191] | 9.5e-12 | Dn faster; not equivalent (90 % [−0.245, −0.196]) |
| M6 | Dn730f vs OlL3 | 1.0307 [1.0226, 1.0389] | −0.177 [−0.224, −0.130] | 3.2e-07 | Dn730f faster; not equivalent |

- **All six are significant after Holm.** M2's direction is unfavourable: exact + checked Tern is 1.3 % slower than
  Kb. TOST: 6 done, 0 equivalent.
- **PREREG-2 §8:** |Δx(M5)| = 0.221, with its 95 % CI bound 0.191 above the 0.1 µs getpid price. This takes the
  attribution branch: perf sc / counters.
  - Already attributed in C1/DX1/DX2 (x86-facts §19.2): OlL3x730 makes 10.02 system calls per request against Dn's
    8.02, i.e. 1 getpid per event. The remaining gap is the map implementation.
  - Prediction P1 (about 0.1) was low.
- **Sensitivity** (drop the first formal rep of each round, n = 18; drop noisy pairs, identical): every decision
  unchanged, e.g. M2 0.9855 [0.9816, 0.9894], M5 Δx −0.231.

## Drift and carry-over (PREREG-2 §5; CIs are within-session)

**Leg × night, global.** log rps ~ (round, rep) blocks + leg + leg:night gives F(21, 378) = 1.345, p = 0.143. Not
significant. A re-implementation on Δx gives p = 0.112, and on the 10 family legs p = 0.36.

**Per contrast** (night 1 / night 2 GMR, Welch p):

| | night 1 | night 2 | Welch p |
|---|---|---|---|
| M1 | 1.2479 | 1.2331 | **0.005** |
| M2 | 0.9864 | 0.9872 | 0.84 |
| M3 | 1.2313 | 1.2214 | 0.11 |
| M4 | 1.2917 | 1.2875 | 0.47 |
| M5 | 1.0379 | 1.0408 | 0.59 |
| M6 | 1.0314 | 1.0301 | 0.87 |

- M1's lead is 1.2 points smaller on night 2 (0.079 µs/event). It is uncorrected, has the same direction on both
  nights, and both are far from the margin. Report it next to M1.

**Carry-over.** x on pred_leg (one-way F per leg, B on its own µs/event): no leg has p < 0.01; the smallest is Dnextpc,
p = 0.143. A re-implementation's global carry-over F gives p = 0.75. Nothing to note in the text.

## Per-leg x (µs/event over B, mean [95 % CI]; median rps)

| leg | x | median rps |
|---|---|---|
| K | 4.034 | 57 966 |
| Kb | 3.045 | 65 605 |
| O | 3.694 | 60 512 |
| L3port | 1.125 | 87 761 |
| Ol | 1.884 | 77 640 |
| Olpc | 2.369 | 71 841 |
| Ol730x | 1.831 | 77 919 |
| OlL3 | 1.349 | 84 431 |
| OlL3x730 | 1.251 | 85 814 |
| OlL3x730pc | 1.726 | 79 243 |
| Dn | 1.030 | 89 177 |
| Dnpc | 1.566 | 81 317 |
| Dn730f | 1.172 | 86 783 |
| Dn/NAP0 | 1.458 | 82 845 |
| Dn/NAP0-WB32 | 1.043 | 88 990 |
| Dnpc/NAP0 | 2.099 | 74 755 |
| Dnpc/NAP0-WB32 | 1.637 | 80 574 |
| Dxn+EXACT | 3.148 | 64 655 |
| Dxn+EXACT/NAP0-WB32 | 3.150 | 64 676 |
| Dnextpc | 1.612 | 80 817 |
| Dnextpc+CAREFUL | 1.610 | 80 713 |

The CIs are in analysis-c2.txt.

## Exploratory (uncorrected 95 % CIs)

- **Lock price:**
  - Tern: Dn730f vs Dn, Δx +0.142 [+0.107, +0.177].
  - upstream + uid/gid cache: OlL3 vs OlL3x730, +0.098 [+0.060, +0.136].
  - upstream: Ol vs Ol730x, +0.053 [+0.007, +0.100].
  - P4 (lock price independent of the base) holds only roughly.
- **uid/gid cache:** Ol vs OlL3, +0.536 [+0.489, +0.582] (P3 predicted 0.562).
- **Careful mode:** Dnextpc+CAREFUL vs Dnextpc, GMR 1.0004 [0.9948, 1.0060], Δx −0.002. Equivalent on both criteria.
- **B6, Dnext with switches off vs Dnpc:** GMR 0.9926; equivalent on both criteria (Δx 90 % CI [+0.021, +0.071]).
- **Checks** (pc vs no checks): Tern +0.536, upstream + L3 + x730 +0.475, upstream +0.484 µs/event.
- **Checked against checked:** Dnpc / OlL3x730pc 1.0259 [1.0211, 1.0308].
- **Ratios:**
  - Dn / Kb 1.3590; Dn / K 1.5350; Dnpc / K 1.4011; Kb / K 1.1295;
  - O / K 1.0410; Ol / K 1.3323; Dn / Ol 1.1522; Dn / O 1.4745;
  - Dnextpc+CAREFUL / Kb 1.2318;
  - Dxn+EXACT/NAP0-WB32 / Kb 0.9865;
  - Dn/NAP0-WB32 / Kb 1.3558;
  - L3port / O 1.4501.

## perf sc (round 2, night 2; system calls per request, median of 3)

| leg | system calls per request |
|---|---|
| Dn730f | 8.02 |
| Dnextpc, Dnextpc+CAREFUL, Dnpc | 10.02 (2 process_vm_readv) |
| Dnpc/NAP0 | 12.02 |
| Dnpc/NAP0-WB32 | 10.09 |
| OlL3x730pc | 12.02 (2 getpid, 2 process_vm_readv) |
| Dxn+EXACT, ± NAP0-WB32 | 40.0 (getpid 8, gettid 8, getuid 6, getgid 6, process_vm_readv 2) |

Careful mode adds no system call in nginx's steady state.
