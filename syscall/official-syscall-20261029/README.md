# Official benchmark/syscall on ARM64 (2026-10-29)

First run of the official `benchmark/syscall` on the Jetson Orin (BTF kernel 6.8.12), now that
the aarch64 system call rewriting exists (dev `7a7d29b8`). Unmodified programs: `syscall.bpf.c`
attaches empty programs to `sys_exit_read` and `sys_exit_write` (one runs per write); `victim`
does 10 x 100,000 `write(2)` of 5 bytes to `/dev/null` (built `-g`, no optimisation, as the
Makefile does). Script unmodified (`benchmark.py`, 10 runs per configuration), run as root from a
symlinked view (`benchmark/`, `build/` -> `build-jetson-daily`, GCC 13, LLVM 15, LTO) with an
empty `HOME`, so the bpftime leg uses the LD_PRELOAD method (syscall-server in the loader,
transformer + agent in the victim). jetson_clocks on, cpu5 at 1728000 before and after
(`run.log`). That bpftime really traces these calls was checked separately, with counting
programs (`../arm64-trampoline-20261028`: 1,000,000 of 1,000,000 writes).

Note for rerunning: the script's cleanup kills (`kill -9`) every process whose command line
contains the words `victim`, `syscall` or `bpftime`; the wrapper (`run_official.sh`) runs a
copy named `runbench.py` so that its own parents survive (a first attempt killed itself, and its
log name contained `syscall`, which killed the background wrapper again).

| Configuration | Mean (ns) | Median | Std dev | Over no tracing |
|---|---:|---:|---:|---:|
| No tracing | 353.45 | 353.58 | 0.38 | - |
| Kernel tracepoint | 438.22 | 438.20 | 1.05 | +84.8 |
| bpftime (userspace) | 640.00 | 641.06 | 4.19 | +286.6 |

- bpftime's overhead is 3.4x the kernel's (the script's "46% more" compares total time).
- Official x86 numbers (`benchmark/syscall/example_results.md`, 2025, kernel 6.11): kernel +34.5,
  bpftime +80.2 ns (2.3x); README 2023 (kernel 6.2): +151.8 vs +232.6 (1.5x). Same direction.
- With one empty program per write bpftime costs +287 ns; with two counting programs per write
  (`../arm64-trampoline-20261028`) +327 ns, while the kernel goes from +85 to +172. Most of
  bpftime's cost is therefore fixed per intercepted system call, not per program (to be broken
  down: trampoline and register save, dispatch, program run). Different victims (`-O0` C++ here,
  `-O2` C there) make this an indication, not a measurement.
