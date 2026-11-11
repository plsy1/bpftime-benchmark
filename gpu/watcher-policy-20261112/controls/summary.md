Runs: 168, valid 168; rounds 1,2,3,4; max temperature 53.6 C


### c01: full load (probe host, load on6, spin 2000 us + 0-1000 us, 3200 launches, 1 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | load vs P | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.013 | +0.000 | 2,574.2 | 2,543.1 | 1.000 (1.000-1.000) | - | - | - |
| M100 | 4 | 0.100 | +0.087 | 2,749.4 | 2,657.3 | 0.980 (0.976-0.982) | 146.9 | 67.9 / 113.9 / 2,587.9 / 5,855.4 | 3.55% |
| A | 4 | 0.103 | +0.090 | 2,752.8 | 2,620.7 | 0.979 (0.972-0.981) | 151.8 | 72.6 / 115.0 / 2,569.6 / 5,828.3 | 3.48% |
| A0 | 4 | 0.034 | +0.020 | 2,691.3 | 2,620.1 | 0.991 (0.990-0.991) | 89.7 | 67.6 / 110.8 / 527.0 / 5,556.4 | 0.84% |
| AC | 4 | 0.037 | +0.024 | 2,700.8 | 2,608.0 | 0.988 (0.986-0.990) | 95.0 | 70.9 / 110.6 / 1,008.8 / 7,108.4 | 1.00% |
| AC0 | 4 | 0.034 | +0.020 | 2,694.9 | 2,620.4 | 0.990 (0.986-0.991) | 91.9 | 67.4 / 110.5 / 655.1 / 7,597.7 | 0.90% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300
- A: req=3201 wfe_waits=0 slow_sleeps=69278 defer_sleeps=0 hot_iters=6264993 parks=607 park_timeouts=556 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=0 win20=0 win80=0 win300=0
- A0: req=3201 wfe_waits=0 slow_sleeps=79617 defer_sleeps=0 hot_iters=0 parks=520 park_timeouts=456 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=0 win20=0 win80=0 win300=0
- AC: req=3201 wfe_waits=0 slow_sleeps=73640 defer_sleeps=25082 hot_iters=0 parks=536 park_timeouts=488 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=66 win0=3201 win20=0 win80=0 win300=0
- AC0: req=3201 wfe_waits=0 slow_sleeps=79542 defer_sleeps=0 hot_iters=0 parks=516 park_timeouts=458 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=3201 win20=0 win80=0 win300=0

### c02: no load (power) (probe host, load off, spin 2000 us + 0-1000 us, 3200 launches, 1 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | VDD_CPU_GPU_CV (mW) | VDD_IN (mW) | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.011 | +0.000 | 2,526.9 | 2,528.1 | 1,687 | 6,931 | - | - | - |
| M100 | 4 | 0.102 | +0.092 | 2,604.4 | 2,640.5 | 1,741 | 6,995 | 64.6 | 63.8 / 107.3 / 117.9 / 141.0 | 0.00% |
| A | 4 | 0.104 | +0.093 | 2,606.3 | 2,594.4 | 1,751 | 7,013 | 68.0 | 70.0 / 109.3 / 118.5 / 473.8 | 0.00% |
| A0 | 4 | 0.030 | +0.019 | 2,606.9 | 2,603.6 | 1,728 | 6,973 | 67.4 | 68.1 / 108.8 / 118.2 / 240.1 | 0.00% |
| AC | 4 | 0.032 | +0.021 | 2,609.1 | 2,586.9 | 1,731 | 6,982 | 68.1 | 69.3 / 108.9 / 118.2 / 557.3 | 0.00% |
| AC0 | 4 | 0.029 | +0.019 | 2,606.3 | 2,604.7 | 1,730 | 6,977 | 67.1 | 67.8 / 108.7 / 118.0 / 147.2 | 0.00% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300
- A: req=3201 wfe_waits=0 slow_sleeps=73936 defer_sleeps=0 hot_iters=6267381 parks=300 park_timeouts=299 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=0 win20=0 win80=0 win300=0
- A0: req=3201 wfe_waits=0 slow_sleeps=79944 defer_sleeps=0 hot_iters=0 parks=299 park_timeouts=298 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=0 win20=0 win80=0 win300=0
- AC: req=3201 wfe_waits=0 slow_sleeps=74262 defer_sleeps=25608 hot_iters=0 parks=297 park_timeouts=296 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=3201 win20=0 win80=0 win300=0
- AC0: req=3201 wfe_waits=0 slow_sleeps=79964 defer_sleeps=0 hot_iters=0 parks=297 park_timeouts=296 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=3201 win20=0 win80=0 win300=0

### c05: burst, full load (probe host, load on6, spin 2000 us + 0-1000 us, 2400 launches, 32 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | load vs P | cost per call (us) |
|---|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.014 | +0.000 | 2,579.6 | 2,551.8 | 1.000 (1.000-1.000) | - |
| M100 | 4 | 0.177 | +0.163 | 3,123.9 | 2,946.0 | 0.969 (0.969-0.970) | 17.0 |
| A | 4 | 0.182 | +0.168 | 3,166.6 | 3,014.8 | 0.968 (0.967-0.970) | 18.3 |
| A0 | 4 | 0.030 | +0.016 | 6,073.0 | 5,913.9 | 0.994 (0.991-0.995) | 109.1 |
| AC | 4 | 0.128 | +0.114 | 3,126.1 | 3,014.9 | 0.975 (0.937-0.977) | 17.1 |
| AC0 | 4 | 0.125 | +0.111 | 3,196.1 | 3,080.9 | 0.977 (0.971-0.979) | 19.3 |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300
- A: req=76832 wfe_waits=0 slow_sleeps=47326 defer_sleeps=0 hot_iters=8606867 parks=535 park_timeouts=488 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=0 win20=0 win80=0 win300=0
- A0: req=76832 wfe_waits=0 slow_sleeps=134014 defer_sleeps=0 hot_iters=0 parks=537 park_timeouts=487 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=0 win20=0 win80=0 win300=0
- AC: req=76832 wfe_waits=0 slow_sleeps=51079 defer_sleeps=17675 hot_iters=4021819 parks=598 park_timeouts=532 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=641 win0=2415 win20=74404 win80=12 win300=0
- AC0: req=76832 wfe_waits=0 slow_sleeps=56705 defer_sleeps=0 hot_iters=4158411 parks=1192 park_timeouts=428 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=2412 win20=74407 win80=12 win300=0

### c06: burst, no load (probe host, load off, spin 2000 us + 0-1000 us, 2400 launches, 32 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | VDD_CPU_GPU_CV (mW) | VDD_IN (mW) | cost per call (us) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.011 | +0.000 | 2,528.7 | 2,533.2 | 1,701 | 6,952 | - |
| M100 | 4 | 0.187 | +0.176 | 2,871.2 | 2,908.8 | 1,781 | 7,048 | 10.7 |
| A | 4 | 0.196 | +0.186 | 2,904.3 | 2,892.9 | 1,776 | 7,043 | 11.7 |
| A0 | 4 | 0.028 | +0.017 | 5,875.6 | 5,874.5 | 1,728 | 6,996 | 104.6 |
| AC | 4 | 0.133 | +0.122 | 2,925.8 | 2,914.0 | 1,773 | 7,044 | 12.4 |
| AC0 | 4 | 0.127 | +0.116 | 3,007.6 | 3,009.2 | 1,773 | 7,037 | 14.9 |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300
- A: req=76832 wfe_waits=0 slow_sleeps=55440 defer_sleeps=0 hot_iters=8608032 parks=298 park_timeouts=297 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=0 win20=0 win80=0 win300=0
- A0: req=76832 wfe_waits=0 slow_sleeps=134370 defer_sleeps=0 hot_iters=0 parks=300 park_timeouts=298 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=0 win20=0 win80=0 win300=0
- AC: req=76832 wfe_waits=0 slow_sleeps=55407 defer_sleeps=21718 hot_iters=4039232 parks=300 park_timeouts=298 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=1 win0=2401 win20=74431 win80=0 win300=0
- AC0: req=76832 wfe_waits=0 slow_sleeps=62046 defer_sleeps=0 hot_iters=4038024 parks=867 park_timeouts=298 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=2401 win20=74420 win80=10 win300=0

### c10: low GPU duty, full load (probe host, load on6, spin 2000 us + 0-1000 us, 400 launches, 1 lane(s), gap 18000 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | load vs P | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.002 | +0.000 | 3,026.4 | 2,970.9 | 1.000 (1.000-1.000) | - | - | - |
| M100 | 4 | 0.025 | +0.022 | 3,157.1 | 3,076.8 | 0.990 (0.987-0.992) | 106.3 | 68.0 / 112.2 / 1,772.2 / 6,978.1 | 1.31% |
| A | 4 | 0.018 | +0.015 | 3,180.8 | 3,119.9 | 0.996 (0.996-0.998) | 88.8 | 67.8 / 111.7 / 121.6 / 5,174.6 | 0.56% |
| A0 | 4 | 0.009 | +0.007 | 3,126.4 | 3,034.9 | 0.996 (0.995-0.998) | 115.7 | 71.8 / 115.5 / 2,240.2 / 7,021.7 | 1.81% |
| AC | 4 | 0.010 | +0.007 | 3,108.7 | 3,041.2 | 0.996 (0.995-0.997) | 112.6 | 70.5 / 115.2 / 1,719.6 / 3,633.2 | 1.88% |
| AC0 | 4 | 0.009 | +0.006 | 3,129.7 | 3,030.2 | 0.998 (0.993-1.000) | 103.6 | 71.9 / 112.4 / 1,673.9 / 2,946.2 | 1.75% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300
- A: req=401 wfe_waits=0 slow_sleeps=12240 defer_sleeps=0 hot_iters=769196 parks=7483 park_timeouts=7083 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=0 win20=0 win80=0 win300=0
- A0: req=401 wfe_waits=0 slow_sleeps=12936 defer_sleeps=0 hot_iters=0 parks=7522 park_timeouts=7122 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=0 win20=0 win80=0 win300=0
- AC: req=401 wfe_waits=0 slow_sleeps=11860 defer_sleeps=3164 hot_iters=0 parks=7648 park_timeouts=7248 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=6 win0=401 win20=0 win80=0 win300=0
- AC0: req=401 wfe_waits=0 slow_sleeps=13087 defer_sleeps=0 hot_iters=0 parks=7572 park_timeouts=7172 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=401 win20=0 win80=0 win300=0

### c15: short kernels, full load (probe host, load on6, spin 200 us + 0-200 us, 4000 launches, 1 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | load vs P | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.090 | +0.000 | 370.6 | 335.3 | 1.000 (1.000-1.000) | - | - | - |
| M100 | 4 | 0.376 | +0.283 | 658.0 | 462.5 | 0.946 (0.941-0.950) | 258.8 | 71.0 / 116.6 / 3,976.5 / 6,023.4 | 6.91% |
| A | 4 | 0.376 | +0.287 | 658.0 | 410.9 | 0.948 (0.946-0.951) | 258.9 | 73.2 / 118.0 / 3,925.1 / 8,079.6 | 6.91% |
| A0 | 4 | 0.113 | +0.023 | 473.9 | 420.2 | 0.989 (0.986-0.995) | 73.8 | 65.4 / 109.2 / 119.1 / 5,901.6 | 0.21% |
| AC | 4 | 0.125 | +0.036 | 480.4 | 406.1 | 0.979 (0.974-0.982) | 81.5 | 69.4 / 110.1 / 119.6 / 5,711.0 | 0.46% |
| AC0 | 4 | 0.110 | +0.021 | 471.2 | 420.4 | 0.982 (0.977-0.991) | 73.8 | 65.8 / 109.9 / 119.6 / 5,267.7 | 0.22% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300
- A: req=4001 wfe_waits=0 slow_sleeps=8445 defer_sleeps=0 hot_iters=7813986 parks=508 park_timeouts=446 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=0 win20=0 win80=0 win300=0
- A0: req=4001 wfe_waits=0 slow_sleeps=16478 defer_sleeps=0 hot_iters=0 parks=563 park_timeouts=513 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=0 win20=0 win80=0 win300=0
- AC: req=4001 wfe_waits=0 slow_sleeps=9146 defer_sleeps=31046 hot_iters=6540 parks=574 park_timeouts=517 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=113 win0=3918 win20=83 win80=0 win300=0
- AC0: req=4001 wfe_waits=0 slow_sleeps=16452 defer_sleeps=0 hot_iters=0 parks=566 park_timeouts=519 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=4001 win20=0 win80=0 win300=0

### c17: load 5 ms on / 5 ms off (probe host, load duty6, spin 2000 us + 0-1000 us, 3200 launches, 1 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | load vs P | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.011 | +0.000 | 3,639.4 | 2,909.2 | 1.000 (1.000-1.000) | - | - | - |
| M100 | 4 | 0.072 | +0.062 | 3,904.7 | 2,851.4 | 0.948 (0.941-0.952) | 963.6 | 84.0 / 3,863.7 / 4,946.1 / 5,077.5 | 25.69% |
| A | 4 | 0.073 | +0.062 | 3,923.8 | 2,837.5 | 0.952 (0.948-0.956) | 1,003.4 | 88.1 / 3,910.7 / 4,945.1 / 5,077.2 | 26.79% |
| A0 | 4 | 0.024 | +0.013 | 3,750.4 | 2,819.2 | 0.965 (0.958-0.970) | 985.4 | 88.8 / 3,760.7 / 4,916.7 / 5,058.8 | 28.05% |
| AC | 4 | 0.025 | +0.014 | 3,913.6 | 2,849.6 | 0.971 (0.967-0.977) | 1,093.2 | 91.7 / 3,927.2 / 4,946.0 / 5,073.0 | 30.74% |
| AC0 | 4 | 0.023 | +0.013 | 3,795.1 | 2,819.2 | 0.969 (0.965-0.974) | 1,009.1 | 89.2 / 3,807.4 / 4,925.4 / 5,064.9 | 28.86% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300
- A: req=3201 wfe_waits=0 slow_sleeps=67527 defer_sleeps=0 hot_iters=6236309 parks=1334 park_timeouts=695 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=0 win20=0 win80=0 win300=0
- A0: req=3201 wfe_waits=0 slow_sleeps=73156 defer_sleeps=0 hot_iters=0 parks=532 park_timeouts=410 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=0 win20=0 win80=0 win300=0
- AC: req=3201 wfe_waits=0 slow_sleeps=69865 defer_sleeps=15953 hot_iters=0 parks=651 park_timeouts=486 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=1166 win0=3201 win20=0 win80=0 win300=0
- AC0: req=3201 wfe_waits=0 slow_sleeps=72826 defer_sleeps=0 hot_iters=0 parks=560 park_timeouts=428 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=3201 win20=0 win80=0 win300=0
