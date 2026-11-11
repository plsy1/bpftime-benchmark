# The WFE watcher under full CPU load, and CUDA's wait flags (2026-11-11)

Direction 2, layer 1, item 3 of `RESEARCH_DIRECTIONS.md` (main). Dev 46f5a7cb makes the host-call
watcher wait with aarch64 WFE while a probed kernel may run (`gpu/hostwake-fix-20261111`). The
open questions: what that costs when all CPU cores are busy, what it costs when the probe makes no
host call at all, and whether bpftime could follow the application's CUDA wait setting instead.

## Setup

- **Victim** `scripts/victim/spin.cu`: a 1x1 kernel spinning on `%globaltimer` for 2,000 or
  20,000 us, launched back to back (launch + `cudaDeviceSynchronize`), 4,000 or 400 launches
  (about 8 s). It prints the timed loop's CLOCK_MONOTONIC window, per-launch times and the
  process CPU seconds in the window (getrusage, all threads, the watcher included). Built with
  nvcc 13.2, `-cudart shared`, legacy launch stubs, as the earlier victims.
- **Probes** (kretprobe on `spinKernel`, `scripts/probe/`): `hostspin` makes one host call (an
  update of an ordinary hash map) at the kernel's exit, timed on the GPU with globaltimer;
  `nohost` keeps the same GPU-array bookkeeping without the host call.
- **Load** `scripts/load.c`: 6 unpinned threads (one per core) in a xorshift loop; a sampler
  records the iteration count every 10 ms. Throughput is taken inside the victim's window.
- **Sets**: P no bpftime; N dev aa6eb973 (the watcher before 46f5a7cb: polls 200 us after a
  request, then sleeps 50 us doubling to 1 ms); W dev 46f5a7cb (WFE while a noted probed launch
  runs); W0 the same build with `BPFTIME_CUDA_WATCHER_WFE=0` (sleeps, a probed launch ends the
  sleep and restarts the 200 us poll).
- **Rounds** (`scripts/rounds.sh`, 20:40-21:07 JST): 5 rounds x 8 cells (load on/off x spin
  2,000/20,000 us x probe host/none) x 4 sets, set order rotating. Checks before every run
  (`scripts/run_one.sh`): time window, cpu5 1728000 and GPU 1020000000 before and after, no
  leftovers, no build, no bpftime segment. All 160 runs valid (`verify.csv`): rcs 0, 4,001 / 401
  records, clocks unchanged; highest temperature 54.5 C.
- **Phase sweep** (`scripts/phase.sh`, no load, N and W0): spin 2,000-3,000 and 20,000-21,000 us
  in 100 us steps (`phase.md`).
- **CUDA flags** (`flags/flags.cu`, `flags/flags.log`): each wait mode set before init, and spin
  set after init; read back with `cudaGetDeviceFlags`, `cuCtxGetFlags` and, from another thread,
  `cuDevicePrimaryCtxGetState`; CPU of the syncing thread over 50 syncs on 20 ms kernels.

## Results (`summary.md`; latency pooled over 5 rounds, the warm-up call dropped)

Load on (6 workers on 6 cores), probe with a host call:

| spin | set | victim CPU s per wall s | load vs P | host call mean (us) | median / p90 / p99 (us) | calls > 3 ms | per-launch mean (us) |
|---|---|---:|---:|---:|---:|---:|---:|
| 2 ms | N | 0.080 | 0.988 | 1,048 | 1,024 / 1,032 / 2,927 | 0.9% | 3,146 |
| 2 ms | W | 0.784 | 0.870 | 476 | 15 / 3,510 / 3,897 | 11.0% | 2,576 |
| 2 ms | W0 | 0.185 | 0.970 | 360 (one phase) | 172 / 658 / 3,394 | 2.2% | 2,469 |
| 20 ms | N | 0.014 | 0.996 | 886 | 933 / 958 / 1,056 | 0.3% | 20,995 |
| 20 ms | W | 0.804 | 0.863 | 985 | 15 / 3,941 / 3,990 | 23.4% | 21,142 |
| 20 ms | W0 | 0.025 | 0.995 | 212 (one phase) | 89 / 726 / 1,065 | 0.1% | 20,342 |

Probe without a host call, load on: W 0.867 / 0.865 of P's throughput (victim 0.81-0.82 CPU s
per wall s), W0 0.979 / 0.997, N 0.998 / 1.001. Load off: W answers in 13.8 / 14.2 us median,
p99 15.0 / 16.4 us, and holds 1.0 CPU s per wall s with or without host calls.

Phase sweep: N's and W0's wait is a sawtooth in the spin length with a period of about 1.055 ms
(the 1 ms sleep plus about 55 us overshoot, timer slack 50 us), from about 30 to 1,030 us. The
victim's fixed launch-sync loop locks the call to one phase, so each r1-r5 number for N and W0 is
one point on the sawtooth. Mean of the medians over the sweep: N 530 / 525 us, W0 539 / 539 us.

## Findings

1. **W holds a core for as long as probed kernels run, host calls or not.** Without load that
   costs nothing measurable here except the core's time (power was not re-measured; WFE drew the
   power of a sleeping core in `gpu/wfe-20261111`). With 6 load threads on 6 cores the load
   loses 13-14%, the fair share of a 7th runnable thread, for the probe without host calls too.
2. **Under full load W's latency advantage disappears.** The median stays at 15 us, but 11% (2 ms)
   and 23% (20 ms) of calls wait more than 3 ms, about one scheduler tick (HZ 250) while the WFE
   thread is preempted. The mean (476 / 985 us) is no better than a sleeping watcher's phase
   average (about 530 us), and at 20 ms the victim's per-launch mean is above N's.
3. **W0 is not an improvement over N for these kernels.** Its low medians in the rounds (172 /
   89 us) are phase luck; over the sweep it equals N. It costs more CPU than N: every probed launch
   ends the sleep and starts a 200 us poll (about 216-246 us of CPU per launch, 2.1-3.0% of the
   load at 2 ms). A case where W0 could win, kernels shorter than the 200 us poll, was not run.
4. **Following CUDA's wait setting is possible but would switch WFE off for default apps.**
   `cuDevicePrimaryCtxGetState` from another thread reads the primary context's scheduling bits
   (mask 0x7; the runtime calls add 0x8, map host), and a change after init is visible. Never set
   and auto both read back as auto (0); CUDA does not report what auto resolves to. On this Orin
   (CUDA 13.2, MAXN_SUPER) auto behaves as blocking sync (sync thread 0.2% CPU; the P victims
   0.2-1.7% for the whole process); spin and yield keep the sync thread at about 100% on an idle
   system. Not covered by that read: contexts made with `cuCtxCreate`, per-event blocking sync,
   green contexts. Following the flag means WFE only for apps that ask for spin or yield.

## Limits

One load level (6 workers, one per core); W's cost scales with the share of time probed kernels
run, here about 100%. One host call per kernel, at its end, from one GPU thread; many calls per
kernel and short kernels were not run. Power not measured. The flag test ran each mode once with
`cudaDeviceSynchronize` only, one context, one power mode. Build labels N / W are by build
directory (`bpftime-gpu-work/l1`, `wake`); the opt-out string is present only in the wake build.

## Review

A workflow of three reviewers and a critic recomputed every number from the raw logs without the
summary script and tried to refute the claims (`review/claims.md`, `review/workflow-result.json`).
All numbers reproduced. Corrections taken in: W0's sawtooth period is about 1.055 ms and its
schedule restarts after the previous request's poll, not at the launch; the 20 ms W0 load costs
(0.3-0.5%) are within the P-vs-P noise (up to 0.6%); W's under-load mean is no better than a
random-phase sleeper; "WFE only for probes with host calls" must also skip the launch note, or it
keeps W0's 2% cost; yield only looks like spin on an idle system.

## Files

`summary.md`, `verify.csv`, `phase.md`, `logs.tar.xz` (all runs, smoke, phase sweep), `scripts/`,
`flags/`, `review/`.
