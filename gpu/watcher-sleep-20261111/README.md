# Short sleeps instead of WFE for the host-call watcher (2026-11-11)

Follows `gpu/wfe-load-20261111`: the WFE watcher (dev 46f5a7cb) answers in 14 us but holds a core
while probed kernels run, costs a fully loaded CPU 13-14% and has a 3-4 ms tail there. Question:
can a sleeping watcher get close to WFE's latency at a small cost? Design: `scripts/DESIGN.md`.

## Setup

- **Calibration** (`calib/calib.c`, `calib/calib.log`, no bpftime): one thread sleeping 10-1,000
  us with `clock_nanosleep`, timer slack 1 us or the default 50 us, load off and on.
- **Experimental build** (not a dev commit): dev 46f5a7cb's tree (`scripts/src.BASE`) with
  `scripts/watcher-sleep.patch`, built with `scripts/build.sh` (wake/build.sh's flags). Env knobs:
  `BPFTIME_CUDA_WATCHER_POLL_US=n` (S<n>: sleep n us instead of WFE while a noted probed launch
  runs; hot for 200 us after a request), `BPFTIME_CUDA_WATCHER_MAX_SLEEP_US=n` (M<n>: the old
  policy with the 1 ms cap lowered to n us, launch notes ignored); both set the watcher thread's
  timer slack to 1 us.
- **Victim** `scripts/victim/spin2.cu`: a 1 x T kernel spinning 2,000 or 20,000 us plus 0-999 us
  per launch (fixed sequence, so calls land at a random phase of any sleep schedule; the earlier
  rounds were phase-locked), back to back. Probes from `gpu/wfe-load-20261111` (kretprobe,
  `hostspin` one host call per lane at exit, `nohost` none). Load: 6 threads on 6 cores.
- **Sets**: P no bpftime, N dev aa6eb973, W dev 46f5a7cb (WFE), S10 / S20 / S50 / S100, M100.
- **Cells**: one lane with host call x load on/off x 2 / 20 ms; no host call x load on x 2 / 20 ms;
  32 lanes with host call x load on/off x 2 ms. 4 rounds (`scripts/rounds.sh`, 22:00-22:43 JST),
  set order rotating. Load-off cells sample tegrastats every 100 ms (whole seconds inside the
  victim's window). All 256 runs valid (`verify.csv`): rcs 0, 3,201 / 401 records, cpu5, cpu0 and
  GPU clocks unchanged; highest temperature 54.4 C.
- **Power regime**: jetson_clocks active (as for all our measurements): cpuidle WFI and c7
  disabled, every core at 1,728 MHz. The power numbers below hold only for that state.

## Calibration (`calib/calib.log`)

With timer slack 1 us a sleep of n us lasts n + 4 us (10 -> 13.7, 50 -> 53.8, 100 -> 103.9), at
1.1-1.4 us of CPU per wake-up (10 us: 7.7% of a core, 50 us: 2.1%, 100 us: 1.2%); with the default
slack every sleep is about 50 us longer (10 -> 62.6, 1,000 -> 1,055: the 1.055 ms period of the
earlier phase sweep). Under load the per-wake-up p99 stays within 5 us of the period, but single
stalls of 4-6 ms occur (count-weighted p99 hides them; host calls land in long intervals more
often).

## Results (`summary.md`; host-call latency GPU-timed, pooled, warm-up dropped)

One lane, no load (2 ms / 20 ms cells):

| set | host call mean (us) | p99 (us) | victim CPU s per wall s | VDD_CPU_GPU_CV (mW), P 1,686 / 1,687 |
|---|---:|---:|---:|---:|
| N | 531 / 540 | 1,023 / 1,059 | 0.079 / 0.014 | 1,731 / 1,692 |
| W | 14.1 / 14.1 | 15.7 / 15.4 | 1.016 / 1.000 | 1,704 / 1,688 |
| S10 | 21.5 / 22.0 | 28.7 / 29.7 | 0.178 / 0.099 | 1,907 / 1,888 |
| S20 | 27.6 / 26.9 | 39.0 / 39.6 | 0.146 / 0.065 | 1,847 / 1,812 |
| S50 | 41.8 / 42.0 | 68.5 / 69.2 | 0.125 / 0.041 | 1,772 / 1,750 |
| S100 | 67.6 / 66.1 | 118.4 / 118.7 | 0.109 / 0.030 | 1,765 / 1,731 |
| M100 | 64.8 / 67.3 | 117.9 / 118.3 | 0.102 / 0.023 | 1,747 / 1,720 |

One lane, full load:

| set | host call mean (us) | calls > 1 ms | load vs P | load vs P, probe without host call |
|---|---:|---:|---:|---:|
| N | 589 / 544 | 6.6% / 7.3% | 0.988 / 0.997 | 0.996 / 0.999 |
| W | 376 / 1,023 | 12.6% / 30.9% | 0.863 / 0.867 | 0.859 / 0.858 |
| S10 | 232 / 163 | 8.5% / 5.8% | 0.939 / 0.945 | 0.942 / 0.948 |
| S20 | 184 / 138 | 6.2% / 4.3% | 0.956 / 0.964 | 0.961 / 0.967 |
| S50 | 165 / 149 | 5.1% / 4.2% | 0.969 / 0.978 | 0.975 / 0.979 |
| S100 | 172 / 118 | 4.4% / 2.0% | 0.976 / 0.985 | 0.980 / 0.988 |
| M100 | 144 / 90 | 3.5% / 0.9% | 0.980 / 0.990 | 0.990 / 0.992 |

32 lanes calling at exit, 2 ms, extra per-launch time / 32 (us): no load W 9.2, S 9.4-10.9, M100
10.8, N 25.5; load W 20.9, S 17.9-19.2, M100 17.3, N 30.1. The differences are the first call's
wake-up spread over 32 calls: after it every set serves a call in 9.0-9.5 us without load.

## Findings

1. **With idle cores W is best**: 14 us, and with jetson_clocks no measurable extra power at 20
   ms (+1 mW) and about +20 mW (+1 to +37, not significant) at 2 ms. Short sleeps cost power in
   that regime: S10 +200-220 mW, S50 +63-86, M100 +33-61 (part of it the 200 us poll after each
   request). With default cpuidle (c7) and DVFS both W and the sleepers would cost more than this
   and the comparison may change: not measured.
2. **With a full CPU W is worst**: the load loses 14% (also with probes that never call the host),
   the probed application itself about 14% at 2 ms, and 13-31% of calls wait over 1 ms. Every S
   set and M100 does better on all three; M100 best (mean 144 / 90 us, 3.5% / 0.9% > 1 ms, load
   -2% / -1%; with no host calls -1%).
3. **Sleepers are not immune to load** (p99 0.8-3.6 ms), as the calibration's 4-6 ms stalls
   predict; the share of slow calls falls with the watcher's CPU use (S10 > S100 > M100).
4. **S<n> costs more CPU than M100 at the same period** (with no host calls 0.073 vs 0.030 s/s at
   2 ms): S-only work is the 200 us hot poll after the probed kernels end and the event checks
   every 100 us while a launch is noted (no ablation run). M100 needs no launch tracking at all.
5. **M100 pays when nothing runs**: it wakes about 10k times a second for the life of the
   process, about 1.2% of a core (calibration); W and S pay nothing then. Not measured with an
   idle agent, nor its power with c7 enabled.

## Limits

One GPU duty cycle (probed kernels run about 100% of the time: W's cost is at its ceiling), two
load points (0 and 6 threads on 6 cores; partial load, where WFE-unless-contended would matter,
not run), one probed process (W holds a core per process running probed kernels), power only with
jetson_clocks, 4 rounds.

## Review

A workflow of two reviewers and a critic recomputed every number from the raw logs without the
summary script (`review/claims.md`, `review/workflow-result.json`). All numbers reproduced.
Corrections taken in: W's 2 ms power is about +20 mW, not "none"; the burst differences are
first-call latency; the calibration's count-weighted p99 does not contradict the tail; the 200 us
post-request poll is paid by M100 too, so it is not why S costs more; the power regime
(jetson_clocks, cpuidle off) limits every power claim; N's mean under load at 2 ms is worse than
W's.

## Files

`summary.md`, `verify.csv`, `calib/`, `logs.tar.xz` (rounds, smoke, build log), `scripts/`
(runner, rounds, summary, build, design, patch, victim), `review/`.
