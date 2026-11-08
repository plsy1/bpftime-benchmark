# Where a traced process's start-up goes (11-08)

perf 6.8 (`/usr/lib/linux-tools/6.8.0-1020-nvidia-tegra/perf`; the `perf` wrapper has no
build for this kernel), `-F 20000 -g`, dev 9c088441 with checks (build `dev-9c088441-probecheck`).

## A process starting under memleak (`memleak-child-children.txt`)

`perf record -- env LD_PRELOAD=<syscall server> ... memleak -c /home/jetson/src/rp/stw 1`
(memleak-start's set-up; exec -> main 344 ms under perf, 309 ms without). Samples of the
traced child (`--comm stampmain`): 35.3% of all; `bpftime_agent_main` 35.2%,
`init_attach_ctx_from_handlers` 35.0%, `bpftime_prog_load` -> `llvmbpf_vm::compile` 32.9%:
**93% of the child's start-up is compiling memleak's 22 programs.** Inside the compile: the
O3 optimizer 15.0% (its pipeline construction: `populateModulePassManager` 2.8%,
`schedulePass` 4.3%), code generation (run at the first lookup) 16.3% (SelectionDAG 4.8%,
machine passes 9.6%). Creating the LLJIT instance is below 1.5%: the work is the optimizer
and code generation proper, so one LLJIT per process would save little.

## bench_aot (../aot-link/bench_aot.cpp), memleak's 12 entry programs, one process

Taken with the first bench binary (1000 placeholder helpers, the AOT tool's set); the binary
was rebuilt afterwards, so the recorded profiles no longer resolve and only these shares are
kept (inclusive, % of the process's samples):

- jit (112 ms): `compile` 83.6%; legacy pass manager runs 38.3% (`optimizeModule` 35.1%,
  `populateModulePassManager` 10.7%, `schedulePass` 16.0%); code generation at lookup 25.7%
  (`addPassesToEmitMC` 7.0%, `AArch64Subtarget` 4.1%); `generateModule` 11.5%;
  `create_and_initialize_lljit_instance` 10.7%.
- load (17.7 ms): `load_aot_object` 47.0%, of which `create_and_initialize_lljit_instance`
  40.7%. Split of that by direct callee (`childsplit.py`): `__sprintf_chk` 17.3%,
  `JITDylib::define` 16.1%, string building 6.4%, `AbsoluteSymbolsMaterializationUnit` 5.5%
  (defining the 1000 placeholder helpers: an artifact of the bench), `LLJIT::LLJIT` 10.6%,
  `create_host_jit_target_machine_builder` 7.9%. Linking the object itself: about 6%.

Re-measured with the agent's own helper groups (`BENCH_HELPERS=real`: kernel utils and shm
maps, the default config, 39 helpers; `../aot-link/results-real-helpers.txt`, 8 runs, CPU 5),
p50:

| programs | JIT | load an AOT object | per program |
|---|---:|---:|---|
| control (1) | 9.08 ms | 0.85 ms | 9.1 -> 0.85 |
| funclatency (2) | 13.44 ms | 1.21 ms | 6.7 -> 0.60 |
| memleak entries (12) | 75.17 ms | 5.01 ms | 6.3 -> 0.42 |

The traced process compiles more per program than the bench (about 14 ms for each of
memleak's 22): its programs are bound to their global data (.rodata, .bss: the array JIT
binding, a second optimizer run after inlining), and the exit programs carry a subprogram.
