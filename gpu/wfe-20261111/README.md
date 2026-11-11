# Can a GPU write wake an aarch64 core in WFE? (2026-11-11)

For direction 2 layer 1, the watcher's wake-up (`../hostwake-20261111`: a host call waits up to
about 1 ms when the watcher sleeps). Standalone test, no bpftime: `wfe_test.cu` (nvcc 13.2,
sm_87; `nvcc -O2 -arch=sm_87 -std=c++17 wfe_test.cu -o wfe_test -lcuda`), Orin Nano, jetson_clocks.

A one-thread kernel acts as a host-call client: each round it waits DELAY us (globaltimer),
writes `flag` = round with a system fence, and times (globaltimer) until the CPU writes `ack`.
The memory is a malloc'd page registered with `cuMemHostRegister(DEVICEMAP | PORTABLE)`, as
bpftime registers its segment. The CPU thread waits for `flag` with:
- **spin**: load-acquire in a loop;
- **sleep**: bpftime's watcher policy (200 us polling after an answer, then 50 us sleeps
  doubling to 1 ms);
- **wfe**: Linux arm64's `__cmpwait` (`sevl; wfe; ldxr; eor; cbnz; wfe`): an exclusive load
  marks the line, WFE waits until it is written or another event (interrupt, SEV, the arch
  timer's event stream: `evtstrm` is on); the wake-ups are counted.

## Results (`logs/sweep1.txt`, 200 rounds per cell; `logs/power-*`)

GPU-timed round trip p50 (us) / CPU time of the waiting thread (% of one core):

| delay (us) | spin | sleep | wfe | wfe wake-ups per round |
|---:|---|---|---|---:|
| 20 | 1.86 / 99.5 | 1.86 / 89.8 | 1.79 / 99.5 | 1.21 |
| 100 | 1.86 / 99.8 | 1.79 / 95.2 | 1.79 / 98.8 | 1.81 |
| 500 | 1.89 / 99.8 | 209.06 / 28.6 | 1.98 / 99.7 | 4.99 |
| 2000 | 2.91 / 99.9 | 15.17 / 9.9 (max 1,054) | 2.56 / 99.8 | 16.82 |
| 10000 | 2.85 / 99.9 | 441.70 / 2.1 | 1.86 / 99.9 | 79.89 |

- **A GPU write wakes the core**: with WFE the round trip stays at 1.8-2.6 us at every delay;
  waking only on the event stream would put it near 50 us on average. The other wake-ups (about
  one per 125 us) are the event stream; the loop re-checks the flag after each.
- **Power** (tegrastats, 10 s at 10 ms delay, the GPU kernel spinning in all runs; medians):
  VDD_CPU_GPU_CV idle 1,444 mW (GPU idle), spin 2,283, sleep 1,685, wfe 1,685 mW; VDD_IN 6,631 /
  7,544 / 6,913 / 6,913 mW. WFE costs no more power than sleeping and about 600 mW less than
  spinning.
- **CPU time**: a core in WFE still counts as running (99.9%): the thread never blocks, so for the
  scheduler it holds a core like a spinning thread, while drawing about the power of an idle one.

Not tested: several waiters, other Tegra parts, a GPU write from a different SM than the one
that read, kernels that keep the CPU's core busy with other threads.
