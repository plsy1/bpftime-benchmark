# P13-20261126-try1 — grace-period counter ablation (x86 NUC), summary

Exploratory (eval plan P13; reviewer W6 "slow hook"). Session ended 03:49:04, exit 0. 100 measured rows (+ warm-up),
all valid; noisy_core rows 0. Policy M, 16 B, S1 = hook-only sslsniff program (no output), cnt loader; 2 rounds × 10
paired reps. Legs B, K@S1, O@S1 (added beyond the plan: the "slow hook" is Tern's vs official's), Dn@S1, Dnngp@S1.
Dnngp = Tern c71a9987 + ladder fa8d07d0: Frida callbacks keep no grace-period counter (`probe_grace::enter` returns
nullptr, `leave` returns; no TLS access, no stores). Detach is unsafe in that build; it only prices the counter.
Tables: summary-p13.md (analyze_paper.py). S1 emits no events, so ev/req and del/exp are 0 by design.

## End to end (extra time per request vs B; per event = /2; 6 probe hits per request at S1)

| difference | us/request (95 % CI) | us/event |
|---|---|---|
| Dn − Dnngp (counter) | +0.051 [−0.037, +0.139] | +0.026; 90 % CI [−0.011, +0.062] → equivalent at ±0.084 |
| Dn − O (Tern hook vs official) | −0.032 [−0.155, +0.091] | −0.016 |
| Dnngp − O | −0.083 [−0.175, +0.008] | −0.042 |
| K − Dn (kernel trap vs Tern hook) | +3.765 [3.666, 3.864] | +1.883 |

Round 0 / round 1: Dn − Dnngp +0.084 / +0.018 us/request; Dn − O −0.027 / −0.037.

## Microbenchmark (uprobe suite __bench_uprobe, ns per hit, 5 runs each, interleaved; micro-uprobe.txt)

official 124.3 [122.9, 124.7]; Tern 133.0 [131.9, 134.4]; Tern without the counter 127.9 [124.1, 132.8].

Follow-up, balanced (micro-uprobe-balanced.txt, micro-balanced-summary.txt; 10 reps, order rotated each rep, after a
same-path control build `dnngpctl` = unmodified c71a9987 built in the Dnngp worktree, so Dnngp and its control share
path and toolchain): official 123.8, Tern 133.0, control 132.0, Dnngp 127.5 ns (medians). Paired: Tern − official
+9.3 [7.1, 11.5]; control − official +8.2 [7.1, 9.4]; Tern − control +1.1 [−1.3, 3.4] (build path/layout negligible);
**control − Dnngp +4.0 [1.8, 6.2]** (the counter); Dnngp − official +4.2 [1.6, 6.9] (the rest, unexplained). Scripting
slip: zsh's 1-based arrays left one slot per rep empty (10 rows without a build, excluded) and the layout control dnr
was not run; CPU frequency and C-states were not controlled.

## Reading (corrected after the independent check)

1. The slow hook is real in isolation: Tern − official = +8.8 ns per hit, paired 95 % CI [6.8, 10.8] (~7 %; E3 had
   131.2 vs 122.8). E6's nginx difference was also (barely) significant when paired: D@S1 − Odiag@S1 +0.055 us/event
   [+0.004, +0.105] (two rounds +0.051, +0.059); "within noise" in the first draft was wrong.
2. This session cannot see it in nginx: Dn − O −0.016 us/event, 95 % CI [−0.078, +0.045]; with a paired SD of 0.131 the
   smallest detectable difference (80 % power) is ~0.09 us/event, so power was 0.43 for E6's 0.055 and 0.1–0.2 for the
   ~0.02–0.035 the microbenchmark predicts. This session's O@S1 sits high (0.625 us/event vs 0.573 in E17 and 0.575 in
   E6) while Dn@S1 (0.609) matches D@S1 0.630 and DX2 0.646. Dnngp − O 90 % CI [−0.080, −0.004] (Dnngp faster than O in
   nginx, slower by ~4 ns in the microbenchmark) also points at a high O leg or a nginx/micro mismatch.
3. The counter explains about half of the microbenchmark gap: in the balanced follow-up with a same-path control,
   control − Dnngp = +4.0 ns [1.8, 6.2] of the +8.2 ns control − official gap; the other ~4.2 ns [1.6, 6.9] is not
   explained. (The first, unbalanced run gave +5.0 [0.3, 9.6] with layout and order confounds; the follow-up removes
   the layout confound — Tern vs its same-path rebuild +1.1 [−1.3, 3.4] — and rotates the order.)
4. What the counter costs in this build: `probe_grace::enter()` is not inlined in Dn (frida_slot_on_enter calls it; it
   saves 6 registers and reaches its thread_local through `__tls_get_addr@plt`, general-dynamic TLS); `leave()` is
   inlined. Dnngp has no call left. So the ablation prices "call + dynamic TLS lookup + stores", an upper bound for the
   mechanism; an initial-exec or inlined counter could be cheaper (Tern dev note, record only).
5. End to end the counter is not detected: Dn − Dnngp +0.026 us/event, 90 % CI [−0.011, +0.062] (95 % upper 0.070;
   Hodges–Lehmann 0.015, Wilcoxon p 0.29). The ±0.084 margin used above is M5's and is not informative here.
6. Orin's slow hook (design.tex:75: 2.92 vs 2.70 us/event) is untouched: P13 ran on x86 only (Orin J14 cut).
7. For the paper: do not drop the phenomenon. Write: Tern's Frida hook costs ~9 ns (7 %) more per hit than official's on
   x86; about half of it (4.0 ns [1.8, 6.2]) is the detach-safety counter (a non-inlined call with a dynamic TLS
   lookup), the rest unexplained; end to end the difference is below what n = 20 can detect (~0.09 us/event). Orin's 0.22 us/event stays
   unexplained unless measured.

## Flags

- O@S1 was not in the planned leg list; its comparisons are exploratory.
- The microbenchmark runs used the R-PR runner (raw/RPR-20261126/scripts/run.sh) under the measurement lock.
