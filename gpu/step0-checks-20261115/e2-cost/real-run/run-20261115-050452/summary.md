# e2-cost summary (10 rounds; medians, [min-max] over rounds)

Smoke or loaded-board numbers are not measurements; only a run on a quiet board counts.

## (1) Reading of the 0.6 ns figure (shape anchor, R = 1, registered)

- today's 1512 op: 0.433 ns per thread [0.432-0.434], 13.85 ns per warp; llama figure 0.621 ns per thread (ratio 0.70, gate ok)
- loop form / exit form of the reference at R = 1: 1.01 (1.00 expected)
- one 32-byte record per warp by lane 0, with membar.sys: 10.06 ns per warp (23.25x the per-thread op, 0.73x the per-warp op)
- verdict: **reading 2 (a record per warp costs about one warp memory instruction of today's op)**
  - secondary anchor, rec_lane0 reg f1 vs ref1512_loop: reading 2 (a record per warp costs about one warp memory instruction of today's op)
  - secondary anchor, rec_lane0 reg f0 vs ref1512: in between
  - secondary large, rec_lane0 reg f1 vs ref1512: reading 2 (a record per warp costs about one warp memory instruction of today's op)
  - secondary large, rec_lane0 reg f0 vs ref1512: in between
  - secondary full, rec_lane0 reg f1 vs ref1512: reading 2 (a record per warp costs about one warp memory instruction of today's op)
  - secondary full, rec_lane0 reg f0 vs ref1512: in between

## (2) [projection] one record per warp on every kernel of a pp512 (6.02M records, 204.6 ms)

| shape | case | memory | fence | ns/record | overhead % [min-max] |
|---|---|---|---|---:|---:|
| anchor | rec_lane0 | reg | membar.sys | 10.06 | 29.6 [29.5-29.8] |
| anchor | rec_lane0 | reg | none | 1.61 | 4.7 [4.7-4.8] |
| anchor | rec_lane0 | dev | membar.sys | 7.98 | 23.5 [23.4-23.5] |
| anchor | rec_lane0 | dev | none | 1.50 | 4.4 [4.4-4.4] |
| anchor | rec_lane4 | reg | membar.sys | 7.41 | 21.8 [21.7-21.9] |
| anchor | rec_lane4 | reg | none | 1.38 | 4.1 [4.0-4.1] |
| anchor | rec_lane4 | dev | membar.sys | 7.63 | 22.4 [22.4-22.6] |
| anchor | rec_lane4 | dev | none | 1.38 | 4.1 [4.0-4.1] |
| anchor | comp_rec | reg | membar.sys | 211.83 | 623.3 [612.7-646.9] |
| anchor | comp_rec | reg | none | 112.42 | 330.8 [316.5-337.3] |
| anchor | comp_rec | dev | membar.sys | 9.22 | 27.1 [26.9-27.2] |
| anchor | comp_rec | dev | none | 2.61 | 7.7 [7.7-7.7] |
| anchor | ref1512 (one op per thread execution) | reg | membar.sys | 0.433 per thread | 40.7 |
| large | rec_lane0 | reg | membar.sys | 9.72 | 28.6 [28.5-28.7] |
| large | rec_lane0 | reg | none | 1.32 | 3.9 [3.9-3.9] |
| large | rec_lane0 | dev | membar.sys | 7.83 | 23.0 [23.0-23.1] |
| large | rec_lane0 | dev | none | 1.32 | 3.9 [3.9-3.9] |
| large | rec_lane4 | reg | membar.sys | 7.19 | 21.2 [21.1-21.3] |
| large | rec_lane4 | reg | none | 1.22 | 3.6 [3.6-3.6] |
| large | rec_lane4 | dev | membar.sys | 7.22 | 21.2 [21.1-21.4] |
| large | rec_lane4 | dev | none | 1.22 | 3.6 [3.6-3.6] |
| large | comp_rec | reg | membar.sys | 216.83 | 638.0 [631.8-652.7] |
| large | comp_rec | reg | none | 110.11 | 324.0 [312.3-331.1] |
| large | comp_rec | dev | membar.sys | 8.78 | 25.8 [25.7-26.0] |
| large | comp_rec | dev | none | 2.39 | 7.0 [7.0-7.0] |
| large | ref1512 (one op per thread execution) | reg | membar.sys | 0.419 per thread | 39.5 |
| full | rec_lane0 | reg | membar.sys | 11.53 | 33.9 [33.4-34.4] |
| full | rec_lane0 | reg | none | 6.54 | 19.2 [19.1-19.3] |
| full | rec_lane0 | dev | membar.sys | 9.63 | 28.3 [28.1-28.5] |
| full | rec_lane0 | dev | none | 5.57 | 16.4 [16.3-16.4] |
| full | rec_lane4 | reg | membar.sys | 8.80 | 25.9 [25.6-26.5] |
| full | rec_lane4 | reg | none | 5.87 | 17.3 [17.2-17.3] |
| full | rec_lane4 | dev | membar.sys | 9.23 | 27.2 [26.9-27.3] |
| full | rec_lane4 | dev | none | 5.56 | 16.4 [16.3-16.5] |
| full | comp_rec | reg | membar.sys | 130.28 | 383.3 [383.1-383.6] |
| full | comp_rec | reg | none | 129.50 | 381.0 [380.6-381.4] |
| full | comp_rec | dev | membar.sys | 13.26 | 39.0 [38.6-39.5] |
| full | comp_rec | dev | none | 7.10 | 20.9 [20.6-21.2] |
| full | ref1512 (one op per thread execution) | reg | membar.sys | 0.615 per thread | 57.9 |

- anchor: single-digit percent: **reachable only with batched publication (only unfenced registered variants < 10%: rec_lane0 4.7%, rec_lane4 4.1%)**; copy-out from device memory: **not required by this rule**

- large: single-digit percent: **reachable only with batched publication (only unfenced registered variants < 10%: rec_lane0 3.9%, rec_lane4 3.6%)**; copy-out from device memory: **not required by this rule**

- full: single-digit percent: **not reachable in registered memory (every registered record variant >= 10%)**; copy-out from device memory: **not required by this rule**

## (3) registered / device ratio (median deltas; NA when the device delta is < 0.5 us or unresolved)

| case | reg delta us | dev delta us | ratio |
|---|---:|---:|---:|
| anchor|atom_lane|*|f0|R1|k0 | 76.3 | 14.9 | 5.1 |
| anchor|atom_lane|*|f1|R1|k0 | 177.1 | 75.2 | 2.4 |
| anchor|atom_warp_all|*|f0|R1|k0 | 15716.1 | 72.1 | 218.1 |
| anchor|atom_warp_all|*|f1|R1|k0 | 20049.6 | 133.0 | 150.7 |
| anchor|atom_warp_leader_sys|*|f0|R1|k0 | 414.1 | 12.9 | 32.2 |
| anchor|atom_warp_leader_sys|*|f1|R1|k0 | 402.1 | 65.5 | 6.1 |
| anchor|atom_warp_leader|*|f0|R1|k0 | 414.0 | 12.9 | 32.2 |
| anchor|atom_warp_leader|*|f1|R1|k0 | 401.8 | 65.5 | 6.1 |
| anchor|comp_rec|*|f0|R1|k0 | 920.9 | 21.4 | 43.0 |
| anchor|comp_rec|*|f1|R1|k0 | 1735.3 | 75.5 | 23.0 |
| anchor|rec_lane0|*|f0|R1|k0 | 13.2 | 12.3 | 1.1 |
| anchor|rec_lane0|*|f1|R1|k0 | 82.4 | 65.3 | 1.3 |
| anchor|rec_lane4|*|f0|R1|k0 | 11.3 | 11.3 | 1.0 |
| anchor|rec_lane4|*|f1|R1|k0 | 60.7 | 62.5 | 1.0 |
| anchor|rec_thread|*|f0|R1|k0 | 784.6 | 196.1 | 4.0 |
| anchor|rec_thread|*|f1|R1|k0 | 774.4 | 208.7 | 3.7 |
| anchor|red_warp_leader|*|f0|R1|k0 | 144.5 | 7.7 | 18.9 |
| anchor|red_warp_leader|*|f1|R1|k0 | 355.9 | 62.0 | 5.7 |
| anchor|ref1512_loop|*|f1|R1|k0 | 114.4 | 97.1 | 1.2 |
| anchor|ref1512|*|f1|R1|k0 | 113.5 | 96.7 | 1.2 |
| anchor|st_lane1|*|f0|R1|k0 | 8.2 | 8.1 | 1.0 |
| anchor|st_lane1|*|f1|R1|k0 | 57.4 | 57.1 | 1.0 |
| anchor|st_lane32|*|f0|R1|k0 | 17.2 | 8.4 | 2.0 |
| anchor|st_lane32|*|f1|R1|k0 | 67.6 | 67.8 | 1.0 |
| full|atom_lane|*|f0|R1|k0 | 6.0 | 2.4 | 2.5 |
| full|atom_lane|*|f1|R1|k0 | 9.1 | 3.8 | 2.4 |
| full|atom_sm|*|f0|R1|k1 | 2.2 | 2.1 | 1.1 |
| full|atom_sm|*|f0|R1|k16 | 15.7 | 2.1 | 7.4 |
| full|atom_sm|*|f0|R1|k2 | 3.8 | 2.1 | 1.8 |
| full|atom_sm|*|f0|R1|k32 | 25.5 | 2.1 | 11.9 |
| full|atom_sm|*|f0|R1|k4 | 5.9 | 2.1 | 2.8 |
| full|atom_sm|*|f0|R1|k48 | 41.1 | 2.1 | 19.2 |
| full|atom_sm|*|f0|R1|k8 | 10.6 | 2.1 | 5.0 |
| full|atom_sm|*|f1|R1|k1 | 2.8 | 2.3 | 1.2 |
| full|atom_sm|*|f1|R1|k16 | 18.0 | 2.6 | 6.8 |
| full|atom_sm|*|f1|R1|k2 | 4.6 | 2.3 | 2.0 |
| full|atom_sm|*|f1|R1|k32 | 28.5 | 3.8 | 7.5 |
| full|atom_sm|*|f1|R1|k4 | 7.0 | 2.3 | 3.1 |
| full|atom_sm|*|f1|R1|k48 | 38.0 | 3.8 | 9.9 |
| full|atom_sm|*|f1|R1|k8 | 10.3 | 2.3 | 4.5 |
| full|atom_warp_all|*|f0|R1|k0 | 999.9 | 5.5 | 183.0 |
| full|atom_warp_all|*|f1|R1|k0 | 962.7 | 7.0 | 137.6 |
| full|atom_warp_leader_sys|*|f0|R1|k0 | 21.4 | 2.1 | 10.0 |
| full|atom_warp_leader_sys|*|f1|R1|k0 | 22.1 | 3.7 | 5.9 |
| full|atom_warp_leader|*|f0|R1|k0 | 21.4 | 2.1 | 10.0 |
| full|atom_warp_leader|*|f1|R1|k0 | 22.1 | 3.7 | 5.9 |
| full|cas_sm|*|f0|R1|k1 | 3.2 | 2.1 | 1.5 |
| full|cas_sm|*|f0|R1|k16 | 130.8 | 7.0 | 18.7 |
| full|cas_sm|*|f0|R1|k2 | 6.4 | 2.6 | 2.5 |
| full|cas_sm|*|f0|R1|k32 | 403.3 | 12.6 | 32.1 |
| full|cas_sm|*|f0|R1|k4 | 13.2 | 3.2 | 4.1 |
| full|cas_sm|*|f0|R1|k48 | 824.2 | 18.3 | 45.1 |
| full|cas_sm|*|f0|R1|k8 | 42.9 | 4.3 | 10.0 |
| full|cas_sm|*|f1|R1|k1 | 3.6 | 2.8 | 1.3 |
| full|cas_sm|*|f1|R1|k16 | 132.5 | 8.1 | 16.3 |
| full|cas_sm|*|f1|R1|k2 | 6.7 | 3.8 | 1.8 |
| full|cas_sm|*|f1|R1|k32 | 404.9 | 13.6 | 29.9 |
| full|cas_sm|*|f1|R1|k4 | 13.6 | 4.3 | 3.2 |
| full|cas_sm|*|f1|R1|k48 | 828.8 | 19.3 | 43.0 |
| full|cas_sm|*|f1|R1|k8 | 43.2 | 5.5 | 7.9 |
| full|comp_rec|*|f0|R1|k0 | 49.7 | 2.7 | 18.2 |
| full|comp_rec|*|f1|R1|k0 | 50.0 | 5.1 | 9.8 |
| full|rec_lane0|*|f0|R1|k0 | 2.5 | 2.1 | 1.2 |
| full|rec_lane0|*|f1|R1|k0 | 4.4 | 3.7 | 1.2 |
| full|rec_lane4|*|f0|R1|k0 | 2.3 | 2.1 | 1.1 |
| full|rec_lane4|*|f1|R1|k0 | 3.4 | 3.5 | 1.0 |
| full|rec_thread|*|f0|R1|k0 | 37.0 | 10.6 | 3.5 |
| full|rec_thread|*|f1|R1|k0 | 39.7 | 12.7 | 3.1 |
| full|red_warp_leader|*|f0|R1|k0 | 17.1 | 1.9 | 8.8 |
| full|red_warp_leader|*|f1|R1|k0 | 22.3 | 3.5 | 6.4 |
| full|ref1512_loop|*|f1|R1|k0 | 7.6 | 6.6 | 1.1 |
| full|ref1512|*|f1|R1|k0 | 7.6 | 6.6 | 1.2 |
| full|st_lane1|*|f0|R1|k0 | 2.1 | 2.1 | 1.0 |
| full|st_lane1|*|f1|R1|k0 | 3.5 | 3.3 | 1.0 |
| full|st_lane32|*|f0|R1|k0 | 2.6 | 2.1 | 1.2 |
| full|st_lane32|*|f1|R1|k0 | 4.1 | 3.5 | 1.2 |
| large|atom_lane|*|f0|R1|k0 | 1154.2 | 643.4 | 1.8 |
| large|atom_lane|*|f1|R1|k0 | 2793.6 | 2127.2 | 1.3 |
| large|atom_sm|*|f0|R1|k48 | 38483.9 | 233.4 | 164.9 |
| large|atom_sm|*|f1|R1|k48 | 29875.1 | 1036.1 | 28.8 |
| large|atom_warp_all|*|f0|R1|k0 | 246965.5 | 1950.1 | 126.6 |
| large|atom_warp_all|*|f1|R1|k0 | 322348.1 | 3741.3 | 86.2 |
| large|atom_warp_leader_sys|*|f0|R1|k0 | 6249.5 | 270.5 | 23.1 |
| large|atom_warp_leader_sys|*|f1|R1|k0 | 6318.2 | 1163.7 | 5.4 |
| large|atom_warp_leader|*|f0|R1|k0 | 6259.1 | 270.4 | 23.1 |
| large|atom_warp_leader|*|f1|R1|k0 | 6309.8 | 1163.1 | 5.4 |
| large|cas_sm|*|f0|R1|k48 | 673814.3 | 5376.8 | 125.3 |
| large|cas_sm|*|f1|R1|k48 | 648079.7 | 5483.6 | 118.2 |
| large|comp_rec|*|f0|R1|k0 | 14432.3 | 313.6 | 46.0 |
| large|comp_rec|*|f1|R1|k0 | 28420.5 | 1151.4 | 24.7 |
| large|rec_lane0|*|f0|R1|k0 | 172.5 | 172.4 | 1.0 |
| large|rec_lane0|*|f1|R1|k0 | 1274.3 | 1026.7 | 1.2 |
| large|rec_lane4|*|f0|R1|k0 | 159.8 | 160.2 | 1.0 |
| large|rec_lane4|*|f1|R1|k0 | 942.4 | 945.7 | 1.0 |
| large|rec_thread|*|f0|R1|k0 | 12296.3 | 3100.5 | 4.0 |
| large|rec_thread|*|f1|R1|k0 | 12143.2 | 3265.2 | 3.7 |
| large|red_warp_leader|*|f0|R1|k0 | 2324.8 | 99.2 | 23.4 |
| large|red_warp_leader|*|f1|R1|k0 | 5556.9 | 1046.0 | 5.3 |
| large|ref1512_loop|*|f1|R1|k0 | 1773.1 | 2127.8 | 0.8 |
| large|ref1512|*|f1|R1|k0 | 1759.3 | 2123.2 | 0.8 |
| large|st_lane1|*|f0|R1|k0 | 106.4 | 106.5 | 1.0 |
| large|st_lane1|*|f1|R1|k0 | 880.5 | 880.1 | 1.0 |
| large|st_lane32|*|f0|R1|k0 | 252.4 | 253.3 | 1.0 |
| large|st_lane32|*|f1|R1|k0 | 1031.8 | 1045.0 | 1.0 |
| tiny|atom_lane|*|f0|R1|k0 | 1.6 | 1.1 | 1.5 |
| tiny|atom_lane|*|f1|R1|k0 | 2.2 | 1.6 | 1.4 |
| tiny|atom_warp_all|*|f0|R1|k0 | 41.7 | 1.1 | 38.1 |
| tiny|atom_warp_all|*|f1|R1|k0 | 42.3 | 1.7 | 25.0 |
| tiny|atom_warp_leader_sys|*|f0|R1|k0 | 2.1 | 1.1 | 2.0 |
| tiny|atom_warp_leader_sys|*|f1|R1|k0 | 2.7 | 1.6 | 1.6 |
| tiny|atom_warp_leader|*|f0|R1|k0 | 2.1 | 1.1 | 2.0 |
| tiny|atom_warp_leader|*|f1|R1|k0 | 2.7 | 1.6 | 1.7 |
| tiny|comp_rec|*|f0|R1|k0 | 2.5 | 1.4 | 1.8 |
| tiny|comp_rec|*|f1|R1|k0 | 3.5 | 2.3 | 1.6 |
| tiny|rec_lane0|*|f0|R1|k0 | 1.1 | 1.1 | 1.0 |
| tiny|rec_lane0|*|f1|R1|k0 | 1.6 | 1.6 | 1.0 |
| tiny|rec_lane4|*|f0|R1|k0 | 1.1 | 1.1 | 1.0 |
| tiny|rec_lane4|*|f1|R1|k0 | 1.6 | 1.6 | 1.0 |
| tiny|rec_thread|*|f0|R1|k0 | 1.1 | 1.1 | 1.0 |
| tiny|rec_thread|*|f1|R1|k0 | 1.6 | 1.6 | 1.0 |
| tiny|red_warp_leader|*|f0|R1|k0 | 1.6 | 1.1 | 1.5 |
| tiny|red_warp_leader|*|f1|R1|k0 | 2.1 | 1.6 | 1.3 |
| tiny|ref1512_loop|*|f1|R1|k0 | 4.1 | 3.6 | 1.1 |
| tiny|ref1512|*|f1|R1|k0 | 3.8 | 3.3 | 1.1 |
| tiny|st_lane1|*|f0|R1|k0 | 1.1 | 1.1 | 1.0 |
| tiny|st_lane1|*|f1|R1|k0 | 1.6 | 1.6 | 1.0 |
| tiny|st_lane32|*|f0|R1|k0 | 1.1 | 1.1 | 1.0 |
| tiny|st_lane32|*|f1|R1|k0 | 1.6 | 1.6 | 1.0 |

## Atomic contention, full occupancy (24 x 512), R = 1 and R = Rbig, ns per counted op

| case | mem | fence | k | R | ns/op [min-max] | slope ns/op |
|---|---|---|---:|---:|---:|---:|
| atom_sm | dev | 0 | 1 | 1 | 259.1 [256.3-263.3] |  |
| atom_sm | dev | 0 | 2 | 1 | 131.0 [129.5-131.9] |  |
| atom_sm | dev | 0 | 4 | 1 | 65.8 [65.1-66.2] |  |
| atom_sm | dev | 0 | 8 | 1 | 33.0 [32.8-33.2] |  |
| atom_sm | dev | 0 | 16 | 1 | 16.6 [16.6-16.8] |  |
| atom_sm | dev | 0 | 32 | 1 | 8.3 [8.3-8.4] |  |
| atom_sm | dev | 0 | 48 | 1 | 5.6 [5.6-5.6] |  |
| atom_sm | dev | 0 | 1 | 16 | 49.9 [49.7-50.0] | 35.8 |
| atom_sm | dev | 0 | 2 | 16 | 25.0 [25.0-25.1] | 18.0 |
| atom_sm | dev | 0 | 4 | 16 | 12.6 [12.5-12.6] | 9.0 |
| atom_sm | dev | 0 | 8 | 16 | 6.3 [6.3-6.3] | 4.5 |
| atom_sm | dev | 0 | 16 | 16 | 3.3 [3.2-3.3] | 2.4 |
| atom_sm | dev | 0 | 32 | 16 | 1.7 [1.7-1.7] | 1.3 |
| atom_sm | dev | 0 | 48 | 16 | 1.3 [1.3-1.3] | 1.0 |
| atom_sm | dev | 1 | 1 | 1 | 285.4 [283.6-288.9] |  |
| atom_sm | dev | 1 | 2 | 1 | 141.7 [140.2-143.2] |  |
| atom_sm | dev | 1 | 4 | 1 | 70.8 [70.4-71.6] |  |
| atom_sm | dev | 1 | 8 | 1 | 35.7 [35.3-36.3] |  |
| atom_sm | dev | 1 | 16 | 1 | 20.6 [20.5-20.9] |  |
| atom_sm | dev | 1 | 32 | 1 | 14.7 [14.7-14.8] |  |
| atom_sm | dev | 1 | 48 | 1 | 10.0 [9.9-10.0] |  |
| atom_sm | dev | 1 | 1 | 16 | 207.7 [207.1-208.0] | 202.5 |
| atom_sm | dev | 1 | 2 | 16 | 104.6 [104.4-104.8] | 102.2 |
| atom_sm | dev | 1 | 4 | 16 | 52.6 [52.5-52.8] | 51.4 |
| atom_sm | dev | 1 | 8 | 16 | 26.7 [26.6-26.9] | 26.1 |
| atom_sm | dev | 1 | 16 | 16 | 18.0 [18.0-18.0] | 17.8 |
| atom_sm | dev | 1 | 32 | 16 | 9.0 [9.0-9.1] | 8.6 |
| atom_sm | dev | 1 | 48 | 16 | 6.1 [6.1-6.1] | 5.8 |
| atom_sm | reg | 0 | 1 | 1 | 279.0 [276.7-279.7] |  |
| atom_sm | reg | 0 | 2 | 1 | 235.0 [222.0-244.8] |  |
| atom_sm | reg | 0 | 4 | 1 | 185.3 [185.1-185.7] |  |
| atom_sm | reg | 0 | 8 | 1 | 166.3 [166.0-166.5] |  |
| atom_sm | reg | 0 | 16 | 1 | 122.9 [122.6-123.2] |  |
| atom_sm | reg | 0 | 32 | 1 | 99.5 [99.3-99.7] |  |
| atom_sm | reg | 0 | 48 | 1 | 107.0 [106.6-107.2] |  |
| atom_sm | reg | 0 | 1 | 16 | 105.8 [105.3-106.1] | 94.3 |
| atom_sm | reg | 0 | 2 | 16 | 89.2 [88.4-89.8] | 79.4 |
| atom_sm | reg | 0 | 4 | 16 | 87.5 [87.4-87.6] | 81.0 |
| atom_sm | reg | 0 | 8 | 16 | 89.2 [89.0-89.3] | 84.0 |
| atom_sm | reg | 0 | 16 | 16 | 84.6 [84.5-84.7] | 82.0 |
| atom_sm | reg | 0 | 32 | 16 | 82.2 [82.1-82.3] | 81.0 |
| atom_sm | reg | 0 | 48 | 16 | 81.7 [81.7-81.9] | 80.0 |
| atom_sm | reg | 1 | 1 | 1 | 344.5 [342.9-346.0] |  |
| atom_sm | reg | 1 | 2 | 1 | 284.6 [281.5-305.9] |  |
| atom_sm | reg | 1 | 4 | 1 | 218.7 [218.3-219.3] |  |
| atom_sm | reg | 1 | 8 | 1 | 160.2 [159.8-160.6] |  |
| atom_sm | reg | 1 | 16 | 1 | 140.5 [140.0-140.7] |  |
| atom_sm | reg | 1 | 32 | 1 | 111.2 [110.8-111.8] |  |
| atom_sm | reg | 1 | 48 | 1 | 99.0 [98.5-99.4] |  |
| atom_sm | reg | 1 | 1 | 16 | 308.6 [307.7-309.2] | 306.1 |
| atom_sm | reg | 1 | 2 | 16 | 179.4 [178.5-180.6] | 172.1 |
| atom_sm | reg | 1 | 4 | 16 | 192.8 [189.4-197.2] | 191.0 |
| atom_sm | reg | 1 | 8 | 16 | 179.5 [176.8-185.8] | 180.8 |
| atom_sm | reg | 1 | 16 | 16 | 191.6 [180.5-197.4] | 195.0 |
| atom_sm | reg | 1 | 32 | 16 | 199.0 [187.1-211.0] | 204.8 |
| atom_sm | reg | 1 | 48 | 16 | 202.9 [188.7-225.8] | 209.8 |
| cas_sm | dev | 0 | 1 | 1 | 266.6 [265.4-267.4] |  |
| cas_sm | dev | 0 | 2 | 1 | 164.2 [163.3-165.1] |  |
| cas_sm | dev | 0 | 4 | 1 | 101.2 [100.7-101.3] |  |
| cas_sm | dev | 0 | 8 | 1 | 67.2 [67.0-67.5] |  |
| cas_sm | dev | 0 | 16 | 1 | 54.6 [54.5-55.2] |  |
| cas_sm | dev | 0 | 32 | 1 | 49.0 [48.9-49.2] |  |
| cas_sm | dev | 0 | 48 | 1 | 47.5 [47.5-47.6] |  |
| cas_sm | dev | 0 | 1 | 16 | 80.3 [80.1-80.5] | 67.9 |
| cas_sm | dev | 0 | 2 | 16 | 60.6 [60.6-60.7] | 53.7 |
| cas_sm | dev | 0 | 4 | 16 | 50.2 [50.2-50.2] | 46.8 |
| cas_sm | dev | 0 | 8 | 16 | 45.4 [45.3-45.4] | 43.9 |
| cas_sm | dev | 0 | 16 | 16 | 43.1 [43.1-43.1] | 42.3 |
| cas_sm | dev | 0 | 32 | 16 | 42.9 [42.8-42.9] | 42.5 |
| cas_sm | dev | 0 | 48 | 16 | 43.8 [43.8-43.9] | 43.6 |
| cas_sm | dev | 1 | 1 | 1 | 344.9 [343.0-346.2] |  |
| cas_sm | dev | 1 | 2 | 1 | 239.2 [238.0-240.2] |  |
| cas_sm | dev | 1 | 4 | 1 | 134.7 [133.9-135.7] |  |
| cas_sm | dev | 1 | 8 | 1 | 85.2 [84.9-85.4] |  |
| cas_sm | dev | 1 | 16 | 1 | 63.4 [63.2-63.5] |  |
| cas_sm | dev | 1 | 32 | 1 | 52.9 [52.8-53.1] |  |
| cas_sm | dev | 1 | 48 | 1 | 50.2 [50.1-50.3] |  |
| cas_sm | dev | 1 | 1 | 16 | 271.1 [270.2-271.8] | 266.2 |
| cas_sm | dev | 1 | 2 | 16 | 140.4 [140.3-140.5] | 133.8 |
| cas_sm | dev | 1 | 4 | 16 | 92.3 [86.8-92.9] | 89.5 |
| cas_sm | dev | 1 | 8 | 16 | 49.3 [49.2-49.4] | 46.9 |
| cas_sm | dev | 1 | 16 | 16 | 45.0 [44.8-45.1] | 43.8 |
| cas_sm | dev | 1 | 32 | 16 | 44.3 [44.2-44.4] | 43.7 |
| cas_sm | dev | 1 | 48 | 16 | 44.5 [44.5-44.7] | 44.2 |
| cas_sm | reg | 0 | 1 | 1 | 402.6 [398.6-404.6] |  |
| cas_sm | reg | 0 | 2 | 1 | 402.7 [386.4-429.6] |  |
| cas_sm | reg | 0 | 4 | 1 | 412.2 [402.2-416.3] |  |
| cas_sm | reg | 0 | 8 | 1 | 670.2 [656.5-672.2] |  |
| cas_sm | reg | 0 | 16 | 1 | 1022.2 [1009.5-1032.5] |  |
| cas_sm | reg | 0 | 32 | 1 | 1575.3 [1562.0-1583.3] |  |
| cas_sm | reg | 0 | 48 | 1 | 2146.4 [2140.6-2150.7] |  |
| cas_sm | reg | 0 | 1 | 16 | 193.4 [192.8-193.9] | 179.4 |
| cas_sm | reg | 0 | 2 | 16 | 232.0 [231.4-233.0] | 220.8 |
| cas_sm | reg | 0 | 4 | 16 | 386.6 [384.6-387.2] | 385.1 |
| cas_sm | reg | 0 | 8 | 16 | 703.8 [702.5-710.5] | 706.0 |
| cas_sm | reg | 0 | 16 | 16 | 1331.1 [1327.1-1340.4] | 1351.9 |
| cas_sm | reg | 0 | 32 | 16 | 2570.5 [2565.9-2573.4] | 2637.4 |
| cas_sm | reg | 0 | 48 | 16 | 3808.5 [3800.4-3817.1] | 3919.3 |
| cas_sm | reg | 1 | 1 | 1 | 449.9 [448.3-452.5] |  |
| cas_sm | reg | 1 | 2 | 1 | 421.1 [395.2-434.9] |  |
| cas_sm | reg | 1 | 4 | 1 | 426.5 [420.1-430.6] |  |
| cas_sm | reg | 1 | 8 | 1 | 675.4 [662.6-680.3] |  |
| cas_sm | reg | 1 | 16 | 1 | 1035.1 [1028.8-1038.6] |  |
| cas_sm | reg | 1 | 32 | 1 | 1581.5 [1571.6-1584.2] |  |
| cas_sm | reg | 1 | 48 | 1 | 2158.4 [2149.5-2167.3] |  |
| cas_sm | reg | 1 | 1 | 16 | 389.7 [388.9-390.5] | 385.6 |
| cas_sm | reg | 1 | 2 | 16 | 218.9 [218.0-220.1] | 205.5 |
| cas_sm | reg | 1 | 4 | 16 | 303.6 [301.4-304.2] | 295.2 |
| cas_sm | reg | 1 | 8 | 16 | 624.8 [614.3-631.5] | 621.4 |
| cas_sm | reg | 1 | 16 | 16 | 1378.6 [1329.2-1402.0] | 1401.5 |
| cas_sm | reg | 1 | 32 | 16 | 2861.7 [2795.1-2945.6] | 2947.0 |
| cas_sm | reg | 1 | 48 | 16 | 4377.7 [4150.8-4497.8] | 4525.5 |

## Sanity

```
{
 "anchor": {
  "ref_empty_minus_empty_us": 0.05119999999999969,
  "ref_loop_over_exit_reg": 1.0081817324580535,
  "ref_loop_over_exit_dev": 1.0045708807128897
 },
 "full": {
  "ref_empty_minus_empty_us": 0.5284500000000001,
  "ref_loop_over_exit_reg": 1.009873489713186,
  "ref_loop_over_exit_dev": 1.0136157221904216
 },
 "large": {
  "ref_empty_minus_empty_us": 0.2744000000000071,
  "ref_loop_over_exit_reg": 1.0078548221357386,
  "ref_loop_over_exit_dev": 1.0021657091166032
 },
 "tiny": {
  "ref_empty_minus_empty_us": 0.10914999999999986,
  "ref_loop_over_exit_reg": 1.0758015952670223,
  "ref_loop_over_exit_dev": 1.0871204188481676
 },
 "hold_capped_rows_excluded": 0,
 "hold_capped_rows_first": []
}
```

validation failures: 0 []
