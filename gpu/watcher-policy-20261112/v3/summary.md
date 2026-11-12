Runs: 448, valid 448; rounds 1,2,3,4; max temperature 54.9 C


### c31: steady request gaps under 50 us, no load (probe host, load off, spin 1 us + 0-0 us, 30000 launches, 1 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | VDD_CPU_GPU_CV (mW) | VDD_IN (mW) | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.771 | +0.000 | 27.0 | 26.9 | - | - | - | - | - |
| M100 | 4 | 1.686 | +0.910 | 50.9 | 50.6 | 2,384 | 7,853 | 14.5 | 14.5 / 15.0 / 15.3 / 45.6 | 0.00% |
| AC | 4 | 1.688 | +0.912 | 53.1 | 52.6 | 2,406 | 7,860 | 14.8 | 14.5 / 15.0 / 29.0 / 87.3 | 0.00% |
| AC2 | 4 | 1.360 | +0.594 | 52.1 | 47.5 | 2,296 | 7,772 | 23.0 | 15.1 / 37.4 / 38.6 / 177.8 | 0.00% |
| AC3 | 4 | 0.661 | -0.109 | 57.0 | 57.8 | 2,163 | 7,592 | 26.3 | 25.0 / 37.0 / 38.5 / 357.2 | 0.00% |
| AC4 | 4 | 0.658 | -0.112 | 57.4 | 57.5 | 2,178 | 7,648 | 24.8 | 23.7 / 37.1 / 38.5 / 149.6 | 0.00% |
| AC5 | 4 | 1.470 | +0.703 | 54.6 | 49.0 | 2,329 | 7,780 | 26.6 | 27.0 / 37.6 / 38.5 / 235.0 | 0.00% |
| ACn | 4 | 0.601 | -0.173 | 64.1 | 71.9 | 2,179 | 7,608 | 32.6 | 35.5 / 37.6 / 38.8 / 291.8 | 0.00% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC: req=30001 wfe_waits=0 slow_sleeps=10 defer_sleeps=1579 hot_iters=13894351 parks=294 park_timeouts=293 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=1554 guard_skips=0 win0=2 win20=4 win80=562 win300=29433 unseen_hits=0 unseen_unlatch=0
- AC2: req=30001 wfe_waits=0 slow_sleeps=11 defer_sleeps=15349 hot_iters=10419854 parks=297 park_timeouts=296 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=5161 win20=0 win80=24840 win300=0 unseen_hits=0 unseen_unlatch=0
- AC3: req=30001 wfe_waits=0 slow_sleeps=15 defer_sleeps=63154 hot_iters=1353982 parks=294 park_timeouts=294 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=4 win0=12386 win20=17615 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC4: req=30001 wfe_waits=0 slow_sleeps=18 defer_sleeps=65346 hot_iters=932266 parks=294 park_timeouts=294 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=2 win0=18017 win20=11984 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC5: req=30001 wfe_waits=0 slow_sleeps=10 defer_sleeps=7535 hot_iters=12960901 parks=296 park_timeouts=296 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=4 win20=4 win80=29994 win300=0 unseen_hits=0 unseen_unlatch=0
- ACn: req=30001 wfe_waits=0 slow_sleeps=11 defer_sleeps=78592 hot_iters=0 parks=297 park_timeouts=296 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=30001 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0

### c32: steady request gaps under 50 us, full load (probe host, load on6, spin 1 us + 0-0 us, 30000 launches, 1 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | load vs P | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.482 | +0.000 | 47.2 | 27.5 | 1.000 (1.000-1.000) | - | - | - |
| M100 | 4 | 0.725 | +0.278 | 149.2 | 59.3 | 0.965 (0.955-0.988) | 59.0 | 14.3 / 26.0 / 2,803.8 / 8,019.4 | 1.12% |
| AC | 4 | 0.590 | +0.122 | 165.3 | 59.8 | 0.994 (0.978-1.029) | 73.5 | 14.5 / 66.9 / 2,458.8 / 5,445.2 | 1.70% |
| AC2 | 4 | 0.387 | -0.093 | 127.6 | 74.5 | 1.008 (0.986-1.040) | 33.9 | 26.5 / 34.8 / 70.5 / 5,487.6 | 0.15% |
| AC3 | 4 | 0.393 | -0.095 | 124.3 | 74.5 | 1.004 (0.984-1.038) | 34.8 | 27.6 / 34.0 / 65.6 / 5,370.2 | 0.17% |
| AC4 | 4 | 0.387 | -0.089 | 126.7 | 74.3 | 1.006 (0.990-1.036) | 34.0 | 26.6 / 32.5 / 69.3 / 7,358.0 | 0.17% |
| AC5 | 4 | 0.506 | +0.028 | 164.7 | 72.2 | 1.005 (0.989-1.042) | 74.4 | 26.2 / 101.5 / 1,674.2 / 5,990.8 | 1.18% |
| ACn | 4 | 0.384 | -0.098 | 128.0 | 74.4 | 1.011 (0.984-1.042) | 34.2 | 27.3 / 31.3 / 63.4 / 5,596.7 | 0.20% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC: req=30001 wfe_waits=0 slow_sleeps=8708 defer_sleeps=2314 hot_iters=14418448 parks=1252 park_timeouts=866 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=1340 guard_skips=4126 win0=915 win20=1788 win80=15694 win300=11450 unseen_hits=0 unseen_unlatch=0
- AC2: req=30001 wfe_waits=0 slow_sleeps=6128 defer_sleeps=89160 hot_iters=81286 parks=1318 park_timeouts=989 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=1215 win0=29817 win20=0 win80=184 win300=0 unseen_hits=0 unseen_unlatch=0
- AC3: req=30001 wfe_waits=0 slow_sleeps=6046 defer_sleeps=88654 hot_iters=7016 parks=1220 park_timeouts=872 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=1107 win0=29910 win20=91 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC4: req=30001 wfe_waits=0 slow_sleeps=5906 defer_sleeps=87828 hot_iters=2638 parks=1274 park_timeouts=924 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=1400 win0=29967 win20=34 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC5: req=30001 wfe_waits=0 slow_sleeps=11443 defer_sleeps=28030 hot_iters=10343960 parks=1102 park_timeouts=767 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=6728 win0=933 win20=4498 win80=24366 win300=0 unseen_hits=0 unseen_unlatch=0
- ACn: req=30001 wfe_waits=0 slow_sleeps=5989 defer_sleeps=89690 hot_iters=0 parks=1294 park_timeouts=924 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=1168 win0=30001 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0

### c23: request gaps about 55-115 us, no load (probe host, load off, spin 30 us + 0-60 us, 4000 launches, 1 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | VDD_CPU_GPU_CV (mW) | VDD_IN (mW) | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.255 | +0.000 | 84.2 | 84.0 | - | - | - | - | - |
| M100 | 4 | 1.296 | +1.039 | 109.8 | 109.7 | - | - | 14.7 | 14.7 / 15.1 / 15.7 / 63.8 | 0.00% |
| AC | 4 | 1.281 | +1.028 | 111.8 | 111.8 | - | - | 15.0 | 14.6 / 15.4 / 28.9 / 112.9 | 0.00% |
| AC2 | 4 | 0.324 | +0.066 | 120.5 | 121.0 | - | - | 26.3 | 26.3 / 35.7 / 38.3 / 103.7 | 0.00% |
| AC3 | 4 | 0.352 | +0.104 | 121.8 | 121.0 | - | - | 26.0 | 25.6 / 35.7 / 38.3 / 109.6 | 0.00% |
| AC4 | 4 | 0.327 | +0.069 | 121.0 | 120.9 | - | - | 26.3 | 26.3 / 35.6 / 38.2 / 188.4 | 0.00% |
| AC5 | 4 | 0.365 | +0.110 | 123.3 | 121.2 | - | - | 25.7 | 25.2 / 35.9 / 38.3 / 106.6 | 0.00% |
| ACn | 4 | 0.353 | +0.105 | 123.2 | 121.6 | - | - | 25.9 | 25.4 / 35.7 / 38.3 / 323.6 | 0.00% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC: req=4001 wfe_waits=0 slow_sleeps=10 defer_sleeps=462 hot_iters=3962236 parks=294 park_timeouts=293 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=435 guard_skips=0 win0=1 win20=2 win80=2 win300=3996 unseen_hits=0 unseen_unlatch=0
- AC2: req=4001 wfe_waits=0 slow_sleeps=10 defer_sleeps=19902 hot_iters=0 parks=295 park_timeouts=294 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=4001 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC3: req=4001 wfe_waits=0 slow_sleeps=10 defer_sleeps=20102 hot_iters=0 parks=295 park_timeouts=294 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=4001 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC4: req=4001 wfe_waits=0 slow_sleeps=11 defer_sleeps=19974 hot_iters=0 parks=296 park_timeouts=294 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=4001 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC5: req=4001 wfe_waits=0 slow_sleeps=10 defer_sleeps=20376 hot_iters=0 parks=297 park_timeouts=296 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=4001 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- ACn: req=4001 wfe_waits=0 slow_sleeps=11 defer_sleeps=20265 hot_iters=0 parks=295 park_timeouts=294 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=4001 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0

### c24: request gaps about 55-115 us, full load (probe host, load on6, spin 30 us + 0-60 us, 4000 launches, 1 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | load vs P | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.253 | +0.000 | 131.7 | 93.4 | 1.000 (1.000-1.000) | - | - | - |
| M100 | 4 | 0.577 | +0.323 | 275.3 | 121.9 | 0.946 (0.940-0.955) | 121.6 | 14.5 / 15.4 / 4,008.6 / 9,418.0 | 2.74% |
| AC | 4 | 0.503 | +0.258 | 274.0 | 128.7 | 0.959 (0.953-0.963) | 120.4 | 14.9 / 90.8 / 3,877.3 / 5,912.4 | 2.80% |
| AC2 | 4 | 0.280 | +0.025 | 193.8 | 132.6 | 0.979 (0.975-0.985) | 37.3 | 26.1 / 37.8 / 109.2 / 5,453.8 | 0.28% |
| AC3 | 4 | 0.270 | +0.025 | 191.6 | 124.9 | 0.974 (0.965-0.977) | 36.2 | 26.2 / 37.9 / 110.7 / 5,872.3 | 0.26% |
| AC4 | 4 | 0.274 | +0.020 | 193.0 | 145.3 | 0.973 (0.967-0.984) | 36.5 | 26.6 / 38.2 / 108.7 / 4,961.2 | 0.26% |
| AC5 | 4 | 0.273 | +0.027 | 192.8 | 134.9 | 0.977 (0.961-0.981) | 35.7 | 26.0 / 37.8 / 108.5 / 5,569.7 | 0.23% |
| ACn | 4 | 0.265 | +0.016 | 192.1 | 125.2 | 0.981 (0.975-0.986) | 36.5 | 26.1 / 37.8 / 107.7 / 5,672.9 | 0.27% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC: req=4001 wfe_waits=0 slow_sleeps=1876 defer_sleeps=1027 hot_iters=3643858 parks=457 park_timeouts=406 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=337 guard_skips=910 win0=180 win20=346 win80=341 win300=3135 unseen_hits=0 unseen_unlatch=0
- AC2: req=4001 wfe_waits=0 slow_sleeps=995 defer_sleeps=21032 hot_iters=0 parks=496 park_timeouts=446 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=137 win0=4001 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC3: req=4001 wfe_waits=0 slow_sleeps=977 defer_sleeps=21070 hot_iters=0 parks=471 park_timeouts=422 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=137 win0=4001 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC4: req=4001 wfe_waits=0 slow_sleeps=1046 defer_sleeps=21064 hot_iters=0 parks=450 park_timeouts=406 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=144 win0=4001 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC5: req=4001 wfe_waits=0 slow_sleeps=1015 defer_sleeps=21104 hot_iters=2830 parks=474 park_timeouts=416 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=130 win0=3964 win20=37 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- ACn: req=4001 wfe_waits=0 slow_sleeps=1078 defer_sleeps=21030 hot_iters=0 parks=550 park_timeouts=500 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=128 win0=4001 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0

### c21: request gaps about 155-295 us, no load (probe host, load off, spin 130 us + 0-140 us, 4000 launches, 1 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | VDD_CPU_GPU_CV (mW) | VDD_IN (mW) | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.101 | +0.000 | 225.5 | 226.9 | - | - | - | - | - |
| M100 | 4 | 0.796 | +0.697 | 297.2 | 302.4 | 2,039 | 7,334 | 59.2 | 59.1 / 104.7 / 117.1 / 163.4 | 0.00% |
| AC | 4 | 0.183 | +0.082 | 301.9 | 300.9 | - | - | 65.0 | 63.9 / 111.0 / 118.0 / 142.0 | 0.00% |
| AC2 | 4 | 0.157 | +0.056 | 308.9 | 296.8 | 1,887 | 7,218 | 69.5 | 73.0 / 111.7 / 118.0 / 325.7 | 0.00% |
| AC3 | 4 | 0.158 | +0.057 | 308.0 | 297.1 | 1,891 | 7,173 | 69.3 | 72.4 / 111.9 / 118.2 / 126.6 | 0.00% |
| AC4 | 4 | 0.156 | +0.058 | 306.2 | 296.9 | 1,907 | 7,182 | 68.7 | 71.2 / 111.9 / 118.2 / 203.7 | 0.00% |
| AC5 | 4 | 0.151 | +0.052 | 305.1 | 296.7 | 1,945 | 7,285 | 68.1 | 69.7 / 111.3 / 117.9 / 1,336.0 | 0.01% |
| ACn | 4 | 0.155 | +0.054 | 307.1 | 297.0 | - | - | 69.2 | 72.0 / 111.9 / 118.6 / 251.6 | 0.00% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC: req=4001 wfe_waits=0 slow_sleeps=3958 defer_sleeps=31610 hot_iters=319187 parks=294 park_timeouts=292 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=8 guard_skips=0 win0=1996 win20=1867 win80=79 win300=51 unseen_hits=0 unseen_unlatch=0
- AC2: req=4001 wfe_waits=0 slow_sleeps=4468 defer_sleeps=31990 hot_iters=0 parks=294 park_timeouts=294 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=2 win0=4001 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC3: req=4001 wfe_waits=0 slow_sleeps=4424 defer_sleeps=32005 hot_iters=0 parks=294 park_timeouts=292 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=4001 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC4: req=4001 wfe_waits=0 slow_sleeps=4354 defer_sleeps=32008 hot_iters=0 parks=294 park_timeouts=293 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=4001 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC5: req=4001 wfe_waits=0 slow_sleeps=4316 defer_sleeps=32004 hot_iters=0 parks=296 park_timeouts=295 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=4001 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- ACn: req=4001 wfe_waits=0 slow_sleeps=4386 defer_sleeps=32004 hot_iters=0 parks=296 park_timeouts=295 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=4001 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0

### c25: 2 calls per launch, no load (probe host, load off, spin 2000 us + 0-1000 us, 2000 launches, 2 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | VDD_CPU_GPU_CV (mW) | VDD_IN (mW) | cost per call (us) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.010 | +0.000 | 2,526.2 | 2,530.8 | 1,686 | 6,928 | - |
| M100 | 4 | 0.105 | +0.095 | 2,613.2 | 2,649.9 | 1,739 | 6,998 | 43.2 |
| AC | 4 | 0.037 | +0.026 | 2,634.4 | 2,621.2 | 1,738 | 6,994 | 54.4 |
| AC2 | 4 | 0.050 | +0.040 | 2,629.7 | 2,657.1 | 1,730 | 6,992 | 51.8 |
| AC3 | 4 | 0.040 | +0.030 | 2,617.1 | 2,613.2 | 1,730 | 6,987 | 45.2 |
| AC4 | 4 | 0.035 | +0.025 | 2,631.2 | 2,620.4 | 1,739 | 6,996 | 52.5 |
| AC5 | 4 | 0.039 | +0.029 | 2,615.6 | 2,613.0 | 1,733 | 6,997 | 44.8 |
| ACn | 4 | 0.031 | +0.021 | 2,632.4 | 2,611.2 | 1,732 | 6,973 | 53.1 |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC: req=4002 wfe_waits=0 slow_sleeps=46242 defer_sleeps=18009 hot_iters=156016 parks=296 park_timeouts=295 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=2001 win20=2001 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC2: req=4002 wfe_waits=0 slow_sleeps=45410 defer_sleeps=18009 hot_iters=892342 parks=295 park_timeouts=294 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=2001 win20=0 win80=2001 win300=0 unseen_hits=0 unseen_unlatch=0
- AC3: req=4002 wfe_waits=0 slow_sleeps=46075 defer_sleeps=16010 hot_iters=392812 parks=296 park_timeouts=295 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=1 win20=4001 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC4: req=4002 wfe_waits=0 slow_sleeps=46204 defer_sleeps=18009 hot_iters=155557 parks=296 park_timeouts=295 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=2002 win20=2000 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC5: req=4002 wfe_waits=0 slow_sleeps=46064 defer_sleeps=16026 hot_iters=366496 parks=296 park_timeouts=294 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=33 win20=3969 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- ACn: req=4002 wfe_waits=0 slow_sleeps=46406 defer_sleeps=18006 hot_iters=0 parks=296 park_timeouts=296 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=4002 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0

### c26: 4 calls per launch, no load (probe host, load off, spin 2000 us + 0-1000 us, 2000 launches, 4 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | VDD_CPU_GPU_CV (mW) | VDD_IN (mW) | cost per call (us) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.010 | +0.000 | 2,525.6 | 2,529.8 | 1,685 | 6,920 | - |
| M100 | 4 | 0.110 | +0.100 | 2,629.8 | 2,667.8 | 1,745 | 7,004 | 26.0 |
| AC | 4 | 0.042 | +0.031 | 2,650.4 | 2,639.2 | 1,730 | 6,974 | 31.3 |
| AC2 | 4 | 0.057 | +0.047 | 2,650.4 | 2,675.9 | 1,735 | 6,983 | 31.2 |
| AC3 | 4 | 0.049 | +0.039 | 2,634.9 | 2,633.5 | 1,734 | 6,975 | 27.4 |
| AC4 | 4 | 0.042 | +0.031 | 2,651.1 | 2,638.9 | 1,735 | 6,986 | 31.4 |
| AC5 | 4 | 0.049 | +0.038 | 2,635.4 | 2,633.8 | 1,737 | 6,976 | 27.5 |
| ACn | 4 | 0.034 | +0.024 | 2,683.3 | 2,662.7 | 1,737 | 6,988 | 39.5 |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC: req=8004 wfe_waits=0 slow_sleeps=46181 defer_sleeps=18012 hot_iters=350955 parks=296 park_timeouts=296 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=2001 win20=6003 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC2: req=8004 wfe_waits=0 slow_sleeps=45462 defer_sleeps=18009 hot_iters=1104510 parks=296 park_timeouts=295 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=2001 win20=0 win80=6003 win300=0 unseen_hits=0 unseen_unlatch=0
- AC3: req=8004 wfe_waits=0 slow_sleeps=46010 defer_sleeps=16010 hot_iters=653777 parks=296 park_timeouts=296 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=1 win20=8003 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC4: req=8004 wfe_waits=0 slow_sleeps=46187 defer_sleeps=18005 hot_iters=358175 parks=295 park_timeouts=294 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=1996 win20=6008 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC5: req=8004 wfe_waits=0 slow_sleeps=45978 defer_sleeps=16014 hot_iters=619200 parks=296 park_timeouts=296 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=7 win20=7984 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- ACn: req=8004 wfe_waits=0 slow_sleeps=46376 defer_sleeps=22011 hot_iters=0 parks=295 park_timeouts=294 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=8004 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0

### c27: 8 calls per launch, no load (probe host, load off, spin 2000 us + 0-1000 us, 2000 launches, 8 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | VDD_CPU_GPU_CV (mW) | VDD_IN (mW) | cost per call (us) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.010 | +0.000 | 2,526.2 | 2,530.6 | 1,685 | 6,920 | - |
| M100 | 4 | 0.123 | +0.113 | 2,665.9 | 2,703.2 | 1,752 | 7,001 | 17.4 |
| AC | 4 | 0.056 | +0.046 | 2,688.4 | 2,678.0 | 1,734 | 6,976 | 20.3 |
| AC2 | 4 | 0.069 | +0.059 | 2,685.4 | 2,714.8 | 1,750 | 7,000 | 19.9 |
| AC3 | 4 | 0.062 | +0.052 | 2,673.5 | 2,673.0 | 1,731 | 6,981 | 18.4 |
| AC4 | 4 | 0.057 | +0.047 | 2,691.3 | 2,679.2 | 1,743 | 7,003 | 20.7 |
| AC5 | 4 | 0.062 | +0.052 | 2,672.8 | 2,673.2 | 1,733 | 6,983 | 18.3 |
| ACn | 4 | 0.035 | +0.024 | 2,781.4 | 2,763.3 | 1,731 | 6,975 | 31.9 |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC: req=16008 wfe_waits=0 slow_sleeps=46162 defer_sleeps=18027 hot_iters=776296 parks=296 park_timeouts=295 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=2001 win20=14007 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC2: req=16008 wfe_waits=0 slow_sleeps=45350 defer_sleeps=18010 hot_iters=1511864 parks=296 park_timeouts=294 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=2002 win20=0 win80=14006 win300=0 unseen_hits=0 unseen_unlatch=0
- AC3: req=16008 wfe_waits=0 slow_sleeps=45985 defer_sleeps=16010 hot_iters=1066266 parks=297 park_timeouts=296 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=1 win20=16007 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC4: req=16008 wfe_waits=0 slow_sleeps=46217 defer_sleeps=18000 hot_iters=770106 parks=296 park_timeouts=294 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=1984 win20=14024 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC5: req=16008 wfe_waits=0 slow_sleeps=45966 defer_sleeps=16016 hot_iters=1079760 parks=294 park_timeouts=294 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=8 win20=15992 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- ACn: req=16008 wfe_waits=0 slow_sleeps=46327 defer_sleeps=30015 hot_iters=0 parks=294 park_timeouts=292 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=16008 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0

### c28: 4 calls per launch, full load (probe host, load on6, spin 2000 us + 0-1000 us, 2000 launches, 4 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | load vs P | cost per call (us) |
|---|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.013 | +0.000 | 2,571.3 | 2,546.1 | 1.000 (1.000-1.000) | - |
| M100 | 4 | 0.109 | +0.096 | 2,779.5 | 2,683.6 | 0.978 (0.976-0.981) | 52.0 |
| AC | 4 | 0.047 | +0.034 | 2,766.3 | 2,661.8 | 0.986 (0.982-0.990) | 49.1 |
| AC2 | 4 | 0.059 | +0.046 | 2,768.8 | 2,697.4 | 0.986 (0.985-0.989) | 49.2 |
| AC3 | 4 | 0.053 | +0.040 | 2,734.7 | 2,654.0 | 0.986 (0.985-0.990) | 40.6 |
| AC4 | 4 | 0.047 | +0.033 | 2,757.4 | 2,659.7 | 0.988 (0.986-0.991) | 46.6 |
| AC5 | 4 | 0.055 | +0.042 | 2,735.5 | 2,657.0 | 0.986 (0.982-0.990) | 41.4 |
| ACn | 4 | 0.038 | +0.025 | 2,792.2 | 2,684.6 | 0.988 (0.986-0.991) | 55.2 |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC: req=8004 wfe_waits=0 slow_sleeps=45650 defer_sleeps=17420 hot_iters=352950 parks=542 park_timeouts=512 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=114 win0=2004 win20=6000 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC2: req=8004 wfe_waits=0 slow_sleeps=44560 defer_sleeps=17054 hot_iters=1084798 parks=498 park_timeouts=466 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=296 win0=2214 win20=0 win80=5790 win300=0 unseen_hits=0 unseen_unlatch=0
- AC3: req=8004 wfe_waits=0 slow_sleeps=45015 defer_sleeps=15275 hot_iters=667254 parks=522 park_timeouts=474 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=76 win0=1 win20=7924 win80=79 win300=0 unseen_hits=0 unseen_unlatch=0
- AC4: req=8004 wfe_waits=0 slow_sleeps=45708 defer_sleeps=17380 hot_iters=351204 parks=558 park_timeouts=528 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=209 win0=2144 win20=5860 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC5: req=8004 wfe_waits=0 slow_sleeps=44806 defer_sleeps=15232 hot_iters=723514 parks=514 park_timeouts=466 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=79 win0=2 win20=6024 win80=1954 win300=0 unseen_hits=0 unseen_unlatch=0
- ACn: req=8004 wfe_waits=0 slow_sleeps=46160 defer_sleeps=21562 hot_iters=0 parks=478 park_timeouts=448 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=150 win0=8004 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0

### c05: burst, full load (probe host, load on6, spin 2000 us + 0-1000 us, 2400 launches, 32 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | load vs P | cost per call (us) |
|---|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.013 | +0.000 | 2,580.1 | 2,547.9 | 1.000 (1.000-1.000) | - |
| M100 | 4 | 0.175 | +0.162 | 3,119.1 | 2,935.2 | 0.969 (0.968-0.969) | 16.9 |
| AC | 4 | 0.128 | +0.115 | 3,111.6 | 3,012.0 | 0.974 (0.973-0.976) | 16.6 |
| AC2 | 4 | 0.128 | +0.115 | 3,280.8 | 2,977.4 | 0.974 (0.973-0.975) | 22.1 |
| AC3 | 4 | 0.136 | +0.122 | 3,080.2 | 2,952.4 | 0.972 (0.971-0.974) | 15.7 |
| AC4 | 4 | 0.118 | +0.105 | 3,247.5 | 3,029.9 | 0.975 (0.973-0.976) | 20.7 |
| AC5 | 4 | 0.138 | +0.125 | 3,079.6 | 2,946.0 | 0.972 (0.972-0.973) | 15.6 |
| ACn | 4 | 0.051 | +0.037 | 3,623.9 | 3,418.3 | 0.984 (0.982-0.985) | 32.6 |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC: req=76832 wfe_waits=0 slow_sleeps=51030 defer_sleeps=17734 hot_iters=3995409 parks=539 park_timeouts=478 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=616 win0=2409 win20=74410 win80=13 win300=0 unseen_hits=0 unseen_unlatch=0
- AC2: req=76832 wfe_waits=0 slow_sleeps=53446 defer_sleeps=19868 hot_iters=4532622 parks=504 park_timeouts=463 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=4506 win0=6818 win20=0 win80=70014 win300=0 unseen_hits=0 unseen_unlatch=0
- AC3: req=76832 wfe_waits=0 slow_sleeps=49876 defer_sleeps=15154 hot_iters=4398934 parks=653 park_timeouts=548 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=386 win0=2 win20=59422 win80=17408 win300=0 unseen_hits=0 unseen_unlatch=0
- AC4: req=76832 wfe_waits=0 slow_sleeps=54543 defer_sleeps=19796 hot_iters=3713012 parks=472 park_timeouts=435 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=4120 win0=6289 win20=70543 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC5: req=76832 wfe_waits=0 slow_sleeps=49704 defer_sleeps=14944 hot_iters=4585713 parks=604 park_timeouts=499 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=406 win0=2 win20=16280 win80=60548 win300=0 unseen_hits=0 unseen_unlatch=0
- ACn: req=76832 wfe_waits=0 slow_sleeps=56698 defer_sleeps=91714 hot_iters=0 parks=467 park_timeouts=428 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=1564 win0=76832 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0

### c06: burst, no load (probe host, load off, spin 2000 us + 0-1000 us, 2400 launches, 32 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | VDD_CPU_GPU_CV (mW) | VDD_IN (mW) | cost per call (us) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.010 | +0.000 | 2,527.7 | 2,531.2 | 1,729 | 6,920 | - |
| M100 | 4 | 0.192 | +0.181 | 2,887.9 | 2,926.2 | 1,776 | 7,033 | 11.3 |
| AC | 4 | 0.133 | +0.123 | 2,927.4 | 2,914.9 | 1,771 | 7,040 | 12.5 |
| AC2 | 4 | 0.144 | +0.134 | 2,918.4 | 2,948.8 | 1,776 | 6,998 | 12.2 |
| AC3 | 4 | 0.143 | +0.133 | 2,903.8 | 2,923.8 | 1,774 | 7,041 | 11.8 |
| AC4 | 4 | 0.131 | +0.120 | 2,919.4 | 2,909.9 | 1,773 | 7,026 | 12.2 |
| AC5 | 4 | 0.138 | +0.127 | 2,902.4 | 2,903.2 | 1,769 | 7,018 | 11.7 |
| ACn | 4 | 0.049 | +0.039 | 3,391.4 | 3,371.2 | 1,769 | 7,020 | 27.0 |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC: req=76832 wfe_waits=0 slow_sleeps=55422 defer_sleeps=21730 hot_iters=3977309 parks=294 park_timeouts=294 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=2401 win20=74431 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC2: req=76832 wfe_waits=0 slow_sleeps=54442 defer_sleeps=21611 hot_iters=4759338 parks=295 park_timeouts=294 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=2402 win20=0 win80=74430 win300=0 unseen_hits=0 unseen_unlatch=0
- AC3: req=76832 wfe_waits=0 slow_sleeps=54785 defer_sleeps=19213 hot_iters=4616856 parks=294 park_timeouts=294 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=1 win20=20457 win80=56374 win300=0 unseen_hits=0 unseen_unlatch=0
- AC4: req=76832 wfe_waits=0 slow_sleeps=55388 defer_sleeps=21584 hot_iters=3933403 parks=294 park_timeouts=294 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=2262 win20=74570 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC5: req=76832 wfe_waits=0 slow_sleeps=55196 defer_sleeps=19224 hot_iters=4199386 parks=294 park_timeouts=294 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=10 win20=76813 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- ACn: req=76832 wfe_waits=0 slow_sleeps=55667 defer_sleeps=93633 hot_iters=0 parks=294 park_timeouts=294 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=4 win0=76832 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0

### c01: full load (probe host, load on6, spin 2000 us + 0-1000 us, 3200 launches, 1 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | load vs P | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.013 | +0.000 | 2,574.9 | 2,542.1 | 1.000 (1.000-1.000) | - | - | - |
| M100 | 4 | 0.100 | +0.087 | 2,745.6 | 2,656.4 | 0.982 (0.980-0.982) | 148.7 | 67.6 / 113.3 / 2,577.1 / 13,737.8 | 3.52% |
| AC | 4 | 0.037 | +0.024 | 2,698.9 | 2,617.7 | 0.989 (0.988-0.991) | 91.0 | 70.4 / 110.2 / 617.4 / 5,624.0 | 0.84% |
| AC2 | 4 | 0.036 | +0.022 | 2,697.5 | 2,605.7 | 0.989 (0.988-0.990) | 94.4 | 70.8 / 110.6 / 1,130.0 / 5,285.5 | 1.02% |
| AC3 | 4 | 0.036 | +0.022 | 2,697.3 | 2,606.4 | 0.989 (0.988-0.990) | 95.0 | 70.9 / 110.8 / 1,154.7 / 5,504.1 | 1.05% |
| AC4 | 4 | 0.036 | +0.023 | 2,697.4 | 2,606.2 | 0.990 (0.989-0.991) | 95.4 | 71.5 / 110.8 / 1,064.9 / 5,786.3 | 1.02% |
| AC5 | 4 | 0.035 | +0.022 | 2,694.6 | 2,606.1 | 0.989 (0.989-0.989) | 92.8 | 71.0 / 110.7 / 798.0 / 10,035.3 | 0.94% |
| ACn | 4 | 0.037 | +0.023 | 2,699.2 | 2,606.0 | 0.990 (0.989-0.990) | 95.2 | 71.2 / 110.8 / 1,113.2 / 4,659.6 | 1.08% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC: req=3201 wfe_waits=0 slow_sleeps=73684 defer_sleeps=25094 hot_iters=0 parks=574 park_timeouts=525 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=64 win0=3201 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC2: req=3201 wfe_waits=0 slow_sleeps=73658 defer_sleeps=25102 hot_iters=0 parks=579 park_timeouts=528 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=62 win0=3201 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC3: req=3201 wfe_waits=0 slow_sleeps=73656 defer_sleeps=25098 hot_iters=0 parks=495 park_timeouts=452 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=62 win0=3201 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC4: req=3201 wfe_waits=0 slow_sleeps=73650 defer_sleeps=25115 hot_iters=0 parks=544 park_timeouts=492 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=59 win0=3201 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC5: req=3201 wfe_waits=0 slow_sleeps=73609 defer_sleeps=25099 hot_iters=393 parks=542 park_timeouts=500 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=60 win0=3196 win20=5 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- ACn: req=3201 wfe_waits=0 slow_sleeps=73686 defer_sleeps=25088 hot_iters=0 parks=580 park_timeouts=532 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=62 win0=3201 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0

### c02: no load (power) (probe host, load off, spin 2000 us + 0-1000 us, 3200 launches, 1 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | VDD_CPU_GPU_CV (mW) | VDD_IN (mW) | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.010 | +0.000 | 2,525.5 | 2,526.7 | 1,690 | 6,917 | - | - | - |
| M100 | 4 | 0.102 | +0.092 | 2,602.8 | 2,640.2 | 1,741 | 6,993 | 64.7 | 63.8 / 107.1 / 118.2 / 149.7 | 0.00% |
| AC | 4 | 0.031 | +0.021 | 2,606.4 | 2,586.5 | 1,732 | 6,967 | 67.7 | 69.6 / 108.6 / 117.9 / 147.7 | 0.00% |
| AC2 | 4 | 0.030 | +0.020 | 2,605.9 | 2,586.4 | 1,728 | 6,958 | 67.8 | 69.4 / 108.6 / 117.9 / 704.9 | 0.00% |
| AC3 | 4 | 0.030 | +0.020 | 2,604.3 | 2,585.6 | 1,727 | 6,962 | 67.6 | 69.3 / 108.3 / 117.8 / 178.0 | 0.00% |
| AC4 | 4 | 0.031 | +0.020 | 2,605.9 | 2,585.9 | 1,731 | 6,961 | 67.8 | 69.3 / 108.7 / 118.0 / 139.0 | 0.00% |
| AC5 | 4 | 0.030 | +0.020 | 2,604.9 | 2,586.0 | 1,730 | 6,967 | 67.7 | 69.4 / 108.5 / 117.8 / 128.3 | 0.00% |
| ACn | 4 | 0.030 | +0.020 | 2,604.9 | 2,586.1 | 1,732 | 6,973 | 67.7 | 69.4 / 108.4 / 117.7 / 476.0 | 0.00% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC: req=3201 wfe_waits=0 slow_sleeps=74209 defer_sleeps=25608 hot_iters=0 parks=294 park_timeouts=294 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=3201 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC2: req=3201 wfe_waits=0 slow_sleeps=74219 defer_sleeps=25608 hot_iters=0 parks=297 park_timeouts=296 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=3201 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC3: req=3201 wfe_waits=0 slow_sleeps=74157 defer_sleeps=25608 hot_iters=0 parks=296 park_timeouts=295 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=3201 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC4: req=3201 wfe_waits=0 slow_sleeps=74188 defer_sleeps=25608 hot_iters=0 parks=293 park_timeouts=292 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=3201 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC5: req=3201 wfe_waits=0 slow_sleeps=74167 defer_sleeps=25608 hot_iters=0 parks=296 park_timeouts=295 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=3201 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- ACn: req=3201 wfe_waits=0 slow_sleeps=74157 defer_sleeps=25600 hot_iters=0 parks=294 park_timeouts=294 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=1 win0=3201 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0

### c15: short kernels, full load (probe host, load on6, spin 200 us + 0-200 us, 4000 launches, 1 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | load vs P | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.082 | +0.000 | 369.6 | 335.7 | 1.000 (1.000-1.000) | - | - | - |
| M100 | 4 | 0.379 | +0.299 | 653.3 | 462.6 | 0.947 (0.945-0.949) | 253.0 | 70.6 / 114.9 / 3,989.7 / 5,654.5 | 6.62% |
| AC | 4 | 0.124 | +0.042 | 476.8 | 405.6 | 0.981 (0.981-0.982) | 80.7 | 68.7 / 109.4 / 118.8 / 5,527.7 | 0.46% |
| AC2 | 4 | 0.129 | +0.047 | 483.2 | 406.4 | 0.981 (0.976-0.982) | 80.8 | 69.1 / 110.2 / 119.6 / 5,980.6 | 0.46% |
| AC3 | 4 | 0.127 | +0.044 | 477.0 | 405.6 | 0.981 (0.975-0.984) | 81.7 | 69.1 / 109.4 / 119.3 / 11,791.6 | 0.46% |
| AC4 | 4 | 0.125 | +0.046 | 480.4 | 405.8 | 0.981 (0.981-0.982) | 80.8 | 68.9 / 109.2 / 118.8 / 5,497.8 | 0.44% |
| AC5 | 4 | 0.134 | +0.051 | 482.8 | 408.2 | 0.980 (0.979-0.981) | 80.4 | 69.0 / 109.9 / 119.5 / 5,562.8 | 0.44% |
| ACn | 4 | 0.123 | +0.040 | 476.5 | 405.6 | 0.980 (0.979-0.981) | 80.7 | 68.9 / 109.6 / 119.2 / 5,804.0 | 0.44% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC: req=4001 wfe_waits=0 slow_sleeps=9066 defer_sleeps=31084 hot_iters=14384 parks=612 park_timeouts=559 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=108 win0=3816 win20=186 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC2: req=4001 wfe_waits=0 slow_sleeps=9219 defer_sleeps=31050 hot_iters=0 parks=504 park_timeouts=451 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=112 win0=4001 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC3: req=4001 wfe_waits=0 slow_sleeps=9126 defer_sleeps=31054 hot_iters=0 parks=518 park_timeouts=462 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=108 win0=4001 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC4: req=4001 wfe_waits=0 slow_sleeps=9224 defer_sleeps=31130 hot_iters=0 parks=586 park_timeouts=522 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=103 win0=4001 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC5: req=4001 wfe_waits=0 slow_sleeps=9202 defer_sleeps=30989 hot_iters=548 parks=552 park_timeouts=502 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=122 win0=3994 win20=7 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- ACn: req=4001 wfe_waits=0 slow_sleeps=9140 defer_sleeps=31142 hot_iters=0 parks=581 park_timeouts=520 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=100 win0=4001 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
