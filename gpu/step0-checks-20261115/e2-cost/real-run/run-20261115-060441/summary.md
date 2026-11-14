# e2-cost summary (10 rounds; medians, [min-max] over rounds)

Smoke or loaded-board numbers are not measurements; only a run on a quiet board counts.

## (1) Reading of the 0.6 ns figure (shape anchor, R = 1, registered)

- today's 1512 op: 0.431 ns per thread [0.430-0.432], 13.79 ns per warp; llama figure 0.621 ns per thread (ratio 0.69, gate ok)
- loop form / exit form of the reference at R = 1: 1.01 (1.00 expected)
- one 32-byte record per warp by lane 0, with membar.sys: 9.91 ns per warp (22.98x the per-thread op, 0.72x the per-warp op)
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
| anchor | rec_lane0 | reg | membar.sys | 9.91 | 29.1 [29.1-29.3] |
| anchor | rec_lane0 | reg | none | 1.52 | 4.5 [4.5-4.5] |
| anchor | rec_lane0 | dev | membar.sys | 7.94 | 23.4 [23.3-23.4] |
| anchor | rec_lane0 | dev | none | 1.50 | 4.4 [4.4-4.4] |
| anchor | rec_lane4 | reg | membar.sys | 7.36 | 21.7 [21.5-21.8] |
| anchor | rec_lane4 | reg | none | 1.37 | 4.0 [4.0-4.1] |
| anchor | rec_lane4 | dev | membar.sys | 7.75 | 22.8 [22.7-22.9] |
| anchor | rec_lane4 | dev | none | 1.37 | 4.0 [4.0-4.1] |
| anchor | comp_rec | reg | membar.sys | 218.93 | 644.2 [616.2-658.1] |
| anchor | comp_rec | reg | none | 112.58 | 331.3 [318.9-337.8] |
| anchor | comp_rec | dev | membar.sys | 9.15 | 26.9 [26.7-27.0] |
| anchor | comp_rec | dev | none | 2.62 | 7.7 [7.7-7.7] |
| anchor | ref1512 (one op per thread execution) | reg | membar.sys | 0.431 per thread | 40.6 |
| large | rec_lane0 | reg | membar.sys | 9.64 | 28.4 [28.3-28.5] |
| large | rec_lane0 | reg | none | 1.32 | 3.9 [3.9-3.9] |
| large | rec_lane0 | dev | membar.sys | 7.80 | 23.0 [22.9-23.0] |
| large | rec_lane0 | dev | none | 1.31 | 3.9 [3.9-3.9] |
| large | rec_lane4 | reg | membar.sys | 7.16 | 21.1 [20.8-21.2] |
| large | rec_lane4 | reg | none | 1.22 | 3.6 [3.6-3.6] |
| large | rec_lane4 | dev | membar.sys | 7.21 | 21.2 [20.9-21.4] |
| large | rec_lane4 | dev | none | 1.22 | 3.6 [3.6-3.6] |
| large | comp_rec | reg | membar.sys | 221.94 | 653.0 [644.7-668.5] |
| large | comp_rec | reg | none | 110.55 | 325.3 [314.0-330.1] |
| large | comp_rec | dev | membar.sys | 8.71 | 25.6 [25.6-25.8] |
| large | comp_rec | dev | none | 2.39 | 7.0 [7.0-7.0] |
| large | ref1512 (one op per thread execution) | reg | membar.sys | 0.418 per thread | 39.3 |
| full | rec_lane0 | reg | membar.sys | 12.91 | 38.0 [37.7-38.1] |
| full | rec_lane0 | reg | none | 6.30 | 18.5 [18.5-18.8] |
| full | rec_lane0 | dev | membar.sys | 9.32 | 27.4 [27.2-27.8] |
| full | rec_lane0 | dev | none | 5.56 | 16.4 [16.3-16.4] |
| full | rec_lane4 | reg | membar.sys | 8.56 | 25.2 [25.1-25.3] |
| full | rec_lane4 | reg | none | 5.69 | 16.8 [16.7-16.8] |
| full | rec_lane4 | dev | membar.sys | 8.92 | 26.2 [26.0-26.5] |
| full | rec_lane4 | dev | none | 5.55 | 16.3 [16.3-16.4] |
| full | comp_rec | reg | membar.sys | 131.30 | 386.3 [385.7-386.8] |
| full | comp_rec | reg | none | 130.36 | 383.6 [383.1-383.8] |
| full | comp_rec | dev | membar.sys | 13.15 | 38.7 [38.1-39.4] |
| full | comp_rec | dev | none | 7.08 | 20.8 [20.7-21.2] |
| full | ref1512 (one op per thread execution) | reg | membar.sys | 0.623 per thread | 58.6 |

- anchor: single-digit percent: **reachable only with batched publication (only unfenced registered variants < 10%: rec_lane0 4.5%, rec_lane4 4.0%)**; copy-out from device memory: **not required by this rule**

- large: single-digit percent: **reachable only with batched publication (only unfenced registered variants < 10%: rec_lane0 3.9%, rec_lane4 3.6%)**; copy-out from device memory: **not required by this rule**

- full: single-digit percent: **not reachable in registered memory (every registered record variant >= 10%)**; copy-out from device memory: **not required by this rule**

## (3) registered / device ratio (median deltas; NA when the device delta is < 0.5 us or unresolved)

| case | reg delta us | dev delta us | ratio |
|---|---:|---:|---:|
| anchor|atom_lane|*|f0|R1|k0 | 74.9 | 14.8 | 5.1 |
| anchor|atom_lane|*|f1|R1|k0 | 176.4 | 73.9 | 2.4 |
| anchor|atom_warp_all|*|f0|R1|k0 | 15680.8 | 72.1 | 217.5 |
| anchor|atom_warp_all|*|f1|R1|k0 | 20072.5 | 131.7 | 152.4 |
| anchor|atom_warp_leader_sys|*|f0|R1|k0 | 410.1 | 12.8 | 31.9 |
| anchor|atom_warp_leader_sys|*|f1|R1|k0 | 397.0 | 65.2 | 6.1 |
| anchor|atom_warp_leader|*|f0|R1|k0 | 409.9 | 12.8 | 31.9 |
| anchor|atom_warp_leader|*|f1|R1|k0 | 398.1 | 65.2 | 6.1 |
| anchor|comp_rec|*|f0|R1|k0 | 922.3 | 21.4 | 43.0 |
| anchor|comp_rec|*|f1|R1|k0 | 1793.4 | 74.9 | 23.9 |
| anchor|rec_lane0|*|f0|R1|k0 | 12.4 | 12.3 | 1.0 |
| anchor|rec_lane0|*|f1|R1|k0 | 81.2 | 65.0 | 1.2 |
| anchor|rec_lane4|*|f0|R1|k0 | 11.3 | 11.2 | 1.0 |
| anchor|rec_lane4|*|f1|R1|k0 | 60.3 | 63.5 | 0.9 |
| anchor|rec_thread|*|f0|R1|k0 | 783.0 | 196.2 | 4.0 |
| anchor|rec_thread|*|f1|R1|k0 | 775.1 | 208.9 | 3.7 |
| anchor|red_warp_leader|*|f0|R1|k0 | 144.2 | 7.6 | 18.9 |
| anchor|red_warp_leader|*|f1|R1|k0 | 353.0 | 63.2 | 5.6 |
| anchor|ref1512_loop|*|f1|R1|k0 | 113.9 | 96.8 | 1.2 |
| anchor|ref1512|*|f1|R1|k0 | 113.0 | 96.4 | 1.2 |
| anchor|st_lane1|*|f0|R1|k0 | 8.2 | 8.1 | 1.0 |
| anchor|st_lane1|*|f1|R1|k0 | 56.4 | 58.2 | 1.0 |
| anchor|st_lane32|*|f0|R1|k0 | 17.0 | 8.4 | 2.0 |
| anchor|st_lane32|*|f1|R1|k0 | 67.5 | 66.5 | 1.0 |
| full|atom_lane|*|f0|R1|k0 | 6.1 | 2.3 | 2.6 |
| full|atom_lane|*|f1|R1|k0 | 9.2 | 3.8 | 2.5 |
| full|atom_sm|*|f0|R1|k1 | 2.2 | 1.9 | 1.1 |
| full|atom_sm|*|f0|R1|k16 | 15.9 | 2.1 | 7.5 |
| full|atom_sm|*|f0|R1|k2 | 3.8 | 2.0 | 1.9 |
| full|atom_sm|*|f0|R1|k32 | 26.4 | 2.1 | 12.4 |
| full|atom_sm|*|f0|R1|k4 | 5.9 | 2.1 | 2.9 |
| full|atom_sm|*|f0|R1|k48 | 40.5 | 2.1 | 19.0 |
| full|atom_sm|*|f0|R1|k8 | 10.2 | 2.1 | 4.8 |
| full|atom_sm|*|f1|R1|k1 | 2.7 | 2.2 | 1.2 |
| full|atom_sm|*|f1|R1|k16 | 17.7 | 2.6 | 6.9 |
| full|atom_sm|*|f1|R1|k2 | 4.7 | 2.2 | 2.1 |
| full|atom_sm|*|f1|R1|k32 | 28.0 | 3.8 | 7.4 |
| full|atom_sm|*|f1|R1|k4 | 6.7 | 2.2 | 3.1 |
| full|atom_sm|*|f1|R1|k48 | 39.3 | 3.8 | 10.3 |
| full|atom_sm|*|f1|R1|k8 | 10.3 | 2.2 | 4.7 |
| full|atom_warp_all|*|f0|R1|k0 | 1070.0 | 5.4 | 196.8 |
| full|atom_warp_all|*|f1|R1|k0 | 1062.9 | 6.9 | 153.5 |
| full|atom_warp_leader_sys|*|f0|R1|k0 | 22.3 | 2.1 | 10.5 |
| full|atom_warp_leader_sys|*|f1|R1|k0 | 22.6 | 3.7 | 6.1 |
| full|atom_warp_leader|*|f0|R1|k0 | 22.3 | 2.1 | 10.5 |
| full|atom_warp_leader|*|f1|R1|k0 | 22.6 | 3.7 | 6.1 |
| full|cas_sm|*|f0|R1|k1 | 3.2 | 2.1 | 1.5 |
| full|cas_sm|*|f0|R1|k16 | 124.5 | 7.0 | 17.9 |
| full|cas_sm|*|f0|R1|k2 | 6.4 | 2.6 | 2.4 |
| full|cas_sm|*|f0|R1|k32 | 395.1 | 12.6 | 31.5 |
| full|cas_sm|*|f0|R1|k4 | 13.9 | 3.2 | 4.3 |
| full|cas_sm|*|f0|R1|k48 | 826.5 | 18.2 | 45.4 |
| full|cas_sm|*|f0|R1|k8 | 42.0 | 4.3 | 9.8 |
| full|cas_sm|*|f1|R1|k1 | 3.6 | 2.7 | 1.3 |
| full|cas_sm|*|f1|R1|k16 | 125.4 | 8.1 | 15.5 |
| full|cas_sm|*|f1|R1|k2 | 6.8 | 3.8 | 1.8 |
| full|cas_sm|*|f1|R1|k32 | 396.2 | 13.5 | 29.4 |
| full|cas_sm|*|f1|R1|k4 | 14.4 | 4.3 | 3.4 |
| full|cas_sm|*|f1|R1|k48 | 827.8 | 19.2 | 43.1 |
| full|cas_sm|*|f1|R1|k8 | 42.4 | 5.4 | 7.8 |
| full|comp_rec|*|f0|R1|k0 | 50.1 | 2.7 | 18.4 |
| full|comp_rec|*|f1|R1|k0 | 50.4 | 5.0 | 10.0 |
| full|rec_lane0|*|f0|R1|k0 | 2.4 | 2.1 | 1.1 |
| full|rec_lane0|*|f1|R1|k0 | 5.0 | 3.6 | 1.4 |
| full|rec_lane4|*|f0|R1|k0 | 2.2 | 2.1 | 1.0 |
| full|rec_lane4|*|f1|R1|k0 | 3.3 | 3.4 | 1.0 |
| full|rec_thread|*|f0|R1|k0 | 35.2 | 10.7 | 3.3 |
| full|rec_thread|*|f1|R1|k0 | 37.6 | 12.8 | 3.0 |
| full|red_warp_leader|*|f0|R1|k0 | 17.3 | 1.7 | 10.2 |
| full|red_warp_leader|*|f1|R1|k0 | 21.2 | 3.4 | 6.2 |
| full|ref1512_loop|*|f1|R1|k0 | 7.7 | 6.6 | 1.2 |
| full|ref1512|*|f1|R1|k0 | 7.7 | 6.5 | 1.2 |
| full|st_lane1|*|f0|R1|k0 | 2.1 | 2.1 | 1.0 |
| full|st_lane1|*|f1|R1|k0 | 3.3 | 3.3 | 1.0 |
| full|st_lane32|*|f0|R1|k0 | 2.7 | 2.1 | 1.3 |
| full|st_lane32|*|f1|R1|k0 | 4.1 | 3.3 | 1.2 |
| large|atom_lane|*|f0|R1|k0 | 1154.5 | 645.8 | 1.8 |
| large|atom_lane|*|f1|R1|k0 | 2786.2 | 2122.0 | 1.3 |
| large|atom_sm|*|f0|R1|k48 | 38793.4 | 233.3 | 166.3 |
| large|atom_sm|*|f1|R1|k48 | 30000.4 | 1015.7 | 29.5 |
| large|atom_warp_all|*|f0|R1|k0 | 246490.5 | 1561.6 | 157.8 |
| large|atom_warp_all|*|f1|R1|k0 | 321662.9 | 3254.2 | 98.8 |
| large|atom_warp_leader_sys|*|f0|R1|k0 | 6204.1 | 230.5 | 26.9 |
| large|atom_warp_leader_sys|*|f1|R1|k0 | 6292.0 | 1119.2 | 5.6 |
| large|atom_warp_leader|*|f0|R1|k0 | 6231.1 | 230.5 | 27.0 |
| large|atom_warp_leader|*|f1|R1|k0 | 6284.9 | 1117.5 | 5.6 |
| large|cas_sm|*|f0|R1|k48 | 668179.4 | 5375.7 | 124.3 |
| large|cas_sm|*|f1|R1|k48 | 656538.4 | 5478.1 | 119.8 |
| large|comp_rec|*|f0|R1|k0 | 14490.4 | 313.5 | 46.2 |
| large|comp_rec|*|f1|R1|k0 | 29089.5 | 1141.8 | 25.5 |
| large|rec_lane0|*|f0|R1|k0 | 172.9 | 172.3 | 1.0 |
| large|rec_lane0|*|f1|R1|k0 | 1263.0 | 1022.6 | 1.2 |
| large|rec_lane4|*|f0|R1|k0 | 159.7 | 160.2 | 1.0 |
| large|rec_lane4|*|f1|R1|k0 | 938.1 | 944.6 | 1.0 |
| large|rec_thread|*|f0|R1|k0 | 12267.4 | 3098.6 | 4.0 |
| large|rec_thread|*|f1|R1|k0 | 12096.4 | 3263.8 | 3.7 |
| large|red_warp_leader|*|f0|R1|k0 | 2327.7 | 99.1 | 23.5 |
| large|red_warp_leader|*|f1|R1|k0 | 5572.1 | 1038.2 | 5.4 |
| large|ref1512_loop|*|f1|R1|k0 | 1765.4 | 2125.4 | 0.8 |
| large|ref1512|*|f1|R1|k0 | 1753.4 | 2122.9 | 0.8 |
| large|st_lane1|*|f0|R1|k0 | 106.4 | 106.5 | 1.0 |
| large|st_lane1|*|f1|R1|k0 | 867.9 | 868.7 | 1.0 |
| large|st_lane32|*|f0|R1|k0 | 252.5 | 253.5 | 1.0 |
| large|st_lane32|*|f1|R1|k0 | 1027.9 | 1043.2 | 1.0 |
| tiny|atom_lane|*|f0|R1|k0 | 1.6 | 1.1 | 1.5 |
| tiny|atom_lane|*|f1|R1|k0 | 2.2 | 1.6 | 1.3 |
| tiny|atom_warp_all|*|f0|R1|k0 | 41.7 | 1.1 | 38.1 |
| tiny|atom_warp_all|*|f1|R1|k0 | 42.2 | 1.7 | 25.2 |
| tiny|atom_warp_leader_sys|*|f0|R1|k0 | 2.1 | 1.1 | 1.9 |
| tiny|atom_warp_leader_sys|*|f1|R1|k0 | 2.7 | 1.6 | 1.6 |
| tiny|atom_warp_leader|*|f0|R1|k0 | 2.1 | 1.1 | 2.0 |
| tiny|atom_warp_leader|*|f1|R1|k0 | 2.7 | 1.6 | 1.6 |
| tiny|comp_rec|*|f0|R1|k0 | 2.5 | 1.3 | 1.9 |
| tiny|comp_rec|*|f1|R1|k0 | 3.5 | 2.2 | 1.6 |
| tiny|rec_lane0|*|f0|R1|k0 | 1.1 | 1.1 | 1.0 |
| tiny|rec_lane0|*|f1|R1|k0 | 1.6 | 1.6 | 1.0 |
| tiny|rec_lane4|*|f0|R1|k0 | 1.1 | 1.1 | 1.0 |
| tiny|rec_lane4|*|f1|R1|k0 | 1.6 | 1.6 | 1.0 |
| tiny|rec_thread|*|f0|R1|k0 | 1.1 | 1.1 | 1.0 |
| tiny|rec_thread|*|f1|R1|k0 | 1.6 | 1.6 | 1.0 |
| tiny|red_warp_leader|*|f0|R1|k0 | 1.6 | 1.1 | 1.5 |
| tiny|red_warp_leader|*|f1|R1|k0 | 1.9 | 1.6 | 1.2 |
| tiny|ref1512_loop|*|f1|R1|k0 | 4.1 | 3.7 | 1.1 |
| tiny|ref1512|*|f1|R1|k0 | 4.1 | 3.4 | 1.2 |
| tiny|st_lane1|*|f0|R1|k0 | 1.1 | 1.1 | 1.0 |
| tiny|st_lane1|*|f1|R1|k0 | 1.6 | 1.6 | 1.0 |
| tiny|st_lane32|*|f0|R1|k0 | 1.0 | 1.0 | 1.0 |
| tiny|st_lane32|*|f1|R1|k0 | 1.6 | 1.6 | 1.0 |

## Atomic contention, full occupancy (24 x 512), R = 1 and R = Rbig, ns per counted op

| case | mem | fence | k | R | ns/op [min-max] | slope ns/op |
|---|---|---|---:|---:|---:|---:|
| atom_sm | dev | 0 | 1 | 1 | 239.8 [219.3-251.3] |  |
| atom_sm | dev | 0 | 2 | 1 | 124.6 [120.5-128.9] |  |
| atom_sm | dev | 0 | 4 | 1 | 64.1 [63.1-65.5] |  |
| atom_sm | dev | 0 | 8 | 1 | 33.0 [32.8-33.1] |  |
| atom_sm | dev | 0 | 16 | 1 | 16.6 [16.6-16.7] |  |
| atom_sm | dev | 0 | 32 | 1 | 8.3 [8.3-8.4] |  |
| atom_sm | dev | 0 | 48 | 1 | 5.6 [5.5-5.6] |  |
| atom_sm | dev | 0 | 1 | 16 | 49.9 [49.7-50.0] | 37.2 |
| atom_sm | dev | 0 | 2 | 16 | 25.0 [25.0-25.1] | 18.4 |
| atom_sm | dev | 0 | 4 | 16 | 12.5 [12.5-12.6] | 9.1 |
| atom_sm | dev | 0 | 8 | 16 | 6.3 [6.3-6.3] | 4.5 |
| atom_sm | dev | 0 | 16 | 16 | 3.2 [3.2-3.3] | 2.4 |
| atom_sm | dev | 0 | 32 | 16 | 1.7 [1.7-1.7] | 1.3 |
| atom_sm | dev | 0 | 48 | 16 | 1.2 [1.2-1.3] | 1.0 |
| atom_sm | dev | 1 | 1 | 1 | 277.2 [275.1-284.5] |  |
| atom_sm | dev | 1 | 2 | 1 | 137.9 [136.5-138.8] |  |
| atom_sm | dev | 1 | 4 | 1 | 68.6 [68.0-69.2] |  |
| atom_sm | dev | 1 | 8 | 1 | 33.9 [33.7-34.9] |  |
| atom_sm | dev | 1 | 16 | 1 | 20.0 [19.5-20.4] |  |
| atom_sm | dev | 1 | 32 | 1 | 14.7 [14.7-14.7] |  |
| atom_sm | dev | 1 | 48 | 1 | 10.0 [9.9-10.0] |  |
| atom_sm | dev | 1 | 1 | 16 | 205.3 [205.1-205.5] | 200.5 |
| atom_sm | dev | 1 | 2 | 16 | 102.7 [102.6-102.7] | 100.3 |
| atom_sm | dev | 1 | 4 | 16 | 51.4 [51.3-51.4] | 50.2 |
| atom_sm | dev | 1 | 8 | 16 | 26.0 [26.0-26.2] | 25.5 |
| atom_sm | dev | 1 | 16 | 16 | 17.3 [17.3-17.4] | 17.1 |
| atom_sm | dev | 1 | 32 | 16 | 8.8 [8.8-8.8] | 8.4 |
| atom_sm | dev | 1 | 48 | 16 | 6.0 [6.0-6.0] | 5.7 |
| atom_sm | reg | 0 | 1 | 1 | 273.1 [269.8-281.2] |  |
| atom_sm | reg | 0 | 2 | 1 | 235.7 [229.9-245.2] |  |
| atom_sm | reg | 0 | 4 | 1 | 185.6 [185.0-186.0] |  |
| atom_sm | reg | 0 | 8 | 1 | 159.2 [157.8-160.1] |  |
| atom_sm | reg | 0 | 16 | 1 | 124.3 [123.8-124.8] |  |
| atom_sm | reg | 0 | 32 | 1 | 103.1 [103.0-103.4] |  |
| atom_sm | reg | 0 | 48 | 1 | 105.5 [103.7-106.7] |  |
| atom_sm | reg | 0 | 1 | 16 | 107.8 [107.6-108.0] | 96.8 |
| atom_sm | reg | 0 | 2 | 16 | 89.6 [89.2-90.2] | 79.7 |
| atom_sm | reg | 0 | 4 | 16 | 87.5 [87.3-87.7] | 81.0 |
| atom_sm | reg | 0 | 8 | 16 | 87.7 [87.5-88.4] | 82.9 |
| atom_sm | reg | 0 | 16 | 16 | 84.9 [84.7-85.0] | 82.2 |
| atom_sm | reg | 0 | 32 | 16 | 83.1 [82.8-83.2] | 81.8 |
| atom_sm | reg | 0 | 48 | 16 | 81.9 [81.7-82.7] | 80.4 |
| atom_sm | reg | 1 | 1 | 1 | 342.2 [341.2-343.6] |  |
| atom_sm | reg | 1 | 2 | 1 | 294.0 [285.8-305.7] |  |
| atom_sm | reg | 1 | 4 | 1 | 209.9 [208.2-214.9] |  |
| atom_sm | reg | 1 | 8 | 1 | 160.8 [160.0-161.3] |  |
| atom_sm | reg | 1 | 16 | 1 | 138.5 [137.4-139.7] |  |
| atom_sm | reg | 1 | 32 | 1 | 109.3 [107.4-111.5] |  |
| atom_sm | reg | 1 | 48 | 1 | 102.3 [101.6-103.1] |  |
| atom_sm | reg | 1 | 1 | 16 | 305.0 [302.4-306.2] | 302.5 |
| atom_sm | reg | 1 | 2 | 16 | 174.8 [174.1-175.7] | 166.6 |
| atom_sm | reg | 1 | 4 | 16 | 195.8 [193.3-198.9] | 194.8 |
| atom_sm | reg | 1 | 8 | 16 | 182.1 [180.4-186.1] | 183.5 |
| atom_sm | reg | 1 | 16 | 16 | 196.3 [188.4-199.4] | 200.2 |
| atom_sm | reg | 1 | 32 | 16 | 202.6 [195.9-209.2] | 208.7 |
| atom_sm | reg | 1 | 48 | 16 | 204.2 [191.8-215.1] | 211.0 |
| cas_sm | dev | 0 | 1 | 1 | 266.7 [265.6-268.3] |  |
| cas_sm | dev | 0 | 2 | 1 | 163.4 [162.1-164.3] |  |
| cas_sm | dev | 0 | 4 | 1 | 100.8 [100.4-101.0] |  |
| cas_sm | dev | 0 | 8 | 1 | 67.0 [67.0-67.2] |  |
| cas_sm | dev | 0 | 16 | 1 | 54.4 [54.3-54.5] |  |
| cas_sm | dev | 0 | 32 | 1 | 49.1 [49.0-49.1] |  |
| cas_sm | dev | 0 | 48 | 1 | 47.4 [47.4-47.5] |  |
| cas_sm | dev | 0 | 1 | 16 | 80.1 [79.9-80.3] | 67.7 |
| cas_sm | dev | 0 | 2 | 16 | 60.6 [60.5-60.6] | 53.7 |
| cas_sm | dev | 0 | 4 | 16 | 50.1 [50.1-50.2] | 46.8 |
| cas_sm | dev | 0 | 8 | 16 | 45.4 [45.3-45.4] | 43.9 |
| cas_sm | dev | 0 | 16 | 16 | 43.1 [43.1-43.1] | 42.3 |
| cas_sm | dev | 0 | 32 | 16 | 42.9 [42.8-42.9] | 42.5 |
| cas_sm | dev | 0 | 48 | 16 | 43.8 [43.8-43.8] | 43.6 |
| cas_sm | dev | 1 | 1 | 1 | 341.8 [340.8-342.5] |  |
| cas_sm | dev | 1 | 2 | 1 | 238.3 [237.4-238.8] |  |
| cas_sm | dev | 1 | 4 | 1 | 134.0 [133.3-134.5] |  |
| cas_sm | dev | 1 | 8 | 1 | 85.1 [84.9-85.4] |  |
| cas_sm | dev | 1 | 16 | 1 | 63.1 [63.1-63.2] |  |
| cas_sm | dev | 1 | 32 | 1 | 52.7 [52.7-52.8] |  |
| cas_sm | dev | 1 | 48 | 1 | 50.0 [49.8-50.1] |  |
| cas_sm | dev | 1 | 1 | 16 | 269.7 [268.0-271.1] | 264.8 |
| cas_sm | dev | 1 | 2 | 16 | 140.1 [140.0-140.2] | 133.6 |
| cas_sm | dev | 1 | 4 | 16 | 91.9 [85.0-92.5] | 89.1 |
| cas_sm | dev | 1 | 8 | 16 | 48.5 [48.3-48.5] | 46.0 |
| cas_sm | dev | 1 | 16 | 16 | 44.9 [44.8-44.9] | 43.6 |
| cas_sm | dev | 1 | 32 | 16 | 44.3 [44.1-44.4] | 43.7 |
| cas_sm | dev | 1 | 48 | 16 | 44.6 [44.5-44.7] | 44.2 |
| cas_sm | reg | 0 | 1 | 1 | 403.2 [400.7-404.7] |  |
| cas_sm | reg | 0 | 2 | 1 | 400.2 [385.1-411.3] |  |
| cas_sm | reg | 0 | 4 | 1 | 433.7 [422.2-442.5] |  |
| cas_sm | reg | 0 | 8 | 1 | 655.7 [645.7-660.2] |  |
| cas_sm | reg | 0 | 16 | 1 | 972.9 [958.9-1002.7] |  |
| cas_sm | reg | 0 | 32 | 1 | 1543.3 [1535.1-1552.4] |  |
| cas_sm | reg | 0 | 48 | 1 | 2152.4 [2142.4-2168.6] |  |
| cas_sm | reg | 0 | 1 | 16 | 196.3 [196.1-196.4] | 182.4 |
| cas_sm | reg | 0 | 2 | 16 | 233.8 [232.4-235.2] | 222.9 |
| cas_sm | reg | 0 | 4 | 16 | 390.9 [389.6-392.0] | 388.2 |
| cas_sm | reg | 0 | 8 | 16 | 708.6 [707.1-715.7] | 712.1 |
| cas_sm | reg | 0 | 16 | 16 | 1337.6 [1332.5-1345.9] | 1362.0 |
| cas_sm | reg | 0 | 32 | 16 | 2576.7 [2575.3-2585.0] | 2645.9 |
| cas_sm | reg | 0 | 48 | 16 | 3823.6 [3820.4-3889.0] | 3934.9 |
| cas_sm | reg | 1 | 1 | 1 | 454.3 [450.0-456.8] |  |
| cas_sm | reg | 1 | 2 | 1 | 426.5 [409.1-451.8] |  |
| cas_sm | reg | 1 | 4 | 1 | 450.6 [438.9-455.6] |  |
| cas_sm | reg | 1 | 8 | 1 | 662.9 [657.8-668.4] |  |
| cas_sm | reg | 1 | 16 | 1 | 980.1 [961.9-999.7] |  |
| cas_sm | reg | 1 | 32 | 1 | 1547.7 [1536.9-1560.5] |  |
| cas_sm | reg | 1 | 48 | 1 | 2155.6 [2151.1-2183.3] |  |
| cas_sm | reg | 1 | 1 | 16 | 389.8 [385.2-392.6] | 385.5 |
| cas_sm | reg | 1 | 2 | 16 | 221.3 [219.7-223.0] | 207.6 |
| cas_sm | reg | 1 | 4 | 16 | 307.5 [304.6-310.7] | 298.0 |
| cas_sm | reg | 1 | 8 | 16 | 630.5 [621.9-638.0] | 628.3 |
| cas_sm | reg | 1 | 16 | 16 | 1374.4 [1347.9-1394.5] | 1399.9 |
| cas_sm | reg | 1 | 32 | 16 | 2854.0 [2793.0-2924.7] | 2941.1 |
| cas_sm | reg | 1 | 48 | 16 | 4367.5 [4191.8-4512.5] | 4515.3 |

## Sanity

```
{
 "anchor": {
  "ref_empty_minus_empty_us": 0.049450000000000216,
  "ref_loop_over_exit_reg": 1.0081429469759227,
  "ref_loop_over_exit_dev": 1.0045407921586247
 },
 "full": {
  "ref_empty_minus_empty_us": 0.5130999999999999,
  "ref_loop_over_exit_reg": 1.0101300568590288,
  "ref_loop_over_exit_dev": 1.0106758838451575
 },
 "large": {
  "ref_empty_minus_empty_us": 0.2671500000000009,
  "ref_loop_over_exit_reg": 1.0068452733083246,
  "ref_loop_over_exit_dev": 1.0011813038295974
 },
 "tiny": {
  "ref_empty_minus_empty_us": 0.07759999999999989,
  "ref_loop_over_exit_reg": 1.0027357951436597,
  "ref_loop_over_exit_dev": 1.0773389199800756
 },
 "hold_capped_rows_excluded": 0,
 "hold_capped_rows_first": []
}
```

validation failures: 0 []
