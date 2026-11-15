# C4 cost summary (bfc)

Medians over rounds of (item - empty kernel of the same shape), paired per round; [min-max] over rounds. ns per warp record = delta / warps. [projection] = 6.02M warp records x ns / 204.6 ms (step 0's pp512 figure, empty kernels). A smoke or a loaded board is not a measurement.

- rows dropped: 0 (gate capped), 0 (post-check failed); invalid items: none
- rounds: 10 of 10 requested; items with fewer usable rounds than that: none

## Shape anchor (decides)

- gate: record only 1.94 ns (step 0: 1.61, ratio 1.20); record + fence.sc.sys 10.31 ns (step 0: 10.06, ratio 1.02) -> **comparable to step 0**

### References (ns per warp)

| item | w32 | l8 | l8 per lane |
|---|---:|---:|---:|
| rec | 1.94 [1.92-1.96] | 2.07 [2.06-2.22] | 0.065 |
| rf_scgpu | 2.63 [2.62-2.64] | 2.13 [2.10-2.15] | 0.067 |
| rf_argpu | 2.63 [2.62-2.64] | 2.12 [2.11-2.14] | 0.066 |
| rf_scsys | 10.31 [10.26-10.37] | 8.12 [8.10-8.15] | 0.254 |
| rf_arsys | 10.31 [10.26-10.35] | 8.12 [8.10-8.15] | 0.254 |
| pw_sc | 10.91 [10.86-11.04] | 8.74 [8.69-8.76] | 0.273 |
| pw_ar | 10.92 [10.86-11.00] | 8.73 [8.71-8.76] | 0.273 |
| fonly_scgpu (lane 0, no store) | 0.89 [0.87-0.90] | | |
| fonly_argpu (lane 0, no store) | 0.88 [0.86-0.90] | | |
| fonly_scsys (lane 0, no store) | 6.50 [6.47-6.52] | | |
| fonly_arsys (lane 0, no store) | 6.49 [6.47-6.51] | | |

### w32: ns per warp record vs warps per system fence (G)

T = 2 x this run's record-only cost = 3.87 ns; secondary 3.22 ns (2 x step 0's 1.61) and 3.40 ns (10%).

| variant | G=1 | G=2 | G=4 | G=8 | G=16 | G=32 | G=48 | G=96 | G=384 | G=1536 | G=8192 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| nofence | 3.89 | 3.26* | 2.96* | 2.80* | 2.73* | 2.65* | 2.64* | 2.61* | 2.57* | 2.56* | 2.56* |
| nosys | 4.51 | 4.24 | 4.08 | 4.00 | 3.95 | 3.60* | 3.48* | 3.34* | 3.25* | 3.23* | 3.22* |
| sysonly | 14.52 | 10.94 | 8.58 | 6.72 | 6.12 | 4.36 | 3.82* | 3.43* | 3.05* | 2.78* | 2.70* |
| tf_sc | 19.48 | 15.57 | 13.00 | 10.58 | 9.90 | 7.48 | 6.40 | 5.11 | 3.79* | 3.52* | 3.34* |
| ar_sc | 19.44 | 15.59 | 13.01 | 10.58 | 9.90 | 7.47 | 6.40 | 5.11 | 3.80* | 3.51* | 3.35* |
| rel_sc | 18.87 | 14.95 | 13.13 | 10.60 | 9.82 | 7.32 | 6.23 | 4.98 | 3.69* | 3.40* | 3.24* |
| ar_ar | 15.41 | 13.81 | 12.38 | 11.73 | 11.70 | 8.11 | 6.71 | 5.13 | 3.79* | 3.48* | 3.32* |
| rel_ar | 14.95 | 13.63 | 12.23 | 11.62 | 11.54 | 7.97 | 6.59 | 5.02 | 3.67* | 3.35* | 3.22* |
| ar_aa | 19.50 | 15.60 | 13.00 | 10.62 | 9.90 | 7.48 | 6.41 | 5.10 | 3.80* | 3.52* | 3.35* |
| rmw_ar | 14.94 | 13.66 | 12.55 | 11.59 | 11.52 | 8.11 | 6.71 | 5.13 | 3.79* | 3.48* | 3.31* |
| ar_sc (counter in registered memory) | 100.97 | 165.40 | 227.14 | 279.34 | 337.34 | 391.85 | 437.55 | 509.54 | 608.94 | 638.67 | 644.01 |

(* = round max <= T)

Decision (anchor, w32):

- tf_sc: **G* = 384**, 3.79 ns per warp record (stable), [projection] 11.1%
- ar_sc: **G* = 384**, 3.80 ns per warp record (stable), [projection] 11.2%
- rel_sc: **G* = 384**, 3.69 ns per warp record (stable), [projection] 10.8%
- ar_ar (literal, shared fence): **G* = 384**, 3.79 ns per warp record (stable), [projection] 11.1%
- rel_ar (literal, shared fence): **G* = 384**, 3.67 ns per warp record (stable), [projection] 10.8%
- ar_aa (literal, GPU side): **G* = 384**, 3.80 ns per warp record (stable), [projection] 11.2%
- rmw_ar (literal, GPU side): **G* = 384**, 3.79 ns per warp record (stable), [projection] 11.2%

- ranking (smallest G*, then ns at G*; unreached last): rel_ar; rel_sc; tf_sc; ar_ar; rmw_ar; ar_sc; ar_aa
- **decided output (anchor, w32): the best literal protocol (GPU side) is rmw_ar (G* = 384, 3.79 ns, 11.2%)**; best with a shared completer fence: rel_ar (G* = 384, 3.67 ns, 10.8%); best overall: rel_ar (G* = 384, 3.67 ns, 10.8%)
- rmw_ar, the only variant whose acquire link is an acquire operation (no reading of fence.acq_rel as the 'acquire memory fence'): rmw_ar (G* = 384, 3.79 ns, 11.2%)
- every variant's GPU-to-host link rests on H1 (README, 'Virtual aliases'), so H1 does not change the ranking
- secondary thresholds: tf_sc: G(3.22)=None G(3.40)=8192; ar_sc: G(3.22)=None G(3.40)=8192; rel_sc: G(3.22)=None G(3.40)=8192; ar_ar: G(3.22)=None G(3.40)=8192; rel_ar: G(3.22)=8192 G(3.40)=1536; ar_aa: G(3.22)=None G(3.40)=8192; rmw_ar: G(3.22)=None G(3.40)=8192

Fence share = protocol - nofence at the same G (ns per warp record):

| variant | G=1 | G=2 | G=4 | G=8 | G=16 | G=32 | G=48 | G=96 | G=384 | G=1536 | G=8192 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| tf_sc | 15.59 | 12.30 | 10.04 | 7.78 | 7.17 | 4.83 | 3.75 | 2.50 | 1.22 | 0.96 | 0.78 |
| ar_sc | 15.55 | 12.33 | 10.05 | 7.79 | 7.17 | 4.82 | 3.75 | 2.50 | 1.22 | 0.95 | 0.79 |
| rel_sc | 14.98 | 11.68 | 10.17 | 7.80 | 7.09 | 4.66 | 3.59 | 2.37 | 1.12 | 0.84 | 0.69 |
| ar_ar | 11.52 | 10.54 | 9.42 | 8.94 | 8.97 | 5.46 | 4.07 | 2.52 | 1.22 | 0.92 | 0.77 |
| rel_ar | 11.06 | 10.36 | 9.27 | 8.82 | 8.81 | 5.32 | 3.94 | 2.41 | 1.10 | 0.80 | 0.66 |
| ar_aa | 15.61 | 12.34 | 10.04 | 7.82 | 7.18 | 4.83 | 3.77 | 2.50 | 1.22 | 0.96 | 0.79 |
| rmw_ar | 11.05 | 10.39 | 9.59 | 8.80 | 8.79 | 5.46 | 4.07 | 2.53 | 1.22 | 0.92 | 0.76 |
| nosys | 0.62 | 0.98 | 1.12 | 1.20 | 1.22 | 0.95 | 0.83 | 0.73 | 0.68 | 0.67 | 0.66 |
| sysonly | 10.63 | 7.67 | 5.62 | 3.92 | 3.40 | 1.70 | 1.17 | 0.82 | 0.47 | 0.22 | 0.14 |

### l8: ns per warp record vs warps per system fence (G)

T = 2 x this run's record-only cost = 4.14 ns; secondary 3.22 ns (2 x step 0's 1.61) and 3.40 ns (10%).

| variant | G=1 | G=2 | G=4 | G=8 | G=16 | G=32 | G=48 | G=96 | G=384 | G=1536 | G=8192 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| nofence | 4.00 | 3.54* | 3.21* | 2.92* | 2.63* | 2.37* | 2.33* | 2.32* | 2.24* | 2.17* | 2.16* |
| nosys | 4.12* | 3.79* | 3.73* | 3.70* | 3.77* | 3.24* | 3.13* | 3.02* | 3.00* | 2.95* | 2.94* |
| sysonly | 14.02 | 10.82 | 7.61 | 5.70 | 5.86 | 3.98* | 3.49* | 3.12* | 2.77* | 2.36* | 2.31* |
| tf_sc | 18.22 | 13.15 | 11.81 | 9.62 | 8.67 | 6.77 | 5.73 | 4.57 | 3.40* | 3.17* | 3.06* |
| ar_sc | 18.26 | 13.16 | 11.76 | 9.60 | 8.70 | 6.77 | 5.74 | 4.57 | 3.43* | 3.17* | 3.08* |
| rel_sc | 18.72 | 12.45 | 11.73 | 9.68 | 8.90 | 6.77 | 5.73 | 4.54 | 3.37* | 3.12* | 3.03* |
| ar_ar | 10.03 | 11.14 | 11.58 | 11.10 | 10.85 | 6.95 | 5.83 | 4.62 | 3.39* | 3.12* | 3.01* |
| rel_ar | 9.99 | 12.03 | 11.59 | 11.10 | 10.75 | 6.98 | 5.79 | 4.57 | 3.34* | 3.07* | 2.98* |
| ar_aa | 18.22 | 13.16 | 11.81 | 9.61 | 8.68 | 6.77 | 5.74 | 4.57 | 3.41* | 3.16* | 3.05* |
| rmw_ar | 10.92 | 12.02 | 11.72 | 11.03 | 10.66 | 6.94 | 5.79 | 4.57 | 3.37* | 3.10* | 3.00* |
| ar32_sc | 23.60 | 22.93 | 22.47 | 21.59 | 22.73 | 27.04 | 30.04 | 31.59 | 33.27 | 33.56 | 33.42 |
| ar32_ar | 21.01 | 18.43 | 20.28 | 20.31 | 22.44 | 26.42 | 29.58 | 31.34 | 33.23 | 33.59 | 33.37 |
| ar32_aa | 23.59 | 22.89 | 22.52 | 21.57 | 22.76 | 27.06 | 29.88 | 31.50 | 33.29 | 33.61 | 33.56 |

(* = round max <= T)

Decision (anchor, l8):

- tf_sc: **G* = 384**, 3.40 ns per warp record (stable), [projection] 10.0%
- ar_sc: **G* = 384**, 3.43 ns per warp record (stable), [projection] 10.1%
- rel_sc: **G* = 384**, 3.37 ns per warp record (stable), [projection] 9.9%
- ar_ar (literal, shared fence): **G* = 384**, 3.39 ns per warp record (stable), [projection] 10.0%
- rel_ar (literal, shared fence): **G* = 384**, 3.34 ns per warp record (stable), [projection] 9.8%
- ar_aa (literal, GPU side): **G* = 384**, 3.41 ns per warp record (stable), [projection] 10.0%
- rmw_ar (literal, GPU side): **G* = 384**, 3.37 ns per warp record (stable), [projection] 9.9%
- ar32_sc: **not under T in the sweep**; lowest 21.59 ns at G = 8, [projection] 63.5%
- ar32_ar (literal, shared fence): **not under T in the sweep**; lowest 18.43 ns at G = 2, [projection] 54.2%
- ar32_aa (literal, GPU side): **not under T in the sweep**; lowest 21.57 ns at G = 8, [projection] 63.5%

- ranking (smallest G*, then ns at G*; unreached last): rel_ar; rmw_ar; rel_sc; ar_ar; tf_sc; ar_aa; ar_sc; ar32_ar; ar32_aa; ar32_sc
- **decided output (anchor, l8): the best literal protocol (GPU side) is rmw_ar (G* = 384, 3.37 ns, 9.9%)**; best with a shared completer fence: rel_ar (G* = 384, 3.34 ns, 9.8%); best overall: rel_ar (G* = 384, 3.34 ns, 9.8%)
- rmw_ar, the only variant whose acquire link is an acquire operation (no reading of fence.acq_rel as the 'acquire memory fence'); in l8 its lane-0 release still rests on bar.warp.sync, and no l8 variant avoids both readings: rmw_ar (G* = 384, 3.37 ns, 9.9%)
- every variant's GPU-to-host link rests on H1 (README, 'Virtual aliases'), so H1 does not change the ranking
- secondary thresholds: tf_sc: G(3.22)=1536 G(3.40)=1536; ar_sc: G(3.22)=1536 G(3.40)=1536; rel_sc: G(3.22)=1536 G(3.40)=384; ar_ar: G(3.22)=1536 G(3.40)=384; rel_ar: G(3.22)=1536 G(3.40)=384; ar_aa: G(3.22)=1536 G(3.40)=1536; rmw_ar: G(3.22)=1536 G(3.40)=384; ar32_sc: G(3.22)=None G(3.40)=None; ar32_ar: G(3.22)=None G(3.40)=None; ar32_aa: G(3.22)=None G(3.40)=None

Fence share = protocol - nofence at the same G (ns per warp record):

| variant | G=1 | G=2 | G=4 | G=8 | G=16 | G=32 | G=48 | G=96 | G=384 | G=1536 | G=8192 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| tf_sc | 14.22 | 9.61 | 8.60 | 6.70 | 6.04 | 4.41 | 3.40 | 2.25 | 1.17 | 1.00 | 0.90 |
| ar_sc | 14.27 | 9.62 | 8.55 | 6.68 | 6.07 | 4.41 | 3.40 | 2.25 | 1.19 | 1.00 | 0.91 |
| rel_sc | 14.72 | 8.91 | 8.52 | 6.76 | 6.27 | 4.41 | 3.39 | 2.22 | 1.13 | 0.95 | 0.87 |
| ar_ar | 6.04 | 7.60 | 8.37 | 8.18 | 8.22 | 4.59 | 3.50 | 2.30 | 1.15 | 0.95 | 0.85 |
| rel_ar | 5.99 | 8.49 | 8.38 | 8.18 | 8.12 | 4.61 | 3.46 | 2.25 | 1.10 | 0.89 | 0.82 |
| ar_aa | 14.22 | 9.62 | 8.60 | 6.69 | 6.05 | 4.41 | 3.41 | 2.25 | 1.18 | 0.99 | 0.89 |
| rmw_ar | 6.92 | 8.48 | 8.51 | 8.11 | 8.03 | 4.57 | 3.46 | 2.25 | 1.13 | 0.93 | 0.84 |
| ar32_sc | 19.60 | 19.39 | 19.26 | 18.67 | 20.10 | 24.68 | 27.71 | 29.27 | 31.03 | 31.38 | 31.25 |
| ar32_ar | 17.01 | 14.89 | 17.07 | 17.39 | 19.81 | 24.05 | 27.25 | 29.01 | 31.00 | 31.42 | 31.21 |
| ar32_aa | 19.59 | 19.35 | 19.31 | 18.65 | 20.13 | 24.69 | 27.54 | 29.18 | 31.06 | 31.44 | 31.40 |
| nosys | 0.12 | 0.25 | 0.52 | 0.78 | 1.14 | 0.88 | 0.80 | 0.70 | 0.76 | 0.78 | 0.77 |
| sysonly | 10.02 | 7.28 | 4.40 | 2.78 | 3.23 | 1.61 | 1.15 | 0.80 | 0.53 | 0.19 | 0.15 |

## Shape large (secondary)

### References (ns per warp)

| item | w32 | l8 | l8 per lane |
|---|---:|---:|---:|
| rec | 1.18 [1.17-1.19] | 1.93 [1.92-2.13] | 0.060 |
| rf_scgpu | 2.27 [2.26-2.27] | 1.93 [1.93-1.95] | 0.060 |
| rf_argpu | 2.27 [2.26-2.27] | 1.93 [1.93-1.94] | 0.060 |
| rf_scsys | 9.76 [9.75-9.82] | 7.75 [7.73-7.78] | 0.242 |
| rf_arsys | 9.77 [9.74-9.86] | 7.75 [7.74-7.78] | 0.242 |
| pw_sc | 10.42 [10.38-10.43] | 8.29 [8.26-8.31] | 0.259 |
| pw_ar | 10.43 [10.37-10.45] | 8.28 [8.25-8.31] | 0.259 |
| fonly_scgpu (lane 0, no store) | 0.83 [0.82-0.83] | | |
| fonly_argpu (lane 0, no store) | 0.83 [0.82-0.83] | | |
| fonly_scsys (lane 0, no store) | 6.34 [6.32-6.35] | | |
| fonly_arsys (lane 0, no store) | 6.34 [6.32-6.35] | | |

### w32: ns per warp record vs warps per system fence (G)

T = 2 x this run's record-only cost = 2.36 ns; secondary 3.22 ns (2 x step 0's 1.61) and 3.40 ns (10%).

| variant | G=1 | G=2 | G=4 | G=8 | G=16 | G=32 | G=48 | G=96 | G=384 | G=1536 | G=8192 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| nofence | 4.29 | 3.17 | 2.67 | 2.46 | 2.37 | 2.32* | 2.29* | 2.26* | 2.23* | 2.22* | 2.21* |
| nosys | 5.15 | 4.01 | 3.86 | 3.75 | 3.70 | 3.33 | 3.20 | 3.08 | 2.98 | 2.98 | 2.97 |
| sysonly | 16.87 | 10.44 | 7.91 | 6.05 | 5.55 | 3.82 | 3.31 | 2.92 | 2.56 | 2.28* | 2.22* |
| tf_sc | 21.10 | 15.50 | 12.78 | 10.19 | 9.46 | 7.04 | 5.98 | 4.69 | 3.39 | 3.08 | 3.00 |
| ar_sc | 21.06 | 15.46 | 12.77 | 10.23 | 9.46 | 7.02 | 5.98 | 4.69 | 3.39 | 3.07 | 2.98 |
| rel_sc | 20.73 | 14.70 | 12.92 | 10.33 | 9.41 | 6.87 | 5.85 | 4.57 | 3.27 | 2.98 | 2.89 |
| ar_ar | 13.79 | 14.47 | 12.22 | 11.57 | 11.61 | 7.80 | 6.36 | 4.77 | 3.39 | 3.06 | 3.00 |
| rel_ar | 13.71 | 14.51 | 12.03 | 11.50 | 11.44 | 7.67 | 6.22 | 4.63 | 3.28 | 2.95 | 2.88 |
| ar_aa | 21.10 | 15.45 | 12.76 | 10.20 | 9.45 | 7.03 | 5.97 | 4.68 | 3.39 | 3.07 | 2.99 |
| rmw_ar | 13.70 | 14.60 | 12.41 | 11.45 | 11.44 | 7.77 | 6.37 | 4.76 | 3.41 | 3.04 | 2.95 |
| ar_sc (counter in registered memory) | 112.73 | 168.20 | 228.57 | 280.53 | 335.40 | 384.76 | 433.12 | 503.73 | 595.42 | 631.52 | 645.27 |

(* = round max <= T)

Decision (large, w32):

- tf_sc: **not under T in the sweep**; lowest 3.00 ns at G = 8192, [projection] 8.8%
- ar_sc: **not under T in the sweep**; lowest 2.98 ns at G = 8192, [projection] 8.8%
- rel_sc: **not under T in the sweep**; lowest 2.89 ns at G = 8192, [projection] 8.5%
- ar_ar (literal, shared fence): **not under T in the sweep**; lowest 3.00 ns at G = 8192, [projection] 8.8%
- rel_ar (literal, shared fence): **not under T in the sweep**; lowest 2.88 ns at G = 8192, [projection] 8.5%
- ar_aa (literal, GPU side): **not under T in the sweep**; lowest 2.99 ns at G = 8192, [projection] 8.8%
- rmw_ar (literal, GPU side): **not under T in the sweep**; lowest 2.95 ns at G = 8192, [projection] 8.7%

- ranking (smallest G*, then ns at G*; unreached last): rel_ar; rel_sc; rmw_ar; ar_sc; ar_aa; ar_ar; tf_sc
- **decided output (large, w32): the best literal protocol (GPU side) is rmw_ar (not reached; lowest 2.95 ns at G = 8192, 8.7%)**; best with a shared completer fence: rel_ar (not reached; lowest 2.88 ns at G = 8192, 8.5%); best overall: rel_ar (not reached; lowest 2.88 ns at G = 8192, 8.5%)
- rmw_ar, the only variant whose acquire link is an acquire operation (no reading of fence.acq_rel as the 'acquire memory fence'): rmw_ar (not reached; lowest 2.95 ns at G = 8192, 8.7%)
- every variant's GPU-to-host link rests on H1 (README, 'Virtual aliases'), so H1 does not change the ranking
- secondary thresholds: tf_sc: G(3.22)=1536 G(3.40)=384; ar_sc: G(3.22)=1536 G(3.40)=384; rel_sc: G(3.22)=1536 G(3.40)=384; ar_ar: G(3.22)=1536 G(3.40)=384; rel_ar: G(3.22)=1536 G(3.40)=384; ar_aa: G(3.22)=1536 G(3.40)=384; rmw_ar: G(3.22)=1536 G(3.40)=1536

Fence share = protocol - nofence at the same G (ns per warp record):

| variant | G=1 | G=2 | G=4 | G=8 | G=16 | G=32 | G=48 | G=96 | G=384 | G=1536 | G=8192 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| tf_sc | 16.80 | 12.34 | 10.12 | 7.73 | 7.09 | 4.72 | 3.69 | 2.42 | 1.16 | 0.86 | 0.79 |
| ar_sc | 16.76 | 12.29 | 10.11 | 7.77 | 7.09 | 4.71 | 3.69 | 2.42 | 1.17 | 0.85 | 0.77 |
| rel_sc | 16.44 | 11.53 | 10.26 | 7.87 | 7.04 | 4.55 | 3.55 | 2.31 | 1.04 | 0.76 | 0.68 |
| ar_ar | 9.50 | 11.31 | 9.55 | 9.11 | 9.24 | 5.48 | 4.06 | 2.51 | 1.16 | 0.84 | 0.79 |
| rel_ar | 9.42 | 11.34 | 9.36 | 9.03 | 9.07 | 5.35 | 3.93 | 2.36 | 1.05 | 0.73 | 0.68 |
| ar_aa | 16.81 | 12.28 | 10.09 | 7.74 | 7.08 | 4.71 | 3.67 | 2.41 | 1.16 | 0.85 | 0.79 |
| rmw_ar | 9.40 | 11.43 | 9.74 | 8.99 | 9.07 | 5.45 | 4.07 | 2.50 | 1.18 | 0.82 | 0.75 |
| nosys | 0.85 | 0.85 | 1.19 | 1.29 | 1.33 | 1.02 | 0.91 | 0.81 | 0.76 | 0.76 | 0.76 |
| sysonly | 12.58 | 7.27 | 5.24 | 3.59 | 3.18 | 1.50 | 1.01 | 0.65 | 0.33 | 0.06 | 0.02 |

### l8: ns per warp record vs warps per system fence (G)

T = 2 x this run's record-only cost = 3.85 ns; secondary 3.22 ns (2 x step 0's 1.61) and 3.40 ns (10%).

| variant | G=1 | G=2 | G=4 | G=8 | G=16 | G=32 | G=48 | G=96 | G=384 | G=1536 | G=8192 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| nofence | 4.52 | 3.73* | 3.15* | 2.80* | 2.51* | 2.25* | 2.18* | 2.08* | 2.00* | 1.96* | 1.94* |
| nosys | 5.05 | 3.73* | 3.53* | 3.36* | 3.41* | 2.99* | 2.88* | 2.77* | 2.74* | 2.64* | 2.62* |
| sysonly | 16.93 | 10.62 | 7.26 | 4.81 | 4.80 | 3.45* | 2.95* | 2.61* | 2.35* | 2.06* | 1.95* |
| tf_sc | 21.78 | 12.97 | 11.70 | 9.53 | 8.17 | 6.33 | 5.27 | 4.10 | 2.97* | 2.71* | 2.63* |
| ar_sc | 21.80 | 12.96 | 11.70 | 9.53 | 8.16 | 6.32 | 5.27 | 4.09 | 2.99* | 2.71* | 2.62* |
| rel_sc | 21.86 | 12.01 | 11.63 | 9.63 | 8.42 | 6.35 | 5.27 | 4.07 | 2.93* | 2.65* | 2.58* |
| ar_ar | 11.43 | 13.11 | 11.51 | 11.14 | 11.25 | 6.59 | 5.40 | 4.17 | 2.98* | 2.69* | 2.63* |
| rel_ar | 11.44 | 13.07 | 11.42 | 11.08 | 11.16 | 6.58 | 5.37 | 4.12 | 2.92* | 2.63* | 2.57* |
| ar_aa | 21.75 | 12.95 | 11.70 | 9.57 | 8.18 | 6.35 | 5.26 | 4.10 | 2.98* | 2.70* | 2.63* |
| rmw_ar | 11.81 | 13.39 | 11.65 | 11.05 | 11.21 | 6.59 | 5.39 | 4.14 | 2.94* | 2.65* | 2.59* |
| ar32_sc | 43.04 | 22.59 | 22.01 | 20.88 | 21.84 | 26.22 | 29.01 | 30.96 | 32.73 | 33.16 | 33.12 |
| ar32_ar | 36.05 | 21.11 | 20.05 | 19.74 | 21.64 | 25.78 | 28.77 | 31.04 | 32.83 | 33.09 | 33.26 |
| ar32_aa | 42.97 | 22.60 | 22.03 | 20.85 | 21.90 | 26.23 | 29.00 | 31.01 | 32.72 | 33.03 | 33.12 |

(* = round max <= T)

Decision (large, l8):

- tf_sc: **G* = 384**, 2.97 ns per warp record (stable), [projection] 8.7%
- ar_sc: **G* = 384**, 2.99 ns per warp record (stable), [projection] 8.8%
- rel_sc: **G* = 384**, 2.93 ns per warp record (stable), [projection] 8.6%
- ar_ar (literal, shared fence): **G* = 384**, 2.98 ns per warp record (stable), [projection] 8.8%
- rel_ar (literal, shared fence): **G* = 384**, 2.92 ns per warp record (stable), [projection] 8.6%
- ar_aa (literal, GPU side): **G* = 384**, 2.98 ns per warp record (stable), [projection] 8.8%
- rmw_ar (literal, GPU side): **G* = 384**, 2.94 ns per warp record (stable), [projection] 8.7%
- ar32_sc: **not under T in the sweep**; lowest 20.88 ns at G = 8, [projection] 61.4%
- ar32_ar (literal, shared fence): **not under T in the sweep**; lowest 19.74 ns at G = 8, [projection] 58.1%
- ar32_aa (literal, GPU side): **not under T in the sweep**; lowest 20.85 ns at G = 8, [projection] 61.3%

- ranking (smallest G*, then ns at G*; unreached last): rel_ar; rel_sc; rmw_ar; tf_sc; ar_aa; ar_ar; ar_sc; ar32_ar; ar32_aa; ar32_sc
- **decided output (large, l8): the best literal protocol (GPU side) is rmw_ar (G* = 384, 2.94 ns, 8.7%)**; best with a shared completer fence: rel_ar (G* = 384, 2.92 ns, 8.6%); best overall: rel_ar (G* = 384, 2.92 ns, 8.6%)
- rmw_ar, the only variant whose acquire link is an acquire operation (no reading of fence.acq_rel as the 'acquire memory fence'); in l8 its lane-0 release still rests on bar.warp.sync, and no l8 variant avoids both readings: rmw_ar (G* = 384, 2.94 ns, 8.7%)
- every variant's GPU-to-host link rests on H1 (README, 'Virtual aliases'), so H1 does not change the ranking
- secondary thresholds: tf_sc: G(3.22)=384 G(3.40)=384; ar_sc: G(3.22)=384 G(3.40)=384; rel_sc: G(3.22)=384 G(3.40)=384; ar_ar: G(3.22)=384 G(3.40)=384; rel_ar: G(3.22)=384 G(3.40)=384; ar_aa: G(3.22)=384 G(3.40)=384; rmw_ar: G(3.22)=384 G(3.40)=384; ar32_sc: G(3.22)=None G(3.40)=None; ar32_ar: G(3.22)=None G(3.40)=None; ar32_aa: G(3.22)=None G(3.40)=None

Fence share = protocol - nofence at the same G (ns per warp record):

| variant | G=1 | G=2 | G=4 | G=8 | G=16 | G=32 | G=48 | G=96 | G=384 | G=1536 | G=8192 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| tf_sc | 17.26 | 9.24 | 8.55 | 6.74 | 5.66 | 4.08 | 3.09 | 2.02 | 0.97 | 0.75 | 0.69 |
| ar_sc | 17.28 | 9.22 | 8.55 | 6.74 | 5.65 | 4.07 | 3.09 | 2.01 | 0.99 | 0.75 | 0.68 |
| rel_sc | 17.33 | 8.28 | 8.48 | 6.83 | 5.91 | 4.11 | 3.09 | 1.99 | 0.93 | 0.70 | 0.64 |
| ar_ar | 6.91 | 9.38 | 8.36 | 8.35 | 8.74 | 4.34 | 3.22 | 2.09 | 0.98 | 0.73 | 0.69 |
| rel_ar | 6.92 | 9.33 | 8.27 | 8.29 | 8.64 | 4.34 | 3.19 | 2.04 | 0.92 | 0.68 | 0.63 |
| ar_aa | 17.23 | 9.21 | 8.55 | 6.78 | 5.67 | 4.10 | 3.08 | 2.02 | 0.97 | 0.75 | 0.69 |
| rmw_ar | 7.29 | 9.66 | 8.50 | 8.26 | 8.70 | 4.34 | 3.20 | 2.06 | 0.94 | 0.70 | 0.65 |
| ar32_sc | 38.52 | 18.86 | 18.87 | 18.09 | 19.32 | 23.98 | 26.83 | 28.88 | 30.73 | 31.20 | 31.18 |
| ar32_ar | 31.53 | 17.37 | 16.90 | 16.94 | 19.13 | 23.53 | 26.59 | 28.96 | 30.83 | 31.13 | 31.32 |
| ar32_aa | 38.44 | 18.86 | 18.88 | 18.05 | 19.39 | 23.99 | 26.82 | 28.93 | 30.72 | 31.08 | 31.19 |
| nosys | 0.53 | -0.00 | 0.38 | 0.56 | 0.90 | 0.75 | 0.69 | 0.69 | 0.74 | 0.68 | 0.68 |
| sysonly | 12.41 | 6.89 | 4.11 | 2.02 | 2.28 | 1.20 | 0.77 | 0.53 | 0.35 | 0.11 | 0.02 |

## Shape full (secondary)

### References (ns per warp)

| item | w32 | l8 | l8 per lane |
|---|---:|---:|---:|
| rec | 7.14 [7.11-7.15] | 6.51 [5.97-6.56] | 0.203 |
| rf_scgpu | 6.78 [6.77-6.80] | 6.55 [6.51-6.57] | 0.205 |
| rf_argpu | 6.79 [6.77-6.80] | 6.54 [6.53-6.59] | 0.204 |
| rf_scsys | 10.91 [10.87-11.06] | 10.77 [10.23-10.81] | 0.337 |
| rf_arsys | 10.90 [10.86-10.93] | 10.77 [10.63-10.80] | 0.336 |
| pw_sc | 14.68 [14.49-14.80] | 13.49 [13.40-13.50] | 0.421 |
| pw_ar | 14.70 [14.50-14.87] | 13.47 [13.22-13.53] | 0.421 |
| fonly_scgpu (lane 0, no store) | 1.44 [1.42-1.48] | | |
| fonly_argpu (lane 0, no store) | 1.43 [1.42-1.47] | | |
| fonly_scsys (lane 0, no store) | 8.30 [8.24-8.35] | | |
| fonly_arsys (lane 0, no store) | 8.27 [8.07-8.35] | | |

### w32: ns per warp record vs warps per system fence (G)

T = 2 x this run's record-only cost = 14.27 ns; secondary 3.22 ns (2 x step 0's 1.61) and 3.40 ns (10%).

| variant | G=1 | G=2 | G=4 | G=8 | G=16 | G=32 | G=48 | G=96 | G=384 | G=1536 | G=8192 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| nofence | 7.08* | 6.76* | 6.68* | 6.77* | 6.72* | 6.73* | 6.74* | 6.76* | 6.71* | - | - |
| nosys | 9.53* | 9.23* | 9.30* | 9.23* | 8.98* | 9.03* | 9.00* | 8.86* | 8.75* | - | - |
| sysonly | 19.49 | 19.54 | 19.21 | 16.34 | 16.36 | 13.93* | 13.60* | 11.13* | 10.79* | - | - |
| tf_sc | 20.31 | 20.64 | 20.45 | 18.36 | 17.76 | 15.86 | 15.31 | 13.30* | 11.41* | - | - |
| ar_sc | 20.16 | 20.56 | 20.46 | 18.38 | 17.75 | 15.86 | 15.29 | 13.26* | 11.36* | - | - |
| rel_sc | 19.89 | 20.35 | 19.27 | 17.60 | 17.59 | 14.91 | 14.91 | 13.67* | 10.96* | - | - |
| ar_ar | 17.02 | 14.76 | 14.21 | 13.76* | 13.72* | 14.14* | 14.15 | 13.16* | 10.95* | - | - |
| rel_ar | 16.57 | 13.73* | 13.68* | 13.66* | 13.62* | 13.99* | 13.94* | 13.20* | 10.79* | - | - |
| ar_aa | 20.21 | 20.62 | 20.47 | 18.38 | 17.79 | 15.90 | 15.27 | 13.25* | 11.42* | - | - |
| rmw_ar | 18.10 | 15.24 | 13.80* | 13.80* | 13.75* | 14.21* | 14.12* | 14.18* | 10.92* | - | - |
| ar_sc (counter in registered memory) | 60.64 | 97.90 | 136.82 | 187.32 | 343.52 | 332.74 | 532.26 | 629.66 | 653.12 | - | - |

(* = round max <= T)

Decision (full, w32):

- tf_sc: **G* = 96**, 13.30 ns per warp record (stable), [projection] 39.1%
- ar_sc: **G* = 96**, 13.26 ns per warp record (stable), [projection] 39.0%
- rel_sc: **G* = 96**, 13.67 ns per warp record (stable), [projection] 40.2%
- ar_ar (literal, shared fence): **G* = 4**, 14.21 ns per warp record (marginal: round max 14.27 > T), [projection] 41.8%
- rel_ar (literal, shared fence): **G* = 2**, 13.73 ns per warp record (stable), [projection] 40.4%
- ar_aa (literal, GPU side): **G* = 96**, 13.25 ns per warp record (stable), [projection] 39.0%
- rmw_ar (literal, GPU side): **G* = 4**, 13.80 ns per warp record (stable), [projection] 40.6%

- ranking (smallest G*, then ns at G*; unreached last): rel_ar; rmw_ar; ar_ar; ar_aa; ar_sc; tf_sc; rel_sc
- **decided output (full, w32): the best literal protocol (GPU side) is rmw_ar (G* = 4, 13.80 ns, 40.6%)**; best with a shared completer fence: rel_ar (G* = 2, 13.73 ns, 40.4%); best overall: rel_ar (G* = 2, 13.73 ns, 40.4%)
- rmw_ar, the only variant whose acquire link is an acquire operation (no reading of fence.acq_rel as the 'acquire memory fence'): rmw_ar (G* = 4, 13.80 ns, 40.6%)
- every variant's GPU-to-host link rests on H1 (README, 'Virtual aliases'), so H1 does not change the ranking
- secondary thresholds: tf_sc: G(3.22)=None G(3.40)=None; ar_sc: G(3.22)=None G(3.40)=None; rel_sc: G(3.22)=None G(3.40)=None; ar_ar: G(3.22)=None G(3.40)=None; rel_ar: G(3.22)=None G(3.40)=None; ar_aa: G(3.22)=None G(3.40)=None; rmw_ar: G(3.22)=None G(3.40)=None

Fence share = protocol - nofence at the same G (ns per warp record):

| variant | G=1 | G=2 | G=4 | G=8 | G=16 | G=32 | G=48 | G=96 | G=384 | G=1536 | G=8192 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| tf_sc | 13.23 | 13.88 | 13.76 | 11.59 | 11.04 | 9.13 | 8.57 | 6.53 | 4.70 | - | - |
| ar_sc | 13.08 | 13.80 | 13.77 | 11.61 | 11.04 | 9.13 | 8.56 | 6.50 | 4.65 | - | - |
| rel_sc | 12.81 | 13.59 | 12.58 | 10.83 | 10.87 | 8.17 | 8.17 | 6.90 | 4.24 | - | - |
| ar_ar | 9.94 | 8.00 | 7.53 | 6.99 | 7.00 | 7.40 | 7.42 | 6.40 | 4.24 | - | - |
| rel_ar | 9.50 | 6.97 | 7.00 | 6.89 | 6.90 | 7.26 | 7.21 | 6.43 | 4.07 | - | - |
| ar_aa | 13.13 | 13.86 | 13.79 | 11.61 | 11.07 | 9.16 | 8.53 | 6.48 | 4.71 | - | - |
| rmw_ar | 11.02 | 8.48 | 7.12 | 7.03 | 7.03 | 7.47 | 7.38 | 7.41 | 4.21 | - | - |
| nosys | 2.45 | 2.47 | 2.62 | 2.46 | 2.26 | 2.29 | 2.26 | 2.10 | 2.04 | - | - |
| sysonly | 12.41 | 12.79 | 12.53 | 9.57 | 9.64 | 7.19 | 6.87 | 4.36 | 4.08 | - | - |

### l8: ns per warp record vs warps per system fence (G)

T = 2 x this run's record-only cost = 13.02 ns; secondary 3.22 ns (2 x step 0's 1.61) and 3.40 ns (10%).

| variant | G=1 | G=2 | G=4 | G=8 | G=16 | G=32 | G=48 | G=96 | G=384 | G=1536 | G=8192 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| nofence | 6.20* | 6.33* | 6.43* | 6.50* | 6.58* | 6.51* | 7.81* | 7.83* | 7.80* | - | - |
| nosys | 8.83* | 7.98* | 7.92* | 7.89* | 8.01* | 7.87* | 7.92* | 7.89* | 7.90* | - | - |
| sysonly | 17.68 | 14.18 | 13.47 | 13.53 | 13.45 | 12.89 | 13.33 | 12.01* | 10.66* | - | - |
| tf_sc | 18.44 | 17.21 | 14.81 | 13.59 | 14.72 | 12.04* | 13.12 | 12.25* | 10.66* | - | - |
| ar_sc | 18.42 | 17.17 | 14.74 | 13.58 | 14.74 | 12.05* | 13.12 | 12.25* | 10.66* | - | - |
| rel_sc | 17.36 | 16.25 | 14.17 | 12.05* | 14.44 | 11.95* | 13.36 | 12.00* | 10.61* | - | - |
| ar_ar | 13.61 | 13.53 | 13.47 | 13.49 | 13.43 | 12.20* | 12.70* | 11.83* | 10.64* | - | - |
| rel_ar | 13.53 | 13.45 | 13.38 | 13.40 | 13.32 | 12.58* | 13.50 | 13.04 | 10.60* | - | - |
| ar_aa | 18.46 | 17.25 | 14.83 | 13.56 | 14.70 | 12.06* | 13.15 | 12.25* | 10.67* | - | - |
| rmw_ar | 13.69 | 13.48 | 13.41 | 13.39 | 13.35 | 12.27* | 13.53 | 12.69* | 10.59* | - | - |
| ar32_sc | 28.95 | 27.42 | 28.64 | 33.67 | 40.35 | 45.17 | 47.44 | 43.15 | 41.33 | - | - |
| ar32_ar | 22.44 | 21.66 | 23.10 | 27.96 | 36.79 | 43.44 | 47.10 | 43.50 | 41.27 | - | - |
| ar32_aa | 28.93 | 27.39 | 28.73 | 33.75 | 40.47 | 45.23 | 47.43 | 43.25 | 41.36 | - | - |

(* = round max <= T)

Decision (full, l8):

- tf_sc: **G* = 32**, 12.04 ns per warp record (stable), [projection] 35.4%; non-monotone: above T again at G = [48]
- ar_sc: **G* = 32**, 12.05 ns per warp record (stable), [projection] 35.4%; non-monotone: above T again at G = [48]
- rel_sc: **G* = 8**, 12.05 ns per warp record (stable), [projection] 35.5%; non-monotone: above T again at G = [16, 48]
- ar_ar (literal, shared fence): **G* = 32**, 12.20 ns per warp record (stable), [projection] 35.9%
- rel_ar (literal, shared fence): **G* = 32**, 12.58 ns per warp record (stable), [projection] 37.0%; non-monotone: above T again at G = [48, 96]
- ar_aa (literal, GPU side): **G* = 32**, 12.06 ns per warp record (stable), [projection] 35.5%; non-monotone: above T again at G = [48]
- rmw_ar (literal, GPU side): **G* = 32**, 12.27 ns per warp record (stable), [projection] 36.1%; non-monotone: above T again at G = [48]
- ar32_sc: **not under T in the sweep**; lowest 27.42 ns at G = 2, [projection] 80.7%
- ar32_ar (literal, shared fence): **not under T in the sweep**; lowest 21.66 ns at G = 2, [projection] 63.7%
- ar32_aa (literal, GPU side): **not under T in the sweep**; lowest 27.39 ns at G = 2, [projection] 80.6%

- ranking (smallest G*, then ns at G*; unreached last): rel_sc; tf_sc; ar_sc; ar_aa; ar_ar; rmw_ar; rel_ar; ar32_ar; ar32_aa; ar32_sc
- **decided output (full, l8): the best literal protocol (GPU side) is ar_aa (G* = 32, 12.06 ns, 35.5%)**; best with a shared completer fence: ar_ar (G* = 32, 12.20 ns, 35.9%); best overall: rel_sc (G* = 8, 12.05 ns, 35.5%)
- rmw_ar, the only variant whose acquire link is an acquire operation (no reading of fence.acq_rel as the 'acquire memory fence'); in l8 its lane-0 release still rests on bar.warp.sync, and no l8 variant avoids both readings: rmw_ar (G* = 32, 12.27 ns, 36.1%)
- every variant's GPU-to-host link rests on H1 (README, 'Virtual aliases'), so H1 does not change the ranking
- secondary thresholds: tf_sc: G(3.22)=None G(3.40)=None; ar_sc: G(3.22)=None G(3.40)=None; rel_sc: G(3.22)=None G(3.40)=None; ar_ar: G(3.22)=None G(3.40)=None; rel_ar: G(3.22)=None G(3.40)=None; ar_aa: G(3.22)=None G(3.40)=None; rmw_ar: G(3.22)=None G(3.40)=None; ar32_sc: G(3.22)=None G(3.40)=None; ar32_ar: G(3.22)=None G(3.40)=None; ar32_aa: G(3.22)=None G(3.40)=None

Fence share = protocol - nofence at the same G (ns per warp record):

| variant | G=1 | G=2 | G=4 | G=8 | G=16 | G=32 | G=48 | G=96 | G=384 | G=1536 | G=8192 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| tf_sc | 12.24 | 10.88 | 8.39 | 7.09 | 8.14 | 5.53 | 5.31 | 4.42 | 2.85 | - | - |
| ar_sc | 12.22 | 10.84 | 8.32 | 7.08 | 8.17 | 5.54 | 5.32 | 4.42 | 2.86 | - | - |
| rel_sc | 11.16 | 9.92 | 7.74 | 5.55 | 7.87 | 5.44 | 5.55 | 4.17 | 2.81 | - | - |
| ar_ar | 7.41 | 7.20 | 7.04 | 6.99 | 6.85 | 5.69 | 4.89 | 3.99 | 2.84 | - | - |
| rel_ar | 7.32 | 7.13 | 6.95 | 6.89 | 6.74 | 6.07 | 5.70 | 5.20 | 2.79 | - | - |
| ar_aa | 12.26 | 10.92 | 8.40 | 7.06 | 8.13 | 5.55 | 5.35 | 4.42 | 2.87 | - | - |
| rmw_ar | 7.49 | 7.16 | 6.98 | 6.88 | 6.77 | 5.76 | 5.72 | 4.86 | 2.79 | - | - |
| ar32_sc | 22.75 | 21.09 | 22.21 | 27.17 | 33.78 | 38.66 | 39.64 | 35.31 | 33.52 | - | - |
| ar32_ar | 16.24 | 15.33 | 16.68 | 21.46 | 30.21 | 36.93 | 39.29 | 35.66 | 33.47 | - | - |
| ar32_aa | 22.73 | 21.06 | 22.31 | 27.24 | 33.90 | 38.72 | 39.62 | 35.42 | 33.56 | - | - |
| nosys | 2.63 | 1.65 | 1.49 | 1.39 | 1.43 | 1.36 | 0.11 | 0.06 | 0.10 | - | - |
| sysonly | 11.48 | 7.85 | 7.04 | 7.03 | 6.87 | 6.38 | 5.52 | 4.18 | 2.85 | - | - |

