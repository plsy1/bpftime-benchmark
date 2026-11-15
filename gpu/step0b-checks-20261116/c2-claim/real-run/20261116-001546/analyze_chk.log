# C2 correctness summary

| group | verdict | per pool |
|---|---|---|
| warp_activemask | incorrect | bmsm: incorrect, bmglob: incorrect, stksm: incorrect, stkglob: incorrect |
| warp_constructed_mask | correct (every pool) | bmsm: correct, bmglob: correct, stksm: correct, stkglob: correct |
| lane | correct (every pool) | lnsm: correct, lnglob: correct, lfsm: correct, lfglob: correct |

## Variants (decision rows)

- **bmsm o_lane (warp_activemask): incorrect**. arbitration at factor 0.5 (retries + claims away from the hint): 1225150; claims 3641439, misses 312146232 (cap 0, empty pool 312146232), violations 41735854
  - incorrect: native/churn f=0.5: 10413712 violations (dbl_claim=135, foreign=8718294, freed_live=1872, lanetag_bad=1306982, bad_rel=135, tag_bad=386294)
  - incorrect: native/churn f=0.5: exit_lanes 1670232 != claim_lanes 218463
  - incorrect: native/churn f=0.5: releases 42927 != claims 43119
  - incorrect: native/churn f=0.5: cap_spin 1452800
  - incorrect: native/churn f=0.5: end state not clean: {"clean": false, "free_slots": 0, "busy_bits": 192, "stack_dups": 0, "stack_wrong_pool": 0, "stack_cycle": 0, "live_nonzero": 192, "ledger_nonzero": 192}
  - incorrect: native/churn f=1: 10443514 violations (dbl_claim=271, foreign=8741412, freed_live=1657, lanetag_bad=1312018, bad_rel=271, tag_bad=387885)
  - incorrect: native/churn f=1: exit_lanes 1829463 != claim_lanes 374507
  - incorrect: native/churn f=1: releases 74772 != claims 75156
  - inconclusive: churn f=0.5: 43119 claims < 90000
  - inconclusive: churn f=1: 75156 claims < 90000
- **bmglob o_lane (warp_activemask): incorrect**. arbitration at factor 0.5 (retries + claims away from the hint): 1353437; claims 2148031, misses 73649417 (cap 0, empty pool 73649417), violations 12154885
  - incorrect: native/churn f=0.5: 11925195 violations (dbl_claim=26, foreign=9337105, freed_live=1419, lanetag_bad=2013699, bad_rel=26, tag_bad=572920)
  - incorrect: native/churn f=0.5: exit_lanes 2081159 != claim_lanes 526032
  - incorrect: native/churn f=0.5: releases 93814 != claims 94006
  - incorrect: native/churn f=0.5: cap_spin 1555736
  - incorrect: native/churn f=0.5: end state not clean: {"clean": false, "free_slots": 0, "busy_bits": 192, "stack_dups": 0, "stack_wrong_pool": 0, "stack_cycle": 0, "live_nonzero": 192, "ledger_nonzero": 192}
  - incorrect: native/churn f=1: 229690 violations (dbl_claim=167, foreign=184793, freed_live=1685, lanetag_bad=33313, bad_rel=167, tag_bad=9565)
  - incorrect: native/churn f=1: exit_lanes 552545 != claim_lanes 522956
  - incorrect: native/churn f=1: releases 100229 != claims 100611
- **stksm o_lane (warp_activemask): incorrect**. arbitration at factor 0.5 (retries + claims away from the hint): 4642672; claims 2080224, misses 160869303 (cap 0, empty pool 160869303), violations 19202231
  - incorrect: native/churn f=0.5: 9565865 violations (dbl_claim=30, foreign=7848449, freed_live=1220, lanetag_bad=1331153, bad_rel=30, tag_bad=384983)
  - incorrect: native/churn f=0.5: exit_lanes 1522366 != claim_lanes 215870
  - incorrect: native/churn f=0.5: releases 41153 != claims 41345
  - incorrect: native/churn f=0.5: cap_spin 1307165
  - incorrect: native/churn f=0.5: end state not clean: {"clean": false, "free_slots": 0, "busy_bits": 0, "stack_dups": 0, "stack_wrong_pool": 0, "stack_cycle": 0, "live_nonzero": 192, "ledger_nonzero": 192}
  - incorrect: native/churn f=1: 9636366 violations (dbl_claim=95, foreign=7900236, freed_live=1027, lanetag_bad=1345726, bad_rel=95, tag_bad=389187)
  - incorrect: native/churn f=1: exit_lanes 1719449 != claim_lanes 404215
  - incorrect: native/churn f=1: releases 81241 != claims 81625
  - inconclusive: churn f=0.5: 41345 claims < 90000
  - inconclusive: churn f=1: 81625 claims < 90000
- **stkglob o_lane (warp_activemask): incorrect**. arbitration at factor 0.5 (retries + claims away from the hint): 87031956; claims 3823054, misses 4313186 (cap 110340, empty pool 4202846), violations 3528668
  - incorrect: native/churn f=0.5: 1155616 violations (dbl_claim=1, foreign=805205, freed_live=21, lanetag_bad=275922, bad_rel=1, tag_bad=74466)
  - incorrect: native/churn f=0.5: exit_lanes 586589 != claim_lanes 452844
  - incorrect: native/churn f=0.5: releases 101868 != claims 102037
  - incorrect: native/churn f=0.5: cap_spin 134073
  - incorrect: native/churn f=0.5: end state not clean: {"clean": false, "free_slots": 23, "busy_bits": 0, "stack_dups": 0, "stack_wrong_pool": 0, "stack_cycle": 0, "live_nonzero": 169, "ledger_nonzero": 169}
  - incorrect: native/churn f=1: 673524 violations (dbl_claim=1, foreign=522569, freed_live=4095, lanetag_bad=113451, bad_rel=1, tag_bad=33407)
  - incorrect: native/churn f=1: exit_lanes 586499 != claim_lanes 499181
  - incorrect: native/churn f=1: releases 109816 != claims 109957
- **bmsm oc_lane (warp_constructed_mask): correct**. arbitration at factor 0.5 (retries + claims away from the hint): 808080; claims 2599370, misses 1495222 (cap 0, empty pool 1495222), violations 0
- **bmglob oc_lane (warp_constructed_mask): correct**. arbitration at factor 0.5 (retries + claims away from the hint): 944309; claims 1550500, misses 754652 (cap 0, empty pool 754652), violations 0
- **stksm oc_lane (warp_constructed_mask): correct**. arbitration at factor 0.5 (retries + claims away from the hint): 3456912; claims 2623336, misses 678680 (cap 12584, empty pool 666096), violations 0
- **stkglob oc_lane (warp_constructed_mask): correct**. arbitration at factor 0.5 (retries + claims away from the hint): 49167369; claims 2627577, misses 758919 (cap 308174, empty pool 450745), violations 0
- **lnsm o_lane (lane): correct**. arbitration at factor 0.5 (retries + claims away from the hint): 10329605; claims 48445261, misses 179 (cap 0, empty pool 179), violations 0
- **lnglob o_lane (lane): correct**. arbitration at factor 0.5 (retries + claims away from the hint): 15427478; claims 32520104, misses 88 (cap 0, empty pool 88), violations 0
- **lfsm o_lane (lane): correct**. arbitration at factor 0.5 (retries + claims away from the hint): 6877341; claims 48445440, misses 0 (cap 0, empty pool 0), violations 0
- **lfglob o_lane (lane): correct**. arbitration at factor 0.5 (retries + claims away from the hint): 7567726; claims 32520192, misses 0 (cap 0, empty pool 0), violations 0

## Reproducer (__activemask + leader work + __shfl_sync)

- native: reproduced: lanes outside their leader's __activemask() at its __shfl_sync, with a loop in the leader-only region (inlined or not); 0 with one atomic; lanes not in their leader's mask {'loop_ni': 253474, 'one_ni': 0, 'loop_inl': 773735} of calls {'loop_ni': 29710069, 'one_ni': 29706691, 'loop_inl': 29709355}
- jit: reproduced: lanes outside their leader's __activemask() at its __shfl_sync, with a loop in the leader-only region (inlined or not); 0 with one atomic; lanes not in their leader's mask {'loop_ni': 255140, 'one_ni': 0, 'loop_inl': 771899} of calls {'loop_ni': 29710069, 'one_ni': 29706691, 'loop_inl': 29709355}

## Claim groups per warp (warp claims, native div; min-max over runs)

- early: 1.00-1.00
- lowlanes: 1.00-1.00
- nested: 1.00-1.00
- loops: 1.00-1.00
- split: 3.00-3.00
- entrydiv: 1.00-1.00
- churn: 76.24-86.77
- churnsync: 16.00-16.00

## Controls

- ABA: ABA provoked without the version tag ([17745098, 148534237]); versioned stacks 0
- oversubscribed full launch hit the barrier cap: True
- first-exit/early release, bmsm o_first: 32 runs, 0 with 0 violations []
- first-exit/early release, bmsm oc_first: 20 runs, 0 with 0 violations []
- first-exit/early release, bmglob o_first: 16 runs, 0 with 0 violations []
- first-exit/early release, bmglob oc_first: 10 runs, 0 with 0 violations []
- first-exit/early release, stksm o_first: 16 runs, 0 with 0 violations []
- first-exit/early release, stksm oc_first: 10 runs, 0 with 0 violations []
- first-exit/early release, stkglob o_first: 32 runs, 0 with 0 violations []
- first-exit/early release, stkglob oc_first: 10 runs, 0 with 0 violations []
- first-exit/early release, lnsm o_first: 32 runs, 0 with 0 violations []
- first-exit/early release, lnglob o_first: 16 runs, 0 with 0 violations []
- first-exit/early release, lfsm o_first: 32 runs, 0 with 0 violations []
- first-exit/early release, lfglob o_first: 16 runs, 0 with 0 violations []

## Informational variants (relaxed ordering, aggregated exit)

- bmsm r_lane: 43 runs, violations 22327090 ['native/churn f=0.5', 'native/churn f=1'], accounting problems 6
- bmsm o_agg: 16 runs, violations 20961362 ['native/churn f=0.5', 'native/churn f=1'], accounting problems 8
- bmglob r_lane: 43 runs, violations 24837714 ['native/churn f=0.5', 'native/churn f=1'], accounting problems 6
- bmglob o_agg: 16 runs, violations 12204166 ['native/churn f=0.5', 'native/churn f=1'], accounting problems 8
- stksm r_lane: 43 runs, violations 20671459 ['native/churn f=0.5', 'native/churn f=1'], accounting problems 6
- stksm o_agg: 16 runs, violations 19293416 ['native/churn f=0.5', 'native/churn f=1'], accounting problems 8
- stkglob r_lane: 43 runs, violations 1269160 ['native/churn f=0.5', 'native/churn f=1'], accounting problems 6
- stkglob o_agg: 16 runs, violations 2160720 ['native/churn f=0.5', 'native/churn f=1'], accounting problems 8
- lnsm r_lane: 43 runs, violations 0 [], accounting problems 0
- lnglob r_lane: 43 runs, violations 0 [], accounting problems 0
- lfsm r_lane: 43 runs, violations 0 [], accounting problems 0
- lfglob r_lane: 43 runs, violations 0 [], accounting problems 0

## Hold without the hog (control)

- bmsm o_lane: violations 0, gaps 0, max gap 0.000 ms
- bmsm r_lane: violations 0, gaps 0, max gap 0.000 ms
- bmsm oc_lane: violations 0, gaps 0, max gap 0.000 ms
- bmglob o_lane: violations 0, gaps 0, max gap 0.000 ms
- bmglob r_lane: violations 0, gaps 0, max gap 0.000 ms
- bmglob oc_lane: violations 0, gaps 0, max gap 0.000 ms
- stksm o_lane: violations 0, gaps 0, max gap 0.000 ms
- stksm r_lane: violations 0, gaps 0, max gap 0.000 ms
- stksm oc_lane: violations 0, gaps 0, max gap 0.000 ms
- stkglob o_lane: violations 0, gaps 0, max gap 0.000 ms
- stkglob r_lane: violations 0, gaps 0, max gap 0.000 ms
- stkglob oc_lane: violations 0, gaps 0, max gap 0.000 ms
- lnsm o_lane: violations 0, gaps 0, max gap 0.000 ms
- lnsm r_lane: violations 0, gaps 0, max gap 0.000 ms
- lnglob o_lane: violations 0, gaps 0, max gap 0.000 ms
- lnglob r_lane: violations 0, gaps 0, max gap 0.000 ms
- lfsm o_lane: violations 0, gaps 0, max gap 0.000 ms
- lfsm r_lane: violations 0, gaps 0, max gap 0.000 ms
- lfglob o_lane: violations 0, gaps 0, max gap 0.000 ms
- lfglob r_lane: violations 0, gaps 0, max gap 0.000 ms
