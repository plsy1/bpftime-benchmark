# Claims to verify (short sleeps vs WFE, 2026-11-11)

Data: `/home/jetson/src/bpftime-gpu-work/sleep/logs/r1..r4/<probe>_<load>_<spin>_t<lanes>_<set>.{env,victim,victim.err,loader,load,power}`
(4 rounds x 8 cells x 8 sets = 256 runs; `logs/summary.md` from `summarize.py`), calibration
`logs/calib.log` (`calib.c`). Design `DESIGN.md`; runner `run_one.sh`, `rounds.sh`; victim
`victim/spin2.cu` (spin + 0-999 us jitter per launch, fixed sequence); experimental watcher patch
`watcher-sleep.patch` (applied to a copy of dev 46f5a7cb in `src/`). Sets: P no bpftime; N dev
aa6eb973 (poll 200 us after a request, sleep 50 us doubling to 1 ms); W dev 46f5a7cb (WFE while a
noted probed launch runs); S<n> experimental build, sleep n us (timer slack 1 us) instead of WFE
while a noted launch runs, hot for 200 us after a request; M100 experimental build, N's policy with
the cap at 100 us, timer slack 1 us, launch notes ignored. `.loader` = GPU-timed host-call ns per
call (`count=`, `ns=`; first value is the warm-up launch); `.load` = load iterations every 10 ms
(CLOCK_MONOTONIC); `.power` = tegrastats every 100 ms (local wall-clock seconds); `.victim` =
window (monotonic and realtime ns), per-launch stats, process CPU s.

- D1. Without load W answers in 14 us mean and adds no measurable power over P (VDD_CPU_GPU_CV
  1,704 / 1,688 mW vs P 1,686 / 1,687), while short sleeps cost power: S10 +220 / +200 mW, S20
  +160 / +125, S50 +86 / +63, M100 +61 / +33 (2 ms / 20 ms cells). Latency: S10 21.5-22, S20 27,
  S50 42, S100 66-68, M100 65-67, N 531-540 us mean.
- D2. Under full load (6 workers on 6 cores) every sleeping set beats W on mean latency and on
  calls > 1 ms, and costs the load less: W mean 376 / 1,023 us, > 1 ms 12.6% / 30.9%, load 0.863 /
  0.867 of P; M100 mean 144 / 90 us, > 1 ms 3.5% / 0.9%, load 0.980 / 0.990.
- D3. Sleeping watchers are not immune to load: their p99 under load is 0.8-3.6 ms although the
  calibration's bare sleeps kept p99 within ~5 us of the period under the same load.
- D4. With probes that make no host call, the load loses: W 14%, S10 5-6%, S20 3-4%, S50 2-2.5%,
  S100 1-2%, M100 0.8-1%, N 0.1-0.4%.
- D5. Burst (32 lanes calling at exit): cost per host call without load W 9.2, S10-S100 9.4-10.9,
  M100 10.8, N 25.5 us; under load W 20.9, S 17.9-19.2, M100 17.3, N 30.1 us.
- D6. S<n> uses more CPU than M100 at the same period (S100 0.111 vs M100 0.101 s/s with host
  calls at 2 ms; 0.073 vs 0.030 without host calls) because of the launch tracking: the 200 us
  hot poll after a request and after the probed kernels end, the notes and the event checks.
- D7. M100's cost is paid also when no probed kernel runs (it sleeps at most 100 us always): from
  the calibration about 1.2% of a core and, from D1, tens of mW; not measured with an idle agent.
