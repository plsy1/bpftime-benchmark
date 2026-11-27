# I7′ L2 pressure on the E-core cluster (x86 NUC, 2026-11-27; exploratory, no PREREG entry)

Session I7-20261127-try1, plan plans/I7.txt.
- rows.jsonl: 144 rows, 24 warm-up, 120 measured, 0 invalid, 0 lost, 0 non-200, 0 wrk errors. check-after PASS.
- Smoke: I7-smoke-20261127-try1.
- Stressor: i7/l2stress.c (sha256 in i7/sha256.txt).

## Setup

**Policy E** (new in harness/config-x86.json; backup .pre-i7):
- nginx (1 worker) on E core 8;
- sslsniff and its consumer on CPU 4 (P core, outside the cluster);
- wrk with 4 threads on 0, 1, 2, 6.

CPUs 8–11 share one 2 MiB L2 (sysfs index2: shared_cpu_list 8-11, size 2048K) and have no SMT.

**Conditions:**
- S: l2stress on each of 9, 10 and 11. Each instance runs a random pointer chase with one store per step over 1 MiB
  (each logged "ready 16384 lines"), 3 MiB in total.
- N: no stressor.
- Blocks run N, S, N, S (rounds 0 and 1, 5 measured reps plus 1 warm-up each), so S always follows N. Each condition
  has n = 10 reps but only 2 blocks.
- Legs: B, K, Kb (K + SSLSNIFF_PB_WAKEUP=32), Ol, Dn, Dnpc. 16 B.

**Checks:**
- busy_all medians: CPU 8 at 0.996 in both conditions (nginx saturated). CPUs 9–11 at 1.0 under S and ≤ 0.003
  under N. wrk CPUs 0.10–0.12. CPU 4 at 0.02–0.09 per traced leg.
- cpu8 clock: 2514 MHz under N and S (freq-log), so core-clock throttling is ruled out. Uncore and ring frequency were
  not logged.
- B round 0 vs round 1 (N), medians: 1.0009.
- No l2stress process survives; checked live after the session, not recorded by it.

## Results

x = (1/rps − 1/rps_B)·1e6/E µs/event, with B paired by condition, round and rep; mean over n = 10. CIs are Welch with
±2.1 SE (exact t gives the same to 3 decimals except K [+0.300, +0.970] and Dnpc [+0.377, +0.830]). These CIs treat the
10 reps as independent although they come from 2 blocks per condition, so they are too narrow (see the per-round
values).

B (medians): 30 970 rps under N, 28 432 under S, −8.2 % (means −7.7 %). The stressor slows the untraced server too.

| leg | x N | x S | S − N [95 % CI] | S − N round 0 / round 1 | rps N / S (medians) |
|---|---|---|---|---|---|
| K | 8.633 | 9.269 | +0.635 [+0.305, +0.966] | 0.80 / 0.47 | 20 154 / 18 628 |
| Kb | 7.205 | 7.609 | +0.403 [+0.154, +0.653] | 0.48 / 0.33 | 21 337 / 19 871 |
| Ol | 7.673 | 8.174 | +0.500 [+0.273, +0.727] | 0.64 / 0.36 | 20 958 / 19 445 |
| Dn | 4.737 | 5.247 | +0.510 [+0.246, +0.775] | 0.68 / 0.35 | 23 831 / 21 921 |
| Dnpc | 7.079 | 7.683 | +0.604 [+0.379, +0.829] | 0.62 / 0.59 | 21 475 / 19 800 |

Change of the gap between two legs under the stressor (difference in differences, S − N, same method), with the two
rounds separately:

| gap | DiD [95 % CI] | round 0 / round 1 |
|---|---|---|
| x(K) − x(Dn) | +0.125 [−0.135, +0.385] | +0.124 / +0.126 |
| x(Kb) − x(Dnpc) | −0.200 [−0.398, −0.003] | −0.140 / −0.261 |
| x(Ol) − x(Dn) | −0.010 [−0.261, +0.241] | −0.033 / +0.012 |
| x(K) − x(Kb) | +0.232 [−0.100, +0.564] | +0.319 / +0.145 |

- **Shift of every leg:** the stressor raises every leg's per-event cost by 0.40–0.64 µs (5.6–10.8 %, about in
  proportion to B's +8.3 % time per request). This fits a generic slowdown of all code on core 8.
- **Kb − Dnpc** is the one gap whose CI excludes 0: the checked Tern build loses about 0.2 µs to Kb under the stressor.
  This is uncorrected over 4 comparisons and comes from 2 blocks, so it is weak.
- **The other gaps:** their CIs have half-widths of 0.25–0.33 µs. Differences in sensitivity of up to about 0.4 µs
  cannot be ruled out. Equivalence is not shown.
- **What "L2 pressure" means here:** the stressor's capacity effect was not separated from its L2-bandwidth, ring/L3
  and writeback load. No cache counters were collected.

## Cross-configuration observation: E core vs P core (not an I7′ comparison)

Per-event costs on the E core (I7 N) against the P-core policy M:

| | E core (I7 N) | P core | inflation E/P |
|---|---|---|---|
| Dn | 4.737 | 1.044 (DN1) | 4.5× |
| Dnpc | 7.079 | 1.607 (DN1) | 4.4× |
| Kb | 7.205 | 3.046 (DN1) | 2.4× |
| K | 8.633 | 4.010 (E4, older session) | 2.2× |
| B, time per request | | | 3.5× |

- The inflation is backend-specific. The bpftime builds inflate more than nginx itself, the kernel less.
- x(Dn)/x(K) is 0.55 on the E core against 0.25 on the P core. Orin's D/K is about 0.50 (orin-facts B1/B3). So the
  smaller core reproduces an Orin-like in-application premium for userspace hooks.
- Policy M differs in more than the core: nginx on {2,6}, wrk on the E cores, and K from another session. This is an
  observation across configurations, not a controlled comparison.

## Reading

- **I7′ does not bear on Q6.** Q6 (eval-plan:162) concerns the pin premium: the pin sequence costs 2.5× more inside
  nginx than in isolation on Orin.
  - No I7′ leg runs the pin. Ol is upstream head, where #836 removed it; Dn, Dnpc, K and Kb never pin.
  - There is no isolated pinbench arm.
  - The eval plan names N6 as the discriminating experiment (declined) and J6 as corroboration.
- **What I7′ shows:** on an E core, a cluster-sharing stressor raises the pin-free per-event costs of K, Kb, Ol, Dn
  and Dnpc by similar amounts. The checked Tern build is weakly more sensitive than Kb, and no gap shrinks or grows by
  more than about 0.4 µs.
- **Worth noting for the paper's Orin discussion:** moving nginx to a smaller core inflates userspace hooks about 4.5×
  against the kernel's about 2.2×. This was observed across configurations, not controlled.
- **Scope:** exploratory, single session, 2 blocks per condition.
