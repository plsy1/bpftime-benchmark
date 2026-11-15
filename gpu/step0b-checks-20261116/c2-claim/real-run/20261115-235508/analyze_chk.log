# C2 correctness summary

| group | verdict | per pool |
|---|---|---|
| warp_activemask | incorrect | bmsm: incorrect, bmglob: incorrect, stksm: incorrect, stkglob: incorrect |
| warp_constructed_mask | correct (every pool) | bmsm: correct, bmglob: correct, stksm: correct, stkglob: correct |
| lane | correct (every pool) | lnsm: correct, lnglob: correct, lfsm: correct, lfglob: correct |

## Variants (decision rows)

- **bmsm o_lane (warp_activemask): incorrect**. arbitration at factor 0.5 (retries + claims away from the hint): 1232549; claims 3647086, misses 312123997 (cap 0, empty pool 312123997), violations 41787194
  - incorrect: native/churn f=0.5: 10424461 violations (dbl_claim=94, foreign=8737464, freed_live=1646, lanetag_bad=1300122, bad_rel=94, tag_bad=385041)
  - incorrect: native/churn f=0.5: exit_lanes 1690607 != claim_lanes 236151
  - incorrect: native/churn f=0.5: releases 46086 != claims 46278
  - incorrect: native/churn f=0.5: cap_spin 1455513
  - incorrect: native/churn f=0.5: end state not clean: {"clean": false, "free_slots": 0, "busy_bits": 192, "stack_dups": 0, "stack_wrong_pool": 0, "stack_cycle": 0, "live_nonzero": 192, "ledger_nonzero": 192}
  - incorrect: native/churn f=1: 10448530 violations (dbl_claim=214, foreign=8746752, freed_live=1415, lanetag_bad=1311821, bad_rel=214, tag_bad=388114)
  - incorrect: native/churn f=1: exit_lanes 1831667 != claim_lanes 375243
  - incorrect: native/churn f=1: releases 75573 != claims 75957
  - inconclusive: churn f=0.5: 46278 claims < 90000
  - inconclusive: churn f=1: 75957 claims < 90000
- **bmglob o_lane (warp_activemask): incorrect**. arbitration at factor 0.5 (retries + claims away from the hint): 1349745; claims 2139390, misses 74273181 (cap 0, empty pool 74273181), violations 12281927
  - incorrect: native/churn f=0.5: 11959868 violations (dbl_claim=63, foreign=9361014, freed_live=1913, lanetag_bad=2022413, bad_rel=63, tag_bad=574402)
  - incorrect: native/churn f=0.5: exit_lanes 2045389 != claim_lanes 486734
  - incorrect: native/churn f=0.5: releases 86126 != claims 86318
  - incorrect: native/churn f=0.5: cap_spin 1559419
  - incorrect: native/churn f=0.5: end state not clean: {"clean": false, "free_slots": 0, "busy_bits": 192, "stack_dups": 0, "stack_wrong_pool": 0, "stack_cycle": 0, "live_nonzero": 192, "ledger_nonzero": 192}
  - incorrect: native/churn f=1: 322059 violations (dbl_claim=196, foreign=259567, freed_live=1837, lanetag_bad=46718, bad_rel=196, tag_bad=13545)
  - incorrect: native/churn f=1: exit_lanes 559656 != claim_lanes 517684
  - incorrect: native/churn f=1: releases 99656 != claims 100038
  - inconclusive: churn f=0.5: 86318 claims < 90000
- **stksm o_lane (warp_activemask): incorrect**. arbitration at factor 0.5 (retries + claims away from the hint): 4691979; claims 2085805, misses 160870728 (cap 0, empty pool 160870728), violations 19183343
  - incorrect: native/churn f=0.5: 9542995 violations (dbl_claim=26, foreign=7842197, freed_live=1127, lanetag_bad=1318349, bad_rel=26, tag_bad=381270)
  - incorrect: native/churn f=0.5: exit_lanes 1534260 != claim_lanes 227670
  - incorrect: native/churn f=0.5: releases 43553 != claims 43745
  - incorrect: native/churn f=0.5: cap_spin 1307238
  - incorrect: native/churn f=0.5: end state not clean: {"clean": false, "free_slots": 0, "busy_bits": 0, "stack_dups": 0, "stack_wrong_pool": 0, "stack_cycle": 0, "live_nonzero": 192, "ledger_nonzero": 192}
  - incorrect: native/churn f=1: 9640348 violations (dbl_claim=65, foreign=7907644, freed_live=1318, lanetag_bad=1343002, bad_rel=65, tag_bad=388254)
  - incorrect: native/churn f=1: exit_lanes 1724113 != claim_lanes 408572
  - incorrect: native/churn f=1: releases 82486 != claims 82870
  - inconclusive: churn f=0.5: 43745 claims < 90000
  - inconclusive: churn f=1: 82870 claims < 90000
- **stkglob o_lane (warp_activemask): incorrect**. arbitration at factor 0.5 (retries + claims away from the hint): 86480221; claims 3828402, misses 4757222 (cap 109375, empty pool 4647847), violations 3999791
  - incorrect: native/churn f=0.5: 1156353 violations (dbl_claim=1, foreign=800742, freed_live=45, lanetag_bad=280080, bad_rel=1, tag_bad=75484)
  - incorrect: native/churn f=0.5: exit_lanes 564999 != claim_lanes 432009
  - incorrect: native/churn f=0.5: releases 101004 != claims 101174
  - incorrect: native/churn f=0.5: cap_spin 133372
  - incorrect: native/churn f=0.5: end state not clean: {"clean": false, "free_slots": 22, "busy_bits": 0, "stack_dups": 0, "stack_wrong_pool": 0, "stack_cycle": 0, "live_nonzero": 170, "ledger_nonzero": 170}
  - incorrect: native/churn f=1: 690077 violations (foreign=521457, freed_live=15147, lanetag_bad=118975, tag_bad=34498)
  - incorrect: native/churn f=1: exit_lanes 599111 != claim_lanes 509877
  - incorrect: native/churn f=1: releases 110685 != claims 110820
- **bmsm oc_lane (warp_constructed_mask): correct**. arbitration at factor 0.5 (retries + claims away from the hint): 810591; claims 2599493, misses 1493563 (cap 0, empty pool 1493563), violations 0
- **bmglob oc_lane (warp_constructed_mask): correct**. arbitration at factor 0.5 (retries + claims away from the hint): 969094; claims 1551905, misses 754783 (cap 0, empty pool 754783), violations 0
- **stksm oc_lane (warp_constructed_mask): correct**. arbitration at factor 0.5 (retries + claims away from the hint): 3405329; claims 2621850, misses 681702 (cap 12792, empty pool 668910), violations 0
- **stkglob oc_lane (warp_constructed_mask): correct**. arbitration at factor 0.5 (retries + claims away from the hint): 48260188; claims 2612574, misses 749346 (cap 315064, empty pool 434282), violations 0
- **lnsm o_lane (lane): correct**. arbitration at factor 0.5 (retries + claims away from the hint): 10365138; claims 48445256, misses 184 (cap 0, empty pool 184), violations 0
- **lnglob o_lane (lane): correct**. arbitration at factor 0.5 (retries + claims away from the hint): 15475741; claims 32520102, misses 90 (cap 0, empty pool 90), violations 0
- **lfsm o_lane (lane): correct**. arbitration at factor 0.5 (retries + claims away from the hint): 6858373; claims 48445440, misses 0 (cap 0, empty pool 0), violations 0
- **lfglob o_lane (lane): correct**. arbitration at factor 0.5 (retries + claims away from the hint): 7566828; claims 32520192, misses 0 (cap 0, empty pool 0), violations 0

## Reproducer (__activemask + leader work + __shfl_sync)

- native: reproduced: lanes outside their leader's __activemask() at its __shfl_sync, with a loop in the leader-only region (inlined or not); 0 with one atomic; lanes not in their leader's mask {'loop_ni': 254547, 'one_ni': 0, 'loop_inl': 772530} of calls {'loop_ni': 29710069, 'one_ni': 29706691, 'loop_inl': 29709355}
- jit: reproduced: lanes outside their leader's __activemask() at its __shfl_sync, with a loop in the leader-only region (inlined or not); 0 with one atomic; lanes not in their leader's mask {'loop_ni': 253822, 'one_ni': 0, 'loop_inl': 772653} of calls {'loop_ni': 29710069, 'one_ni': 29706691, 'loop_inl': 29709355}

## Claim groups per warp (warp claims, native div; min-max over runs)

- early: 1.00-1.00
- lowlanes: 1.00-1.00
- nested: 1.00-1.00
- loops: 1.00-1.00
- split: 3.00-3.00
- entrydiv: 1.00-1.00
- churn: 76.60-86.77
- churnsync: 16.00-16.00

## Controls

- ABA: ABA not provoked in both no-version stacks ([0, 138978813]): the version tag's necessity is not shown
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

- bmsm r_lane: 43 runs, violations 22356299 ['native/churn f=0.5', 'native/churn f=1'], accounting problems 6
- bmsm o_agg: 16 runs, violations 20942486 ['native/churn f=0.5', 'native/churn f=1'], accounting problems 8
- bmglob r_lane: 43 runs, violations 13124112 ['native/churn f=0.5', 'native/churn f=1'], accounting problems 6
- bmglob o_agg: 16 runs, violations 12158088 ['native/churn f=0.5', 'native/churn f=1'], accounting problems 8
- stksm r_lane: 43 runs, violations 20567227 ['native/churn f=0.5', 'native/churn f=1'], accounting problems 6
- stksm o_agg: 16 runs, violations 19239070 ['native/churn f=0.5', 'native/churn f=1'], accounting problems 8
- stkglob r_lane: 43 runs, violations 1100254 ['native/churn f=0.5', 'native/churn f=1'], accounting problems 6
- stkglob o_agg: 16 runs, violations 1937547 ['native/churn f=0.5', 'native/churn f=1'], accounting problems 8
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
