Runs: 224, valid 224; rounds 1,2,3,4; max temperature 53.4 C


### c01: full load (probe host, load on6, spin 2000 us + 0-1000 us, 3200 launches, 1 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | load vs P | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.013 | +0.000 | 2,575.2 | 2,542.4 | 1.000 (1.000-1.000) | - | - | - |
| W | 4 | 0.845 | +0.832 | 2,906.0 | 2,650.7 | 0.859 (0.854-0.865) | 290.7 | 14.6 / 418.7 / 3,482.8 / 7,600.8 | 9.58% |
| D | 4 | 0.036 | +0.023 | 2,695.2 | 2,604.8 | 0.989 (0.988-0.990) | 94.3 | 71.1 / 110.9 / 1,230.8 / 5,014.8 | 1.17% |
| AC3 | 4 | 0.036 | +0.023 | 2,699.6 | 2,606.6 | 0.989 (0.987-0.990) | 95.7 | 70.8 / 110.4 / 1,154.9 / 5,619.2 | 1.05% |
| M100 | 4 | 0.098 | +0.086 | 2,747.1 | 2,656.2 | 0.980 (0.978-0.981) | 153.8 | 67.3 / 113.1 / 2,672.4 / 6,217.3 | 3.83% |
| AC2 | 4 | 0.036 | +0.023 | 2,695.3 | 2,606.4 | 0.989 (0.988-0.989) | 94.7 | 70.8 / 110.5 / 854.7 / 5,753.7 | 0.96% |
| AC4 | 4 | 0.036 | +0.023 | 2,697.8 | 2,605.8 | 0.988 (0.987-0.989) | 93.9 | 70.8 / 111.0 / 918.1 / 5,419.1 | 0.96% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC3: req=3201 wfe_waits=0 slow_sleeps=73812 defer_sleeps=25150 hot_iters=0 parks=454 park_timeouts=412 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=57 win0=3201 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC2: req=3201 wfe_waits=0 slow_sleeps=73786 defer_sleeps=25136 hot_iters=0 parks=596 park_timeouts=551 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=57 win0=3201 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC4: req=3201 wfe_waits=0 slow_sleeps=73614 defer_sleeps=25106 hot_iters=0 parks=541 park_timeouts=494 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=60 win0=3201 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0

### c02: no load (power) (probe host, load off, spin 2000 us + 0-1000 us, 3200 launches, 1 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | VDD_CPU_GPU_CV (mW) | VDD_IN (mW) | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.010 | +0.000 | 2,525.6 | 2,526.4 | 1,685 | 6,914 | - | - | - |
| W | 4 | 1.012 | +1.002 | 2,550.7 | 2,552.8 | 1,696 | 6,936 | 13.9 | 13.7 / 14.6 / 15.0 / 49.2 | 0.00% |
| D | 4 | 0.030 | +0.020 | 2,605.6 | 2,585.1 | 1,730 | 6,963 | 68.1 | 69.5 / 109.1 / 118.1 / 408.4 | 0.00% |
| AC3 | 4 | 0.031 | +0.021 | 2,606.4 | 2,586.6 | 1,728 | 6,962 | 68.2 | 69.7 / 109.2 / 118.7 / 139.2 | 0.00% |
| M100 | 4 | 0.102 | +0.092 | 2,602.4 | 2,640.7 | 1,733 | 6,979 | 64.4 | 63.6 / 106.5 / 117.8 / 1,339.1 | 0.01% |
| AC2 | 4 | 0.031 | +0.021 | 2,605.0 | 2,586.5 | 1,726 | 6,961 | 67.8 | 69.4 / 108.8 / 117.7 / 121.0 | 0.00% |
| AC4 | 4 | 0.031 | +0.021 | 2,605.9 | 2,586.6 | 1,727 | 6,959 | 67.9 | 69.3 / 108.6 / 118.0 / 712.9 | 0.00% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC3: req=3201 wfe_waits=0 slow_sleeps=74197 defer_sleeps=25604 hot_iters=0 parks=298 park_timeouts=297 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=3201 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC2: req=3201 wfe_waits=0 slow_sleeps=74164 defer_sleeps=25608 hot_iters=0 parks=297 park_timeouts=296 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=3201 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC4: req=3201 wfe_waits=0 slow_sleeps=74173 defer_sleeps=25604 hot_iters=0 parks=297 park_timeouts=296 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=3201 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0

### c05: burst, full load (probe host, load on6, spin 2000 us + 0-1000 us, 2400 launches, 32 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | load vs P | cost per call (us) |
|---|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.014 | +0.000 | 2,577.5 | 2,549.5 | 1.000 (1.000-1.000) | - |
| W | 4 | 0.798 | +0.784 | 3,267.3 | 2,958.6 | 0.868 (0.865-0.871) | 21.6 |
| D | 4 | 0.136 | +0.123 | 3,069.7 | 2,944.9 | 0.974 (0.974-0.974) | 15.4 |
| AC3 | 4 | 0.137 | +0.124 | 3,078.1 | 2,945.8 | 0.973 (0.971-0.973) | 15.6 |
| M100 | 4 | 0.176 | +0.162 | 3,121.8 | 2,932.9 | 0.968 (0.966-0.969) | 17.0 |
| AC2 | 4 | 0.129 | +0.115 | 3,280.8 | 2,979.1 | 0.974 (0.973-0.975) | 22.0 |
| AC4 | 4 | 0.118 | +0.104 | 3,253.6 | 3,029.2 | 0.976 (0.976-0.977) | 21.1 |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC3: req=76832 wfe_waits=0 slow_sleeps=49726 defer_sleeps=15030 hot_iters=4547342 parks=582 park_timeouts=484 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=392 win0=1 win20=38184 win80=38646 win300=0 unseen_hits=0 unseen_unlatch=0
- AC2: req=76832 wfe_waits=0 slow_sleeps=53506 defer_sleeps=19730 hot_iters=4628560 parks=460 park_timeouts=422 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=4624 win0=6927 win20=0 win80=69905 win300=0 unseen_hits=0 unseen_unlatch=0
- AC4: req=76832 wfe_waits=0 slow_sleeps=54602 defer_sleeps=19798 hot_iters=3711611 parks=526 park_timeouts=482 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=4130 win0=6307 win20=70525 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0

### c06: burst, no load (probe host, load off, spin 2000 us + 0-1000 us, 2400 launches, 32 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | VDD_CPU_GPU_CV (mW) | VDD_IN (mW) | cost per call (us) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.010 | +0.000 | 2,527.4 | 2,531.0 | 1,693 | 6,936 | - |
| W | 4 | 1.012 | +1.002 | 2,823.2 | 2,826.6 | 1,732 | 6,972 | 9.2 |
| D | 4 | 0.138 | +0.127 | 2,903.2 | 2,902.8 | 1,768 | 7,012 | 11.7 |
| AC3 | 4 | 0.141 | +0.131 | 2,902.9 | 2,908.0 | 1,774 | 7,021 | 11.7 |
| M100 | 4 | 0.189 | +0.178 | 2,879.0 | 2,917.4 | 1,774 | 7,039 | 11.0 |
| AC2 | 4 | 0.144 | +0.134 | 2,915.1 | 2,946.2 | 1,769 | 7,027 | 12.1 |
| AC4 | 4 | 0.132 | +0.122 | 2,924.2 | 2,914.2 | 1,773 | 7,022 | 12.4 |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC3: req=76832 wfe_waits=0 slow_sleeps=54988 defer_sleeps=19210 hot_iters=4487318 parks=300 park_timeouts=298 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=1 win20=45056 win80=31775 win300=0 unseen_hits=0 unseen_unlatch=0
- AC2: req=76832 wfe_waits=0 slow_sleeps=54448 defer_sleeps=21610 hot_iters=4760930 parks=296 park_timeouts=296 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=2402 win20=0 win80=74430 win300=0 unseen_hits=0 unseen_unlatch=0
- AC4: req=76832 wfe_waits=0 slow_sleeps=55394 defer_sleeps=21582 hot_iters=4053323 parks=298 park_timeouts=297 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=2256 win20=74572 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0

### c11: idle agent: one launch, then 8 s (probe host, load off, spin 2000 us + 0-0 us, 1 launches, 1 lane(s), gap 8000000 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | VDD_CPU_GPU_CV (mW) | VDD_IN (mW) | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.000 | +0.000 | 2,035.1 | 2,035.1 | 1,447 | 6,615 | - | - | - |
| W | 4 | 0.002 | +0.002 | 2,066.3 | 2,066.3 | 1,447 | 6,627 | 14.8 | 14.8 / 15.0 / 15.0 / 15.0 | 0.00% |
| D | 4 | 0.002 | +0.002 | 2,124.3 | 2,124.3 | 1,458 | 6,635 | 68.1 | 68.2 / 116.7 / 116.7 / 116.7 | 0.00% |
| AC3 | 4 | 0.002 | +0.002 | 2,172.0 | 2,172.0 | 1,451 | 6,623 | 110.6 | 114.1 / 118.1 / 118.1 / 118.1 | 0.00% |
| M100 | 4 | 0.011 | +0.010 | 2,114.3 | 2,114.3 | 1,481 | 6,659 | 61.9 | 61.3 / 77.4 / 77.4 / 77.4 | 0.00% |
| AC2 | 4 | 0.002 | +0.002 | 2,124.1 | 2,124.1 | 1,457 | 6,640 | 67.7 | 65.6 / 117.6 / 117.6 / 117.6 | 0.00% |
| AC4 | 4 | 0.002 | +0.002 | 2,072.6 | 2,072.6 | 1,450 | 6,635 | 42.6 | 21.1 / 113.1 / 113.1 / 113.1 | 0.00% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC3: req=2 wfe_waits=0 slow_sleeps=19 defer_sleeps=16 hot_iters=0 parks=8259 park_timeouts=8258 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=2 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC2: req=2 wfe_waits=0 slow_sleeps=18 defer_sleeps=16 hot_iters=0 parks=8258 park_timeouts=8258 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=2 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC4: req=2 wfe_waits=0 slow_sleeps=18 defer_sleeps=16 hot_iters=0 parks=8256 park_timeouts=8256 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=2 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0

### c17: load 5 ms on / 5 ms off (probe host, load duty6, spin 2000 us + 0-1000 us, 3200 launches, 1 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | load vs P | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.011 | +0.000 | 3,639.2 | 2,907.4 | 1.000 (1.000-1.000) | - | - | - |
| W | 4 | 0.849 | +0.839 | 3,197.7 | 2,785.2 | 0.886 (0.880-0.889) | 18.8 | 14.5 / 15.7 / 17.5 / 4,801.8 | 0.11% |
| D | 4 | 0.024 | +0.014 | 3,935.4 | 2,832.4 | 0.972 (0.970-0.975) | 1,123.3 | 92.3 / 3,983.0 / 4,961.7 / 5,064.0 | 31.23% |
| AC3 | 4 | 0.024 | +0.013 | 3,967.3 | 2,844.5 | 0.974 (0.970-0.975) | 1,138.6 | 92.4 / 3,981.7 / 4,966.3 / 5,069.1 | 31.71% |
| M100 | 4 | 0.071 | +0.061 | 3,926.6 | 2,852.1 | 0.951 (0.943-0.955) | 985.9 | 83.0 / 3,878.1 / 4,926.8 / 5,071.0 | 26.30% |
| AC2 | 4 | 0.024 | +0.013 | 3,912.6 | 2,843.0 | 0.970 (0.968-0.976) | 1,108.2 | 91.2 / 3,953.7 / 4,961.6 / 5,059.4 | 30.75% |
| AC4 | 4 | 0.024 | +0.013 | 3,929.1 | 2,843.2 | 0.975 (0.972-0.977) | 1,131.6 | 92.5 / 3,977.6 / 4,960.0 / 5,062.3 | 31.68% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC3: req=3201 wfe_waits=0 slow_sleeps=70268 defer_sleeps=15650 hot_iters=0 parks=577 park_timeouts=446 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=1202 win0=3201 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC2: req=3201 wfe_waits=0 slow_sleeps=70469 defer_sleeps=16116 hot_iters=0 parks=594 park_timeouts=450 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=1150 win0=3201 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC4: req=3201 wfe_waits=0 slow_sleeps=69834 defer_sleeps=15670 hot_iters=0 parks=558 park_timeouts=432 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=1202 win0=3201 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0

### c23: request gaps about 55-115 us, no load (probe host, load off, spin 30 us + 0-60 us, 4000 launches, 1 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | VDD_CPU_GPU_CV (mW) | VDD_IN (mW) | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.265 | +0.000 | 83.5 | 83.3 | - | - | - | - | - |
| W | 4 | 1.302 | +1.036 | 109.9 | 109.8 | - | - | 14.1 | 13.9 / 14.8 / 15.6 / 328.9 | 0.00% |
| D | 4 | 0.343 | +0.078 | 122.2 | 120.8 | - | - | 25.9 | 25.3 / 35.9 / 38.3 / 114.5 | 0.00% |
| AC3 | 4 | 0.324 | +0.057 | 120.1 | 120.7 | - | - | 26.3 | 26.4 / 35.6 / 38.3 / 50.7 | 0.00% |
| M100 | 4 | 1.297 | +1.032 | 109.9 | 109.8 | - | - | 14.7 | 14.7 / 15.0 / 15.6 / 149.4 | 0.00% |
| AC2 | 4 | 0.356 | +0.097 | 122.5 | 121.3 | - | - | 25.8 | 25.3 / 35.8 / 38.4 / 101.5 | 0.00% |
| AC4 | 4 | 0.321 | +0.059 | 121.0 | 120.9 | - | - | 26.1 | 25.8 / 35.7 / 38.3 / 410.7 | 0.00% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC3: req=4001 wfe_waits=0 slow_sleeps=10 defer_sleeps=19872 hot_iters=0 parks=297 park_timeouts=296 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=4001 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC2: req=4001 wfe_waits=0 slow_sleeps=10 defer_sleeps=20266 hot_iters=0 parks=296 park_timeouts=296 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=4001 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC4: req=4001 wfe_waits=0 slow_sleeps=12 defer_sleeps=20001 hot_iters=0 parks=296 park_timeouts=294 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=4001 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0

### c25: 2 calls per launch, no load (probe host, load off, spin 2000 us + 0-1000 us, 2000 launches, 2 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | VDD_CPU_GPU_CV (mW) | VDD_IN (mW) | cost per call (us) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.010 | +0.000 | 2,526.2 | 2,530.4 | 1,686 | 6,917 | - |
| W | 4 | 1.013 | +1.003 | 2,561.0 | 2,567.1 | 1,701 | 6,945 | 17.3 |
| D | 4 | 0.041 | +0.030 | 2,615.5 | 2,612.3 | 1,735 | 6,972 | 44.8 |
| AC3 | 4 | 0.041 | +0.030 | 2,616.2 | 2,613.6 | 1,730 | 6,965 | 45.1 |
| M100 | 4 | 0.106 | +0.095 | 2,612.6 | 2,649.5 | 1,743 | 6,985 | 43.1 |
| AC2 | 4 | 0.050 | +0.039 | 2,629.9 | 2,656.7 | 1,727 | 6,962 | 51.8 |
| AC4 | 4 | 0.036 | +0.025 | 2,633.1 | 2,621.2 | 1,726 | 6,961 | 53.4 |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC3: req=4002 wfe_waits=0 slow_sleeps=46100 defer_sleeps=16010 hot_iters=393056 parks=298 park_timeouts=298 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=1 win20=4001 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
- AC2: req=4002 wfe_waits=0 slow_sleeps=45434 defer_sleeps=18009 hot_iters=898600 parks=298 park_timeouts=296 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=2001 win20=0 win80=2001 win300=0 unseen_hits=0 unseen_unlatch=0
- AC4: req=4002 wfe_waits=0 slow_sleeps=46221 defer_sleeps=18009 hot_iters=155056 parks=298 park_timeouts=297 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=2001 win20=2001 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
