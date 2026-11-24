# x86 E2o: official itself under policy S (review r2: the +17% was measured on Odiag only)

Session E2o-official-S-20261124-try1, 60 rows, all valid, exit 0. Policy S = nginx on one core; O and K under M in the same reps.

rows=60 valid=60 reps=10 payload=None exp=E2o
| leg | n | median rps | CV% | GMR vs K [95% CI] | x vs B us/req [95% CI] | ev/req | del/exp | lost/req | missing-reported |
|---|---:|---:|---:|---|---|---:|---:|---:|---:|
| Odiag#S | 10 | 67923 | 0.59 | 1.1725 [1.1664, 1.1786] | 5.519 [5.441, 5.597] | 2.0000 | 1.00000 | 0.000000 | 0..245 |
| K | 10 | 58111 | 0.48 |  |  | 1.9999 | 0.99994 | 0.000114 | -369..2 |
| K#S | 10 | 58132 | 0.42 |  | 8.053 [8.019, 8.087] | 2.0000 | 1.00000 | 0.000000 | -308..2 |
| O | 10 | 60105 | 0.62 | 1.0385 [1.0314, 1.0456] |  | 1.9999 | 0.99994 | 0.000000 | 0..239 |
| O#S | 10 | 67943 | 0.58 | 1.1690 [1.1625, 1.1755] | 5.563 [5.491, 5.634] | 2.0000 | 1.00000 | 0.000000 | 0..472 |
| B#S | 10 | 109213 | 0.67 | 1.8779 [1.8718, 1.8839] |  |  |  |  |  |
TOST O#S vs Odiag#S margin ±0.015: ratio 0.9970 90% CI [0.9917, 1.0024] n=10 -> EQUIVALENT
DIFF x(O) - x(O#S) = 1.859 us/request [1.792, 1.927] n=10; per event (/2) 0.930 us
DIFF x(Odiag#S) - x(O#S) = -0.044 us/request [-0.141, 0.053] n=10; per event (/2) -0.022 us

Paired vs K#S (95% t-CI, n=10): O#S 1.169 [1.1625, 1.1755]; Odiag#S 1.1725 [1.1664, 1.1786]. Same reps under M: O/K 1.0385 [1.0314, 1.0456].
O#S vs Odiag#S TOST ±1.5%: equivalent (0.9970, 90% CI [0.9917, 1.0024]).

Mitigations on the NUC at measurement time: x86-mitigations-20261124.txt (default; RFDS clears the register file on return to user; spectre_v2 eIBRS; vmscape IBPB before exit to userspace).
