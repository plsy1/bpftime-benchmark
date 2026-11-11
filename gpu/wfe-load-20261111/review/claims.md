# Claims to verify (WFE under load, 2026-11-11)

Data: `/home/jetson/src/bpftime-gpu-work/load/logs/r1..r5/<load>_<spin_us>_<probe>_<set>.{env,victim,loader,load}`
(5 rounds x 32 runs; `logs/summary.md` from `summarize.py`), phase sweep `logs/phase1/` (no load,
N and W0, spin 2000-3000 and 20000-21000 us in 100 us steps), CUDA flag test `flags/flags.log`
(`flags/flags.cu`). Design: `DESIGN.md`; runner `run_one.sh`, `rounds.sh`, `phase.sh`.
Sets: P no bpftime; N dev aa6eb973 (old watcher); W dev 46f5a7cb (WFE while a noted probed launch
runs); W0 = W build with BPFTIME_CUDA_WATCHER_WFE=0. Watcher code:
`/home/jetson/src/bpftime-daily-state/worktrees/4708f4092d1e/candidate/runtime/src/attach/bpf_attach_ctx_cuda.cpp`
(lines ~185-360) and `attach/nv_attach_impl/probed_launch_activity.cpp`.
`.loader` holds `<name> count=N` and `<name> ns=v1,v2,...` (GPU-timed host-call latency, ns).
`.load` holds `<CLOCK_MONOTONIC ns> <cumulative worker iterations>` every 10 ms; `.victim` holds
`window_ns=<start> <end>` of the timed loop, per-launch stats, `cpu_s` / `wall_s` (getrusage of the
whole victim process during the loop).

- C1. W holds about one core whatever the probe does: victim CPU s per wall s 0.78-1.02 in every W
  cell (with or without host call, load on or off); under load the load workers run at 0.863-0.870
  of P's throughput in all four W cells, the no-host-call probe included.
- C2. W0 costs the load 3.0% (2 ms, host call), 2.1% (2 ms, no host call), 0.5% / 0.3% (20 ms).
- C3. N costs the load at most 1.2%.
- C4. Under load W's tail is worse than N's: share of calls > 3 ms is 11.0% (2 ms) and 23.3%
  (20 ms) for W against 0.9% / 0.3% for N; W p90 3.5-3.9 ms against N about 1.0 ms. At 20 ms under
  load W's per-launch mean (21,142 us) is above N's (20,995) and W0's (20,342).
- C5. Without load W answers in 13.8-14.2 us median, p99 at most 16.4 us, at both spin lengths.
- C6. N's and W0's host-call wait is a sawtooth in the spin length (period 1 ms, 0 to ~1 ms): the
  watcher's backoff schedule restarts at a fixed point (N: the previous request; W0: the launch
  note), so the call's arrival has a fixed phase in the 1 ms sleeps. W0's low medians at 2,000 and
  20,000 us (180 / 83 us) are where the sawtooth happens to be low; over the sweep W0 is not
  better than N on average.
- C7. W0's extra CPU over N with no host call (0.123 vs 0.021 s/s at 2 ms) comes from the launch
  note ending the sleep and the watcher then polling for the 200 us busy window, once per launch.
- C8. CUDA flags on this Orin (CUDA 13.2, MAXN_SUPER): never set or set to auto reads back as auto
  (scheduling bits 0) from cudaGetDeviceFlags, cuCtxGetFlags and, from another thread,
  cuDevicePrimaryCtxGetState; auto behaves as blocking sync (sync thread 0.2% CPU); spin and yield
  both keep the sync thread at ~100% CPU; blocking 0.2%; a change after init is visible to another
  thread. So bpftime's watcher can read the app's choice, but cannot read what auto resolves to.
