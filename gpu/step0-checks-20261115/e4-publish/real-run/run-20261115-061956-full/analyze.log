# e4-publish summary

| run | rounds verified | violating rounds (in a launch's first round) | stale / poison / future words | healed / persistent | rate (rounds/s) | budget hit |
|---|---:|---:|---|---|---:|---|
| t1_A | 1000000128 | 0 (0) | 0 / 0 / 0 | 0 / 0 | 16011406 | False |
| t1_A0_stress | 200000256 | 0 (0) | 0 / 0 / 0 | 0 / 0 | 3448038 | False |
| t1_A_long | 100000128 | 0 (0) | 0 / 0 / 0 | 0 / 0 | 12701222 | False |
| t1_A_stress | 1000000128 | 0 (0) | 0 / 0 / 0 | 0 / 0 | 3441168 | False |
| t1_Am | 10000128 | 0 (0) | 0 / 0 / 0 | 0 / 0 | 15910301 | False |
| t1_Am0mix_stress | 10000128 | 0 (0) | 0 / 0 / 0 | 0 / 0 | 3924213 | False |
| t1_Am_stress | 10000128 | 0 (0) | 0 / 0 / 0 | 0 / 0 | 3487417 | False |
| t1_B | 100000128 | 0 (0) | 0 / 0 / 0 | 0 / 0 | 16066858 | False |
| t1_B_stress | 200000256 | 0 (0) | 0 / 0 / 0 | 0 / 0 | 3491058 | False |
| t1_C | 200000256 | 1776 (1776) | 27447 / 0 / 0 | 200 / 0 | 19816087 | False |
| t1_C0_stress | 10000128 | 27969 (27245) | 8618 / 497786 / 0 | 200 / 0 | 3341324 | False |
| t1_C_long | 100000128 | 1 (0) | 24 / 0 / 0 | 1 / 0 | 12860266 | False |
| t1_C_stress | 200000256 | 465548 (449791) | 100532 / 8066269 / 0 | 200 / 0 | 3480591 | False |
| t1_Cd | 200000256 | 0 (0) | 0 / 0 / 0 | 0 / 0 | 13551522 | False |
| t1_Cd15 | 10000128 | 0 (0) | 0 / 0 / 0 | 0 / 0 | 17794439 | False |
| t1_Cd15_stress | 10000128 | 3379 (3319) | 264 / 63594 / 0 | 200 / 0 | 3379314 | False |
| t1_Cd_stress | 200000256 | 0 (0) | 0 / 0 / 0 | 0 / 0 | 3521327 | False |
| t1_D1 | 100000128 | 0 (0) | 0 / 0 / 0 | 0 / 0 | 16148601 | False |
| t1_D1_stress | 200000256 | 0 (0) | 0 / 0 / 0 | 0 / 0 | 3446596 | False |
| t1_D2 | 100000128 | 0 (0) | 0 / 0 / 0 | 0 / 0 | 16457847 | False |
| t1_D2_stress | 200000256 | 0 (0) | 0 / 0 / 0 | 0 / 0 | 3469423 | False |
| t1_E | 10000128 | 2945 (2945) | 55191 / 0 / 0 | 200 / 0 | 15179147 | False |
| t1_E_stress | 10000128 | 68339 (68338) | 240948 / 3993592 / 0 | 200 / 0 | 3393106 | False |
| t2_cas | 10000000 | stale rounds 0, double winners 0 | GPU payload bad rounds 0 | - | 58220 | False |
| t2_cas_stress | 3000000 | stale rounds 0, double winners 0 | GPU payload bad rounds 0 | - | 107437 | False |
| t2_casacq | 3000000 | stale rounds 0, double winners 0 | GPU payload bad rounds 0 | - | 51574 | False |
| t2_casfence | 10000000 | stale rounds 0, double winners 0 | GPU payload bad rounds 0 | - | 54648 | False |
| t2_control_cas | 1000000 | stale rounds 226096, double winners 0 | GPU payload bad rounds 207196 | - | 42124 | False |
| t2_control_casfence | 1000000 | stale rounds 193, double winners 0 | GPU payload bad rounds 168 | - | 45834 | False |

## Test 1, stress

- verdict: **no violation observed in 1000000128 rounds of A; C failed 465548 times in 200000256 rounds of the same setup: evidence that one leader fence after __syncwarp suffices in this layout (the publisher's own payload stores share instructions with the other lanes'; whether the fence orders stores of lanes that did not fence is decided by A0, lane 0 idle) (95% upper bound on A's rate: 3.00e-09 per round)**
- attribution: the fence's duration: Cd (a fenceless wait of 27948 ns, A's median fence 27776 ns) showed 0 in 200000256 rounds where C-like failures would be 465548.0, so A is 'safe in practice here, not attributable to fence semantics'
- A_rounds: 1000000128
- A_violations: 0
- C_rounds: 200000256
- C_violations: 465548
- expected_A_violations_if_like_C: 2327737.3184469524
- Cd_rounds: 200000256
- Cd_violations: 0
- Cd_mean_delay_ns: 27947.7
- A_fence_ns_first_round: {'n': 2500224, 'p50': 27776, 'p90': 37632, 'p99': 39424, 'overflow': 0}
- A_fence_ns_other_rounds: {'n': 7499904, 'p50': 26368, 'p90': 34560, 'p99': 40576, 'overflow': 0}
- Am_rounds: 10000128
- Am_violations: 0
- Cd15_info: C plus a fixed 1.5 us fenceless wait (informational, not a rule): 3379 violating rounds in 10000128
- rate_A: 3441168.2
- rate_B: 3491058.2

## Test 1, normal

- verdict: **no violation observed in 1000000128 rounds of A; C failed 1776 times in 200000256 rounds of the same setup: evidence that one leader fence after __syncwarp suffices in this layout (the publisher's own payload stores share instructions with the other lanes'; whether the fence orders stores of lanes that did not fence is decided by A0, lane 0 idle) (95% upper bound on A's rate: 3.00e-09 per round)**
- attribution: the fence's duration: Cd (a fenceless wait of 6594 ns, A's median fence 6400 ns) showed 0 in 200000256 rounds where C-like failures would be 1776.0, so A is 'safe in practice here, not attributable to fence semantics'
- A_rounds: 1000000128
- A_violations: 0
- C_rounds: 200000256
- C_violations: 1776
- expected_A_violations_if_like_C: 8879.989770253094
- Cd_rounds: 200000256
- Cd_violations: 0
- Cd_mean_delay_ns: 6593.6
- A_fence_ns_first_round: {'n': 2500224, 'p50': 6400, 'p90': 7232, 'p99': 8064, 'overflow': 0}
- A_fence_ns_other_rounds: {'n': 7499904, 'p50': 5824, 'p90': 7168, 'p99': 8320, 'overflow': 0}
- Am_rounds: 10000128
- Am_violations: 0
- Cd15_info: C plus a fixed 1.5 us fenceless wait (informational, not a rule): 0 violating rounds in 10000128
- rate_A: 16011406.2
- rate_B: 16066858.4

## Test 1, long

- verdict: **no violation observed in 100000128 rounds of A; C failed 1 times in 100000128 rounds of the same setup: consistent with one fence per warp, but A's run had too few rounds to expect 3 C-like failures (95% upper bound on A's rate: 3.00e-08 per round)**
- A_rounds: 100000128
- A_violations: 0
- C_rounds: 100000128
- C_violations: 1
- expected_A_violations_if_like_C: 1.0
- rate_A: 12701222.3

## Test 1, stress, publishing lane writes no payload (A0, C0, Am0mix)

- verdict: **no violation observed in 200000256 rounds of A0 (the publishing lane wrote no payload) while C0 failed 27969 times in 10000128 (95% upper bound 1.50e-08 per round), but not separable from the fence's duration: a fence in a warp without payload took 1.05x as long under the same load, so a fence that ordered nothing of the payload would have hidden the race too**
- A0_rounds: 200000256
- A0_violations: 0
- C0_rounds: 10000128
- C0_violations: 27969
- expected_A0_violations_if_like_C0: 559373.5560248828
- fence_ns_first_round_p50_lane0_idle_warps: 21184
- fence_ns_first_round_p50_warps_without_payload: 22208
- fence_ratio_without_over_with_own_stores: 1.0483383685800605

## Test 2

- control stale rounds: {'cas': 226096, 'casfence': 193} (checker detects: True)
- t2_cas: **no stale read and no double winner in 10000000 rounds (95% bound 3.00e-07 per round); the control shows the check can detect stale data**
- t2_casfence: **no stale read and no double winner in 10000000 rounds (95% bound 3.00e-07 per round); the control shows the check can detect stale data**
- t2_casacq: **no stale read and no double winner in 3000000 rounds (95% bound 1.00e-06 per round); the control shows the check can detect stale data**
- t2_cas_stress: **no stale read and no double winner in 3000000 rounds (95% bound 1.00e-06 per round); the control shows the check can detect stale data**
