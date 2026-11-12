Runs: 272, valid 272; rounds 1,2,3,4; max temperature 55.3 C


### c01: full load (probe host, load on6, spin 2000 us + 0-1000 us, 3200 launches, 1 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | load vs P | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.014 | +0.000 | 2,581.2 | 2,543.7 | 1.000 (1.000-1.000) | - | - | - |
| W | 4 | 0.836 | +0.822 | 2,897.4 | 2,643.2 | 0.863 (0.862-0.891) | 284.0 | 14.6 / 385.6 / 3,519.9 / 7,189.5 | 9.40% |
| D | 4 | 0.037 | +0.023 | 2,698.4 | 2,611.7 | 0.988 (0.982-1.010) | 93.2 | 70.7 / 110.6 / 934.2 / 8,292.1 | 0.98% |
| AC3 | 4 | 0.037 | +0.023 | 2,706.8 | 2,614.3 | 0.990 (0.989-1.003) | 95.4 | 70.6 / 110.7 / 992.0 / 5,722.5 | 0.99% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC3: req=3201 wfe_waits=0 slow_sleeps=73667 defer_sleeps=25074 hot_iters=0 parks=572 park_timeouts=517 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=66 win0=3201 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0

### c02: no load (power) (probe host, load off, spin 2000 us + 0-1000 us, 3200 launches, 1 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | VDD_CPU_GPU_CV (mW) | VDD_IN (mW) | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.010 | +0.000 | 2,526.4 | 2,527.7 | 1,703 | 6,904 | - | - | - |
| W | 4 | 1.014 | +1.004 | 2,554.4 | 2,555.1 | 1,699 | 6,961 | 14.2 | 13.9 / 14.8 / 15.1 / 911.6 | 0.00% |
| D | 4 | 0.031 | +0.021 | 2,607.6 | 2,585.1 | 1,729 | 6,978 | 68.2 | 69.1 / 108.7 / 117.9 / 3,146.4 | 0.01% |
| AC3 | 4 | 0.032 | +0.022 | 2,608.8 | 2,586.8 | 1,743 | 6,991 | 68.3 | 69.6 / 108.8 / 118.3 / 2,408.6 | 0.01% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC3: req=3201 wfe_waits=0 slow_sleeps=74210 defer_sleeps=25600 hot_iters=0 parks=298 park_timeouts=296 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=1 win0=3201 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0

### c05: burst, full load (probe host, load on6, spin 2000 us + 0-1000 us, 2400 launches, 32 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | load vs P | cost per call (us) |
|---|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.014 | +0.000 | 2,576.4 | 2,549.2 | 1.000 (1.000-1.000) | - |
| W | 4 | 0.786 | +0.773 | 3,282.4 | 2,957.2 | 0.868 (0.852-0.883) | 22.1 |
| D | 4 | 0.139 | +0.125 | 3,084.3 | 2,952.2 | 0.975 (0.966-0.977) | 15.8 |
| AC3 | 4 | 0.136 | +0.123 | 3,079.9 | 2,950.4 | 0.974 (0.914-0.978) | 15.8 |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC3: req=76832 wfe_waits=0 slow_sleeps=50006 defer_sleeps=14987 hot_iters=4531888 parks=566 park_timeouts=464 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=416 win0=2 win20=61204 win80=15627 win300=0 unseen_hits=0 unseen_unlatch=0

### c06: burst, no load (probe host, load off, spin 2000 us + 0-1000 us, 2400 launches, 32 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | VDD_CPU_GPU_CV (mW) | VDD_IN (mW) | cost per call (us) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.010 | +0.000 | 2,528.2 | 2,532.1 | 1,697 | 6,924 | - |
| W | 4 | 1.011 | +1.001 | 2,821.1 | 2,826.6 | 1,734 | 6,995 | 9.2 |
| D | 4 | 0.143 | +0.133 | 2,900.8 | 2,918.3 | 1,781 | 7,046 | 11.6 |
| AC3 | 4 | 0.140 | +0.129 | 2,904.2 | 2,906.6 | 1,774 | 7,020 | 11.8 |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC3: req=76832 wfe_waits=0 slow_sleeps=55095 defer_sleeps=19210 hot_iters=4473368 parks=296 park_timeouts=296 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=1 win20=60888 win80=15944 win300=0 unseen_hits=0 unseen_unlatch=0

### c11: idle agent: one launch, then 8 s (probe host, load off, spin 2000 us + 0-0 us, 1 launches, 1 lane(s), gap 8000000 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | VDD_CPU_GPU_CV (mW) | VDD_IN (mW) | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.000 | +0.000 | 2,034.3 | 2,034.3 | 1,451 | 6,644 | - | - | - |
| W | 4 | 0.002 | +0.002 | 2,073.6 | 2,073.6 | 1,467 | 6,667 | 14.6 | 14.5 / 14.8 / 14.8 / 14.8 | 0.00% |
| D | 4 | 0.002 | +0.002 | 2,130.8 | 2,130.8 | 1,469 | 6,672 | 73.9 | 74.1 / 117.2 / 117.2 / 117.2 | 0.00% |
| AC3 | 4 | 0.002 | +0.002 | 2,156.4 | 2,156.4 | 1,466 | 6,666 | 89.0 | 100.3 / 113.0 / 113.0 / 113.0 | 0.00% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC3: req=2 wfe_waits=0 slow_sleeps=19 defer_sleeps=16 hot_iters=0 parks=8256 park_timeouts=8255 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=2 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0

### c15: short kernels, full load (probe host, load on6, spin 200 us + 0-200 us, 4000 launches, 1 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | load vs P | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.086 | +0.000 | 373.5 | 337.2 | 1.000 (1.000-1.000) | - | - | - |
| W | 4 | 0.774 | +0.688 | 509.2 | 367.9 | 0.887 (0.855-0.920) | 126.8 | 14.5 / 15.1 / 3,900.7 / 7,975.4 | 2.98% |
| D | 4 | 0.133 | +0.048 | 485.6 | 406.9 | 0.969 (0.952-0.992) | 80.0 | 69.5 / 109.7 / 119.7 / 7,832.1 | 0.42% |
| AC3 | 4 | 0.138 | +0.052 | 483.7 | 415.6 | 0.981 (0.974-0.990) | 81.8 | 69.3 / 109.8 / 119.4 / 5,395.9 | 0.44% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC3: req=4001 wfe_waits=0 slow_sleeps=9338 defer_sleeps=30989 hot_iters=0 parks=531 park_timeouts=487 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=123 win0=4001 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0

### c17: load 5 ms on / 5 ms off (probe host, load duty6, spin 2000 us + 0-1000 us, 3200 launches, 1 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | load vs P | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.011 | +0.000 | 3,657.1 | 2,930.6 | 1.000 (1.000-1.000) | - | - | - |
| W | 4 | 0.863 | +0.852 | 3,138.7 | 2,756.6 | 0.886 (0.879-0.896) | 19.4 | 14.5 / 15.5 / 17.9 / 4,722.2 | 0.16% |
| D | 4 | 0.024 | +0.013 | 3,921.4 | 2,839.6 | 0.969 (0.960-0.991) | 1,096.0 | 91.1 / 3,956.9 / 4,954.1 / 5,073.3 | 30.39% |
| AC3 | 4 | 0.024 | +0.013 | 3,907.4 | 2,842.3 | 0.974 (0.965-0.980) | 1,080.3 | 91.1 / 3,932.6 / 4,951.1 / 5,065.3 | 30.28% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC3: req=3201 wfe_waits=0 slow_sleeps=70154 defer_sleeps=16186 hot_iters=0 parks=680 park_timeouts=510 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=1142 win0=3201 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0

### c18: CUDA graph launches (not noted) (probe host, load off, spin 2000 us + 0-1000 us, 3200 launches, 1 lane(s), gap 0 us, graph 1, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | VDD_CPU_GPU_CV (mW) | VDD_IN (mW) | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.009 | +0.000 | 2,527.6 | 2,527.8 | 1,699 | 6,944 | - | - | - |
| W | 4 | 0.080 | +0.071 | 3,073.1 | 3,071.2 | 1,763 | 7,041 | 534.4 | 534.2 / 939.2 / 1,026.4 / 1,103.4 | 3.80% |
| D | 4 | 0.027 | +0.017 | 2,606.2 | 2,582.9 | 1,751 | 7,005 | 69.3 | 70.0 / 109.2 / 118.2 / 2,305.8 | 0.02% |
| AC3 | 4 | 0.029 | +0.019 | 2,607.8 | 2,584.4 | 1,766 | 7,040 | 69.6 | 69.4 / 108.4 / 117.9 / 5,944.2 | 0.05% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC3: req=3202 wfe_waits=0 slow_sleeps=74051 defer_sleeps=25576 hot_iters=0 parks=301 park_timeouts=300 unseen_latch=1 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=6 win0=3202 win20=0 win80=0 win300=0 unseen_hits=2 unseen_unlatch=0

### c21: request gaps about 155-295 us, no load (probe host, load off, spin 130 us + 0-140 us, 4000 launches, 1 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | VDD_CPU_GPU_CV (mW) | VDD_IN (mW) | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.099 | +0.000 | 225.6 | 226.7 | - | - | - | - | - |
| W | 4 | 1.112 | +1.016 | 251.8 | 252.3 | - | - | 14.5 | 14.3 / 15.3 / 15.8 / 444.2 | 0.00% |
| D | 4 | 0.158 | +0.059 | 308.9 | 296.7 | 1,895 | 7,218 | 70.0 | 72.9 / 111.9 / 118.0 / 3,914.7 | 0.01% |
| AC3 | 4 | 0.154 | +0.058 | 307.6 | 297.0 | 1,887 | 7,214 | 69.6 | 72.0 / 112.3 / 118.7 / 1,514.7 | 0.01% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC3: req=4001 wfe_waits=0 slow_sleeps=4416 defer_sleeps=32006 hot_iters=0 parks=298 park_timeouts=296 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=4001 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0

### c23: request gaps about 55-115 us, no load (probe host, load off, spin 30 us + 0-60 us, 4000 launches, 1 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | VDD_CPU_GPU_CV (mW) | VDD_IN (mW) | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.264 | +0.000 | 84.8 | 84.7 | - | - | - | - | - |
| W | 4 | 1.262 | +1.014 | 110.0 | 109.7 | - | - | 14.2 | 13.9 / 14.8 / 15.8 / 131.8 | 0.00% |
| D | 4 | 0.342 | +0.090 | 122.0 | 120.9 | - | - | 25.9 | 25.4 / 35.8 / 38.3 / 112.4 | 0.00% |
| AC3 | 4 | 0.320 | +0.052 | 121.3 | 120.8 | - | - | 26.3 | 26.1 / 35.9 / 38.5 / 243.5 | 0.00% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC3: req=4001 wfe_waits=0 slow_sleeps=12 defer_sleeps=19998 hot_iters=0 parks=296 park_timeouts=295 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=4001 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0

### c24: request gaps about 55-115 us, full load (probe host, load on6, spin 30 us + 0-60 us, 4000 launches, 1 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | load vs P | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.245 | +0.000 | 128.3 | 92.5 | 1.000 (1.000-1.000) | - | - | - |
| W | 4 | 0.756 | +0.499 | 214.2 | 119.9 | 0.911 (0.887-0.955) | 65.6 | 14.5 / 15.0 / 3,868.6 / 8,016.8 | 1.30% |
| D | 4 | 0.279 | +0.032 | 196.6 | 144.7 | 0.985 (0.972-0.993) | 36.6 | 26.4 / 37.9 / 110.8 / 5,071.9 | 0.28% |
| AC3 | 4 | 0.276 | +0.027 | 193.3 | 127.1 | 0.979 (0.924-0.987) | 37.6 | 26.3 / 37.9 / 112.7 / 5,392.4 | 0.29% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC3: req=4001 wfe_waits=0 slow_sleeps=1068 defer_sleeps=20868 hot_iters=0 parks=498 park_timeouts=444 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=145 win0=4001 win20=0 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0

### c25: 2 calls per launch, no load (probe host, load off, spin 2000 us + 0-1000 us, 2000 launches, 2 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | VDD_CPU_GPU_CV (mW) | VDD_IN (mW) | cost per call (us) |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.011 | +0.000 | 2,526.6 | 2,531.0 | 1,699 | 6,940 | - |
| W | 4 | 1.010 | +1.000 | 2,562.0 | 2,566.2 | 1,723 | 6,983 | 18.0 |
| D | 4 | 0.041 | +0.030 | 2,617.7 | 2,612.3 | 1,735 | 6,980 | 45.3 |
| AC3 | 4 | 0.041 | +0.031 | 2,616.8 | 2,613.5 | 1,769 | 7,035 | 45.4 |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC3: req=4002 wfe_waits=0 slow_sleeps=46064 defer_sleeps=15999 hot_iters=402678 parks=300 park_timeouts=298 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=2 win0=1 win20=4001 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0

### c28: 4 calls per launch, full load (probe host, load on6, spin 2000 us + 0-1000 us, 2000 launches, 4 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | load vs P | cost per call (us) |
|---|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.014 | +0.000 | 2,583.3 | 2,550.4 | 1.000 (1.000-1.000) | - |
| W | 4 | 0.784 | +0.771 | 3,030.4 | 2,689.3 | 0.873 (0.853-0.900) | 113.1 |
| D | 4 | 0.052 | +0.038 | 2,731.9 | 2,652.4 | 0.988 (0.980-1.016) | 37.3 |
| AC3 | 4 | 0.053 | +0.039 | 2,734.4 | 2,654.1 | 0.989 (0.987-1.015) | 39.0 |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC3: req=8004 wfe_waits=0 slow_sleeps=45122 defer_sleeps=15226 hot_iters=648474 parks=604 park_timeouts=556 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=82 win0=1 win20=8003 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0

### c29: graph and plain launches alternate (probe host, load off, spin 2000 us + 0-1000 us, 2000 launches, 1 lane(s), gap 0 us, graph 2, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | VDD_CPU_GPU_CV (mW) | VDD_IN (mW) | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.011 | +0.000 | 2,527.8 | 2,531.8 | 1,687 | 6,927 | - | - | - |
| W | 4 | 0.519 | +0.508 | 2,832.0 | 3,006.3 | 1,725 | 6,963 | 293.0 | 15.5 / 876.1 / 1,054.0 / 1,070.3 | 3.64% |
| D | 4 | 0.030 | +0.019 | 2,609.1 | 2,586.1 | 1,741 | 6,988 | 68.6 | 69.5 / 108.3 / 118.3 / 611.5 | 0.00% |
| AC3 | 4 | 0.030 | +0.019 | 2,608.8 | 2,587.1 | 1,733 | 6,977 | 69.2 | 70.3 / 109.0 / 118.6 / 606.0 | 0.00% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC3: req=2002 wfe_waits=0 slow_sleeps=46398 defer_sleeps=16014 hot_iters=0 parks=300 park_timeouts=299 unseen_latch=1 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=2002 win20=0 win80=0 win300=0 unseen_hits=2 unseen_unlatch=0

### c30: graphs, then plain launches (unlatch) (probe host, load off, spin 2000 us + 0-1000 us, 2000 launches, 1 lane(s), gap 0 us, graph 3, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | VDD_CPU_GPU_CV (mW) | VDD_IN (mW) | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.010 | +0.000 | 2,526.7 | 2,530.7 | 1,712 | 6,970 | - | - | - |
| W | 4 | 0.503 | +0.493 | 2,811.3 | 3,036.3 | 1,721 | 6,970 | 273.6 | 18.6 / 834.5 / 1,014.3 / 1,410.6 | 1.97% |
| D | 4 | 0.029 | +0.019 | 2,607.3 | 2,584.6 | 1,758 | 7,023 | 70.2 | 70.1 / 109.2 / 118.1 / 3,433.2 | 0.05% |
| AC3 | 4 | 0.029 | +0.019 | 2,606.2 | 2,585.4 | 1,728 | 6,960 | 69.0 | 70.3 / 109.2 / 118.0 / 626.7 | 0.00% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC3: req=2002 wfe_waits=0 slow_sleeps=46368 defer_sleeps=16016 hot_iters=0 parks=302 park_timeouts=302 unseen_latch=1 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=2002 win20=0 win80=0 win300=0 unseen_hits=2 unseen_unlatch=1

### c31: steady request gaps under 50 us, no load (probe host, load off, spin 1 us + 0-0 us, 30000 launches, 1 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | VDD_CPU_GPU_CV (mW) | VDD_IN (mW) | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.771 | +0.000 | 27.4 | 27.1 | - | - | - | - | - |
| W | 4 | 1.678 | +0.910 | 50.4 | 50.0 | 2,177 | 7,638 | 13.9 | 13.7 / 14.6 / 15.8 / 114.4 | 0.00% |
| D | 4 | 0.612 | -0.160 | 67.5 | 72.3 | 2,163 | 7,582 | 31.8 | 33.7 / 36.9 / 38.2 / 110.3 | 0.00% |
| AC3 | 4 | 0.663 | -0.109 | 60.7 | 58.2 | 2,187 | 7,608 | 27.8 | 30.3 / 36.9 / 38.4 / 84.7 | 0.00% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC3: req=30001 wfe_waits=0 slow_sleeps=10 defer_sleeps=69174 hot_iters=1046080 parks=298 park_timeouts=298 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=0 win0=16308 win20=13692 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0

### c32: steady request gaps under 50 us, full load (probe host, load on6, spin 1 us + 0-0 us, 30000 launches, 1 lane(s), gap 0 us, graph 0, 1 process(es))

| set | runs | victim CPU s per wall s | minus P | per-launch mean (us) | per-launch median (us) | load vs P | host call mean (us) | median / p90 / p99 / max (us) | > 1 ms |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| P | 4 | 0.530 | +0.000 | 44.5 | 27.9 | 1.000 (1.000-1.000) | - | - | - |
| W | 4 | 0.815 | +0.285 | 133.8 | 62.5 | 0.989 (0.951-1.060) | 60.0 | 13.3 / 14.3 / 3,590.6 / 8,025.3 | 1.20% |
| D | 4 | 0.393 | -0.139 | 125.3 | 74.3 | 1.039 (0.974-1.120) | 34.0 | 27.8 / 33.6 / 66.0 / 5,259.3 | 0.14% |
| AC3 | 4 | 0.403 | -0.138 | 123.2 | 74.2 | 1.035 (0.971-1.113) | 35.1 | 26.9 / 33.4 / 71.7 / 5,601.1 | 0.17% |

Watcher counters (sum over the run's processes, median over runs): req, wfe_waits, slow_sleeps, defer_sleeps, hot_iters, parks, park_timeouts, unseen_latch, gate_opens, latch_trips, cap_forced, guard_skips, win0, win20, win80, win300, unseen_hits, unseen_unlatch
- AC3: req=30001 wfe_waits=0 slow_sleeps=6024 defer_sleeps=86894 hot_iters=28554 parks=1250 park_timeouts=926 unseen_latch=0 gate_opens=0 latch_trips=0 cap_forced=0 guard_skips=1406 win0=29632 win20=369 win80=0 win300=0 unseen_hits=0 unseen_unlatch=0
