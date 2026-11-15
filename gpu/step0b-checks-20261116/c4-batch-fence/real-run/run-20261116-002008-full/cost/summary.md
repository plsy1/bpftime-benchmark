# C4 cost summary (bfc)

Medians over rounds of (item - empty kernel of the same shape), paired per round; [min-max] over rounds. ns per warp record = delta / warps. [projection] = 6.02M warp records x ns / 204.6 ms (step 0's pp512 figure, empty kernels). A smoke or a loaded board is not a measurement.

- rows dropped: 0 (gate capped), 0 (post-check failed); invalid items: none
- rounds: 10 of 10 requested; items with fewer usable rounds than that: none

## Shape anchor (decides)

- gate: record only 1.90 ns (step 0: 1.61, ratio 1.18); record + fence.sc.sys 10.64 ns (step 0: 10.06, ratio 1.06) -> **comparable to step 0**

### References (ns per warp)

| item | w32 | l8 | l8 per lane |
|---|---:|---:|---:|
| rec | 1.90 [1.88-1.94] | 2.09 [2.07-2.10] | 0.065 |
| rf_scgpu | 2.58 [2.57-2.60] | 2.10 [2.09-2.12] | 0.066 |
| rf_argpu | 2.58 [2.57-2.71] | 2.10 [2.09-2.11] | 0.066 |
| rf_scsys | 10.64 [10.60-10.68] | 8.15 [8.13-8.19] | 0.255 |
| rf_arsys | 10.63 [10.59-10.69] | 8.15 [8.08-8.19] | 0.255 |
| pw_sc | 11.23 [11.18-11.27] | 8.74 [8.71-8.78] | 0.273 |
| pw_ar | 11.20 [11.14-11.23] | 8.74 [8.71-8.76] | 0.273 |
| fonly_scgpu (lane 0, no store) | 0.88 [0.87-0.90] | | |
| fonly_argpu (lane 0, no store) | 0.88 [0.87-0.89] | | |
| fonly_scsys (lane 0, no store) | 6.64 [6.63-6.67] | | |
| fonly_arsys (lane 0, no store) | 6.63 [6.62-6.68] | | |

### w32: ns per warp record vs warps per system fence (G)

T = 2 x this run's record-only cost = 3.81 ns; secondary 3.22 ns (2 x step 0's 1.61) and 3.40 ns (10%).

| variant | G=1 | G=2 | G=4 | G=8 | G=16 | G=32 | G=48 | G=96 | G=384 | G=1536 | G=8192 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| nofence | 3.76* | 3.09* | 2.81* | 2.70* | 2.65* | 2.57* | 2.56* | 2.52* | 2.49* | 2.48* | 2.48* |
| nosys | 4.51 | 4.24 | 4.09 | 4.00 | 3.93 | 3.59* | 3.47* | 3.33* | 3.23* | 3.22* | 3.22* |
| sysonly | 15.05 | 11.03 | 8.60 | 6.75 | 6.19 | 4.40 | 3.88 | 3.48* | 3.03* | 2.71* | 2.62* |
| tf_sc | 19.80 | 15.56 | 12.99 | 10.55 | 9.85 | 7.47 | 6.37 | 5.07 | 3.80 | 3.50* | 3.33* |
| ar_sc | 19.85 | 15.59 | 12.98 | 10.56 | 9.86 | 7.46 | 6.37 | 5.08 | 3.80 | 3.50* | 3.33* |
| rel_sc | 19.08 | 14.90 | 13.11 | 10.58 | 9.80 | 7.30 | 6.21 | 4.96 | 3.68* | 3.38* | 3.23* |
| ar_ar | 15.51 | 13.92 | 12.42 | 11.78 | 11.75 | 8.12 | 6.68 | 5.12 | 3.80 | 3.46* | 3.30* |
| rel_ar | 15.10 | 13.80 | 12.23 | 11.69 | 11.56 | 7.99 | 6.54 | 4.99 | 3.68* | 3.34* | 3.20* |
| ar_aa | 19.80 | 15.56 | 12.99 | 10.52 | 9.89 | 7.46 | 6.38 | 5.09 | 3.82 | 3.50* | 3.34* |
| rmw_ar | 15.13 | 13.81 | 12.52 | 11.67 | 11.55 | 8.10 | 6.67 | 5.11 | 3.82 | 3.46* | 3.29* |
| ar_sc (counter in registered memory) | 98.33 | 162.23 | 226.58 | 274.83 | 337.03 | 380.03 | 435.85 | 515.11 | 615.03 | 650.60 | 655.50 |

(* = round max <= T)

Decision (anchor, w32):

- tf_sc: **G* = 384**, 3.80 ns per warp record (marginal: round max 3.83 > T), [projection] 11.2%
- ar_sc: **G* = 384**, 3.80 ns per warp record (marginal: round max 3.83 > T), [projection] 11.2%
- rel_sc: **G* = 384**, 3.68 ns per warp record (stable), [projection] 10.8%
- ar_ar (literal, shared fence): **G* = 384**, 3.80 ns per warp record (marginal: round max 3.81 > T), [projection] 11.2%
- rel_ar (literal, shared fence): **G* = 384**, 3.68 ns per warp record (stable), [projection] 10.8%
- ar_aa (literal, GPU side): **G* = 1536**, 3.50 ns per warp record (stable), [projection] 10.3%
- rmw_ar (literal, GPU side): **G* = 1536**, 3.46 ns per warp record (stable), [projection] 10.2%

- ranking (smallest G*, then ns at G*; unreached last): rel_sc; rel_ar; tf_sc; ar_ar; ar_sc; rmw_ar; ar_aa
- **decided output (anchor, w32): the best literal protocol (GPU side) is rmw_ar (G* = 1536, 3.46 ns, 10.2%)**; best with a shared completer fence: rel_ar (G* = 384, 3.68 ns, 10.8%); best overall: rel_sc (G* = 384, 3.68 ns, 10.8%)
- rmw_ar, the only variant whose acquire link is an acquire operation (no reading of fence.acq_rel as the 'acquire memory fence'): rmw_ar (G* = 1536, 3.46 ns, 10.2%)
- every variant's GPU-to-host link rests on H1 (README, 'Virtual aliases'), so H1 does not change the ranking
- secondary thresholds: tf_sc: G(3.22)=None G(3.40)=8192; ar_sc: G(3.22)=None G(3.40)=8192; rel_sc: G(3.22)=None G(3.40)=1536; ar_ar: G(3.22)=None G(3.40)=8192; rel_ar: G(3.22)=8192 G(3.40)=1536; ar_aa: G(3.22)=None G(3.40)=8192; rmw_ar: G(3.22)=None G(3.40)=8192

Fence share = protocol - nofence at the same G (ns per warp record):

| variant | G=1 | G=2 | G=4 | G=8 | G=16 | G=32 | G=48 | G=96 | G=384 | G=1536 | G=8192 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| tf_sc | 16.04 | 12.47 | 10.18 | 7.85 | 7.20 | 4.90 | 3.81 | 2.54 | 1.31 | 1.01 | 0.85 |
| ar_sc | 16.10 | 12.50 | 10.17 | 7.86 | 7.20 | 4.89 | 3.81 | 2.56 | 1.31 | 1.02 | 0.86 |
| rel_sc | 15.32 | 11.81 | 10.30 | 7.88 | 7.14 | 4.73 | 3.65 | 2.44 | 1.18 | 0.90 | 0.75 |
| ar_ar | 11.76 | 10.83 | 9.61 | 9.08 | 9.09 | 5.55 | 4.12 | 2.60 | 1.31 | 0.98 | 0.83 |
| rel_ar | 11.34 | 10.71 | 9.42 | 8.99 | 8.91 | 5.42 | 3.98 | 2.47 | 1.18 | 0.85 | 0.72 |
| ar_aa | 16.04 | 12.47 | 10.18 | 7.82 | 7.24 | 4.89 | 3.82 | 2.56 | 1.32 | 1.02 | 0.86 |
| rmw_ar | 11.37 | 10.72 | 9.71 | 8.97 | 8.90 | 5.53 | 4.11 | 2.59 | 1.33 | 0.98 | 0.81 |
| nosys | 0.75 | 1.15 | 1.28 | 1.30 | 1.28 | 1.02 | 0.91 | 0.81 | 0.74 | 0.74 | 0.74 |
| sysonly | 11.29 | 7.94 | 5.78 | 4.05 | 3.54 | 1.83 | 1.32 | 0.96 | 0.54 | 0.22 | 0.14 |

### l8: ns per warp record vs warps per system fence (G)

T = 2 x this run's record-only cost = 4.17 ns; secondary 3.22 ns (2 x step 0's 1.61) and 3.40 ns (10%).

| variant | G=1 | G=2 | G=4 | G=8 | G=16 | G=32 | G=48 | G=96 | G=384 | G=1536 | G=8192 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| nofence | 3.95* | 3.46* | 3.16* | 2.87* | 2.58* | 2.33* | 2.30* | 2.25* | 2.22* | 2.15* | 2.14* |
| nosys | 4.12* | 3.79* | 3.73* | 3.72* | 3.78* | 3.25* | 3.14* | 3.04* | 3.00* | 2.93* | 2.93* |
| sysonly | 14.90 | 10.87 | 7.55 | 5.57 | 5.89 | 3.84* | 3.48* | 3.15* | 2.77* | 2.38* | 2.28* |
| tf_sc | 18.65 | 13.24 | 11.83 | 9.59 | 8.61 | 6.67 | 5.62 | 4.40 | 3.37* | 3.15* | 3.06* |
| ar_sc | 18.74 | 13.25 | 11.85 | 9.57 | 8.62 | 6.66 | 5.59 | 4.39 | 3.37* | 3.14* | 3.07* |
| rel_sc | 19.41 | 12.50 | 11.77 | 9.65 | 8.88 | 6.68 | 5.57 | 4.34 | 3.31* | 3.10* | 3.02* |
| ar_ar | 9.97 | 11.77 | 11.60 | 11.14 | 10.81 | 6.97 | 5.75 | 4.47 | 3.32* | 3.10* | 2.99* |
| rel_ar | 9.95 | 12.44 | 11.67 | 11.22 | 10.78 | 6.94 | 5.65 | 4.41 | 3.26* | 3.06* | 2.99* |
| ar_aa | 18.70 | 13.25 | 11.84 | 9.57 | 8.65 | 6.66 | 5.60 | 4.40 | 3.36* | 3.14* | 3.06* |
| rmw_ar | 11.90 | 12.63 | 11.77 | 11.12 | 10.68 | 6.91 | 5.65 | 4.42 | 3.30* | 3.09* | 3.00* |
| ar32_sc | 23.81 | 22.92 | 22.57 | 21.57 | 22.71 | 27.01 | 29.91 | 31.64 | 33.28 | 33.60 | 33.40 |
| ar32_ar | 21.25 | 18.74 | 20.38 | 20.38 | 22.45 | 26.54 | 29.63 | 31.27 | 33.16 | 33.50 | 33.38 |
| ar32_aa | 23.76 | 22.89 | 22.49 | 21.63 | 22.75 | 27.03 | 29.82 | 31.56 | 33.26 | 33.53 | 33.41 |

(* = round max <= T)

Decision (anchor, l8):

- tf_sc: **G* = 384**, 3.37 ns per warp record (stable), [projection] 9.9%
- ar_sc: **G* = 384**, 3.37 ns per warp record (stable), [projection] 9.9%
- rel_sc: **G* = 384**, 3.31 ns per warp record (stable), [projection] 9.7%
- ar_ar (literal, shared fence): **G* = 384**, 3.32 ns per warp record (stable), [projection] 9.8%
- rel_ar (literal, shared fence): **G* = 384**, 3.26 ns per warp record (stable), [projection] 9.6%
- ar_aa (literal, GPU side): **G* = 384**, 3.36 ns per warp record (stable), [projection] 9.9%
- rmw_ar (literal, GPU side): **G* = 384**, 3.30 ns per warp record (stable), [projection] 9.7%
- ar32_sc: **not under T in the sweep**; lowest 21.57 ns at G = 8, [projection] 63.5%
- ar32_ar (literal, shared fence): **not under T in the sweep**; lowest 18.74 ns at G = 2, [projection] 55.1%
- ar32_aa (literal, GPU side): **not under T in the sweep**; lowest 21.63 ns at G = 8, [projection] 63.6%

- ranking (smallest G*, then ns at G*; unreached last): rel_ar; rmw_ar; rel_sc; ar_ar; ar_aa; tf_sc; ar_sc; ar32_ar; ar32_sc; ar32_aa
- **decided output (anchor, l8): the best literal protocol (GPU side) is rmw_ar (G* = 384, 3.30 ns, 9.7%)**; best with a shared completer fence: rel_ar (G* = 384, 3.26 ns, 9.6%); best overall: rel_ar (G* = 384, 3.26 ns, 9.6%)
- rmw_ar, the only variant whose acquire link is an acquire operation (no reading of fence.acq_rel as the 'acquire memory fence'); in l8 its lane-0 release still rests on bar.warp.sync, and no l8 variant avoids both readings: rmw_ar (G* = 384, 3.30 ns, 9.7%)
- every variant's GPU-to-host link rests on H1 (README, 'Virtual aliases'), so H1 does not change the ranking
- secondary thresholds: tf_sc: G(3.22)=1536 G(3.40)=384; ar_sc: G(3.22)=1536 G(3.40)=384; rel_sc: G(3.22)=1536 G(3.40)=384; ar_ar: G(3.22)=1536 G(3.40)=384; rel_ar: G(3.22)=1536 G(3.40)=384; ar_aa: G(3.22)=1536 G(3.40)=384; rmw_ar: G(3.22)=1536 G(3.40)=384; ar32_sc: G(3.22)=None G(3.40)=None; ar32_ar: G(3.22)=None G(3.40)=None; ar32_aa: G(3.22)=None G(3.40)=None

Fence share = protocol - nofence at the same G (ns per warp record):

| variant | G=1 | G=2 | G=4 | G=8 | G=16 | G=32 | G=48 | G=96 | G=384 | G=1536 | G=8192 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| tf_sc | 14.70 | 9.78 | 8.67 | 6.72 | 6.03 | 4.34 | 3.32 | 2.14 | 1.14 | 1.00 | 0.92 |
| ar_sc | 14.79 | 9.79 | 8.69 | 6.71 | 6.04 | 4.33 | 3.29 | 2.14 | 1.15 | 0.99 | 0.93 |
| rel_sc | 15.46 | 9.04 | 8.61 | 6.78 | 6.30 | 4.35 | 3.27 | 2.09 | 1.08 | 0.95 | 0.88 |
| ar_ar | 6.03 | 8.32 | 8.44 | 8.27 | 8.23 | 4.64 | 3.44 | 2.22 | 1.10 | 0.95 | 0.86 |
| rel_ar | 6.00 | 8.99 | 8.51 | 8.36 | 8.20 | 4.61 | 3.35 | 2.16 | 1.04 | 0.91 | 0.85 |
| ar_aa | 14.75 | 9.80 | 8.69 | 6.70 | 6.07 | 4.33 | 3.30 | 2.15 | 1.13 | 0.99 | 0.92 |
| rmw_ar | 7.95 | 9.18 | 8.61 | 8.26 | 8.10 | 4.58 | 3.35 | 2.17 | 1.07 | 0.94 | 0.86 |
| ar32_sc | 19.86 | 19.47 | 19.41 | 18.70 | 20.12 | 24.68 | 27.60 | 29.39 | 31.06 | 31.45 | 31.26 |
| ar32_ar | 17.30 | 15.29 | 17.22 | 17.52 | 19.87 | 24.21 | 27.32 | 29.01 | 30.93 | 31.35 | 31.24 |
| ar32_aa | 19.81 | 19.43 | 19.33 | 18.76 | 20.17 | 24.70 | 27.51 | 29.31 | 31.03 | 31.39 | 31.28 |
| nosys | 0.17 | 0.34 | 0.58 | 0.86 | 1.19 | 0.92 | 0.84 | 0.78 | 0.78 | 0.78 | 0.80 |
| sysonly | 10.95 | 7.42 | 4.40 | 2.70 | 3.31 | 1.51 | 1.18 | 0.89 | 0.55 | 0.23 | 0.15 |

## Shape large (secondary)

### References (ns per warp)

| item | w32 | l8 | l8 per lane |
|---|---:|---:|---:|
| rec | 1.19 [1.17-1.20] | 1.93 [1.92-1.97] | 0.060 |
| rf_scgpu | 2.26 [2.25-2.27] | 1.93 [1.93-1.94] | 0.060 |
| rf_argpu | 2.26 [2.25-2.41] | 1.93 [1.93-1.94] | 0.060 |
| rf_scsys | 10.23 [10.15-10.27] | 7.82 [7.78-7.86] | 0.245 |
| rf_arsys | 10.22 [10.20-10.29] | 7.82 [7.79-7.84] | 0.244 |
| pw_sc | 10.82 [10.77-10.85] | 8.30 [8.27-8.31] | 0.259 |
| pw_ar | 10.82 [10.75-10.86] | 8.29 [8.25-8.33] | 0.259 |
| fonly_scgpu (lane 0, no store) | 0.83 [0.82-0.83] | | |
| fonly_argpu (lane 0, no store) | 0.83 [0.82-0.84] | | |
| fonly_scsys (lane 0, no store) | 6.49 [6.47-6.50] | | |
| fonly_arsys (lane 0, no store) | 6.48 [6.46-6.53] | | |

### w32: ns per warp record vs warps per system fence (G)

T = 2 x this run's record-only cost = 2.38 ns; secondary 3.22 ns (2 x step 0's 1.61) and 3.40 ns (10%).

| variant | G=1 | G=2 | G=4 | G=8 | G=16 | G=32 | G=48 | G=96 | G=384 | G=1536 | G=8192 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| nofence | 4.29 | 3.12 | 2.68 | 2.46 | 2.37* | 2.32* | 2.29* | 2.26* | 2.22* | 2.21* | 2.21* |
| nosys | 5.16 | 4.02 | 3.86 | 3.76 | 3.70 | 3.33 | 3.21 | 3.08 | 2.98 | 2.96 | 2.94 |
| sysonly | 17.38 | 10.37 | 7.94 | 6.02 | 5.53 | 3.83 | 3.37 | 2.99 | 2.60 | 2.27* | 2.21* |
| tf_sc | 21.20 | 15.41 | 12.71 | 10.18 | 9.45 | 7.01 | 5.95 | 4.67 | 3.41 | 3.09 | 2.99 |
| ar_sc | 21.22 | 15.44 | 12.69 | 10.19 | 9.42 | 7.00 | 5.95 | 4.66 | 3.41 | 3.08 | 2.98 |
| rel_sc | 20.83 | 14.64 | 12.88 | 10.26 | 9.40 | 6.84 | 5.82 | 4.54 | 3.28 | 2.97 | 2.88 |
| ar_ar | 13.95 | 14.48 | 12.26 | 11.64 | 11.65 | 7.78 | 6.33 | 4.75 | 3.41 | 3.08 | 2.98 |
| rel_ar | 13.91 | 14.48 | 12.07 | 11.55 | 11.49 | 7.65 | 6.18 | 4.62 | 3.29 | 2.95 | 2.87 |
| ar_aa | 21.25 | 15.43 | 12.73 | 10.14 | 9.42 | 7.01 | 5.93 | 4.67 | 3.40 | 3.08 | 2.99 |
| rmw_ar | 13.94 | 14.63 | 12.41 | 11.51 | 11.45 | 7.75 | 6.31 | 4.75 | 3.43 | 3.06 | 2.95 |
| ar_sc (counter in registered memory) | 113.10 | 165.08 | 228.43 | 280.32 | 336.57 | 386.48 | 432.27 | 504.35 | 593.88 | 631.86 | 652.52 |

(* = round max <= T)

Decision (large, w32):

- tf_sc: **not under T in the sweep**; lowest 2.99 ns at G = 8192, [projection] 8.8%
- ar_sc: **not under T in the sweep**; lowest 2.98 ns at G = 8192, [projection] 8.8%
- rel_sc: **not under T in the sweep**; lowest 2.88 ns at G = 8192, [projection] 8.5%
- ar_ar (literal, shared fence): **not under T in the sweep**; lowest 2.98 ns at G = 8192, [projection] 8.8%
- rel_ar (literal, shared fence): **not under T in the sweep**; lowest 2.87 ns at G = 8192, [projection] 8.5%
- ar_aa (literal, GPU side): **not under T in the sweep**; lowest 2.99 ns at G = 8192, [projection] 8.8%
- rmw_ar (literal, GPU side): **not under T in the sweep**; lowest 2.95 ns at G = 8192, [projection] 8.7%

- ranking (smallest G*, then ns at G*; unreached last): rel_ar; rel_sc; rmw_ar; ar_sc; ar_ar; tf_sc; ar_aa
- **decided output (large, w32): the best literal protocol (GPU side) is rmw_ar (not reached; lowest 2.95 ns at G = 8192, 8.7%)**; best with a shared completer fence: rel_ar (not reached; lowest 2.87 ns at G = 8192, 8.5%); best overall: rel_ar (not reached; lowest 2.87 ns at G = 8192, 8.5%)
- rmw_ar, the only variant whose acquire link is an acquire operation (no reading of fence.acq_rel as the 'acquire memory fence'): rmw_ar (not reached; lowest 2.95 ns at G = 8192, 8.7%)
- every variant's GPU-to-host link rests on H1 (README, 'Virtual aliases'), so H1 does not change the ranking
- secondary thresholds: tf_sc: G(3.22)=1536 G(3.40)=1536; ar_sc: G(3.22)=1536 G(3.40)=1536; rel_sc: G(3.22)=1536 G(3.40)=384; ar_ar: G(3.22)=1536 G(3.40)=1536; rel_ar: G(3.22)=1536 G(3.40)=384; ar_aa: G(3.22)=1536 G(3.40)=1536; rmw_ar: G(3.22)=1536 G(3.40)=1536

Fence share = protocol - nofence at the same G (ns per warp record):

| variant | G=1 | G=2 | G=4 | G=8 | G=16 | G=32 | G=48 | G=96 | G=384 | G=1536 | G=8192 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| tf_sc | 16.91 | 12.29 | 10.02 | 7.72 | 7.08 | 4.69 | 3.66 | 2.41 | 1.19 | 0.88 | 0.78 |
| ar_sc | 16.93 | 12.32 | 10.01 | 7.72 | 7.05 | 4.68 | 3.65 | 2.40 | 1.19 | 0.87 | 0.77 |
| rel_sc | 16.55 | 11.52 | 10.20 | 7.79 | 7.03 | 4.52 | 3.52 | 2.28 | 1.06 | 0.76 | 0.67 |
| ar_ar | 9.67 | 11.36 | 9.58 | 9.18 | 9.29 | 5.47 | 4.04 | 2.49 | 1.19 | 0.87 | 0.77 |
| rel_ar | 9.62 | 11.36 | 9.39 | 9.09 | 9.12 | 5.33 | 3.89 | 2.35 | 1.06 | 0.75 | 0.67 |
| ar_aa | 16.97 | 12.31 | 10.05 | 7.68 | 7.05 | 4.69 | 3.63 | 2.41 | 1.18 | 0.87 | 0.79 |
| rmw_ar | 9.65 | 11.51 | 9.73 | 9.05 | 9.09 | 5.43 | 4.02 | 2.48 | 1.21 | 0.85 | 0.74 |
| nosys | 0.87 | 0.91 | 1.18 | 1.30 | 1.33 | 1.01 | 0.91 | 0.81 | 0.76 | 0.75 | 0.74 |
| sysonly | 13.09 | 7.25 | 5.25 | 3.55 | 3.16 | 1.51 | 1.08 | 0.73 | 0.38 | 0.06 | 0.00 |

### l8: ns per warp record vs warps per system fence (G)

T = 2 x this run's record-only cost = 3.85 ns; secondary 3.22 ns (2 x step 0's 1.61) and 3.40 ns (10%).

| variant | G=1 | G=2 | G=4 | G=8 | G=16 | G=32 | G=48 | G=96 | G=384 | G=1536 | G=8192 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| nofence | 4.46 | 3.67* | 3.17* | 2.81* | 2.55* | 2.27* | 2.18* | 2.09* | 2.00* | 1.96* | 1.95* |
| nosys | 5.10 | 3.74* | 3.53* | 3.39* | 3.41* | 2.99* | 2.88* | 2.78* | 2.74* | 2.64* | 2.61* |
| sysonly | 17.61 | 10.67 | 7.28 | 4.78 | 4.91 | 3.32* | 2.97* | 2.63* | 2.36* | 2.09* | 1.97* |
| tf_sc | 22.13 | 13.07 | 11.75 | 9.52 | 8.02 | 6.29 | 5.19 | 3.99 | 2.95* | 2.72* | 2.63* |
| ar_sc | 22.06 | 13.07 | 11.76 | 9.49 | 8.00 | 6.26 | 5.19 | 3.97 | 2.96* | 2.70* | 2.62* |
| rel_sc | 21.86 | 12.01 | 11.70 | 9.57 | 8.39 | 6.29 | 5.18 | 3.93 | 2.90* | 2.65* | 2.58* |
| ar_ar | 11.74 | 13.38 | 11.54 | 11.30 | 11.27 | 6.59 | 5.34 | 4.03 | 2.94* | 2.69* | 2.62* |
| rel_ar | 11.73 | 13.29 | 11.52 | 11.19 | 11.16 | 6.55 | 5.29 | 4.00 | 2.88* | 2.63* | 2.56* |
| ar_aa | 22.11 | 13.06 | 11.75 | 9.50 | 8.05 | 6.28 | 5.19 | 3.99 | 2.94* | 2.70* | 2.61* |
| rmw_ar | 12.40 | 13.65 | 11.74 | 11.21 | 11.23 | 6.57 | 5.30 | 4.00 | 2.90* | 2.65* | 2.57* |
| ar32_sc | 45.00 | 22.63 | 22.07 | 20.93 | 21.90 | 26.21 | 28.91 | 30.94 | 32.75 | 33.12 | 33.12 |
| ar32_ar | 37.43 | 21.35 | 20.02 | 19.82 | 21.68 | 25.82 | 28.71 | 31.03 | 32.78 | 33.05 | 33.24 |
| ar32_aa | 44.98 | 22.72 | 22.08 | 20.88 | 21.83 | 26.26 | 29.03 | 31.06 | 32.74 | 33.18 | 33.12 |

(* = round max <= T)

Decision (large, l8):

- tf_sc: **G* = 384**, 2.95 ns per warp record (stable), [projection] 8.7%
- ar_sc: **G* = 384**, 2.96 ns per warp record (stable), [projection] 8.7%
- rel_sc: **G* = 384**, 2.90 ns per warp record (stable), [projection] 8.5%
- ar_ar (literal, shared fence): **G* = 384**, 2.94 ns per warp record (stable), [projection] 8.6%
- rel_ar (literal, shared fence): **G* = 384**, 2.88 ns per warp record (stable), [projection] 8.5%
- ar_aa (literal, GPU side): **G* = 384**, 2.94 ns per warp record (stable), [projection] 8.7%
- rmw_ar (literal, GPU side): **G* = 384**, 2.90 ns per warp record (stable), [projection] 8.5%
- ar32_sc: **not under T in the sweep**; lowest 20.93 ns at G = 8, [projection] 61.6%
- ar32_ar (literal, shared fence): **not under T in the sweep**; lowest 19.82 ns at G = 8, [projection] 58.3%
- ar32_aa (literal, GPU side): **not under T in the sweep**; lowest 20.88 ns at G = 8, [projection] 61.4%

- ranking (smallest G*, then ns at G*; unreached last): rel_ar; rel_sc; rmw_ar; ar_ar; ar_aa; tf_sc; ar_sc; ar32_ar; ar32_aa; ar32_sc
- **decided output (large, l8): the best literal protocol (GPU side) is rmw_ar (G* = 384, 2.90 ns, 8.5%)**; best with a shared completer fence: rel_ar (G* = 384, 2.88 ns, 8.5%); best overall: rel_ar (G* = 384, 2.88 ns, 8.5%)
- rmw_ar, the only variant whose acquire link is an acquire operation (no reading of fence.acq_rel as the 'acquire memory fence'); in l8 its lane-0 release still rests on bar.warp.sync, and no l8 variant avoids both readings: rmw_ar (G* = 384, 2.90 ns, 8.5%)
- every variant's GPU-to-host link rests on H1 (README, 'Virtual aliases'), so H1 does not change the ranking
- secondary thresholds: tf_sc: G(3.22)=384 G(3.40)=384; ar_sc: G(3.22)=384 G(3.40)=384; rel_sc: G(3.22)=384 G(3.40)=384; ar_ar: G(3.22)=384 G(3.40)=384; rel_ar: G(3.22)=384 G(3.40)=384; ar_aa: G(3.22)=384 G(3.40)=384; rmw_ar: G(3.22)=384 G(3.40)=384; ar32_sc: G(3.22)=None G(3.40)=None; ar32_ar: G(3.22)=None G(3.40)=None; ar32_aa: G(3.22)=None G(3.40)=None

Fence share = protocol - nofence at the same G (ns per warp record):

| variant | G=1 | G=2 | G=4 | G=8 | G=16 | G=32 | G=48 | G=96 | G=384 | G=1536 | G=8192 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| tf_sc | 17.67 | 9.40 | 8.57 | 6.70 | 5.47 | 4.02 | 3.01 | 1.90 | 0.95 | 0.76 | 0.68 |
| ar_sc | 17.59 | 9.40 | 8.59 | 6.68 | 5.46 | 3.99 | 3.02 | 1.89 | 0.95 | 0.74 | 0.68 |
| rel_sc | 17.39 | 8.34 | 8.53 | 6.75 | 5.85 | 4.02 | 3.00 | 1.84 | 0.89 | 0.70 | 0.63 |
| ar_ar | 7.28 | 9.71 | 8.36 | 8.49 | 8.72 | 4.33 | 3.16 | 1.95 | 0.93 | 0.73 | 0.68 |
| rel_ar | 7.26 | 9.62 | 8.35 | 8.38 | 8.62 | 4.28 | 3.11 | 1.91 | 0.88 | 0.67 | 0.62 |
| ar_aa | 17.65 | 9.39 | 8.58 | 6.68 | 5.50 | 4.02 | 3.01 | 1.91 | 0.94 | 0.74 | 0.66 |
| rmw_ar | 7.94 | 9.98 | 8.57 | 8.40 | 8.68 | 4.30 | 3.12 | 1.92 | 0.89 | 0.70 | 0.63 |
| ar32_sc | 40.53 | 18.96 | 18.90 | 18.11 | 19.35 | 23.95 | 26.73 | 28.85 | 30.75 | 31.16 | 31.17 |
| ar32_ar | 32.96 | 17.68 | 16.85 | 17.00 | 19.14 | 23.55 | 26.54 | 28.94 | 30.77 | 31.09 | 31.29 |
| ar32_aa | 40.51 | 19.05 | 18.90 | 18.07 | 19.29 | 24.00 | 26.85 | 28.97 | 30.74 | 31.22 | 31.18 |
| nosys | 0.64 | 0.07 | 0.36 | 0.57 | 0.87 | 0.73 | 0.70 | 0.69 | 0.73 | 0.68 | 0.66 |
| sysonly | 13.15 | 7.00 | 4.11 | 1.96 | 2.36 | 1.05 | 0.79 | 0.54 | 0.36 | 0.13 | 0.03 |

## Shape full (secondary)

### References (ns per warp)

| item | w32 | l8 | l8 per lane |
|---|---:|---:|---:|
| rec | 8.47 [8.39-8.51] | 6.30 [6.27-6.33] | 0.197 |
| rf_scgpu | 8.53 [8.50-8.59] | 6.44 [6.38-6.46] | 0.201 |
| rf_argpu | 8.54 [8.49-8.58] | 6.42 [6.39-6.47] | 0.201 |
| rf_scsys | 12.49 [12.36-12.55] | 10.61 [10.54-10.74] | 0.331 |
| rf_arsys | 12.49 [11.88-12.56] | 10.59 [10.40-10.75] | 0.331 |
| pw_sc | 14.58 [14.47-14.60] | 12.89 [12.85-12.95] | 0.403 |
| pw_ar | 14.60 [14.54-14.65] | 12.90 [12.82-12.97] | 0.403 |
| fonly_scgpu (lane 0, no store) | 1.42 [1.39-1.44] | | |
| fonly_argpu (lane 0, no store) | 1.42 [1.29-1.44] | | |
| fonly_scsys (lane 0, no store) | 8.34 [7.95-8.39] | | |
| fonly_arsys (lane 0, no store) | 8.34 [8.04-8.41] | | |

### w32: ns per warp record vs warps per system fence (G)

T = 2 x this run's record-only cost = 16.94 ns; secondary 3.22 ns (2 x step 0's 1.61) and 3.40 ns (10%).

| variant | G=1 | G=2 | G=4 | G=8 | G=16 | G=32 | G=48 | G=96 | G=384 | G=1536 | G=8192 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| nofence | 7.92* | 7.80* | 8.01* | 8.42* | 8.75* | 8.70* | 8.81* | 8.79* | 8.82* | - | - |
| nosys | 10.28* | 9.77* | 9.33* | 9.42* | 9.44* | 9.13* | 9.28* | 9.28* | 9.29* | - | - |
| sysonly | 20.86 | 20.67 | 20.98 | 18.90 | 18.56 | 15.61* | 15.81* | 14.31* | 11.63* | - | - |
| tf_sc | 22.40 | 22.49 | 22.16 | 19.86 | 19.15 | 16.77* | 16.18* | 14.12* | 12.12* | - | - |
| ar_sc | 22.31 | 22.44 | 22.16 | 19.87 | 19.17 | 16.73* | 16.12* | 14.10* | 12.13* | - | - |
| rel_sc | 22.31 | 22.17 | 21.69 | 19.00 | 19.02 | 16.13* | 16.14* | 14.99* | 12.01* | - | - |
| ar_ar | 18.69 | 15.34* | 14.62* | 14.53* | 14.79* | 15.75* | 15.96* | 14.12* | 12.09* | - | - |
| rel_ar | 18.23 | 14.92* | 14.87* | 14.89* | 14.86* | 15.26* | 15.19* | 15.11* | 11.99* | - | - |
| ar_aa | 22.32 | 22.48 | 22.14 | 19.89 | 19.18 | 16.76* | 16.14* | 14.09* | 12.11* | - | - |
| rmw_ar | 20.42 | 17.40 | 15.71* | 15.07* | 15.06* | 15.37* | 15.39* | 14.95* | 12.13* | - | - |
| ar_sc (counter in registered memory) | 61.74 | 107.78 | 138.84 | 176.80 | 390.08 | 349.85 | 560.87 | 641.16 | 665.24 | - | - |

(* = round max <= T)

Decision (full, w32):

- tf_sc: **G* = 32**, 16.77 ns per warp record (stable), [projection] 49.3%
- ar_sc: **G* = 32**, 16.73 ns per warp record (stable), [projection] 49.2%
- rel_sc: **G* = 32**, 16.13 ns per warp record (stable), [projection] 47.5%
- ar_ar (literal, shared fence): **G* = 2**, 15.34 ns per warp record (stable), [projection] 45.1%
- rel_ar (literal, shared fence): **G* = 2**, 14.92 ns per warp record (stable), [projection] 43.9%
- ar_aa (literal, GPU side): **G* = 32**, 16.76 ns per warp record (stable), [projection] 49.3%
- rmw_ar (literal, GPU side): **G* = 4**, 15.71 ns per warp record (stable), [projection] 46.2%

- ranking (smallest G*, then ns at G*; unreached last): rel_ar; ar_ar; rmw_ar; rel_sc; ar_sc; ar_aa; tf_sc
- **decided output (full, w32): the best literal protocol (GPU side) is rmw_ar (G* = 4, 15.71 ns, 46.2%)**; best with a shared completer fence: rel_ar (G* = 2, 14.92 ns, 43.9%); best overall: rel_ar (G* = 2, 14.92 ns, 43.9%)
- rmw_ar, the only variant whose acquire link is an acquire operation (no reading of fence.acq_rel as the 'acquire memory fence'): rmw_ar (G* = 4, 15.71 ns, 46.2%)
- every variant's GPU-to-host link rests on H1 (README, 'Virtual aliases'), so H1 does not change the ranking
- secondary thresholds: tf_sc: G(3.22)=None G(3.40)=None; ar_sc: G(3.22)=None G(3.40)=None; rel_sc: G(3.22)=None G(3.40)=None; ar_ar: G(3.22)=None G(3.40)=None; rel_ar: G(3.22)=None G(3.40)=None; ar_aa: G(3.22)=None G(3.40)=None; rmw_ar: G(3.22)=None G(3.40)=None

Fence share = protocol - nofence at the same G (ns per warp record):

| variant | G=1 | G=2 | G=4 | G=8 | G=16 | G=32 | G=48 | G=96 | G=384 | G=1536 | G=8192 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| tf_sc | 14.48 | 14.69 | 14.15 | 11.44 | 10.40 | 8.07 | 7.37 | 5.33 | 3.30 | - | - |
| ar_sc | 14.39 | 14.64 | 14.15 | 11.45 | 10.42 | 8.03 | 7.31 | 5.31 | 3.32 | - | - |
| rel_sc | 14.39 | 14.37 | 13.69 | 10.58 | 10.27 | 7.43 | 7.33 | 6.20 | 3.19 | - | - |
| ar_ar | 10.77 | 7.54 | 6.61 | 6.11 | 6.03 | 7.05 | 7.15 | 5.33 | 3.28 | - | - |
| rel_ar | 10.32 | 7.13 | 6.86 | 6.48 | 6.10 | 6.56 | 6.38 | 6.32 | 3.17 | - | - |
| ar_aa | 14.40 | 14.68 | 14.13 | 11.47 | 10.43 | 8.06 | 7.33 | 5.30 | 3.30 | - | - |
| rmw_ar | 12.51 | 9.61 | 7.71 | 6.65 | 6.31 | 6.67 | 6.58 | 6.15 | 3.31 | - | - |
| nosys | 2.36 | 1.97 | 1.32 | 1.01 | 0.68 | 0.43 | 0.47 | 0.49 | 0.47 | - | - |
| sysonly | 12.95 | 12.87 | 12.97 | 10.49 | 9.81 | 6.91 | 7.00 | 5.51 | 2.81 | - | - |

### l8: ns per warp record vs warps per system fence (G)

T = 2 x this run's record-only cost = 12.61 ns; secondary 3.22 ns (2 x step 0's 1.61) and 3.40 ns (10%).

| variant | G=1 | G=2 | G=4 | G=8 | G=16 | G=32 | G=48 | G=96 | G=384 | G=1536 | G=8192 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| nofence | 6.29* | 6.33* | 6.36* | 6.36* | 6.40* | 6.40* | 7.57* | 7.76* | 7.65* | - | - |
| nosys | 8.94* | 7.82* | 7.77* | 7.79* | 7.78* | 7.79* | 7.79* | 7.79* | 7.81* | - | - |
| sysonly | 18.03 | 13.78 | 13.62 | 13.43 | 13.42 | 12.58 | 11.88* | 11.97* | 10.61* | - | - |
| tf_sc | 19.67 | 17.64 | 15.63 | 13.93 | 14.80 | 12.03* | 13.09 | 12.22* | 10.61* | - | - |
| ar_sc | 19.65 | 17.70 | 15.64 | 13.90 | 14.84 | 12.03* | 13.08 | 12.24* | 10.60* | - | - |
| rel_sc | 17.99 | 16.29 | 14.03 | 12.03* | 14.87 | 12.04* | 13.41 | 12.02* | 10.59* | - | - |
| ar_ar | 13.89 | 13.53 | 13.52 | 13.40 | 13.42 | 12.23* | 12.90 | 11.98* | 10.61* | - | - |
| rel_ar | 13.87 | 13.52 | 13.49 | 13.42 | 13.44 | 12.48* | 13.42 | 11.88* | 10.59* | - | - |
| ar_aa | 19.76 | 17.63 | 15.58 | 13.89 | 14.82 | 12.02* | 13.12 | 12.24* | 10.60* | - | - |
| rmw_ar | 13.96 | 13.54 | 13.49 | 13.42 | 13.43 | 11.95* | 13.47 | 12.33* | 10.60* | - | - |
| ar32_sc | 29.72 | 27.68 | 28.16 | 33.55 | 40.98 | 44.70 | 47.68 | 42.34 | 41.75 | - | - |
| ar32_ar | 23.49 | 21.61 | 22.66 | 27.87 | 36.89 | 41.84 | 46.99 | 42.54 | 41.03 | - | - |
| ar32_aa | 29.61 | 27.57 | 28.02 | 33.62 | 41.11 | 44.81 | 47.80 | 42.46 | 41.74 | - | - |

(* = round max <= T)

Decision (full, l8):

- tf_sc: **G* = 32**, 12.03 ns per warp record (stable), [projection] 35.4%; non-monotone: above T again at G = [48]
- ar_sc: **G* = 32**, 12.03 ns per warp record (stable), [projection] 35.4%; non-monotone: above T again at G = [48]
- rel_sc: **G* = 8**, 12.03 ns per warp record (stable), [projection] 35.4%; non-monotone: above T again at G = [16, 48]
- ar_ar (literal, shared fence): **G* = 32**, 12.23 ns per warp record (stable), [projection] 36.0%; non-monotone: above T again at G = [48]
- rel_ar (literal, shared fence): **G* = 32**, 12.48 ns per warp record (stable), [projection] 36.7%; non-monotone: above T again at G = [48]
- ar_aa (literal, GPU side): **G* = 32**, 12.02 ns per warp record (stable), [projection] 35.4%; non-monotone: above T again at G = [48]
- rmw_ar (literal, GPU side): **G* = 32**, 11.95 ns per warp record (stable), [projection] 35.2%; non-monotone: above T again at G = [48]
- ar32_sc: **not under T in the sweep**; lowest 27.68 ns at G = 2, [projection] 81.4%
- ar32_ar (literal, shared fence): **not under T in the sweep**; lowest 21.61 ns at G = 2, [projection] 63.6%
- ar32_aa (literal, GPU side): **not under T in the sweep**; lowest 27.57 ns at G = 2, [projection] 81.1%

- ranking (smallest G*, then ns at G*; unreached last): rel_sc; rmw_ar; ar_aa; ar_sc; tf_sc; ar_ar; rel_ar; ar32_ar; ar32_aa; ar32_sc
- **decided output (full, l8): the best literal protocol (GPU side) is rmw_ar (G* = 32, 11.95 ns, 35.2%)**; best with a shared completer fence: ar_ar (G* = 32, 12.23 ns, 36.0%); best overall: rel_sc (G* = 8, 12.03 ns, 35.4%)
- rmw_ar, the only variant whose acquire link is an acquire operation (no reading of fence.acq_rel as the 'acquire memory fence'); in l8 its lane-0 release still rests on bar.warp.sync, and no l8 variant avoids both readings: rmw_ar (G* = 32, 11.95 ns, 35.2%)
- every variant's GPU-to-host link rests on H1 (README, 'Virtual aliases'), so H1 does not change the ranking
- secondary thresholds: tf_sc: G(3.22)=None G(3.40)=None; ar_sc: G(3.22)=None G(3.40)=None; rel_sc: G(3.22)=None G(3.40)=None; ar_ar: G(3.22)=None G(3.40)=None; rel_ar: G(3.22)=None G(3.40)=None; ar_aa: G(3.22)=None G(3.40)=None; rmw_ar: G(3.22)=None G(3.40)=None; ar32_sc: G(3.22)=None G(3.40)=None; ar32_ar: G(3.22)=None G(3.40)=None; ar32_aa: G(3.22)=None G(3.40)=None

Fence share = protocol - nofence at the same G (ns per warp record):

| variant | G=1 | G=2 | G=4 | G=8 | G=16 | G=32 | G=48 | G=96 | G=384 | G=1536 | G=8192 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| tf_sc | 13.38 | 11.32 | 9.26 | 7.58 | 8.40 | 5.63 | 5.52 | 4.46 | 2.96 | - | - |
| ar_sc | 13.37 | 11.38 | 9.28 | 7.54 | 8.44 | 5.63 | 5.51 | 4.48 | 2.95 | - | - |
| rel_sc | 11.70 | 9.96 | 7.67 | 5.67 | 8.47 | 5.64 | 5.84 | 4.26 | 2.93 | - | - |
| ar_ar | 7.60 | 7.21 | 7.16 | 7.05 | 7.02 | 5.83 | 5.33 | 4.22 | 2.95 | - | - |
| rel_ar | 7.59 | 7.19 | 7.13 | 7.06 | 7.04 | 6.08 | 5.84 | 4.12 | 2.93 | - | - |
| ar_aa | 13.48 | 11.31 | 9.22 | 7.53 | 8.42 | 5.62 | 5.55 | 4.47 | 2.94 | - | - |
| rmw_ar | 7.68 | 7.21 | 7.13 | 7.06 | 7.03 | 5.55 | 5.90 | 4.57 | 2.94 | - | - |
| ar32_sc | 23.44 | 21.36 | 21.80 | 27.19 | 34.58 | 38.30 | 40.11 | 34.58 | 34.10 | - | - |
| ar32_ar | 17.21 | 15.29 | 16.30 | 21.51 | 30.49 | 35.44 | 39.42 | 34.78 | 33.38 | - | - |
| ar32_aa | 23.32 | 21.24 | 21.66 | 27.26 | 34.71 | 38.42 | 40.22 | 34.70 | 34.08 | - | - |
| nosys | 2.66 | 1.49 | 1.41 | 1.43 | 1.38 | 1.39 | 0.22 | 0.03 | 0.15 | - | - |
| sysonly | 11.74 | 7.46 | 7.26 | 7.07 | 7.02 | 6.18 | 4.31 | 4.21 | 2.96 | - | - |

