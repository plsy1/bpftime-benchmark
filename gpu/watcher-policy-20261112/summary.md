Runs: 504, valid 504; rounds 1,2,3,4; max temperature 54.4 C


### c01: full load (probe host, load on6, spin 2000 us + 0-1000 us, 3200 launches, 1 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | load vs P | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.014 | +0.000 | 2,576.6 | 2,544.8 | 1.000 (1.000-1.000) | - | - | - |
| W | 4 | 0.814 | +0.800 | 2,945.6 | 2,648.7 | 0.865 (0.859-0.869) | 347.1 | 14.9 / 2,105.1 / 3,621.6 / 9,211.9 | 11.29% |
| M100 | 4 | 0.101 | +0.088 | 2,748.4 | 2,657.4 | 0.980 (0.978-0.982) | 144.8 | 68.1 / 113.8 / 2,310.6 / 7,276.4 | 3.50% |
| A | 4 | 0.103 | +0.089 | 2,757.7 | 2,623.4 | 0.980 (0.978-0.985) | 152.5 | 72.9 / 115.2 / 2,463.2 / 8,032.7 | 3.69% |
| AC | 4 | 0.037 | +0.024 | 2,704.2 | 2,610.3 | 0.989 (0.979-0.992) | 95.8 | 71.0 / 110.9 / 1,105.2 / 6,484.2 | 1.04% |
| AB | 4 | 0.105 | +0.091 | 2,765.2 | 2,627.6 | 0.980 (0.977-0.985) | 165.4 | 71.3 / 116.7 / 2,731.1 / 7,283.8 | 4.14% |
| ABC | 4 | 0.042 | +0.029 | 2,709.2 | 2,630.1 | 0.988 (0.988-0.989) | 101.9 | 69.5 / 112.2 / 1,599.6 / 7,934.3 | 1.26% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300
- A: req=3201 wfe_waits=0 slow_sleeps=69224 defer_sleeps=0 hot_iters=6236471 parks=634 park_timeouts=572 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=0 win20=0 win80=0 win300=0
- AC: req=3201 wfe_waits=0 slow_sleeps=73524 defer_sleeps=25013 hot_iters=0 parks=474 park_timeouts=420 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=74 win0=3201 win20=0 win80=0 win300=0
- AB: req=3201 wfe_waits=0 slow_sleeps=68694 defer_sleeps=0 hot_iters=5864406 parks=590 park_timeouts=512 unseen_latch=0 gate_opens=0 latch_trips=1 cap_forced=0 guard_skips=0 win0=0 win20=0 win80=0 win300=0
- ABC: req=3201 wfe_waits=0 slow_sleeps=73036 defer_sleeps=24866 hot_iters=0 parks=535 park_timeouts=488 unseen_latch=0 gate_opens=0 latch_trips=1 cap_forced=0 guard_skips=92 win0=3201 win20=0 win80=0 win300=0

### c02: no load (power) (probe host, load off, spin 2000 us + 0-1000 us, 3200 launches, 1 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | VDD_CPU_GPU_CV (mW) | VDD_IN (mW) | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.011 | +0.000 | 2,526.2 | 2,527.2 | 1,699 | 6,951 | - | - | - |
| W | 4 | 1.012 | +1.002 | 2,552.6 | 2,554.3 | 1,713 | 6,968 | 14.1 | 13.9 / 14.8 / 15.1 / 103.7 | 0.00% |
| M100 | 4 | 0.103 | +0.092 | 2,604.1 | 2,640.1 | 1,766 | 7,014 | 64.5 | 63.4 / 106.3 / 117.7 / 700.4 | 0.00% |
| A | 4 | 0.104 | +0.094 | 2,607.7 | 2,594.3 | 1,760 | 7,026 | 68.0 | 70.0 / 109.4 / 118.2 / 448.3 | 0.00% |
| AC | 4 | 0.032 | +0.021 | 2,606.9 | 2,587.1 | 1,747 | 7,003 | 67.8 | 69.2 / 108.4 / 118.1 / 162.0 | 0.00% |
| AB | 4 | 1.011 | +1.000 | 2,555.4 | 2,556.8 | 1,706 | 6,966 | 15.1 | 14.8 / 15.5 / 15.8 / 818.1 | 0.00% |
| ABC | 4 | 0.944 | +0.933 | 2,555.9 | 2,557.5 | 1,727 | 6,987 | 14.8 | 14.7 / 14.9 / 15.8 / 117.5 | 0.00% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300
- A: req=3201 wfe_waits=0 slow_sleeps=73914 defer_sleeps=0 hot_iters=6250626 parks=296 park_timeouts=293 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=0 win20=0 win80=0 win300=0
- AC: req=3201 wfe_waits=0 slow_sleeps=74129 defer_sleeps=25594 hot_iters=0 parks=295 park_timeouts=294 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=2 win0=3201 win20=0 win80=0 win300=0
- AB: req=3201 wfe_waits=85384 slow_sleeps=92 defer_sleeps=0 hot_iters=27056 parks=292 park_timeouts=292 unseen_latch=0 gate_opens=4 latch_trips=0 cap_forced=0 guard_skips=0 win0=0 win20=0 win80=0 win300=0
- ABC: req=3201 wfe_waits=69506 slow_sleeps=70 defer_sleeps=25604 hot_iters=0 parks=296 park_timeouts=296 unseen_latch=0 gate_opens=3 latch_trips=0 cap_forced=0 guard_skips=1 win0=3201 win20=0 win80=0 win300=0

### c03: W's worst (probe host, load on6, spin 20000 us + 0-1000 us, 400 launches, 1 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | load vs P | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.002 | +0.000 | 20,603.2 | 20,542.5 | 1.000 (1.000-1.000) | - | - | - |
| W | 4 | 0.823 | +0.820 | 21,388.4 | 20,718.7 | 0.857 (0.851-0.871) | 706.3 | 15.5 / 3,281.3 / 3,943.0 / 7,506.0 | 21.38% |
| M100 | 4 | 0.025 | +0.023 | 20,695.1 | 20,626.2 | 0.990 (0.987-0.992) | 85.7 | 67.4 / 108.6 / 215.0 / 4,348.5 | 0.75% |
| A | 4 | 0.028 | +0.025 | 20,716.2 | 20,624.5 | 0.991 (0.988-0.995) | 91.4 | 68.6 / 111.4 / 973.0 / 4,261.7 | 0.94% |
| AC | 4 | 0.019 | +0.017 | 20,710.0 | 20,630.8 | 0.993 (0.991-0.993) | 95.6 | 69.3 / 110.1 / 875.0 / 4,954.9 | 0.88% |
| AB | 4 | 0.032 | +0.030 | 20,738.0 | 20,624.7 | 0.989 (0.988-0.991) | 110.0 | 68.7 / 112.3 / 2,401.5 / 4,039.0 | 1.44% |
| ABC | 4 | 0.024 | +0.021 | 20,738.2 | 20,621.7 | 0.990 (0.988-0.991) | 108.3 | 69.6 / 111.3 / 1,799.8 / 7,140.5 | 1.38% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300
- A: req=401 wfe_waits=0 slow_sleeps=75917 defer_sleeps=0 hot_iters=767998 parks=358 park_timeouts=346 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=0 win20=0 win80=0 win300=0
- AC: req=401 wfe_waits=0 slow_sleeps=76764 defer_sleeps=3116 hot_iters=0 parks=467 park_timeouts=459 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=12 win0=401 win20=0 win80=0 win300=0
- AB: req=401 wfe_waits=0 slow_sleeps=75462 defer_sleeps=0 hot_iters=747206 parks=376 park_timeouts=364 unseen_latch=0 gate_opens=0 latch_trips=1 cap_forced=0 guard_skips=0 win0=0 win20=0 win80=0 win300=0
- ABC: req=401 wfe_waits=0 slow_sleeps=75942 defer_sleeps=3080 hot_iters=0 parks=358 park_timeouts=348 unseen_latch=0 gate_opens=0 latch_trips=1 cap_forced=0 guard_skips=18 win0=401 win20=0 win80=0 win300=0

### c04: pure cost (probe none, load on6, spin 2000 us + 0-1000 us, 3200 launches, 1 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | load vs P |
|---|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.013 | +0.000 | 2,574.4 | 2,543.1 | 1.000 (1.000-1.000) |
| W | 4 | 0.806 | +0.793 | 2,606.0 | 2,566.8 | 0.865 (0.854-0.871) |
| M100 | 4 | 0.031 | +0.018 | 2,603.4 | 2,559.8 | 0.990 (0.990-0.991) |
| A | 4 | 0.034 | +0.021 | 2,602.9 | 2,564.2 | 0.990 (0.989-0.991) |
| AC | 4 | 0.035 | +0.021 | 2,603.3 | 2,569.8 | 0.990 (0.989-0.992) |
| AB | 4 | 0.039 | +0.026 | 2,602.8 | 2,568.4 | 0.989 (0.988-0.989) |
| ABC | 4 | 0.039 | +0.026 | 2,602.1 | 2,565.4 | 0.989 (0.988-0.989) |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300
- A: req=0 wfe_waits=0 slow_sleeps=76701 defer_sleeps=0 hot_iters=0 parks=566 park_timeouts=366 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=0 win20=0 win80=0 win300=0
- AC: req=0 wfe_waits=0 slow_sleeps=76777 defer_sleeps=0 hot_iters=0 parks=548 park_timeouts=359 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=0 win20=0 win80=0 win300=0
- AB: req=0 wfe_waits=0 slow_sleeps=76188 defer_sleeps=0 hot_iters=0 parks=588 park_timeouts=386 unseen_latch=0 gate_opens=0 latch_trips=1 cap_forced=0 guard_skips=0 win0=0 win20=0 win80=0 win300=0
- ABC: req=0 wfe_waits=0 slow_sleeps=76104 defer_sleeps=0 hot_iters=0 parks=578 park_timeouts=376 unseen_latch=0 gate_opens=0 latch_trips=1 cap_forced=0 guard_skips=0 win0=0 win20=0 win80=0 win300=0

### c05: burst, full load (probe host, load on6, spin 2000 us + 0-1000 us, 2400 launches, 32 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | load vs P | cost per call (us) |
|---|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.014 | +0.000 | 2,576.9 | 2,550.0 | 1.000 (1.000-1.000) | - |
| W | 4 | 0.791 | +0.777 | 3,274.4 | 2,950.2 | 0.870 (0.864-0.872) | 21.7 |
| M100 | 4 | 0.176 | +0.163 | 3,126.2 | 2,943.5 | 0.970 (0.964-0.973) | 17.2 |
| A | 4 | 0.182 | +0.169 | 3,162.6 | 3,011.5 | 0.967 (0.964-0.968) | 18.2 |
| AC | 4 | 0.128 | +0.114 | 3,124.3 | 3,023.9 | 0.974 (0.970-0.977) | 17.1 |
| AB | 4 | 0.183 | +0.169 | 3,168.8 | 3,022.4 | 0.967 (0.964-0.968) | 18.5 |
| ABC | 4 | 0.058 | +0.045 | 3,271.7 | 3,136.8 | 0.981 (0.981-0.982) | 21.6 |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300
- A: req=76832 wfe_waits=0 slow_sleeps=47458 defer_sleeps=0 hot_iters=8548998 parks=551 park_timeouts=492 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=0 win20=0 win80=0 win300=0
- AC: req=76832 wfe_waits=0 slow_sleeps=51014 defer_sleeps=17701 hot_iters=4023654 parks=562 park_timeouts=493 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=654 win0=2414 win20=74416 win80=2 win300=0
- AB: req=76832 wfe_waits=0 slow_sleeps=46918 defer_sleeps=0 hot_iters=8128736 parks=472 park_timeouts=396 unseen_latch=0 gate_opens=0 latch_trips=1 cap_forced=0 guard_skips=0 win0=0 win20=0 win80=0 win300=0
- ABC: req=76832 wfe_waits=0 slow_sleeps=53973 defer_sleeps=21214 hot_iters=76728 parks=489 park_timeouts=453 unseen_latch=0 gate_opens=0 latch_trips=1 cap_forced=0 guard_skips=156 win0=2462 win20=72432 win80=1938 win300=0

### c06: burst, no load (probe host, load off, spin 2000 us + 0-1000 us, 2400 launches, 32 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | VDD_CPU_GPU_CV (mW) | VDD_IN (mW) | cost per call (us) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.011 | +0.000 | 2,529.2 | 2,532.2 | 1,703 | 6,957 | - |
| W | 4 | 1.009 | +0.998 | 2,823.2 | 2,827.6 | 1,741 | 7,004 | 9.2 |
| M100 | 4 | 0.191 | +0.180 | 2,887.7 | 2,924.3 | 1,798 | 7,059 | 11.2 |
| A | 4 | 0.196 | +0.185 | 2,903.5 | 2,891.6 | 1,790 | 7,048 | 11.7 |
| AC | 4 | 0.133 | +0.122 | 2,926.1 | 2,913.0 | 1,780 | 7,053 | 12.4 |
| AB | 4 | 1.010 | +0.999 | 2,853.1 | 2,857.6 | 1,741 | 7,015 | 10.1 |
| ABC | 4 | 0.941 | +0.930 | 2,873.7 | 2,877.5 | 1,749 | 7,023 | 10.8 |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300
- A: req=76832 wfe_waits=0 slow_sleeps=55436 defer_sleeps=0 hot_iters=8552274 parks=297 park_timeouts=296 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=0 win20=0 win80=0 win300=0
- AC: req=76832 wfe_waits=0 slow_sleeps=55418 defer_sleeps=21746 hot_iters=4068632 parks=296 park_timeouts=294 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=2401 win20=74431 win80=0 win300=0
- AB: req=76832 wfe_waits=511168 slow_sleeps=28 defer_sleeps=0 hot_iters=466540 parks=297 park_timeouts=296 unseen_latch=0 gate_opens=2 latch_trips=0 cap_forced=0 guard_skips=0 win0=0 win20=0 win80=0 win300=0
- ABC: req=76832 wfe_waits=492071 slow_sleeps=25 defer_sleeps=21634 hot_iters=441846 parks=296 park_timeouts=295 unseen_latch=0 gate_opens=2 latch_trips=0 cap_forced=0 guard_skips=0 win0=2401 win20=74424 win80=7 win300=0

### c07: partial load (probe host, load on3, spin 2000 us + 0-1000 us, 3200 launches, 1 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | load vs P | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.011 | +0.000 | 2,529.8 | 2,530.1 | 1.000 (1.000-1.000) | - | - | - |
| W | 4 | 1.014 | +1.003 | 2,562.2 | 2,561.1 | 0.998 (0.998-1.002) | 14.3 | 13.9 / 14.7 / 15.2 / 2,217.3 | 0.01% |
| M100 | 4 | 0.104 | +0.092 | 2,609.1 | 2,639.6 | 0.999 (0.998-1.000) | 66.2 | 64.3 / 107.9 / 118.0 / 3,641.6 | 0.03% |
| A | 4 | 0.105 | +0.093 | 2,613.6 | 2,595.9 | 1.000 (0.998-1.001) | 68.7 | 69.5 / 109.0 / 118.5 / 2,389.6 | 0.04% |
| AC | 4 | 0.035 | +0.024 | 2,613.9 | 2,596.7 | 1.000 (0.999-1.000) | 69.3 | 67.4 / 109.1 / 119.2 / 4,602.0 | 0.08% |
| AB | 4 | 0.994 | +0.982 | 2,562.1 | 2,560.1 | 1.000 (1.000-1.001) | 15.8 | 14.8 / 15.6 / 57.7 / 878.1 | 0.00% |
| ABC | 4 | 0.917 | +0.905 | 2,559.6 | 2,560.3 | 1.000 (0.999-1.000) | 16.1 | 14.7 / 15.1 / 76.1 / 545.6 | 0.00% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300
- A: req=3201 wfe_waits=0 slow_sleeps=73958 defer_sleeps=0 hot_iters=6209004 parks=312 park_timeouts=295 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=0 win20=0 win80=0 win300=0
- AC: req=3201 wfe_waits=0 slow_sleeps=73433 defer_sleeps=25482 hot_iters=0 parks=298 park_timeouts=295 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=18 win0=3201 win20=0 win80=0 win300=0
- AB: req=3201 wfe_waits=84782 slow_sleeps=1376 defer_sleeps=0 hot_iters=127052 parks=302 park_timeouts=294 unseen_latch=0 gate_opens=42 latch_trips=2 cap_forced=0 guard_skips=0 win0=0 win20=0 win80=0 win300=0
- ABC: req=3201 wfe_waits=67126 slow_sleeps=1489 defer_sleeps=25538 hot_iters=0 parks=304 park_timeouts=300 unseen_latch=0 gate_opens=52 latch_trips=14 cap_forced=0 guard_skips=10 win0=3201 win20=0 win80=0 win300=0

### c08: burst, partial load (probe host, load on3, spin 2000 us + 0-1000 us, 2400 launches, 32 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | load vs P | cost per call (us) |
|---|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.012 | +0.000 | 2,536.2 | 2,537.8 | 1.000 (1.000-1.000) | - |
| W | 4 | 1.010 | +0.998 | 2,839.1 | 2,837.9 | 0.999 (0.998-1.002) | 9.5 |
| M100 | 4 | 0.190 | +0.177 | 2,895.4 | 2,916.3 | 0.999 (0.998-1.000) | 11.3 |
| A | 4 | 0.199 | +0.187 | 2,917.9 | 2,901.1 | 1.000 (0.998-1.000) | 12.0 |
| AC | 4 | 0.133 | +0.121 | 2,931.3 | 2,914.7 | 1.000 (1.000-1.002) | 12.4 |
| AB | 4 | 0.997 | +0.984 | 2,863.4 | 2,865.8 | 1.000 (0.998-1.000) | 10.3 |
| ABC | 4 | 0.925 | +0.913 | 2,884.6 | 2,884.8 | 0.999 (0.997-1.000) | 11.0 |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300
- A: req=76832 wfe_waits=0 slow_sleeps=55281 defer_sleeps=0 hot_iters=8539925 parks=308 park_timeouts=300 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=0 win20=0 win80=0 win300=0
- AC: req=76832 wfe_waits=0 slow_sleeps=55246 defer_sleeps=21712 hot_iters=3944090 parks=299 park_timeouts=293 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=23 win0=2402 win20=74428 win80=3 win300=0
- AB: req=76832 wfe_waits=502272 slow_sleeps=964 defer_sleeps=0 hot_iters=588747 parks=314 park_timeouts=298 unseen_latch=0 gate_opens=38 latch_trips=6 cap_forced=0 guard_skips=0 win0=0 win20=0 win80=0 win300=0
- ABC: req=76832 wfe_waits=483942 slow_sleeps=950 defer_sleeps=21627 hot_iters=435780 parks=308 park_timeouts=298 unseen_latch=0 gate_opens=38 latch_trips=4 cap_forced=0 guard_skips=3 win0=2402 win20=74342 win80=88 win300=0

### c09: low GPU duty (~12%), no load (probe host, load off, spin 2000 us + 0-1000 us, 400 launches, 1 lane(s), gap 18000 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | VDD_CPU_GPU_CV (mW) | VDD_IN (mW) | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.002 | +0.000 | 2,983.7 | 2,970.7 | 1,501 | 6,702 | - | - | - |
| W | 4 | 0.158 | +0.156 | 3,011.1 | 2,998.8 | 1,525 | 6,730 | 15.5 | 15.4 / 16.3 / 17.8 / 21.2 | 0.00% |
| M100 | 4 | 0.023 | +0.021 | 3,064.6 | 3,060.6 | 1,541 | 6,742 | 68.3 | 70.0 / 109.7 / 118.7 / 124.4 | 0.00% |
| A | 4 | 0.017 | +0.015 | 3,064.5 | 3,078.8 | 1,531 | 6,730 | 67.0 | 67.1 / 109.0 / 119.6 / 127.9 | 0.00% |
| AC | 4 | 0.008 | +0.006 | 3,062.4 | 3,076.6 | 1,537 | 6,751 | 68.1 | 67.5 / 109.8 / 119.5 / 202.2 | 0.00% |
| AB | 4 | 0.172 | +0.170 | 3,012.1 | 3,001.2 | 1,515 | 6,728 | 16.2 | 16.1 / 17.1 / 19.2 / 22.0 | 0.00% |
| ABC | 4 | 0.162 | +0.160 | 3,012.6 | 2,998.8 | 1,515 | 6,725 | 16.4 | 16.2 / 17.4 / 20.1 / 75.8 | 0.00% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300
- A: req=401 wfe_waits=0 slow_sleeps=12558 defer_sleeps=0 hot_iters=781598 parks=7496 park_timeouts=7096 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=0 win20=0 win80=0 win300=0
- AC: req=401 wfe_waits=0 slow_sleeps=12575 defer_sleeps=3208 hot_iters=0 parks=7496 park_timeouts=7096 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=401 win20=0 win80=0 win300=0
- AB: req=401 wfe_waits=13600 slow_sleeps=18 defer_sleeps=0 hot_iters=3730 parks=7474 park_timeouts=7074 unseen_latch=0 gate_opens=1 latch_trips=0 cap_forced=0 guard_skips=0 win0=0 win20=0 win80=0 win300=0
- ABC: req=401 wfe_waits=11747 slow_sleeps=33 defer_sleeps=3208 hot_iters=0 parks=7472 park_timeouts=7072 unseen_latch=0 gate_opens=2 latch_trips=0 cap_forced=0 guard_skips=0 win0=401 win20=0 win80=0 win300=0

### c10: low GPU duty, full load (probe host, load on6, spin 2000 us + 0-1000 us, 400 launches, 1 lane(s), gap 18000 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | load vs P | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.002 | +0.000 | 3,017.4 | 2,965.7 | 1.000 (1.000-1.000) | - | - | - |
| W | 4 | 0.149 | +0.147 | 3,644.6 | 3,081.1 | 0.974 (0.974-0.976) | 487.4 | 15.9 / 3,180.0 / 3,960.8 / 5,811.1 | 14.19% |
| M100 | 4 | 0.025 | +0.023 | 3,129.1 | 3,074.2 | 0.991 (0.990-0.992) | 82.1 | 68.1 / 111.3 / 286.8 / 2,890.0 | 0.50% |
| A | 4 | 0.018 | +0.015 | 3,198.6 | 3,118.4 | 0.995 (0.994-0.997) | 76.8 | 69.1 / 110.8 / 121.3 / 4,758.2 | 0.19% |
| AC | 4 | 0.009 | +0.007 | 3,130.1 | 3,034.8 | 0.997 (0.996-0.999) | 111.6 | 70.0 / 114.6 / 2,065.9 / 3,537.7 | 1.69% |
| AB | 4 | 0.019 | +0.016 | 3,170.8 | 3,098.9 | 0.996 (0.995-0.997) | 103.5 | 71.6 / 113.9 / 1,688.9 / 3,382.8 | 1.50% |
| ABC | 4 | 0.011 | +0.008 | 3,125.2 | 3,058.3 | 0.998 (0.989-0.999) | 121.4 | 72.5 / 115.3 / 1,781.1 / 8,911.5 | 2.25% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300
- A: req=401 wfe_waits=0 slow_sleeps=12238 defer_sleeps=0 hot_iters=763680 parks=7466 park_timeouts=7066 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=0 win20=0 win80=0 win300=0
- AC: req=401 wfe_waits=0 slow_sleeps=11967 defer_sleeps=3152 hot_iters=0 parks=7567 park_timeouts=7167 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=8 win0=401 win20=0 win80=0 win300=0
- AB: req=401 wfe_waits=0 slow_sleeps=12032 defer_sleeps=0 hot_iters=744722 parks=7454 park_timeouts=7054 unseen_latch=0 gate_opens=0 latch_trips=1 cap_forced=0 guard_skips=0 win0=0 win20=0 win80=0 win300=0
- ABC: req=401 wfe_waits=0 slow_sleeps=11820 defer_sleeps=3160 hot_iters=0 parks=7624 park_timeouts=7224 unseen_latch=0 gate_opens=0 latch_trips=1 cap_forced=0 guard_skips=6 win0=401 win20=0 win80=0 win300=0

### c11: idle agent: one launch, then 8 s (probe host, load off, spin 2000 us + 0-0 us, 1 launches, 1 lane(s), gap 8000000 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | VDD_CPU_GPU_CV (mW) | VDD_IN (mW) | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.000 | +0.000 | 2,034.0 | 2,034.0 | 1,458 | 6,653 | - | - | - |
| W | 4 | 0.003 | +0.002 | 2,070.7 | 2,070.7 | 1,476 | 6,678 | 14.6 | 14.6 / 14.7 / 14.7 / 14.7 | 0.00% |
| M100 | 4 | 0.011 | +0.011 | 2,103.4 | 2,103.4 | 1,509 | 6,704 | 52.3 | 48.7 / 75.6 / 75.6 / 75.6 | 0.00% |
| A | 4 | 0.002 | +0.002 | 2,093.8 | 2,093.8 | 1,477 | 6,681 | 50.5 | 40.2 / 96.4 / 96.4 / 96.4 | 0.00% |
| AC | 4 | 0.002 | +0.002 | 2,097.1 | 2,097.1 | 1,470 | 6,667 | 53.8 | 41.8 / 112.6 / 112.6 / 112.6 | 0.00% |
| AB | 4 | 0.002 | +0.002 | 2,083.2 | 2,083.2 | 1,469 | 6,671 | 27.6 | 27.6 / 34.6 / 34.6 / 34.6 | 0.00% |
| ABC | 4 | 0.003 | +0.002 | 2,107.9 | 2,107.9 | 1,463 | 6,662 | 58.6 | 51.8 / 85.6 / 85.6 / 85.6 | 0.00% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300
- A: req=2 wfe_waits=0 slow_sleeps=18 defer_sleeps=0 hot_iters=3884 parks=8255 park_timeouts=8254 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=0 win20=0 win80=0 win300=0
- AC: req=2 wfe_waits=0 slow_sleeps=18 defer_sleeps=16 hot_iters=0 parks=8252 park_timeouts=8252 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=2 win20=0 win80=0 win300=0
- AB: req=2 wfe_waits=3 slow_sleeps=18 defer_sleeps=0 hot_iters=1582 parks=8254 park_timeouts=8253 unseen_latch=0 gate_opens=1 latch_trips=0 cap_forced=0 guard_skips=0 win0=0 win20=0 win80=0 win300=0
- ABC: req=2 wfe_waits=0 slow_sleeps=18 defer_sleeps=16 hot_iters=0 parks=8250 park_timeouts=8248 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=2 win20=0 win80=0 win300=0

### c12: two processes, full load (probe host, load on6, spin 2000 us + 0-1000 us, 3200 launches, 1 lane(s), gap 0 us, graph 0, 2 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | load vs P | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.016 | +0.000 | 4,970.5 | 4,909.5 | 1.000 (1.000-1.000) | - | - | - |
| W | 4 | 1.339 | +1.323 | 6,595.2 | 6,493.9 | 0.776 (0.771-0.781) | 1,294.4 | 17.0 / 4,409.9 / 6,413.5 / 15,525.2 | 37.23% |
| M100 | 4 | 0.124 | +0.107 | 5,160.1 | 5,059.8 | 0.970 (0.964-0.972) | 213.1 | 72.0 / 242.4 / 2,523.9 / 11,564.8 | 5.02% |
| A | 4 | 0.127 | +0.111 | 5,157.0 | 5,027.1 | 0.967 (0.966-0.969) | 209.0 | 68.7 / 120.0 / 2,525.7 / 7,056.7 | 5.14% |
| AC | 4 | 0.059 | +0.043 | 5,134.2 | 5,036.1 | 0.978 (0.974-0.978) | 197.0 | 69.9 / 119.3 / 2,509.8 / 15,020.0 | 4.62% |
| AB | 4 | 0.134 | +0.117 | 5,167.5 | 5,044.7 | 0.967 (0.964-0.968) | 213.1 | 70.4 / 119.9 / 2,527.7 / 7,047.7 | 5.38% |
| ABC | 4 | 0.067 | +0.051 | 5,135.9 | 5,045.8 | 0.975 (0.973-0.977) | 205.9 | 71.5 / 119.9 / 2,521.4 / 8,564.0 | 4.94% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300
- A: req=6402 wfe_waits=0 slow_sleeps=279522 defer_sleeps=0 hot_iters=12241447 parks=1230 park_timeouts=1002 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=0 win20=0 win80=0 win300=0
- AC: req=6402 wfe_waits=0 slow_sleeps=290552 defer_sleeps=49528 hot_iters=0 parks=1329 park_timeouts=1145 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=226 win0=6402 win20=0 win80=0 win300=0
- AB: req=6402 wfe_waits=0 slow_sleeps=278779 defer_sleeps=0 hot_iters=11729648 parks=1488 park_timeouts=1246 unseen_latch=0 gate_opens=0 latch_trips=2 cap_forced=0 guard_skips=0 win0=0 win20=0 win80=0 win300=0
- ABC: req=6402 wfe_waits=0 slow_sleeps=288702 defer_sleeps=49286 hot_iters=0 parks=1418 park_timeouts=1239 unseen_latch=0 gate_opens=0 latch_trips=2 cap_forced=0 guard_skips=254 win0=6402 win20=0 win80=0 win300=0

### c13: two processes, no load (probe host, load off, spin 2000 us + 0-1000 us, 3200 launches, 1 lane(s), gap 0 us, graph 0, 2 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | VDD_CPU_GPU_CV (mW) | VDD_IN (mW) | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.013 | +0.000 | 4,958.5 | 4,919.2 | 1,735 | 7,037 | - | - | - |
| W | 4 | 2.010 | +1.997 | 4,997.1 | 4,932.0 | 1,765 | 7,077 | 40.9 | 15.4 / 16.2 / 372.7 / 2,441.6 | 0.93% |
| M100 | 4 | 0.119 | +0.106 | 5,098.7 | 5,034.3 | 1,821 | 7,134 | 168.9 | 71.8 / 114.9 / 2,500.6 / 4,520.6 | 3.88% |
| A | 4 | 0.123 | +0.110 | 5,095.6 | 5,043.5 | 1,825 | 7,151 | 165.7 | 71.8 / 117.6 / 2,487.1 / 2,544.0 | 3.48% |
| AC | 4 | 0.051 | +0.038 | 5,096.1 | 5,074.5 | 1,808 | 7,116 | 170.6 | 71.3 / 117.7 / 2,489.5 / 2,539.7 | 3.69% |
| AB | 4 | 2.009 | +1.996 | 4,997.2 | 4,929.2 | 1,752 | 7,065 | 41.8 | 15.9 / 16.6 / 373.4 / 2,457.3 | 0.93% |
| ABC | 4 | 1.936 | +1.923 | 4,997.5 | 4,932.9 | 1,762 | 7,064 | 42.2 | 15.7 / 16.2 / 374.0 / 2,454.1 | 0.96% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300
- A: req=6402 wfe_waits=0 slow_sleeps=300062 defer_sleeps=0 hot_iters=12451158 parks=648 park_timeouts=618 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=0 win20=0 win80=0 win300=0
- AC: req=6402 wfe_waits=0 slow_sleeps=299926 defer_sleeps=51205 hot_iters=0 parks=654 park_timeouts=618 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=2 win0=6402 win20=0 win80=0 win300=0
- AB: req=6402 wfe_waits=291224 slow_sleeps=319 defer_sleeps=0 hot_iters=54720 parks=670 park_timeouts=626 unseen_latch=0 gate_opens=12 latch_trips=0 cap_forced=0 guard_skips=0 win0=0 win20=0 win80=0 win300=0
- ABC: req=6402 wfe_waits=260748 slow_sleeps=714 defer_sleeps=51168 hot_iters=0 parks=676 park_timeouts=634 unseen_latch=0 gate_opens=20 latch_trips=4 cap_forced=0 guard_skips=6 win0=6402 win20=0 win80=0 win300=0

### c14: short kernels, no load (probe host, load off, spin 200 us + 0-200 us, 4000 launches, 1 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | VDD_CPU_GPU_CV (mW) | VDD_IN (mW) | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.070 | +0.000 | 324.6 | 324.1 | - | - | - | - | - |
| W | 4 | 1.085 | +1.014 | 350.1 | 349.2 | 1,824 | 7,134 | 14.0 | 13.8 / 14.8 / 15.1 / 99.8 | 0.00% |
| M100 | 4 | 0.584 | +0.514 | 403.5 | 360.6 | 1,959 | 7,263 | 68.0 | 67.6 / 108.1 / 117.9 / 1,208.2 | 0.01% |
| A | 4 | 0.584 | +0.516 | 402.7 | 408.7 | 1,946 | 7,279 | 66.8 | 65.2 / 108.2 / 118.2 / 189.3 | 0.00% |
| AC | 4 | 0.128 | +0.060 | 403.8 | 401.3 | 1,868 | 7,180 | 66.3 | 66.8 / 107.9 / 118.1 / 1,995.0 | 0.01% |
| AB | 4 | 1.095 | +1.025 | 352.3 | 352.1 | 1,807 | 7,115 | 14.8 | 14.8 / 15.0 / 15.1 / 117.6 | 0.00% |
| ABC | 4 | 0.583 | +0.513 | 352.6 | 351.5 | 1,874 | 7,198 | 15.5 | 14.9 / 15.6 / 30.4 / 184.8 | 0.00% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300
- A: req=4001 wfe_waits=0 slow_sleeps=7758 defer_sleeps=0 hot_iters=7822796 parks=296 park_timeouts=296 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=0 win20=0 win80=0 win300=0
- AC: req=4001 wfe_waits=0 slow_sleeps=8004 defer_sleeps=31960 hot_iters=56866 parks=294 park_timeouts=294 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=5 win0=3276 win20=724 win80=0 win300=0
- AB: req=4001 wfe_waits=34591 slow_sleeps=10 defer_sleeps=0 hot_iters=31192 parks=294 park_timeouts=293 unseen_latch=0 gate_opens=1 latch_trips=0 cap_forced=0 guard_skips=0 win0=0 win20=0 win80=0 win300=0
- ABC: req=4001 wfe_waits=19684 slow_sleeps=14 defer_sleeps=31902 hot_iters=3816 parks=296 park_timeouts=294 unseen_latch=0 gate_opens=2 latch_trips=0 cap_forced=0 guard_skips=0 win0=3052 win20=900 win80=50 win300=2

### c15: short kernels, full load (probe host, load on6, spin 200 us + 0-200 us, 4000 launches, 1 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | load vs P | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.090 | +0.000 | 371.4 | 335.2 | 1.000 (1.000-1.000) | - | - | - |
| W | 4 | 0.699 | +0.609 | 563.9 | 373.6 | 0.892 (0.875-0.908) | 158.3 | 14.6 / 15.1 / 3,926.3 / 7,953.5 | 3.93% |
| M100 | 4 | 0.380 | +0.293 | 651.7 | 462.8 | 0.942 (0.940-0.952) | 253.2 | 71.3 / 115.6 / 3,937.4 / 5,862.5 | 6.61% |
| A | 4 | 0.367 | +0.278 | 665.5 | 411.1 | 0.950 (0.944-0.959) | 267.4 | 73.4 / 118.5 / 3,929.8 / 5,895.0 | 7.19% |
| AC | 4 | 0.133 | +0.044 | 483.2 | 420.4 | 0.980 (0.963-0.982) | 81.5 | 69.3 / 109.9 / 119.9 / 5,380.7 | 0.45% |
| AB | 4 | 0.375 | +0.285 | 662.4 | 411.4 | 0.946 (0.940-0.957) | 259.9 | 73.4 / 118.2 / 3,878.8 / 6,027.2 | 6.97% |
| ABC | 4 | 0.132 | +0.044 | 482.9 | 409.0 | 0.973 (0.969-0.991) | 82.5 | 69.8 / 110.2 / 121.7 / 5,520.9 | 0.47% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300
- A: req=4001 wfe_waits=0 slow_sleeps=8490 defer_sleeps=0 hot_iters=7631524 parks=612 park_timeouts=520 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=0 win20=0 win80=0 win300=0
- AC: req=4001 wfe_waits=0 slow_sleeps=9330 defer_sleeps=30944 hot_iters=2996 parks=552 park_timeouts=501 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=129 win0=3962 win20=39 win80=0 win300=0
- AB: req=4001 wfe_waits=0 slow_sleeps=8417 defer_sleeps=0 hot_iters=7454955 parks=534 park_timeouts=464 unseen_latch=0 gate_opens=0 latch_trips=1 cap_forced=0 guard_skips=0 win0=0 win20=0 win80=0 win300=0
- ABC: req=4001 wfe_waits=0 slow_sleeps=9130 defer_sleeps=30910 hot_iters=98 parks=548 park_timeouts=492 unseen_latch=0 gate_opens=0 latch_trips=1 cap_forced=0 guard_skips=128 win0=3904 win20=98 win80=0 win300=0

### c16: two processes, partial load (probe host, load on3, spin 2000 us + 0-1000 us, 3200 launches, 1 lane(s), gap 0 us, graph 0, 2 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | load vs P | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.016 | +0.000 | 4,963.9 | 4,918.6 | 1.000 (1.000-1.000) | - | - | - |
| W | 4 | 2.004 | +1.988 | 5,006.4 | 4,939.3 | 1.000 (0.996-1.001) | 41.2 | 15.4 / 16.3 / 375.4 / 4,310.0 | 0.92% |
| M100 | 4 | 0.129 | +0.113 | 5,096.6 | 5,036.6 | 1.000 (0.998-1.002) | 170.2 | 71.4 / 117.7 / 2,497.5 / 4,231.9 | 3.84% |
| A | 4 | 0.131 | +0.114 | 5,091.0 | 5,031.4 | 1.000 (0.998-1.000) | 170.2 | 69.0 / 119.0 / 2,495.5 / 4,303.8 | 3.71% |
| AC | 4 | 0.082 | +0.065 | 5,101.0 | 5,054.3 | 1.000 (0.998-1.000) | 174.8 | 70.7 / 120.6 / 2,498.4 / 3,422.8 | 3.86% |
| AB | 4 | 1.716 | +1.700 | 5,019.0 | 4,971.6 | 1.000 (0.998-1.002) | 63.6 | 16.1 / 56.4 / 2,439.8 / 4,130.1 | 1.52% |
| ABC | 4 | 1.708 | +1.692 | 5,022.0 | 4,977.9 | 1.000 (0.998-1.002) | 63.2 | 16.0 / 53.1 / 2,440.1 / 5,993.3 | 1.50% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300
- A: req=6402 wfe_waits=0 slow_sleeps=295704 defer_sleeps=0 hot_iters=12380096 parks=672 park_timeouts=615 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=0 win20=0 win80=0 win300=0
- AC: req=6402 wfe_waits=0 slow_sleeps=288714 defer_sleeps=50952 hot_iters=0 parks=698 park_timeouts=634 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=35 win0=6402 win20=0 win80=0 win300=0
- AB: req=6402 wfe_waits=247807 slow_sleeps=44712 defer_sleeps=0 hot_iters=1758842 parks=824 park_timeouts=681 unseen_latch=0 gate_opens=1248 latch_trips=89 cap_forced=0 guard_skips=0 win0=0 win20=0 win80=0 win300=0
- ABC: req=6402 wfe_waits=231014 slow_sleeps=34708 defer_sleeps=50900 hot_iters=0 parks=793 park_timeouts=664 unseen_latch=0 gate_opens=966 latch_trips=83 cap_forced=0 guard_skips=44 win0=6402 win20=0 win80=0 win300=0

### c17: load 5 ms on / 5 ms off (probe host, load duty6, spin 2000 us + 0-1000 us, 3200 launches, 1 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | load vs P | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.011 | +0.000 | 3,645.1 | 2,910.3 | 1.000 (1.000-1.000) | - | - | - |
| W | 4 | 0.872 | +0.861 | 3,104.8 | 2,757.7 | 0.879 (0.873-0.887) | 20.1 | 14.5 / 15.7 / 18.0 / 4,962.0 | 0.15% |
| M100 | 4 | 0.072 | +0.062 | 3,879.8 | 2,850.7 | 0.944 (0.941-0.952) | 942.7 | 82.9 / 3,833.3 / 4,916.7 / 5,071.4 | 25.14% |
| A | 4 | 0.073 | +0.062 | 3,939.8 | 2,889.6 | 0.954 (0.952-0.957) | 1,008.2 | 89.4 / 3,909.1 / 4,943.4 / 5,077.9 | 27.14% |
| AC | 4 | 0.025 | +0.014 | 3,917.3 | 2,841.6 | 0.970 (0.967-0.976) | 1,098.6 | 90.8 / 3,960.0 / 4,962.8 / 5,067.0 | 30.66% |
| AB | 4 | 0.366 | +0.355 | 3,871.2 | 3,018.9 | 0.907 (0.897-0.923) | 379.7 | 16.8 / 2,225.4 / 4,307.1 / 4,974.8 | 11.70% |
| ABC | 4 | 0.334 | +0.323 | 3,611.2 | 2,879.7 | 0.927 (0.917-0.932) | 411.1 | 19.5 / 2,228.5 / 4,456.4 / 4,972.0 | 13.01% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300
- A: req=3201 wfe_waits=0 slow_sleeps=67392 defer_sleeps=0 hot_iters=6218002 parks=1402 park_timeouts=748 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=0 win20=0 win80=0 win300=0
- AC: req=3201 wfe_waits=0 slow_sleeps=70074 defer_sleeps=15730 hot_iters=0 parks=634 park_timeouts=477 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=1192 win0=3201 win20=0 win80=0 win300=0
- AB: req=3201 wfe_waits=42962 slow_sleeps=40017 defer_sleeps=0 hot_iters=2767724 parks=2440 park_timeouts=1585 unseen_latch=0 gate_opens=1226 latch_trips=156 cap_forced=0 guard_skips=0 win0=0 win20=0 win80=0 win300=0
- ABC: req=3201 wfe_waits=35165 slow_sleeps=38960 defer_sleeps=18355 hot_iters=0 parks=1578 park_timeouts=1076 unseen_latch=0 gate_opens=1069 latch_trips=92 cap_forced=0 guard_skips=888 win0=3201 win20=0 win80=0 win300=0

### c18: CUDA graph launches (not noted) (probe host, load off, spin 2000 us + 0-1000 us, 3200 launches, 1 lane(s), gap 0 us, graph 1, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | VDD_CPU_GPU_CV (mW) | VDD_IN (mW) | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.010 | +0.000 | 2,526.6 | 2,528.0 | 1,685 | 6,919 | - | - | - |
| W | 4 | 0.079 | +0.070 | 3,073.1 | 3,071.3 | 1,723 | 6,967 | 535.0 | 533.3 / 939.6 / 1,027.0 / 1,069.2 | 3.85% |
| M100 | 4 | 0.100 | +0.090 | 2,602.4 | 2,640.1 | 1,743 | 6,999 | 64.7 | 63.9 / 106.6 / 118.1 / 142.8 | 0.00% |
| A | 4 | 0.100 | +0.091 | 2,606.4 | 2,592.3 | 1,745 | 7,008 | 68.2 | 69.8 / 109.3 / 118.3 / 620.4 | 0.00% |
| AC | 4 | 0.028 | +0.019 | 2,606.7 | 2,584.6 | 1,728 | 6,966 | 68.6 | 69.7 / 109.0 / 118.0 / 727.9 | 0.00% |
| AB | 4 | 1.008 | +0.998 | 2,552.5 | 2,553.1 | 1,702 | 6,949 | 15.3 | 14.9 / 15.1 / 15.4 / 680.6 | 0.00% |
| ABC | 4 | 0.938 | +0.928 | 2,553.4 | 2,554.1 | 1,701 | 6,946 | 20.7 | 14.8 / 20.4 / 110.0 / 614.5 | 0.00% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300
- A: req=3202 wfe_waits=0 slow_sleeps=73951 defer_sleeps=0 hot_iters=6265144 parks=298 park_timeouts=296 unseen_latch=1 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=0 win20=0 win80=0 win300=0
- AC: req=3202 wfe_waits=0 slow_sleeps=74214 defer_sleeps=25610 hot_iters=0 parks=298 park_timeouts=296 unseen_latch=1 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=1 win0=3202 win20=0 win80=0 win300=0
- AB: req=3202 wfe_waits=85510 slow_sleeps=36 defer_sleeps=0 hot_iters=24490 parks=298 park_timeouts=296 unseen_latch=1 gate_opens=2 latch_trips=0 cap_forced=0 guard_skips=0 win0=0 win20=0 win80=0 win300=0
- ABC: req=3202 wfe_waits=68698 slow_sleeps=74 defer_sleeps=25616 hot_iters=0 parks=297 park_timeouts=296 unseen_latch=1 gate_opens=3 latch_trips=0 cap_forced=0 guard_skips=0 win0=3202 win20=0 win80=0 win300=0
