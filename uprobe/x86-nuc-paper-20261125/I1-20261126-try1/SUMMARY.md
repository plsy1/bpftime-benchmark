# I1 Redis TLS with io-threads (x86 NUC, 2026-11-26/27; exploratory, no PREREG entry)

Session I1-20261126-try1, plan plans/I1.txt, rows.jsonl (277 rows: 17 warm-up, 260 measured; 0 invalid,
0 non-empty invalid_reason, redis-benchmark rc 0 on every row). Redis 7.0.15 with TLS (image
bpftime-ssl-redis:x86-20261126; libssl 3.0.13-0ubuntu3.16 as in the frozen nginx image, per docker-redis/Dockerfile),
redis-benchmark --tls --threads 6 -c 96 -P 1 -d 16 -t set,get on CPUs 0,1,8-11. rps = commands/s (harmonic mean of
redis-benchmark's SET and GET rates over 2N commands); each command is 2 sslsniff events (SSL_read + SSL_write), so
x = (1/rps − 1/rps_B)·1e6/2 µs/event, paired with B of the same round and rep. Consumer on CPU 4.

- R4: io-threads 4 (main + io_thd_1..3 pinned to CPUs 2,3,6,7 like H4; these are 2 physical P cores with SMT:
  2/3 and 6/7 are SMT sibling pairs, core_id 4 and 12), -n 1500000, 2 rounds × 10 (+1 warm-up), 9 legs. The threaded-I/O read path is active on
  ≥ 99.56 % of commands on every R4 row (io_threaded_frac; the counter likely also includes the main thread's own share of the dispatch — not checked
  against the Redis source — so it shows the threaded path is used, not which thread runs each SSL call).
  B generator wall 9.5 s (median). Redis 7.0 io threads spin while active, so server-core busy (0.96–0.97 at B) is not
  work; the generator E cores 8–11 are 0.93–0.95 busy at B, so B (and the ratios against it) may be partly
  generator-limited.
- R1: io-threads 1 under the mask {2,6} (like H1/M; 2 and 6 are different physical cores), -n 700000, 1 round × 10
  (+1 warm-up), 8 legs. B generator wall 8.2 s, generator E cores 0.59 busy.

## Results

Tables: medians of measured reps; x = median of per-rep x. Ratios: geometric mean of per-rep paired rps ratios with t
95 % CI. Δx: mean of per-rep paired x differences (median in parentheses where it differs by > 0.005).

### R4 (4 io threads, n = 20)

| leg | rps | /B | x µs/event |
|---|---|---|---|
| B | 321 837 | 1 | – |
| Dn | 277 123 | 0.861 | 0.251 |
| Dn730f | 270 673 | 0.841 | 0.294 |
| OlL3x730 | 259 246 | 0.806 | 0.375 |
| OlL3 | 258 487 | 0.803 | 0.381 |
| Dnpc | 247 954 | 0.770 | 0.463 |
| Ol | 243 378 | 0.756 | 0.500 |
| Kb (K + SSLSNIFF_PB_WAKEUP=32) | 212 645 | 0.661 | 0.798 |
| K | 189 627 | 0.589 | 1.083 |

- Dn / OlL3x730 (both without the #730 lock) **1.0636 [1.0578, 1.0695]**, Δx −0.115 (−0.125) µs/event.
  Dn730f / OlL3 (both with it) **1.0494 [1.0452, 1.0535]**, Δx −0.091 (−0.085). E7b HAProxy, same comparison:
  1.0526 [1.0494, 1.0557].
- Dnpc (checked reads) trails the best upstream: Dnpc / OlL3x730 0.954 [0.951, 0.957], Δx +0.093 (+0.088);
  as on HAProxy (E7b 0.956 [0.952, 0.959], recomputed from H7b rows).
- Dn / Ol 1.134; Dn / K 1.455; Dn / Kb 1.305; Dnpc / Kb 1.170 (E7b 1.267); Dnpc / Ol 1.017 (E7b 1.061); Kb / K 1.115.
- #730 lock: on Tern, Dn730f / Dn 0.982 [0.977, 0.987], Δx +0.034 (+0.043) µs/event (Dn730f = Tern + #730 lock + the
  per-CPU lookup fix 1a2d42ec, PREREG-2b; the price as defined in §19.9). On upstream, OlL3 / OlL3x730 0.9951
  [0.9918, 0.9984], Δx +0.010. So lock + fix costs Tern 1.8 % rps (E7b 1.9 %), while the lock costs upstream only 0.5 % here
  (E7b 1.9 %). The 1.3-point gap [0.7, 2.0] is within the fix's own E4f price (+2.0 % at T=1, §19.9), so it is not
  attributed to the lock.
- Round 0 vs round 1 medians agree within ~0.1 % for every leg (Dn 0.11 %) except Kb (r0/r1 0.9923).

### R1 (io-threads 1, n = 10, one round)

| leg | rps | /B | x µs/event |
|---|---|---|---|
| B | 173 385 | 1 | – |
| Dn | 146 184 | 0.843 | 0.537 |
| OlL3x730 | 135 698 | 0.783 | 0.798 |
| Dnpc | 126 217 | 0.728 | 1.076 |
| Ol | 120 898 | 0.697 | 1.253 |
| Kb | 99 471 | 0.574 | 2.142 |
| Dxn + BPFTIME_DIAG_EXACT=1 | 96 019 | 0.554 | 2.324 |
| K | 83 175 | 0.480 | 3.127 |

- Dn / OlL3x730 1.0573 [1.0423, 1.0724], Δx −0.198 (−0.184); Dnpc / OlL3x730 0.915 [0.903, 0.928], Δx +0.338 (+0.351)
  (E7a 0.941 [0.934, 0.948]);
  Dn / Ol 1.197; Dn / Kb 1.452; Dnpc / Kb 1.257; Dn / K 1.737; Kb / K 1.196.
- Dxn+EXACT / Kb **0.9635 [0.9597, 0.9674]**: exact mode 3.7 % slower than Kb here (E7a HAProxy: 0.995 [0.988, 1.002]).
- R1 per-event costs are 2.1–2.9× R4's (Dn 2.1, Ol 2.5, K 2.9). Observed only; R1 and R4 differ in thread count, SMT
  (R4: 4 hyperthreads on 2 physical cores; R1: one thread with a whole core), spinning io threads, -n and generator load,
  so no per-thread scaling is inferred.

## Event accounting

unreported = 2·commands − delivered events − reported lost. It equals a constant offset on all 150 bpftime and kernel
rows (2 for Dn/Dn730f/Dnpc/Dxn: the two CONFIG GETs share one read and one write; 4 for K/Kb: + the readiness PING the
loader misses) and on the 62 upstream rows without silent loss.

| leg | R4: reps with reported lost, sum | R4: reps with silent loss, sum (max) | R1: reported | R1: silent |
|---|---|---|---|---|
| Dn | 8/20, 67 780 | 0/20 | 5/10, 2 521 | 0/10 |
| Dn730f | 10/20, 69 542 | 0/20 | – | – |
| Dnpc | 8/20, 41 516 | 0/20 | 3/10, 1 757 | 0/10 |
| Dxn+EXACT | – | – | 3/10, 1 133 | 0/10 |
| K | 7/20, 18 308 | 0/20 | 2/10, 331 | 0/10 |
| Kb | 3/20, 12 327 | 0/20 | 3/10, 1 936 | 0/10 |
| Ol | 0 | 6/20, 15 379 (9 349) | 0 | 3/10, 2 166 (845) |
| OlL3 | 0 | 3/20, 58 253 (51 621) | – | – |
| OlL3x730 | 0 | 4/20, 16 817 (10 559) | 0 | 2/10, 1 530 (909) |

- Every bpftime and kernel loss is reported; upstream reports 0 lost and loses silently in 13/60 R4 and 5/20 R1 reps.
- Per leg the loss is ≤ 5.8e-4 of events; per rep it reaches 8.6e-3 (OlL3, silent) and 2.5e-3 (Dn, reported).
- Aggregate reported loss is higher on Dn (5.6e-4) than on K (1.5e-4) at 46 % more events/s, but it occurs in a similar
  number of reps (8/20 vs 7/20): the difference is burst size, not frequency. Cause not investigated.

## Reading

- On a second real TLS server with named io threads, Dn leads the best upstream build by a similar margin as on HAProxy
  (Redis 1.057 / 1.064 vs HAProxy 1.038 / 1.053 at 1 / 4 threads) and K/Kb by 1.3–1.7 (Dn/Kb Redis 1.45 / 1.30 vs
  HAProxy 1.40 / 1.40), Dnpc trails the best unchecked upstream build as on HAProxy, and upstream again loses silently.
- Differences from HAProxy: exact mode does not reach Kb (0.964 vs 0.995); the lock costs upstream only 0.5 % (1.9 % in
  E7b); Dn's lead over K, Kb and Ol shrinks from R1 to R4 (Dn/K 1.74 → 1.46, Dn/Kb 1.45 → 1.30, Dn/Ol 1.20 → 1.13).
- Caveats: exploratory, single session, not pre-registered; R1 has one round; R4 is 2 physical cores with SMT and
  spinning io threads; the R4 B generator is near saturation, which may compress all R4 ratios toward 1. Report as
  "ratio in another configuration", not next to C2.
