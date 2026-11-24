# E18: Tern rebased onto official 70f7bc1f (new dev c71a9987), x86 NUC

Session E18-newdev-20261124-try1, 2026-11-24 16:08–16:52 JST. 175 rows, all valid, exit 0. 16 B payload, policy M unless marked `#S`.

## Builds

| leg | source | checks |
|---|---|---|
| D, Dpc | old dev 9ccda7e8 (frozen `builds/dev`, `builds/dpc`) | off / on |
| Dn | new dev `dev/uprobe-official-70f7bc1f3363` = c71a9987: official 70f7bc1f + 240 replayed dev commits + f9479e57 (reconcile) + 24999e3c (no per-CPU exec lock) + d922dd0b (no epoll signal taking) + c71a9987 (tests); pushed by the Jetson session | off |
| Dnpc | same | on |
| Dxn | c71a9987 + paper diag, local branch `ladder/dexact-c71a9987`: fa6e5ee9 (87e0117a cherry-picked cleanly) and f39f7362 (66ed229e ported: the prctl block sits first in upstream's pthread_once comm helper). Patches are in `patches/`. | on |
| O, Ol, Olpc | as E17 | |

Dn, Dnpc and Dxn use the same `build-arm.sh` options as dev/dpc (see `builds/`).

## Round 0: throughput (10 reps, rotated, paired)

| ratio | GMR [95% CI] |
|---|---|
| Dn / K | 1.5327 [1.5184, 1.5471] |
| Dnpc / K | 1.4023 [1.3965, 1.4082] |
| Dxn / K | 1.3986 [1.3892, 1.4081] |
| **Dxn+EXACT / K** | **1.1203 [1.1120, 1.1286]** (E16: 1.1220) |
| **Dxn+EXACT / K_wb32** | **0.9886 [0.9816, 0.9956]** (E16: 0.9909) |
| Dn / K_wb32 | 1.3525 [1.3384, 1.3668] |
| Dnpc / K_wb32 | 1.2375 [1.2311, 1.2439] |
| D / K_wb32 | 1.3476 [1.3416, 1.3536] |
| Dpc / K_wb32 | 1.2362 [1.2257, 1.2467] |
| Ol / K_wb32 | 1.1832 [1.1744, 1.1921] (E17: 1.1777) |
| Olpc / K_wb32 | 1.0969 [1.0899, 1.1040] (E17: 1.0899) |
| O / K_wb32 | 0.9262 [0.9217, 0.9307] |
| K_wb32 / K | 1.1332 [1.1280, 1.1384] |
| Dn / Ol | 1.1431 [1.1313, 1.1551] |
| Dnpc / Olpc | 1.1281 [1.1195, 1.1369] |
| Dxn+EXACT / Olpc | 0.9012 [0.8963, 0.9062] |

Equivalence (TOST, ±1.5%):
- Dn vs D: 1.0037, 90% CI [0.9971, 1.0103]. EQUIVALENT.
- Dnpc vs Dpc: 1.0011 [0.9975, 1.0047]. EQUIVALENT.
- Dxn (switch off) vs Dnpc: 0.9974 [0.9922, 1.0026]. EQUIVALENT.

Paired extra time per event:
- x(Dxn+EXACT) − x(Dxn) = 1.530 µs [1.483, 1.578] (E16: 1.507). This is the cost of exactness.
- x(K) − x(Dxn+EXACT) = 0.925 µs.
- x(K_wb32) − x(Dxn+EXACT) = −0.088 µs.
- x(Dnpc) − x(Dn) = 0.522 µs. This is the probe-read check.
- x(D) − x(Dn) = 0.020 µs [−0.026, 0.066].

## Round 1: policy S

Dn#S / K#S = 1.5331 [1.5257, 1.5406] and Dnpc#S / K#S = 1.3994 [1.3863, 1.4127]. Both are the same as under policy M (Dn/K 1.5327, Dnpc/K 1.4023).

## Round 2: system calls per request on the nginx worker (perf stat, 3 reps, medians)

| leg | all | getpid | gettid | getuid | getgid | process_vm_readv | other |
|---|---:|---:|---:|---:|---:|---:|---:|
| B | 8.02 | 0 | 0 | 0 | 0 | 0 | 8 |
| Dn@S4 | 8.03 | 0.01 | 0 | 0 | 0 | 0 | 8 |
| Dnpc@S4 | 10.02 | 0 | 0 | 0 | 0 | 2 | 8 |
| Dxn@S4 | 10.02 | 0 | 0 | 0 | 0 | 2 | 8 |
| Dxn+EXACT@S4 | 39.99 | 8 | 8 | 6 | 6 | 2 | 10 (8 + 2, presumably prctl; not in the counted set) |

The new dev, like the old one, makes no system call per event beyond the checked reads.

## Reported losses

Every perf-buffer leg reports a few counted losses in some runs. Sum over 10 runs (maximum in one run): K 596 (289), K_wb32 1,095 (359), D 1,804 (525), Dpc 2,411 (628), Dn 703 (382), Dnpc 2,616 (857), Dxn 781 (278), Dxn+EXACT 789 (300). Each run has about 1.6 million events. The old and new dev builds lose alike, and so does the kernel, so the smoke run's Dnpc lost=41 is not specific to the new dev. Every run passes the audit (delivered + lost = generated, within tail events).

## Reading

The rebase onto upstream head changes nothing measurable on x86. The new Tern matches the old one within 1.5%. Exactness still costs 1.53 µs per event. The exact, checked runtime is 12.0% faster than the stock kernel and 1.1% slower than the batched one. The paper's x86 Tern and exact-runtime numbers (E15/E16, dev 9ccda7e8) therefore also describe Tern on current upstream. Tern on upstream head is 1.14× upstream's throughput (checks on: 1.13×).

## Probe-attachment scan (all x86 paper sessions)

The Orin E18 found one stock-segment run in which the probe was not working (sslsniff CPU 0.03 s, rps equal to B). On x86, every throughput leg in E2o, E7c, E15, E16, E17 and E18 is a counted leg, so each run has direct evidence: delivered/expected ≥ 0.9993 and sslsniff CPU ≥ 1.0 s on every non-B row, and no leg duration above twice the median. The rows that deliver 0 events are all hook-stage (S1) legs, which emit no output by design. Each of those runs is clearly below B in the same round (K@S1 about 70k, O/Ol/D/Odiag@S1 about 91–98k, B about 108k), so the hook ran in every one. No x86 row needs to be excluded.
