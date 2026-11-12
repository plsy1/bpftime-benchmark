Runs: 304, valid 304; rounds 1,2,3,4; max temperature 54.1 C


### c21: request gaps about 155-295 us, no load (probe host, load off, spin 130 us + 0-140 us, 4000 launches, 1 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | VDD_CPU_GPU_CV (mW) | VDD_IN (mW) | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.101 | +0.000 | 226.0 | 227.1 | - | - | - | - | - |
| M100 | 4 | 0.793 | +0.695 | 295.6 | 257.5 | 2,003 | 7,370 | 59.1 | 59.2 / 105.0 / 117.3 / 120.7 | 0.00% |
| AC | 4 | 0.189 | +0.091 | 305.0 | 302.8 | - | - | 66.4 | 66.8 / 111.3 / 118.0 / 272.3 | 0.00% |
| AC2 | 4 | 0.148 | +0.048 | 304.0 | 296.6 | - | - | 68.2 | 69.2 / 111.8 / 118.3 / 2,594.2 | 0.01% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC: req=4001 wfe_waits=0 slow_sleeps=4070 defer_sleeps=31696 hot_iters=275076 parks=293 park_timeouts=292 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=5 guard_skips=0 win0=2040 win20=1856 win80=68 win300=38 unseen_hits=0 unseen_unlatch=0
- AC2: req=4001 wfe_waits=0 slow_sleeps=4264 defer_sleeps=31997 hot_iters=0 parks=293 park_timeouts=292 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=4001 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0

### c22: request gaps about 155-295 us, full load (probe host, load on6, spin 130 us + 0-140 us, 4000 launches, 1 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | load vs P | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.123 | +0.000 | 274.1 | 237.2 | 1.000 (1.000-1.000) | - | - | - |
| M100 | 4 | 0.428 | +0.314 | 559.1 | 358.8 | 0.949 (0.934-0.966) | 260.6 | 63.1 / 113.9 / 4,708.1 / 6,093.3 | 6.14% |
| AC | 4 | 0.160 | +0.036 | 384.5 | 301.6 | 0.983 (0.977-1.000) | 86.1 | 75.8 / 112.2 / 119.1 / 5,887.1 | 0.54% |
| AC2 | 4 | 0.160 | +0.041 | 390.3 | 302.4 | 0.981 (0.973-0.989) | 84.7 | 76.5 / 111.9 / 119.4 / 5,866.7 | 0.40% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC: req=4001 wfe_waits=0 slow_sleeps=5500 defer_sleeps=30672 hot_iters=77963 parks=547 park_timeouts=494 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=148 win0=3512 win20=462 win80=13 win300=6 unseen_hits=0 unseen_unlatch=0
- AC2: req=4001 wfe_waits=0 slow_sleeps=5756 defer_sleeps=30828 hot_iters=0 parks=476 park_timeouts=435 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=136 win0=4001 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0

### c23: request gaps about 55-115 us, no load (probe host, load off, spin 30 us + 0-60 us, 4000 launches, 1 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | VDD_CPU_GPU_CV (mW) | VDD_IN (mW) | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.263 | +0.000 | 84.5 | 84.3 | - | - | - | - | - |
| M100 | 4 | 1.296 | +1.032 | 110.9 | 110.8 | - | - | 14.8 | 14.8 / 15.1 / 15.6 / 183.0 | 0.00% |
| AC | 4 | 1.282 | +1.026 | 111.7 | 111.4 | - | - | 15.0 | 14.7 / 15.0 / 29.2 / 141.5 | 0.00% |
| AC2 | 4 | 0.328 | +0.072 | 121.5 | 121.0 | - | - | 26.2 | 25.9 / 36.0 / 38.4 / 107.3 | 0.00% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC: req=4001 wfe_waits=0 slow_sleeps=10 defer_sleeps=461 hot_iters=4031502 parks=294 park_timeouts=293 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=435 guard_skips=0 win0=1 win20=2 win80=2 win300=3996 unseen_hits=0 unseen_unlatch=0
- AC2: req=4001 wfe_waits=0 slow_sleeps=10 defer_sleeps=20078 hot_iters=0 parks=293 park_timeouts=292 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=4001 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0

### c24: request gaps about 55-115 us, full load (probe host, load on6, spin 30 us + 0-60 us, 4000 launches, 1 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | load vs P | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.258 | +0.000 | 130.2 | 92.2 | 1.000 (1.000-1.000) | - | - | - |
| M100 | 4 | 0.585 | +0.323 | 272.6 | 121.8 | 0.949 (0.943-0.956) | 117.2 | 14.5 / 14.9 / 4,008.9 / 12,003.1 | 2.63% |
| AC | 4 | 0.496 | +0.240 | 282.6 | 132.1 | 0.962 (0.940-0.975) | 117.6 | 14.9 / 91.1 / 3,864.7 / 7,945.1 | 2.72% |
| AC2 | 4 | 0.285 | +0.028 | 200.5 | 146.7 | 0.977 (0.966-0.996) | 38.2 | 26.3 / 38.3 / 113.9 / 6,043.6 | 0.27% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC: req=4001 wfe_waits=0 slow_sleeps=1916 defer_sleeps=1828 hot_iters=3424702 parks=530 park_timeouts=484 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=292 guard_skips=932 win0=222 win20=430 win80=416 win300=2933 unseen_hits=0 unseen_unlatch=0
- AC2: req=4001 wfe_waits=0 slow_sleeps=1142 defer_sleeps=21016 hot_iters=0 parks=442 park_timeouts=391 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=168 win0=4001 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0

### c25: 2 calls per launch, no load (probe host, load off, spin 2000 us + 0-1000 us, 2000 launches, 2 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | VDD_CPU_GPU_CV (mW) | VDD_IN (mW) | cost per call (us) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.011 | +0.000 | 2,526.2 | 2,530.4 | 1,685 | 6,930 | - |
| M100 | 4 | 0.105 | +0.095 | 2,612.1 | 2,649.6 | 1,759 | 7,007 | 42.9 |
| AC | 4 | 0.036 | +0.025 | 2,631.9 | 2,620.6 | 1,737 | 7,002 | 52.7 |
| AC2 | 4 | 0.049 | +0.039 | 2,628.7 | 2,657.4 | 1,736 | 6,995 | 51.4 |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC: req=4002 wfe_waits=0 slow_sleeps=46176 defer_sleeps=18009 hot_iters=156486 parks=293 park_timeouts=292 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=2001 win20=2001 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC2: req=4002 wfe_waits=0 slow_sleeps=45408 defer_sleeps=18008 hot_iters=896190 parks=294 park_timeouts=293 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=2002 win20=0 win80=2000 win300=0 unseen_hits=0 unseen_unlatch=0

### c26: 4 calls per launch, no load (probe host, load off, spin 2000 us + 0-1000 us, 2000 launches, 4 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | VDD_CPU_GPU_CV (mW) | VDD_IN (mW) | cost per call (us) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.011 | +0.000 | 2,526.2 | 2,530.6 | 1,685 | 6,921 | - |
| M100 | 4 | 0.111 | +0.101 | 2,629.3 | 2,667.3 | 1,764 | 7,021 | 25.7 |
| AC | 4 | 0.043 | +0.032 | 2,651.1 | 2,639.7 | 1,735 | 6,997 | 31.3 |
| AC2 | 4 | 0.056 | +0.046 | 2,648.8 | 2,676.6 | 1,743 | 7,007 | 30.6 |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC: req=8004 wfe_waits=0 slow_sleeps=46193 defer_sleeps=18016 hot_iters=352215 parks=294 park_timeouts=294 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=2001 win20=6003 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC2: req=8004 wfe_waits=0 slow_sleeps=45426 defer_sleeps=18009 hot_iters=1108323 parks=294 park_timeouts=294 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=2002 win20=0 win80=6002 win300=0 unseen_hits=0 unseen_unlatch=0

### c27: 8 calls per launch, no load (probe host, load off, spin 2000 us + 0-1000 us, 2000 launches, 8 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | VDD_CPU_GPU_CV (mW) | VDD_IN (mW) | cost per call (us) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.011 | +0.000 | 2,527.2 | 2,531.8 | 1,692 | 6,942 | - |
| M100 | 4 | 0.123 | +0.112 | 2,664.6 | 2,702.8 | 1,765 | 7,005 | 17.2 |
| AC | 4 | 0.056 | +0.046 | 2,691.4 | 2,678.4 | 1,735 | 6,992 | 20.5 |
| AC2 | 4 | 0.070 | +0.059 | 2,687.2 | 2,714.8 | 1,739 | 7,005 | 20.1 |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC: req=16008 wfe_waits=0 slow_sleeps=46216 defer_sleeps=18027 hot_iters=771710 parks=296 park_timeouts=295 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=2001 win20=14007 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC2: req=16008 wfe_waits=0 slow_sleeps=45424 defer_sleeps=18009 hot_iters=1530601 parks=294 park_timeouts=294 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=2001 win20=0 win80=14007 win300=0 unseen_hits=0 unseen_unlatch=0

### c28: 4 calls per launch, full load (probe host, load on6, spin 2000 us + 0-1000 us, 2000 launches, 4 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | load vs P | cost per call (us) |
|---|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.014 | +0.000 | 2,577.2 | 2,548.3 | 1.000 (1.000-1.000) | - |
| M100 | 4 | 0.109 | +0.095 | 2,778.4 | 2,684.2 | 0.978 (0.976-0.980) | 50.8 |
| AC | 4 | 0.047 | +0.033 | 2,763.9 | 2,661.1 | 0.986 (0.986-0.989) | 46.7 |
| AC2 | 4 | 0.059 | +0.045 | 2,770.2 | 2,697.9 | 0.986 (0.985-0.987) | 48.4 |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC: req=8004 wfe_waits=0 slow_sleeps=45698 defer_sleeps=17418 hot_iters=358888 parks=498 park_timeouts=466 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=113 win0=2004 win20=6000 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC2: req=8004 wfe_waits=0 slow_sleeps=44442 defer_sleeps=17052 hot_iters=1063630 parks=514 park_timeouts=485 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=294 win0=2212 win20=0 win80=5792 win300=0 unseen_hits=0 unseen_unlatch=0

### c29: graph and plain launches alternate (probe host, load off, spin 2000 us + 0-1000 us, 2000 launches, 1 lane(s), gap 0 us, graph 2, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | VDD_CPU_GPU_CV (mW) | VDD_IN (mW) | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.011 | +0.000 | 2,528.1 | 2,532.2 | 1,691 | 6,942 | - | - | - |
| M100 | 4 | 0.102 | +0.091 | 2,605.4 | 2,640.6 | 1,760 | 7,013 | 64.1 | 63.1 / 106.4 / 117.8 / 172.6 | 0.00% |
| AC | 4 | 0.030 | +0.019 | 2,609.4 | 2,585.3 | 1,735 | 6,997 | 68.8 | 69.7 / 108.5 / 118.1 / 607.5 | 0.00% |
| AC2 | 4 | 0.031 | +0.020 | 2,609.8 | 2,586.9 | 1,743 | 6,997 | 69.0 | 70.2 / 108.8 / 118.4 / 608.7 | 0.00% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC: req=2002 wfe_waits=0 slow_sleeps=46440 defer_sleeps=16016 hot_iters=0 parks=293 park_timeouts=292 unseen_latch=1 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=2002 win20=0 win80=0 win300=0 unseen_hits=2 unseen_unlatch=0
- AC2: req=2002 wfe_waits=0 slow_sleeps=46410 defer_sleeps=16016 hot_iters=0 parks=297 park_timeouts=296 unseen_latch=1 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=2002 win20=0 win80=0 win300=0 unseen_hits=2 unseen_unlatch=0

### c30: graphs, then plain launches (unlatch) (probe host, load off, spin 2000 us + 0-1000 us, 2000 launches, 1 lane(s), gap 0 us, graph 3, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | VDD_CPU_GPU_CV (mW) | VDD_IN (mW) | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.010 | +0.000 | 2,526.8 | 2,530.1 | 1,688 | 6,928 | - | - | - |
| M100 | 4 | 0.101 | +0.091 | 2,602.9 | 2,640.4 | 1,760 | 7,013 | 64.7 | 63.4 / 106.9 / 118.0 / 762.9 | 0.00% |
| AC | 4 | 0.029 | +0.019 | 2,607.1 | 2,584.6 | 1,729 | 6,976 | 69.2 | 70.3 / 108.9 / 118.0 / 611.1 | 0.00% |
| AC2 | 4 | 0.030 | +0.020 | 2,607.3 | 2,586.5 | 1,734 | 6,978 | 69.4 | 70.6 / 108.7 / 118.4 / 1,638.6 | 0.01% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC: req=2002 wfe_waits=0 slow_sleeps=46390 defer_sleeps=16016 hot_iters=0 parks=298 park_timeouts=298 unseen_latch=1 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=2002 win20=0 win80=0 win300=0 unseen_hits=2 unseen_unlatch=0
- AC2: req=2002 wfe_waits=0 slow_sleeps=46340 defer_sleeps=16016 hot_iters=0 parks=298 park_timeouts=296 unseen_latch=1 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=2002 win20=0 win80=0 win300=0 unseen_hits=2 unseen_unlatch=1

### c18: CUDA graph launches (not noted) (probe host, load off, spin 2000 us + 0-1000 us, 3200 launches, 1 lane(s), gap 0 us, graph 1, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | VDD_CPU_GPU_CV (mW) | VDD_IN (mW) | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.010 | +0.000 | 2,526.2 | 2,527.8 | 1,685 | 6,920 | - | - | - |
| M100 | 4 | 0.100 | +0.090 | 2,603.5 | 2,640.1 | 1,740 | 6,999 | 65.2 | 64.0 / 106.8 / 118.0 / 1,075.5 | 0.01% |
| AC | 4 | 0.028 | +0.018 | 2,605.3 | 2,583.9 | 1,734 | 6,975 | 68.0 | 69.1 / 108.8 / 117.8 / 609.4 | 0.00% |
| AC2 | 4 | 0.028 | +0.018 | 2,605.4 | 2,584.3 | 1,736 | 6,973 | 68.7 | 69.6 / 109.3 / 118.0 / 726.7 | 0.00% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC: req=3202 wfe_waits=0 slow_sleeps=74210 defer_sleeps=25612 hot_iters=0 parks=298 park_timeouts=297 unseen_latch=1 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=3202 win20=0 win80=0 win300=0 unseen_hits=2 unseen_unlatch=0
- AC2: req=3202 wfe_waits=0 slow_sleeps=74227 defer_sleeps=25616 hot_iters=0 parks=298 park_timeouts=297 unseen_latch=1 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=3202 win20=0 win80=0 win300=0 unseen_hits=2 unseen_unlatch=0

### c01: full load (probe host, load on6, spin 2000 us + 0-1000 us, 3200 launches, 1 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | load vs P | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.013 | +0.000 | 2,575.7 | 2,541.7 | 1.000 (1.000-1.000) | - | - | - |
| M100 | 4 | 0.101 | +0.088 | 2,746.7 | 2,656.9 | 0.980 (0.979-1.161) | 147.4 | 67.4 / 112.7 / 2,619.5 / 5,013.3 | 3.62% |
| AC | 4 | 0.037 | +0.024 | 2,704.7 | 2,607.2 | 0.990 (0.989-1.136) | 94.0 | 70.8 / 111.0 / 845.8 / 8,727.1 | 0.97% |
| AC2 | 4 | 0.037 | +0.024 | 2,698.6 | 2,608.1 | 0.989 (0.989-1.172) | 95.5 | 70.1 / 110.7 / 1,148.1 / 7,481.3 | 1.04% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC: req=3201 wfe_waits=0 slow_sleeps=73555 defer_sleeps=25044 hot_iters=0 parks=540 park_timeouts=486 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=70 win0=3201 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC2: req=3201 wfe_waits=0 slow_sleeps=73634 defer_sleeps=25090 hot_iters=0 parks=540 park_timeouts=490 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=64 win0=3201 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0

### c02: no load (power) (probe host, load off, spin 2000 us + 0-1000 us, 3200 launches, 1 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | VDD_CPU_GPU_CV (mW) | VDD_IN (mW) | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.010 | +0.000 | 2,526.3 | 2,527.2 | 1,689 | 6,926 | - | - | - |
| M100 | 4 | 0.102 | +0.092 | 2,605.4 | 2,640.4 | 1,759 | 7,026 | 64.7 | 64.0 / 106.9 / 118.0 / 1,693.9 | 0.01% |
| AC | 4 | 0.031 | +0.021 | 2,607.2 | 2,586.7 | 1,730 | 6,977 | 68.3 | 69.5 / 109.1 / 118.1 / 2,048.5 | 0.02% |
| AC2 | 4 | 0.032 | +0.021 | 2,608.1 | 2,586.8 | 1,740 | 7,006 | 71.3 | 69.2 / 109.0 / 118.4 / 5,299.3 | 0.12% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC: req=3201 wfe_waits=0 slow_sleeps=74156 defer_sleeps=25597 hot_iters=0 parks=296 park_timeouts=295 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=2 win0=3201 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC2: req=3201 wfe_waits=0 slow_sleeps=74132 defer_sleeps=25586 hot_iters=0 parks=298 park_timeouts=296 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=3 win0=3201 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0

### c05: burst, full load (probe host, load on6, spin 2000 us + 0-1000 us, 2400 launches, 32 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | load vs P | cost per call (us) |
|---|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.013 | +0.000 | 2,578.7 | 2,551.3 | 1.000 (1.000-1.000) | - |
| M100 | 4 | 0.174 | +0.161 | 3,119.4 | 2,930.7 | 0.969 (0.968-0.970) | 16.6 |
| AC | 4 | 0.127 | +0.114 | 3,117.2 | 3,029.4 | 0.975 (0.963-0.978) | 16.9 |
| AC2 | 4 | 0.128 | +0.115 | 3,286.8 | 2,972.1 | 0.975 (0.974-0.976) | 22.1 |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC: req=76832 wfe_waits=0 slow_sleeps=51204 defer_sleeps=17809 hot_iters=3989951 parks=575 park_timeouts=514 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=616 win0=2416 win20=74416 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC2: req=76832 wfe_waits=0 slow_sleeps=53582 defer_sleeps=19770 hot_iters=4615404 parks=562 park_timeouts=526 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=4754 win0=7050 win20=0 win80=69782 win300=0 unseen_hits=0 unseen_unlatch=0

### c06: burst, no load (probe host, load off, spin 2000 us + 0-1000 us, 2400 launches, 32 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | VDD_CPU_GPU_CV (mW) | VDD_IN (mW) | cost per call (us) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.010 | +0.000 | 2,528.6 | 2,532.1 | 1,694 | 6,951 | - |
| M100 | 4 | 0.190 | +0.179 | 2,882.6 | 2,919.1 | 1,780 | 7,043 | 11.1 |
| AC | 4 | 0.132 | +0.121 | 2,920.4 | 2,910.7 | 1,777 | 7,038 | 12.3 |
| AC2 | 4 | 0.146 | +0.135 | 2,922.2 | 2,950.8 | 1,785 | 7,059 | 12.3 |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC: req=76832 wfe_waits=0 slow_sleeps=55406 defer_sleeps=21721 hot_iters=3929007 parks=294 park_timeouts=293 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=2401 win20=74424 win80=7 win300=0 unseen_hits=0 unseen_unlatch=0
- AC2: req=76832 wfe_waits=0 slow_sleeps=54451 defer_sleeps=21612 hot_iters=4965615 parks=296 park_timeouts=296 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=2404 win20=0 win80=74428 win300=0 unseen_hits=0 unseen_unlatch=0

### c10: low GPU duty, full load (probe host, load on6, spin 2000 us + 0-1000 us, 400 launches, 1 lane(s), gap 18000 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | load vs P | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.002 | +0.000 | 3,021.4 | 2,963.3 | 1.000 (1.000-1.000) | - | - | - |
| M100 | 4 | 0.025 | +0.023 | 3,151.8 | 3,074.7 | 0.992 (0.990-0.994) | 101.3 | 70.3 / 111.7 / 1,662.4 / 4,574.6 | 1.31% |
| AC | 4 | 0.009 | +0.007 | 3,133.0 | 3,031.8 | 0.997 (0.995-0.999) | 116.4 | 70.9 / 113.9 / 2,157.5 / 5,526.9 | 1.69% |
| AC2 | 4 | 0.009 | +0.007 | 3,119.1 | 3,033.9 | 0.998 (0.996-1.001) | 105.7 | 68.5 / 113.9 / 2,039.4 / 4,414.5 | 1.56% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC: req=401 wfe_waits=0 slow_sleeps=12123 defer_sleeps=3154 hot_iters=0 parks=7588 park_timeouts=7188 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=8 win0=401 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC2: req=401 wfe_waits=0 slow_sleeps=12080 defer_sleeps=3192 hot_iters=0 parks=7592 park_timeouts=7193 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=2 win0=401 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0

### c11: idle agent: one launch, then 8 s (probe host, load off, spin 2000 us + 0-0 us, 1 launches, 1 lane(s), gap 8000000 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | VDD_CPU_GPU_CV (mW) | VDD_IN (mW) | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.000 | +0.000 | 2,033.4 | 2,033.4 | 1,448 | 6,640 | - | - | - |
| M100 | 4 | 0.011 | +0.010 | 2,084.8 | 2,084.8 | 1,497 | 6,692 | 39.6 | 35.1 / 60.1 / 60.1 / 60.1 | 0.00% |
| AC | 4 | 0.002 | +0.002 | 2,137.7 | 2,137.7 | 1,454 | 6,646 | 74.9 | 82.8 / 117.8 / 117.8 / 117.8 | 0.00% |
| AC2 | 4 | 0.002 | +0.002 | 2,085.4 | 2,085.4 | 1,457 | 6,654 | 52.0 | 36.3 / 118.2 / 118.2 / 118.2 | 0.00% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC: req=2 wfe_waits=0 slow_sleeps=18 defer_sleeps=16 hot_iters=0 parks=8254 park_timeouts=8254 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=2 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC2: req=2 wfe_waits=0 slow_sleeps=18 defer_sleeps=16 hot_iters=0 parks=8254 park_timeouts=8253 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=2 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0

### c15: short kernels, full load (probe host, load on6, spin 200 us + 0-200 us, 4000 launches, 1 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | load vs P | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.090 | +0.000 | 373.1 | 334.6 | 1.000 (1.000-1.000) | - | - | - |
| M100 | 4 | 0.374 | +0.286 | 659.8 | 462.9 | 0.947 (0.942-0.959) | 260.8 | 71.3 / 116.7 / 4,026.2 / 6,152.5 | 6.88% |
| AC | 4 | 0.130 | +0.038 | 481.2 | 406.0 | 0.982 (0.979-0.995) | 81.5 | 68.9 / 109.6 / 119.0 / 5,319.5 | 0.45% |
| AC2 | 4 | 0.131 | +0.038 | 479.1 | 405.9 | 0.980 (0.976-0.995) | 80.6 | 69.1 / 109.7 / 119.4 / 5,397.0 | 0.42% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC: req=4001 wfe_waits=0 slow_sleeps=9114 defer_sleeps=31047 hot_iters=5924 parks=512 park_timeouts=450 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=113 win0=3924 win20=76 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC2: req=4001 wfe_waits=0 slow_sleeps=9102 defer_sleeps=31112 hot_iters=0 parks=520 park_timeouts=467 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=104 win0=4001 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0

### c17: load 5 ms on / 5 ms off (probe host, load duty6, spin 2000 us + 0-1000 us, 3200 launches, 1 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | load vs P | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.011 | +0.000 | 3,645.8 | 2,913.7 | 1.000 (1.000-1.000) | - | - | - |
| M100 | 4 | 0.071 | +0.061 | 3,925.2 | 2,851.3 | 0.946 (0.941-0.948) | 948.5 | 82.7 / 3,850.6 / 4,934.9 / 5,071.7 | 25.23% |
| AC | 4 | 0.024 | +0.013 | 3,908.0 | 2,837.3 | 0.969 (0.965-0.973) | 1,080.6 | 91.3 / 3,931.1 / 4,945.1 / 5,066.9 | 30.26% |
| AC2 | 4 | 0.024 | +0.014 | 3,920.9 | 2,834.9 | 0.970 (0.966-0.973) | 1,090.8 | 91.7 / 3,946.1 / 4,964.9 / 5,060.1 | 30.47% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC: req=3201 wfe_waits=0 slow_sleeps=69962 defer_sleeps=16051 hot_iters=0 parks=614 park_timeouts=462 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=1152 win0=3201 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC2: req=3201 wfe_waits=0 slow_sleeps=70082 defer_sleeps=16024 hot_iters=0 parks=635 park_timeouts=478 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=1158 win0=3201 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
