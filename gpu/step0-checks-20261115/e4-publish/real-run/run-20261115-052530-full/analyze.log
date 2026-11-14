# e4-publish summary

| run | rounds verified | violating rounds (in a launch's first round) | stale / poison / future words | healed / persistent | rate (rounds/s) | budget hit |
|---|---:|---:|---|---|---:|---|
| t1_A | 1000000128 | 0 (0) | 0 / 0 / 0 | 0 / 0 | 16301463 | False |
| t1_A0_stress | 200000256 | 0 (0) | 0 / 0 / 0 | 0 / 0 | 3449952 | False |
| t1_A_long | 100000128 | 0 (0) | 0 / 0 / 0 | 0 / 0 | 12907045 | False |
| t1_A_stress | 1000000128 | 0 (0) | 0 / 0 / 0 | 0 / 0 | 3446511 | False |
| t1_Am | 10000128 | 0 (0) | 0 / 0 / 0 | 0 / 0 | 16080365 | False |
| t1_Am0mix_stress | 10000128 | 0 (0) | 0 / 0 / 0 | 0 / 0 | 3911041 | False |
| t1_Am_stress | 10000128 | 0 (0) | 0 / 0 / 0 | 0 / 0 | 3517790 | False |
| t1_B | 100000128 | 0 (0) | 0 / 0 / 0 | 0 / 0 | 16320977 | False |
| t1_B_stress | 200000256 | 0 (0) | 0 / 0 / 0 | 0 / 0 | 3440315 | False |
| t1_C | 200000256 | 1370 (1369) | 21971 / 0 / 0 | 200 / 0 | 19200519 | False |
| t1_C0_stress | 10000128 | 17210 (16795) | 1641 / 283770 / 0 | 200 / 0 | 3404418 | False |
| t1_C_long | 100000128 | 4 (0) | 86 / 0 / 0 | 4 / 0 | 12477518 | False |
| t1_C_stress | 200000256 | 299982 (288173) | 22747 / 4789326 / 0 | 200 / 0 | 3454520 | False |
| t1_Cd | 200000256 | 2 (0) | 64 / 0 / 0 | 2 / 0 | 13369278 | False |
| t1_Cd15 | 10000128 | 0 (0) | 0 / 0 / 0 | 0 / 0 | 17278574 | False |
| t1_Cd15_stress | 10000128 | 17341 (17158) | 10879 / 321675 / 0 | 200 / 0 | 3334744 | False |
| t1_Cd_stress | 200000256 | 0 (0) | 0 / 0 / 0 | 0 / 0 | 3485639 | False |
| t1_D1 | 100000128 | 0 (0) | 0 / 0 / 0 | 0 / 0 | 16605963 | False |
| t1_D1_stress | 200000256 | 0 (0) | 0 / 0 / 0 | 0 / 0 | 3439403 | False |
| t1_D2 | 100000128 | 0 (0) | 0 / 0 / 0 | 0 / 0 | 16125688 | False |
| t1_D2_stress | 200000256 | 0 (0) | 0 / 0 / 0 | 0 / 0 | 3493113 | False |
| t1_E | 10000128 | 3320 (3319) | 57279 / 0 / 0 | 200 / 0 | 15115698 | False |
| t1_E_stress | 10000128 | 73799 (73797) | 327002 / 4419280 / 0 | 200 / 0 | 3291036 | False |
| t2_cas | 10000000 | stale rounds 0, double winners 0 | GPU payload bad rounds 0 | - | 131119 | False |
| t2_cas_stress | 3000000 | stale rounds 0, double winners 0 | GPU payload bad rounds 0 | - | 60739 | False |
| t2_casacq | 3000000 | stale rounds 0, double winners 0 | GPU payload bad rounds 0 | - | 53937 | False |
| t2_casfence | 10000000 | stale rounds 0, double winners 0 | GPU payload bad rounds 0 | - | 58458 | False |
| t2_control_cas | 1000000 | stale rounds 253301, double winners 0 | GPU payload bad rounds 231675 | - | 46957 | False |
| t2_control_casfence | 1000000 | stale rounds 174, double winners 0 | GPU payload bad rounds 157 | - | 47220 | False |

## Test 1, stress

- verdict: **no violation observed in 1000000128 rounds of A; C failed 299982 times in 200000256 rounds of the same setup: evidence that one leader fence after __syncwarp suffices in this layout (the publisher's own payload stores share instructions with the other lanes'; whether the fence orders stores of lanes that did not fence is decided by A0, lane 0 idle) (95% upper bound on A's rate: 3.00e-09 per round)**
- attribution: the fence's duration: Cd (a fenceless wait of 28205 ns, A's median fence 28032 ns) showed 0 in 200000256 rounds where C-like failures would be 299982.0, so A is 'safe in practice here, not attributable to fence semantics'
- A_rounds: 1000000128
- A_violations: 0
- C_rounds: 200000256
- C_violations: 299982
- expected_A_violations_if_like_C: 1499908.2721058917
- Cd_rounds: 200000256
- Cd_violations: 0
- Cd_mean_delay_ns: 28204.7
- A_fence_ns_first_round: {'n': 2500224, 'p50': 28032, 'p90': 38848, 'p99': 41024, 'overflow': 0}
- A_fence_ns_other_rounds: {'n': 7499904, 'p50': 26624, 'p90': 36288, 'p99': 42368, 'overflow': 0}
- Am_rounds: 10000128
- Am_violations: 0
- Cd15_info: C plus a fixed 1.5 us fenceless wait (informational, not a rule): 17341 violating rounds in 10000128
- rate_A: 3446510.8
- rate_B: 3440315.4

## Test 1, normal

- verdict: **no violation observed in 1000000128 rounds of A; C failed 1370 times in 200000256 rounds of the same setup: evidence that one leader fence after __syncwarp suffices in this layout (the publisher's own payload stores share instructions with the other lanes'; whether the fence orders stores of lanes that did not fence is decided by A0, lane 0 idle) (95% upper bound on A's rate: 3.00e-09 per round)**
- attribution: undecided: Cd failed 2 times in 200000256 rounds, too few to separate A's zero from a delay (needs >= 3 Cd failures and an expectation of >= 3 in A's rounds)
- A_rounds: 1000000128
- A_violations: 0
- C_rounds: 200000256
- C_violations: 1370
- expected_A_violations_if_like_C: 6849.992108810101
- Cd_rounds: 200000256
- Cd_violations: 2
- Cd_mean_delay_ns: 6597.1
- A_fence_ns_first_round: {'n': 2500224, 'p50': 6400, 'p90': 7552, 'p99': 8448, 'overflow': 0}
- A_fence_ns_other_rounds: {'n': 7499904, 'p50': 5248, 'p90': 6592, 'p99': 7680, 'overflow': 0}
- Am_rounds: 10000128
- Am_violations: 0
- Cd15_info: C plus a fixed 1.5 us fenceless wait (informational, not a rule): 0 violating rounds in 10000128
- rate_A: 16301463.3
- rate_B: 16320977.2

## Test 1, long

- verdict: **no violation observed in 100000128 rounds of A; C failed 4 times in 100000128 rounds of the same setup: evidence that one leader fence after __syncwarp suffices in this layout (the publisher's own payload stores share instructions with the other lanes'; whether the fence orders stores of lanes that did not fence is decided by A0, lane 0 idle) (95% upper bound on A's rate: 3.00e-08 per round)**
- A_rounds: 100000128
- A_violations: 0
- C_rounds: 100000128
- C_violations: 4
- expected_A_violations_if_like_C: 4.0
- rate_A: 12907044.6

## Test 1, stress, publishing lane writes no payload (A0, C0, Am0mix)

- verdict: **no violation observed in 200000256 rounds of A0 (the publishing lane wrote no payload) while C0 failed 17210 times in 10000128 (95% upper bound 1.50e-08 per round), but not separable from the fence's duration: a fence in a warp without payload took 1.05x as long under the same load, so a fence that ordered nothing of the payload would have hidden the race too**
- A0_rounds: 200000256
- A0_violations: 0
- C0_rounds: 10000128
- C0_violations: 17210
- expected_A0_violations_if_like_C0: 344196.03486675373
- fence_ns_first_round_p50_lane0_idle_warps: 21632
- fence_ns_first_round_p50_warps_without_payload: 22656
- fence_ratio_without_over_with_own_stores: 1.047337278106509

## Test 2

- control stale rounds: {'cas': 253301, 'casfence': 174} (checker detects: True)
- t2_cas: **no stale read and no double winner in 10000000 rounds (95% bound 3.00e-07 per round); the control shows the check can detect stale data**
- t2_casfence: **no stale read and no double winner in 10000000 rounds (95% bound 3.00e-07 per round); the control shows the check can detect stale data**
- t2_casacq: **no stale read and no double winner in 3000000 rounds (95% bound 1.00e-06 per round); the control shows the check can detect stale data**
- t2_cas_stress: **no stale read and no double winner in 3000000 rounds (95% bound 1.00e-06 per round); the control shows the check can detect stale data**
