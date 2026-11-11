# Host-call latency of a sparse caller: the watcher's wake-up (2026-11-11)

Direction 2, layer 1, item "watcher wake-up": measured before any change. Dev aa6eb973
(`bpftime-gpu-work/l1/build-cuda`), Orin Nano, jetson_clocks.

The agent's watcher (`runtime/src/attach/bpf_attach_ctx_cuda.cpp:184-257`) polls the host-call
slot for 200 us after a request, then sleeps 50 us, doubling to 1 ms, while idle. A GPU thread
whose request arrives during a sleep waits for it to end.

## Method

- **Probe** (`probe/hostwake.bpf.c`, loader `probe/hostwake.c`): at vectorAdd entry, globaltimer,
  then `bpf_map_update_elem` on an ordinary hash map (a host call in every build), then
  globaltimer again; the difference (ns) goes into a GPU array (1503, created as 1513), one slot
  per call. The loader prints all of them at SIGINT. Timed on the GPU, so the launch path and the
  GPU's own wake-up from idle are not in it.
- **Victim** (`victim/vec_add.cu`): upstream's vec_add with a 5th argument, microseconds of
  `usleep` before every timed launch (outside the launch's timing); its "Average kernel time" is
  the mean of the launches (upstream's includes the whole loop). Run as `vec_add_gap 1 200 1 1
  GAP`: one thread, so one host call per launch, 200 launches + 1 warm-up (dropped).
- **Runs** (`scripts/run_wake.sh`, `scripts/sweep.sh`, 14:36-14:38 JST): 15 gaps from 0 to 20 ms,
  3 rounds, each round followed by the same gaps without bpftime. Checks before each round: time
  window, clocks, leftovers, no build, no segment. 600 calls per gap.

## Results (`summary.md`)

| gap (us) | host call p50 (us) | p90 | launch p50 with probe | launch p50 plain |
|---:|---:|---:|---:|---:|
| 0 | 15.0 | 15.3 | 49.9 | 28.7 |
| 50 | 14.3 | 14.9 | 50.6 | 29.2 |
| 100 | 112.8 | 115.6 | 149.4 | 30.2 |
| 200 | 162.8 | 167.0 | 202.1 | 30.3 |
| 400 | 215.2 | 218.8 | 254.7 | 30.6 |
| 750 | 317.4 | 321.6 | 357.7 | 30.2 |
| 1000 | 66.5 | 71.3 | 107.3 | 31.0 |
| 2000 | 566.5 | 593.9 | 1,015.1 | 431.5 |
| 5000 | 714.8 | 746.1 | 1,174.1 | 32.2 |
| 10000 | 951.0 | 989.4 | 1,437.7 | 445.7 |
| 20000 | 444.4 | 477.8 | 917.6 | 461.4 |

- A call that finds the watcher polling takes 14-15 us. Once the gap between calls outlasts the
  200 us window, a call waits for the watcher's current sleep: 60-950 us at the median depending
  on how the gap falls against the 50/100/200/400/800/1000 us sleeps (not monotonic in the gap),
  up to about 1 ms. Each value is tight within its gap (p90 within 5% of p50): the phase is
  deterministic.
- `usleep(100)` already outlasts the window (usleep's own slack plus the 50 us launch).
- Separate effect, without bpftime: after some idle gaps (2, 10, 20 ms; 500 us in an earlier
  check) a plain launch takes 430-460 us instead of 30 (the GPU waking from idle; elpg/aelpg are
  enabled, railgating is off). It is in the launch time, not in the host-call time above.

## Fine sweep 2.0-3.1 ms (one round, 100 launches per gap, `logs/fine/`)

Once the sleeps have reached 1 ms, the wait depends only on where the call falls in the current
1 ms sleep: each 100 us more of gap is about 100 us less of wait, until the call falls into the
next sleep and the wait jumps back up (p50, us): 2000: 970, 2100: 872, 2200: 772, 2300: 672,
2400: 570, 2500: 470, 2600: 376, 2700: 908, 2800: 485 (spread 46-850), 2900: 706, 3000: 610,
3100: 525. The 2000 us value differs from the 3-round sweep (566): the exact phase moves between
runs; the range does not (0 to about 1 ms).

## Files

`summary.md`; `logs/r<round>/gap<GAP>.{loader,victim}.{out,err}`, `logs/plain<round>/`,
`logs/sweep.log`; `probe/`, `victim/`, `scripts/`.
