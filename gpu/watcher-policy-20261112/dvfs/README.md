# Clock regimes: the watcher policies with and without jetson_clocks (2026-11-12)

Every earlier round of this study ran under `jetson_clocks` (CPU, GPU and EMC at their highest
frequency, CPU idle states disabled). The question here: do the comparisons that chose AC3 (`../v3/`,
`../dev/`) hold when the board scales its clocks and uses its idle states? Eight of the 17 cells,
all seven sets, both regimes in every round. Checked afterwards by an independent review (three
lenses: numbers, wording, method; the three weightiest findings of each checked by a refuter, the
rest against the data by me; `review/`), whose confirmed findings are worked in below.

## Regimes (`scripts/clocks.sh`)

| regime | CPU | CPU idle states | GPU | EMC |
|---|---|---|---|---|
| on | `jetson_clocks`: 1728 MHz fixed | disabled (cores idle in WFI only) | governor `performance`, 1020 MHz | 3199 MHz (floor = maximum) |
| off | schedutil, 729.6-1728 MHz (floor from `/etc/systemd/nvpower.sh`) | enabled (WFI, c7) | `nvhost_podgov` with nvpower.sh's parameters, 306-1020 MHz | floor 204 MHz requested; rate set by the BPMP, not recorded during runs |

The off values are as written by `clocks.sh off`: taken from the boot scripts where they set them,
otherwise the lowest available frequency. They are not proven equal to a freshly booted board (that
needs a reboot). EMC: the bpmp-bwmgr devfreq governor only books time to the floor written to
`min_freq` (its trans_stat never changed state during a run), while the clock itself is chosen by
the BPMP: `bwmgr` `cur_freq` read 2133 MHz at every switch to off (`clocks-r*-off.txt`), and nothing
recorded it during the runs. GPU rail gating stays off in both (this board refuses it: "Railgating
is not supported").

That the regime held: before each run `run_one.sh` checked it (on: cpu5, cpu0 and the GPU at
1728 / 1728 / 1020 MHz, before and after the victims; off: `clocks.sh check-off`, which covers
the CPU floor and ceiling, every idle state's switch, the GPU governor, floor and ceiling, the EMC
floor and rail gating, but not podgov's tunables, whose writes are not checked). `verify.csv`'s
`clocks_ok` for off only records that label. The measurements show it: in the timed window of every
off run of c02, c06, c11 and c25 tegrastats reads the CPU at 729-771 MHz (W's cluster excepted, below),
and the cpuidle counters show c7 in use (`.resid`). After the last round the board went back to
`jetson_clocks`, and its snapshot (`clocks-after.txt`) equals the one taken before
(`clocks-before.txt`) apart from current frequencies and the fan.

### What the clocks were in each cell

Two sources. tegrastats (100 ms samples, run in the no-load cells only) gives each core's frequency
inside the timed window (`scripts/inwin.py`, `inwin.txt`); ranges over the four rounds, off regime
(under `on` every sample is 1728 MHz):

| cell | CPU cluster | P | AC3, D, AC2, AC4 | M100 | W |
|---|---|---|---|---|---|
| c02 no load | policy0 (cpu0-3, MHz) | 729-736 | 729-769 | 729-759 | 1581-1705 |
| | policy4 (cpu4-5, MHz) | 729-731 | 729-734 | 729-746 | 759-878 |
| c06 32 calls, no load | policy0 | 729-771 | 729-765 | 729-759 | 1659-1728 |
| c25 2 calls per launch | policy0 | 729-743 | 729-737 | 729 | 1709-1728 |
| c11 idle | policy0 | 729-735 | 729-762 | 731-748 | 729-742 |

So with a core free, P and the sleeping sets run at the CPU floor, and W's waiting thread, which
the scheduler counts as running, keeps its cluster near 1728 MHz while probed kernels run.

`.resid` (`scripts/resid.py`: cpufreq time_in_state, cpuidle usage and time, GPU devfreq trans_stat)
covers the victims' whole life, agent and CUDA start-up included (the window exceeds the timed loop
by 0.2-0.5 s for P, 0.7-1.2 s for the bpftime sets), so its means mix start-up with the timed loop: c02 AC3 788 MHz there is start-up, the timed
loop sits at the floor as P. What it does show:
- Full load (c01, c05): every core at 1728 MHz all the time in both regimes, and no core idles (c7
  share at most 0.00015 of CPU time, WFI at most 0.001). These cells differ between regimes only in
  the GPU (nvhost_podgov, run means 938-1002 MHz, against 1020 fixed) and the EMC setting: they test
  neither CPU frequency scaling nor idle states.
- No load: c7 share of CPU time 0.17-0.38 for P (c02 0.33, c11 0.31); GPU 306 MHz idle (c11),
  950-1010 MHz in c02, c06 and c25.
- c17 (load 5 ms on / 5 ms off): no c7 (share at most 0.0004; idle time in WFI); P's run had its
  clusters at 1505 / 768 MHz, W's at 1703 / 1212, AC3's at 1449 / 1135 (medians).

## Cells, sets, runs

Cells (`scripts/rounds7.sh`, the same definitions as `../dev/`): c01 full load (6 load threads),
c02 no load, c05 32 calls per launch under full load, c06 the same without load, c11 idle agent
(one launch, then 8 s), c17 load 5 ms on / 5 ms off, c23 request gaps of about 55-115 us, c25 2
calls per launch. Sets: P (no bpftime), W (dev 46f5a7cb's build, WFE while a noted launch may
run), D (dev 52126f33's build, AC3), and from the experimental build (`../v3/`): AC3, AC2 (window
sized 2 x gap, closed by one long gap, as KVM 6.8 resets it), AC4 (window halved by each long gap,
as KVM does now), M100 (sleeps of 100 us at most, 200 us hot window after a request). 4 rounds
(21:13-22:22 JST), each running all cells in both regimes (odd rounds off first, even rounds on
first), cells in a fixed order, sets shuffled per round, regime and cell (seeded); 448 runs, all
valid (`on/verify.csv`, `off/verify.csv`); highest temperature 53.4 C.

Measures as in `../dev/`: CPU = victim CPU seconds per wall-clock second minus P's of the same round
(1.0 = one core); cost per call = (per-launch mean - P's) / calls per launch; power = VDD_IN from
tegrastats (100 ms samples, whole seconds inside the window; c23's runs are too short for any).
Per-round values in `perround.txt` (`scripts/perround.py`); the tables give ranges over the four
rounds.

Two cautions for the off regime:
- **CPU seconds depend on the clock each set ran at**: across regimes, and without `jetson_clocks`
  between sets too (W at about 1700 MHz, the others at the floor). Measured off/on ratios of the
  CPU figures run from 1.2 (M100 c02) to 4.5 (AC4 c23). Compare CPU within a regime, and in off
  only between sets at the same clock (P, the AC sets and M100 in c02, c06, c25).
- **P is not a zero for W without `jetson_clocks`**: P's victim runs at the CPU floor, W's on the
  cluster W keeps fast. W's "cost over P" in the no-load cells is biased low (by about 10 us per
  call in c25); W's own latency is given instead.

## Results

### 1. With a core free (frequency scaling and idle states act): AC3 keeps its place

| cell | measure | regime | AC3 | D | AC2 | AC4 | M100 | W |
|---|---|---|---|---|---|---|---|---|
| c25 2 calls per launch | cost per call (us) | on | 44.3-46.5 | 43.8-45.0 | 51.2-52.4 | 52.3-53.6 | 42.1-43.8 | 17.2-19.0 |
| | | off | 47.5-51.6 | 45.5-49.0 | 59.0-72.1 | 59.1-61.5 | 49.3-58.2 | (biased) |
| c06 32 calls, no load | cost per call (us) | on | 11.7-12.0 | 11.7-11.9 | 12.1-12.4 | 12.3-12.4 | 10.9-11.3 | 9.1-9.3 |
| | | off | 13.6-14.5 | 13.8-14.6 | 14.6-15.2 | 14.8-15.5 | 14.1-14.6 | (biased) |
| c02 no load | host call median (us) | on | 69.1-70.4 | 69.0-69.8 | 69.1-69.7 | 68.9-70.0 | 63.1-64.6 | 13.5-13.9 |
| | | off | 71.5-73.7 | 70.5-72.4 | 71.4-72.8 | 72.5-73.6 | 71.1-72.7 | 15.4-15.7 |
| c02 | CPU (cores) | on | 0.019-0.022 | 0.020 | 0.020-0.023 | 0.019-0.024 | 0.091-0.095 | 1.001-1.005 |
| | | off | 0.047-0.056 | 0.043-0.054 | 0.045-0.056 | 0.050-0.060 | 0.110-0.117 | 0.988-1.000 |
| c23 gaps 55-115 us | CPU (cores) | on | 0.050-0.070 | 0.067-0.099 | 0.075-0.104 | 0.042-0.113 | 1.001-1.042 | 1.011-1.044 |
| | | off | 0.083-0.226 | 0.156-0.223 | 0.138-0.241 | 0.205-0.298 | 0.871-1.091 | 0.952-1.097 |

- At 2 calls per launch AC3 stays below AC2 and AC4 in every round in both regimes, by 5.0-9.1 us
  (on) and 7.4-24.6 us (off) paired by round (medians 6.7-8.3 -> 10.6-11.1 us; the 24.6 is AC2's
  round 2, 72.1 us). With 32 calls and no load it stays 0.4-1 us below them (ranges just apart in
  both regimes).
- M100 loses its place: with `jetson_clocks` it is the fastest sleeping set at 2 calls per launch
  and in c06 and answers a single call about 6 us sooner (c02 63.1-64.6 us); without, its ranges
  overlap AC3's in all three (c02 71.1-72.7, c06 14.1-14.6, c25 49.3-58.2).
- CPU with a free core: W holds about one core in c02 and c23 in both regimes; M100 holds one only
  at c23's gaps (its hot window covers them; 0.09-0.12 in c02); the AC sets 0.02 (on) or about 0.05
  (off) in c02.
- c23: no ranking among AC3, AC2 and AC4. None of them opens its window there in any run (0 hot
  iterations, window 0 for all 4,001 requests, both regimes). With `jetson_clocks` AC3's range lies
  just below AC2's (0.050-0.070 vs 0.075-0.104) and AC4's spans both, but D, the same policy as AC3,
  is also above AC3 in every round, and `../v3/` measured the opposite order (AC2 0.066, AC3 0.104)
  with a noise floor of about 0.05 core between identical behaviour. Without, all three overlap.
- W with a core free: its own latency got a little worse without `jetson_clocks` in every no-load
  cell (c02 median 13.5-13.9 -> 15.4-15.7 us, c23 13.7-14.3 -> 15.9-16.1, c25 per launch (two calls)
  22.4-23.6 -> 26.2-26.5; c11, one call per run after a launch: 14.6-15.0 -> 40.2-43.3); its lead
  over AC3 for a single call stays about 56 us. Its cost over P in c25 fell (17.2-19.0 -> 8.3-13.5
  us per call) only because P slowed more: P's per-launch mean rose 28 us (2,526 -> 2,554), W's 15
  (2,561 -> 2,576), AC3's 37 (2,616 -> 2,653). Possibly the victim's own host work ran on
  the fast cluster W kept up (the victim is not pinned; not separated from c7 exits).

### 2. Under full load (c01, c05): unchanged, but only the GPU and EMC settings differ there

| cell | measure | regime | AC3 | D | AC2 | AC4 | M100 | W |
|---|---|---|---|---|---|---|---|---|
| c05 32 calls, full load | cost per call (us) | on | 15.3-16.1 | 15.3-15.5 | 21.4-22.3 | 21.0-21.9 | 16.7-17.4 | 21.1-22.9 |
| | | off | 17.7-18.8 | 17.6-17.9 | 24.8-25.7 | 24.0-25.3 | 20.2-20.7 | 20.9-23.2 |
| c01 full load | load vs P | on | 0.987-0.990 | 0.988-0.990 | 0.988-0.989 | 0.987-0.989 | 0.978-0.981 | 0.854-0.865 |
| | | off | 0.985-0.990 | 0.987-0.990 | 0.987-0.989 | 0.987-0.990 | 0.976-0.980 | 0.850-0.870 |
| c01 | host calls > 1 ms | on | 0.91-1.16% | 1.06-1.38% | 0.88-1.12% | 0.81-1.09% | 3.53-4.00% | 7.78-11.66% |
| | | off | 0.84-1.12% | 0.91-1.00% | 0.84-1.31% | 0.81-1.06% | 3.38-3.53% | 5.75-13.44% |

AC3 (and D) stay 5.5-7 us per call below AC2 and AC4 in 32-call bursts under full load (medians;
D 5.7-7.4), and the sleeping sets keep 98.5-99% of the load's throughput with about 1% of host
calls over 1 ms, W 85-87% and 6-13%. All cores are at 1728 MHz and never idle in both regimes
here (above), so this shows the comparison does not depend on the GPU governor or the EMC setting;
it says nothing about CPU frequency scaling or idle states. The sleeping sets' bursts cost 2.4-3.9 us
more per call without `jetson_clocks`, W's 0.5 (GPU at 938-1002 instead of 1020 MHz, EMC unknown).

### 3. Power: under jetson_clocks W draws a little less; without, no difference is resolved

VDD_IN (mW), per round:

| cell | regime | P | W | AC3 | D | M100 |
|---|---|---|---|---|---|---|
| c02 no load | on | 6914-6927 | 6920-6944 | 6955-6973 | 6961-6970 | 6973-6992 |
| | off | 5698-5760 | 5776-5826 | 5786-5807 | 5793-5816 | 5798-5827 |
| c06 32 calls, no load | on | 6931-6943 | 6959-6991 | 7009-7039 | 7000-7037 | 7024-7047 |
| | off | 5748-5769 | 5825-5893 | 5838-5872 | 5831-5882 | 5808-5862 |
| c25 2 calls per launch | on | 6897-6925 | 6924-6957 | 6956-6984 | 6960-6977 | 6963-7013 |
| | off | 5680-5730 | 5763-5804 | 5774-5795 | 5775-5792 | 5793-5827 |
| c11 idle | on | 6603-6650 | 6608-6636 | 6617-6627 | 6630-6651 | 6653-6674 |
| | off | 4600-4688 | 4622-4693 | 4632-4661 | 4611-4680 | 4629-4710 |

tegrastats reads VDD_IN in steps of about 40 mW (6913, 6953, 6993, ...), and the junction
temperature drifts 0.5-1.4 C across a cell's seven runs (cells run in a fixed order after each
regime switch; sets shuffled); every difference below is within a few steps.
- With `jetson_clocks`, W is below every sleeping set (D, AC3, M100, AC2, AC4) in every round of
  c02, c06 and c25, paired by round, by 1-90 mW (W - AC3: c02 -35 to -20, c06 -79 to -18, c25 -37 to
  -1, the -1 being equal within a step). A likely reason, not measured: W's core waits in WFE, the
  sleeping sets wake every 100 us.
- Without, no difference between W and the sleeping sets is resolved (W - AC3: c02 -11 to +22, c06
  -29 to +21, c25 -25 to +20): c02 argues against one of the on regime's size, c06 and c25 are too
  noisy to rule it out.
- Over P with a kernel running (c02, c06, c25): 6-56 mW for W, 34-108 mW for AC3 under on; about
  35-125 mW for both without (at most about 2% of VDD_IN). Idle (c11): only M100 stands out, above P in every round (+3 to
  +59 mW under on, as `../README.md` found); the others are within the noise.
- So against AC3, W's cost is the core it holds (and under full load 13-15% of the load's
  throughput), not power, as far as these cells measure (power was not measured under load).
- The on regime draws 1.2 W (c02 P: 6.91 vs 5.73 W) and 2.0 W (c11 P: 6.61 vs 4.63 W) more than this
  off regime. Most of it is VDD_SOC (memory controller and EMC: 2407 vs 1469 and 2411 vs 1458 mW),
  which depends on the EMC setting (unrecorded in off); VDD_CPU_GPU_CV 1685 vs 1551 mW with a kernel
  running, 1447 vs 528 idle (GPU 1020 vs 306 MHz, idle states).

### 4. c17 (load 5 ms on, 5 ms off): without jetson_clocks "load vs P" does not isolate the watcher's cost

Load vs P without `jetson_clocks` is above 1 in every round of every set but one D round: W
1.090-1.114, D 0.987-1.093, AC3 1.001-1.054, M100 1.023-1.043 (with: W 0.880-0.889, AC3
0.970-0.975). The clusters ran at other frequencies than in P's run (P 1505 / 768 MHz; W 1703 /
1212; AC3 1449 / 1135, its policy0 below P's in every round, policy4 above; D, M100, AC2, AC4 both
above; `.resid`, start-up included: at most 1.2 s of about 13 s, up to about 90 MHz of these means). Why the sleeping sets raise policy4 (and, but for AC3, policy0) is not
established. In this regime the cell's load ratio is confounded with frequency, so it neither shows
nor rules out a cost; one policy's rounds spread up to 0.11 (D). The full-load cells (c01, c05) keep
their load ratio in both regimes.

### 5. The rest of AC3's behaviour without jetson_clocks

- Latency (host-call medians): c02 1.7-2.9 us higher for AC3, D, AC2 and W, 4.0 for AC4, 8.5 for M100;
  c23 6-7 us for the AC sets (AC3 26.2-26.7 -> 32.4-33.2), 2 for W and M100.
- Calls over 1 ms: no sleeping set's share moved beyond the round spread except D in c01, lower
  (1.06-1.38% -> 0.91-1.00%); c23 AC3 had one call of 3.8 ms in one round. W in c17 rose by median
  (0.11% -> 0.40%; ranges 0.06-0.16 and 0.16-0.53 touch).
- The window was larger at detection without `jetson_clocks`, consistent with longer gaps within
  bursts at lower clocks (gaps were not recorded): in c06 10-20 us for 45,056 and 40-50 us for 31,775
  of 76,832 requests with, 40-50 us for 76,828 without; in c25 10-20 us for 4,001 of 4,002 with,
  40-50 us for 3,994 without (counter buckets `win20` = 1-20 us, `win80` = 21-80 us; the window takes
  10, 20, 40, 50; medians over runs).
- The late guard: without `jetson_clocks` it tripped rarely in the no-load cells (skips in 12 of 16
  runs of c02, c06, c23, c25, 0-25 per run; with, 2 of 16 runs, 1 each), while the end-of-run lateness
  average stayed at 3.3-4.6 us (`late_ewma_us`; -0.4 to -0.1 with), far below its 30 us threshold.
  Under full load it trips in both regimes, more often without in c05 (371-438 with, 444-490
  without); c01 within the round spread (54-59, 57-79).
- c17's weakness stays: 28.4-33.0% of host calls over 1 ms for the AC sets without `jetson_clocks`
  (29.9-32.7% with), W 0.16-0.53%.
- D matches AC3 within the round spread in the tables above, as in `../dev/`, but for three small
  differences under `jetson_clocks`: c23 median 25.0-25.7 vs 26.2-26.7 us (`../dev/`: 25.4 vs
  26.1), c05 load 0.974 vs 0.971-0.973, c11 power 6630-6651 vs 6617-6627 mW (within a step).

## What this does not cover

- One board (6 Cortex-A78AE cores in two clusters, integrated GPU sharing memory with the CPU); x86,
  a discrete GPU behind PCIe (other request round trips, so other gap distributions for the window
  rules), other kernels and timer settings are untested.
- Eight of the 17 cells: graphs (c18, c29), dense requests (c31, c32), c21, c24 and c26-c28 were not
  rerun. Under full load the CPU neither scaled nor idled, so the burst-under-load comparison was
  tested against the GPU and EMC settings only.
- The off regime as written, not a verified boot state; the EMC rate during the runs is unknown.
- 4 rounds; tegrastats' 40 mW step; `.resid` includes start-up.

## Files

- `on/summary.md`, `off/summary.md`, `on/verify.csv`, `off/verify.csv`: `summarize.py` per regime
  (`SETS="P W D AC3 M100 AC2 AC4" ROUNDS_FILE=rounds7.sh [CLOCKS=off] summarize.py LOG_DIR`).
- `perround.txt`: per-round values and `.resid` medians (`scripts/perround.py`, run in the policy
  work directory; `emc_floor` is the requested floor, not the rate).
- `inwin.txt`: CPU frequency inside the timed window from tegrastats (`scripts/inwin.py`).
- `review/`: the review's findings and verdicts (`findings.txt`), the workflow script.
- `scripts/`: `clocks.sh` (regimes, checks, snapshots), `rounds7.sh` (the batch), `resid.py`,
  `perround.py`, `inwin.py`, `run_one.sh` and `summarize.py` with their changes for this round
  (`*.diff`: the clock regime check and `clocks=` line, the residency snapshots; `summarize.py`'s
  clock check per regime).
- `logs.tar.xz`: `dvfs/` (every run, clock snapshots per round and regime, `rounds.log`,
  `clocks-before.txt`, `clocks-after.txt`) and `dvfs-smoke/` (5 smoke runs, the off and back-on
  snapshots).
