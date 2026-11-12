Runs: 224, valid 224; rounds 1,2,3,4; max temperature 51.6 C


### c01: full load (probe host, load on6, spin 2000 us + 0-1000 us, 3200 launches, 1 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | load vs P | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.014 | +0.000 | 2,579.8 | 2,546.4 | 1.000 (1.000-1.000) | - | - | - |
| W | 4 | 0.839 | +0.825 | 2,894.2 | 2,641.3 | 0.861 (0.850-0.870) | 291.7 | 16.2 / 554.8 / 3,562.7 / 11,022.0 | 9.66% |
| D | 4 | 0.036 | +0.022 | 2,703.1 | 2,603.6 | 0.989 (0.987-0.990) | 96.2 | 72.4 / 112.2 / 845.1 / 8,633.2 | 0.97% |
| AC3 | 4 | 0.038 | +0.023 | 2,702.2 | 2,608.9 | 0.989 (0.985-0.990) | 96.4 | 72.0 / 112.1 / 998.0 / 5,063.6 | 0.99% |
| M100 | 4 | 0.101 | +0.087 | 2,754.3 | 2,657.6 | 0.980 (0.976-0.980) | 148.6 | 68.8 / 114.8 / 2,624.8 / 5,442.8 | 3.48% |
| AC2 | 4 | 0.037 | +0.022 | 2,703.1 | 2,608.2 | 0.989 (0.987-0.989) | 98.1 | 72.4 / 112.1 / 1,279.0 / 5,275.3 | 1.15% |
| AC4 | 4 | 0.037 | +0.023 | 2,703.8 | 2,608.3 | 0.989 (0.987-0.990) | 95.9 | 72.7 / 112.2 / 854.3 / 5,706.3 | 0.93% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC3: req=3201 wfe_waits=0 slow_sleeps=73650 defer_sleeps=25094 hot_iters=0 parks=624 park_timeouts=577 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=65 win0=3201 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC2: req=3201 wfe_waits=0 slow_sleeps=73716 defer_sleeps=25060 hot_iters=0 parks=510 park_timeouts=462 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=67 win0=3201 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC4: req=3201 wfe_waits=0 slow_sleeps=73756 defer_sleeps=25099 hot_iters=0 parks=584 park_timeouts=532 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=62 win0=3201 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0

### c02: no load (power) (probe host, load off, spin 2000 us + 0-1000 us, 3200 launches, 1 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | VDD_CPU_GPU_CV (mW) | VDD_IN (mW) | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.019 | +0.000 | 2,557.8 | 2,557.9 | 1,551 | 5,734 | - | - | - |
| W | 4 | 1.012 | +0.993 | 2,567.1 | 2,567.6 | 1,626 | 5,805 | 16.0 | 15.6 / 16.9 / 17.4 / 847.6 | 0.00% |
| D | 4 | 0.068 | +0.049 | 2,637.9 | 2,623.1 | 1,620 | 5,800 | 71.1 | 71.5 / 113.8 / 123.9 / 1,716.7 | 0.01% |
| AC3 | 4 | 0.074 | +0.053 | 2,644.2 | 2,626.8 | 1,622 | 5,803 | 71.8 | 72.4 / 114.1 / 124.7 / 365.6 | 0.00% |
| M100 | 4 | 0.133 | +0.113 | 2,652.9 | 2,642.2 | 1,632 | 5,810 | 71.1 | 71.7 / 114.2 / 123.8 / 725.9 | 0.00% |
| AC2 | 4 | 0.070 | +0.050 | 2,641.4 | 2,627.2 | 1,622 | 5,808 | 71.6 | 72.2 / 114.3 / 124.5 / 276.3 | 0.00% |
| AC4 | 4 | 0.074 | +0.053 | 2,648.2 | 2,627.7 | 1,636 | 5,821 | 72.1 | 73.1 / 114.4 / 124.5 / 294.5 | 0.00% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC3: req=3201 wfe_waits=0 slow_sleeps=71240 defer_sleeps=25582 hot_iters=0 parks=348 park_timeouts=327 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=4 win0=3201 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC2: req=3201 wfe_waits=0 slow_sleeps=71150 defer_sleeps=25581 hot_iters=0 parks=342 park_timeouts=324 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=4 win0=3201 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC4: req=3201 wfe_waits=0 slow_sleeps=71206 defer_sleeps=25589 hot_iters=0 parks=348 park_timeouts=324 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=4 win0=3201 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0

### c05: burst, full load (probe host, load on6, spin 2000 us + 0-1000 us, 2400 launches, 32 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | load vs P | cost per call (us) |
|---|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.014 | +0.000 | 2,579.4 | 2,552.8 | 1.000 (1.000-1.000) | - |
| W | 4 | 0.803 | +0.789 | 3,292.8 | 3,000.8 | 0.868 (0.862-0.878) | 22.1 |
| D | 4 | 0.152 | +0.138 | 3,147.6 | 3,009.4 | 0.972 (0.969-0.984) | 17.8 |
| AC3 | 4 | 0.156 | +0.141 | 3,158.6 | 3,018.3 | 0.971 (0.969-0.983) | 18.3 |
| M100 | 4 | 0.196 | +0.182 | 3,235.1 | 3,053.6 | 0.966 (0.963-0.976) | 20.6 |
| AC2 | 4 | 0.140 | +0.127 | 3,386.2 | 3,049.4 | 0.972 (0.968-0.982) | 25.2 |
| AC4 | 4 | 0.135 | +0.121 | 3,378.6 | 3,097.1 | 0.974 (0.973-0.986) | 25.0 |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC3: req=76832 wfe_waits=0 slow_sleeps=48578 defer_sleeps=14250 hot_iters=4991001 parks=644 park_timeouts=516 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=474 win0=1 win20=2 win80=76829 win300=0 unseen_hits=0 unseen_unlatch=0
- AC2: req=76832 wfe_waits=0 slow_sleeps=53506 defer_sleeps=19492 hot_iters=4729661 parks=533 park_timeouts=493 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=5252 win0=7548 win20=0 win80=69284 win300=0 unseen_hits=0 unseen_unlatch=0
- AC4: req=76832 wfe_waits=0 slow_sleeps=54086 defer_sleeps=20035 hot_iters=4094290 parks=514 park_timeouts=473 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=4865 win0=4774 win20=71724 win80=413 win300=0 unseen_hits=0 unseen_unlatch=0

### c06: burst, no load (probe host, load off, spin 2000 us + 0-1000 us, 2400 launches, 32 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | VDD_CPU_GPU_CV (mW) | VDD_IN (mW) | cost per call (us) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.019 | +0.000 | 2,552.6 | 2,556.0 | 1,580 | 5,759 | - |
| W | 4 | 1.013 | +0.994 | 2,885.0 | 2,889.6 | 1,641 | 5,836 | 10.4 |
| D | 4 | 0.198 | +0.179 | 3,002.4 | 3,019.6 | 1,641 | 5,834 | 14.1 |
| AC3 | 4 | 0.196 | +0.177 | 2,999.4 | 3,021.3 | 1,648 | 5,851 | 14.0 |
| M100 | 4 | 0.239 | +0.220 | 3,011.9 | 3,003.2 | 1,633 | 5,834 | 14.4 |
| AC2 | 4 | 0.196 | +0.177 | 3,025.8 | 3,038.1 | 1,650 | 5,845 | 14.8 |
| AC4 | 4 | 0.189 | +0.170 | 3,028.3 | 3,031.8 | 1,645 | 5,842 | 14.9 |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC3: req=76832 wfe_waits=0 slow_sleeps=52262 defer_sleeps=19204 hot_iters=2550926 parks=334 park_timeouts=322 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=1 win0=2 win20=2 win80=76828 win300=0 unseen_hits=0 unseen_unlatch=0
- AC2: req=76832 wfe_waits=0 slow_sleeps=52320 defer_sleeps=21611 hot_iters=2683220 parks=340 park_timeouts=324 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=6 win0=2412 win20=0 win80=74420 win300=0 unseen_hits=0 unseen_unlatch=0
- AC4: req=76832 wfe_waits=0 slow_sleeps=53011 defer_sleeps=21654 hot_iters=2372336 parks=360 park_timeouts=324 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=8 win0=16 win20=76272 win80=539 win300=0 unseen_hits=0 unseen_unlatch=0

### c11: idle agent: one launch, then 8 s (probe host, load off, spin 2000 us + 0-0 us, 1 launches, 1 lane(s), gap 8000000 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | VDD_CPU_GPU_CV (mW) | VDD_IN (mW) | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.000 | +0.000 | 2,049.5 | 2,049.5 | 528 | 4,626 | - | - | - |
| W | 4 | 0.006 | +0.006 | 2,152.5 | 2,152.5 | 535 | 4,633 | 41.9 | 42.0 / 43.3 / 43.3 / 43.3 | 0.00% |
| D | 4 | 0.006 | +0.006 | 2,236.9 | 2,236.9 | 547 | 4,653 | 134.0 | 135.6 / 139.7 / 139.7 / 139.7 | 0.00% |
| AC3 | 4 | 0.006 | +0.006 | 2,193.2 | 2,193.2 | 535 | 4,636 | 89.0 | 86.0 / 140.1 / 140.1 / 140.1 | 0.00% |
| M100 | 4 | 0.030 | +0.030 | 2,168.5 | 2,168.5 | 558 | 4,663 | 58.0 | 58.7 / 71.9 / 71.9 / 71.9 | 0.00% |
| AC2 | 4 | 0.006 | +0.006 | 2,193.2 | 2,193.2 | 557 | 4,661 | 89.6 | 86.1 / 140.2 / 140.2 / 140.2 | 0.00% |
| AC4 | 4 | 0.006 | +0.006 | 2,189.1 | 2,189.1 | 547 | 4,650 | 93.1 | 79.1 / 144.7 / 144.7 / 144.7 | 0.00% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC3: req=2 wfe_waits=0 slow_sleeps=18 defer_sleeps=16 hot_iters=0 parks=8240 park_timeouts=8238 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=2 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC2: req=2 wfe_waits=0 slow_sleeps=19 defer_sleeps=16 hot_iters=0 parks=8240 park_timeouts=8238 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=2 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC4: req=2 wfe_waits=0 slow_sleeps=18 defer_sleeps=16 hot_iters=0 parks=8233 park_timeouts=8232 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=2 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0

### c17: load 5 ms on / 5 ms off (probe host, load duty6, spin 2000 us + 0-1000 us, 3200 launches, 1 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | load vs P | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.014 | +0.000 | 3,173.9 | 2,722.6 | 1.000 (1.000-1.000) | - | - | - |
| W | 4 | 0.817 | +0.803 | 3,279.0 | 2,803.7 | 1.113 (1.090-1.114) | 27.0 | 16.0 / 17.4 / 243.7 / 4,342.8 | 0.37% |
| D | 4 | 0.027 | +0.013 | 3,895.8 | 2,865.2 | 1.079 (0.987-1.093) | 1,075.9 | 93.0 / 3,896.6 / 4,929.5 / 5,088.5 | 30.20% |
| AC3 | 4 | 0.029 | +0.015 | 3,955.3 | 2,901.9 | 1.030 (1.001-1.054) | 1,106.7 | 94.7 / 3,897.6 / 4,935.4 / 5,056.2 | 31.07% |
| M100 | 4 | 0.077 | +0.063 | 3,845.5 | 2,862.8 | 1.031 (1.023-1.043) | 948.7 | 86.9 / 3,792.0 / 4,934.1 / 5,084.2 | 25.59% |
| AC2 | 4 | 0.028 | +0.013 | 3,889.2 | 2,862.3 | 1.069 (1.020-1.090) | 1,076.7 | 92.9 / 3,875.7 / 4,937.6 / 5,093.7 | 30.15% |
| AC4 | 4 | 0.028 | +0.014 | 3,898.6 | 2,862.8 | 1.074 (1.040-1.090) | 1,062.9 | 91.5 / 3,876.5 / 4,927.6 / 5,067.3 | 29.71% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC3: req=3201 wfe_waits=0 slow_sleeps=69752 defer_sleeps=15463 hot_iters=0 parks=703 park_timeouts=484 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=1233 win0=3201 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC2: req=3201 wfe_waits=0 slow_sleeps=69447 defer_sleeps=15920 hot_iters=0 parks=669 park_timeouts=500 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=1175 win0=3201 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC4: req=3201 wfe_waits=0 slow_sleeps=69962 defer_sleeps=16019 hot_iters=0 parks=692 park_timeouts=509 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=1168 win0=3201 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0

### c23: request gaps about 55-115 us, no load (probe host, load off, spin 30 us + 0-60 us, 4000 launches, 1 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | VDD_CPU_GPU_CV (mW) | VDD_IN (mW) | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.348 | +0.000 | 108.4 | 99.8 | - | - | - | - | - |
| W | 4 | 1.396 | +1.058 | 139.4 | 135.8 | - | - | 20.0 | 15.9 / 40.0 / 40.8 / 203.4 | 0.00% |
| D | 4 | 0.547 | +0.193 | 158.8 | 149.0 | - | - | 34.1 | 32.4 / 50.2 / 67.8 / 329.5 | 0.00% |
| AC3 | 4 | 0.512 | +0.166 | 156.2 | 149.4 | - | - | 34.8 | 32.7 / 51.2 / 69.3 / 3,843.4 | 0.01% |
| M100 | 4 | 1.261 | +0.912 | 147.9 | 135.7 | - | - | 24.1 | 16.6 / 40.4 / 98.9 / 147.3 | 0.00% |
| AC2 | 4 | 0.544 | +0.206 | 156.9 | 150.8 | - | - | 34.1 | 32.7 / 49.6 / 68.0 / 409.0 | 0.00% |
| AC4 | 4 | 0.609 | +0.267 | 164.3 | 167.2 | - | - | 34.1 | 32.6 / 49.5 / 67.9 / 386.1 | 0.00% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC3: req=4001 wfe_waits=0 slow_sleeps=48 defer_sleeps=21700 hot_iters=0 parks=324 park_timeouts=322 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=2 win0=4001 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC2: req=4001 wfe_waits=0 slow_sleeps=26 defer_sleeps=21934 hot_iters=0 parks=323 park_timeouts=322 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=2 win0=4001 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC4: req=4001 wfe_waits=0 slow_sleeps=42 defer_sleeps=22768 hot_iters=0 parks=324 park_timeouts=323 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=2 win0=4001 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0

### c25: 2 calls per launch, no load (probe host, load off, spin 2000 us + 0-1000 us, 2000 launches, 2 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | VDD_CPU_GPU_CV (mW) | VDD_IN (mW) | cost per call (us) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.019 | +0.000 | 2,554.3 | 2,557.0 | 1,543 | 5,705 | - |
| W | 4 | 1.015 | +0.996 | 2,575.8 | 2,580.5 | 1,600 | 5,781 | 9.5 |
| D | 4 | 0.086 | +0.067 | 2,650.9 | 2,658.9 | 1,608 | 5,784 | 48.2 |
| AC3 | 4 | 0.089 | +0.070 | 2,653.4 | 2,665.5 | 1,603 | 5,785 | 49.4 |
| M100 | 4 | 0.139 | +0.120 | 2,658.4 | 2,655.4 | 1,623 | 5,797 | 51.5 |
| AC2 | 4 | 0.089 | +0.070 | 2,674.8 | 2,690.1 | 1,606 | 5,793 | 60.0 |
| AC4 | 4 | 0.077 | +0.059 | 2,675.2 | 2,667.3 | 1,616 | 5,793 | 60.5 |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC3: req=4002 wfe_waits=0 slow_sleeps=43728 defer_sleeps=15996 hot_iters=376866 parks=334 park_timeouts=322 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=3 win0=2 win20=6 win80=3994 win300=0 unseen_hits=0 unseen_unlatch=0
- AC2: req=4002 wfe_waits=0 slow_sleeps=43710 defer_sleeps=18002 hot_iters=418386 parks=346 park_timeouts=327 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=2 win0=2002 win20=0 win80=2000 win300=0 unseen_hits=0 unseen_unlatch=0
- AC4: req=4002 wfe_waits=0 slow_sleeps=44298 defer_sleeps=17970 hot_iters=67568 parks=334 park_timeouts=326 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=6 win0=2002 win20=2000 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
