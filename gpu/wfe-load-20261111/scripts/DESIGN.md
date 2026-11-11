# WFE watcher under full CPU load (2026-11-11)

Question: what does the WFE watcher (dev 46f5a7cb) cost when all 6 CPU cores are busy, and what
host-call latency does a probe get then? Before/after compared at the same time.

## Worst case for WFE

Long probed kernels back to back, one sparse host call each: the watcher sits in WFE for the
whole kernel (counts as a runnable thread), the call comes late in the kernel.

- Victim `spin` (new): 1 block x 1 thread kernel `spinKernel(ns)` spinning on %globaltimer for
  D us; L launches back to back (launch + cudaDeviceSynchronize each). Prints CLOCK_MONOTONIC at
  the timed loop's start/end, per-launch mean/median, and getrusage(RUSAGE_SELF) CPU seconds
  spent during the timed loop (all threads, includes the watcher).
- Probe `hostspin` (copy of hostwake.bpf.c): kretprobe on spinKernel, one hash update (host
  call) timed with globaltimer, stored in a GPU array (4096 slots).
- D = 2,000 us (L = 4,000, ~8 s) and D = 20,000 us (L = 400, ~8 s).

## CPU load

`load` (new): 6 worker threads (unpinned), each a xorshift loop adding to its own counter (own
cache line); a sampler thread records the sum every 10 ms with CLOCK_MONOTONIC to a file. Started
before the victim, stopped after. Load ops inside the victim's [start, end] window -> ops/s.

## Sets (same rounds, order rotating)

- P: victim without bpftime (reference)
- N: dev aa6eb973 build (`l1/build-cuda`): old watcher, sleep 50 us doubling to 1 ms
- W: dev 46f5a7cb build (`wake/build-cuda`): WFE while a noted launch runs
- W0: same build with BPFTIME_CUDA_WATCHER_WFE=0 (sleep path, a note ends the sleep)

Each with load on and load off; D in {2000, 20000}; 5 rounds. ~80 runs x ~12 s.

## Metrics

1. Load throughput (ops/s) during the window, relative to P with load.
2. Host-call latency (GPU-timed): median, p90, max.
3. Victim per-launch time (us) and process CPU seconds per wall second in the window.
4. Thermal / clock check after each run (cpu5 1728000, GPU 1020000000), temps logged.

Checks as run_wake.sh: window 03:25-04:15, clocks, leftovers, no build, no bpftime segment;
timeouts outside LD_PRELOAD; segment cleanup by name after fuser.
