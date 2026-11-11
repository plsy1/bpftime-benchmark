# Short sleeps instead of WFE while probed kernels run: test design (2026-11-11)

Question: can a sleeping watcher get close to WFE's latency at a small CPU cost, and does it hold
up better than WFE when all cores are busy? Draw the latency-vs-CPU curve between N (sleep up to
1 ms) and W (WFE, 46f5a7cb).

## Step 0: calibration (no bpftime, ~2 min)

`calib.c`: one thread loops `clock_nanosleep(period)` for 2 s and records the real interval; the
thread's CPU from RUSAGE_THREAD. Periods 10, 20, 50, 100, 200, 1000 us; timer slack 1 us
(`prctl(PR_SET_TIMERSLACK, 1000)`) and the default 50 us; load off and on (load.c, 6 threads).
Gives: real sleep length, CPU per wake-up, wake-up delay under load.

## Step 1: experimental build (not a dev commit)

Separate git worktree under `bpftime-gpu-work/sleep/src` (branch `exp/watcher-sleep` from
46f5a7cb; not under `bpftime-daily-state/worktrees`), own build dir `sleep/build-cuda` (same
flags as `wake/build.sh`; the last full build took about 11 min). Env-controlled so one build
serves every set:

- `BPFTIME_CUDA_WATCHER_POLL_US=<n>` (S sets): while a noted probed launch runs, sleep n us with
  `clock_nanosleep` instead of WFE, then check flag1. After a served request, poll hot for the
  existing 200 us busy window before sleeping again (otherwise every call of a burst would pay
  n us). Launch note, `any_running` every 100 us and the note-ended long sleep stay as in W.
- `BPFTIME_CUDA_WATCHER_MAX_SLEEP_US=<n>` with notes ignored (set M100): the plain change, the
  old N policy with the 1 ms cap lowered to n us and no launch tracking at all.
- `BPFTIME_CUDA_WATCHER_SLACK_NS=<n>` (default 1000 when either is set): the watcher thread's
  timer slack.

## Step 2: rounds

- Victim: `load/victim/spin.cu` with one addition, a per-launch random extra spin of 0-1,000 us
  (fixed seed, same sequence for every set), so host calls land at a random phase of any sleep
  schedule. The previous rounds were phase-locked (N and W0 numbers were one point on a
  sawtooth).
- Probes: `hostspin` (one host call at exit), `nohost`.
- Sets: P (no bpftime), N (aa6eb973), W (46f5a7cb, WFE), S20 / S50 / S100 / S200 (experimental
  build, slack 1 us), M100 (max sleep 100 us, no launch tracking). Step 0 prunes S periods the
  kernel cannot tell apart (e.g. S20 = S50 if sleeps under ~60 us are not possible).
- Cells: probe host x load on/off x spin 2 / 20 ms (latency and cost); probe none x load on x
  spin 2 / 20 ms (pure cost); burst: `spinKernel<<<1,32>>>`, all 32 lanes call at exit, load
  off and on, spin 2 ms (cost per call = (per-launch time - P's) / 32; the probe's slot counter
  is not atomic across lanes, so its per-call GPU timings are not used there). 8 cells x 8 sets
  = 64 runs per round, ~12 min; 4 rounds, set order rotating, ~50 min.
- Power: load-off cells sample tegrastats (100 ms) for VDD_CPU_GPU_CV and VDD_IN; tegrastats is
  stopped after each run and is on the leftover list.
- Checks as `load/run_one.sh`: window 03:25-04:15, clocks before and after, leftovers, no build,
  no segment; timeouts outside LD_PRELOAD.

## Metrics

Host call (GPU-timed, warm-up dropped): mean, median, p90, p99, max, share > 1 ms. Victim CPU s
per wall s, per-launch mean. Load throughput vs P (same round). Power (mW) load off. One plot:
mean host-call latency against victim CPU s per wall s, load off and on, one point per set.

## Decision rule

A period S is a candidate default if, load on and off, its mean latency is within about 3x of
W's no-load mean and its CPU stays under about 10% of a core (load loss under about 2%), and its
under-load tail (share > 1 ms) is below W's, and the burst cell costs no more than W and N.
A chosen period then goes through the 24-case subset rounds (N vs new, as the F rounds) before
it becomes a dev commit.

## Time

calibration ~2 min -> build ~11-15 min (nothing measured meanwhile) -> smoke ~3 min -> rounds
~50 min -> summary and review. About 1.5 h in all.
