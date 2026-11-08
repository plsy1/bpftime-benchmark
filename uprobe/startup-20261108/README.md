# startup-20261108: the AOT cache (traced processes link precompiled programs)

The one slow item fixed from `remeasure-20261108` (decided 11-08): a traced process's start.
Every traced process compiled every program with a fresh LLJIT at O3 (93% of a start under
memleak, almost all the optimizer and code generation proper). Now the syscall server compiles
each program once, when it is loaded, and agents link that object.

Dev commits (on 8a0afd36, candidate worktree, local):

| commit | what |
|---|---|
| 33bba8fe | one function chooses a program's helpers (agent and server register the same set) |
| a90d8d0b | the server compiles an AOT object at BPF_PROG_LOAD into the handler (`aot_insns`, `aot_key`, written before the handler is published); agents link it when their key matches, else compile |
| 9898ed19 | bound programs too: map values and inlined arrays' value bases are symbols each process defines (llvmbpf patches regenerated, large code model, inlining in the AOT compile) |
| c9d4d974 | CLAUDE.md: the cache's invariants, measurements, limits |

The helper programs (stampmain, spawnburst) are built from the sources by the scripts; the runs
used copies built from the same sources. Builds (`ssl-nginx-2x2-20261023/builds`, checks on): old = `dev-9c088441-probecheck` (8a0afd36 adds
a test only), new = `dev-9898ed19-probecheck` (c9d4d974 changes only CLAUDE.md), new:noaot = the
same binaries with `BPFTIME_AOT_CACHE=0` in the tool (agents compile as before; for the real tools
the hard-linked copy `dev-9898ed19-noaot`). `go.sh`, 22:26-23:12; `aot-ab/ab.py`, 23:15-23:33; CPU 5
at 1728000 before and after.

## 1. A process traced by memleak -c (`memleak-start/`, 10 runs, ms, p50 [min-max])

| cell | exec -> main | tool set-up until it starts the command |
|---|---:|---:|
| bpf | 0.7 [0.7-0.8] | 153.8 [144.6-173.4] |
| old | 308.3 [305.9-311.4] | 431.5 [430.3-435.7] |
| new | **38.1** [37.9-38.7] | 743.1 [739.5-753.4] |
| new, cache off | 309.2 [305.9-312.1] | 431.1 [430.1-432.9] |

The compile moved into the tool: +312 ms once, for memleak's 22 programs.

## 2. Processes starting at once, one program (`concurrent-start/`, 20 rounds, two passes)

| K at once | bpf child p50 / max | old child p50 / max | new child p50 / max | new round | old round |
|---:|---|---|---|---:|---:|
| 1 | 0.5 / 0.6 | 22.5 / 23.6 | **13.6 / 14.0** | 13.6 | 22.5 |
| 2 | 0.5 / 0.7 | 23.7 / 36.5 | 14.3 / 19.5 | 18.4 | 35.7 |
| 4 | 0.6 / 0.9 | 36.8 / 63.4 | 19.3 / 29.2 | 28.1 | 62.3 |
| 8 | 0.8 / 1.7 | 64.7 / 115.4 | 29.5 / 46.5 | 47.1 | 115.6 |
| 16 | 1.5 / 3.2 | 118.9 / 220.0 | 49.0 / 80.3 | 86.8 | 220.6 |

(`summary.md` has both passes and the cache-off cells, which equal old.) Each start's CPU time:
22.6 -> 13.8 ms. Starts still queue on `shm_lifecycle_lock` (held from opening the segment until
attaching is done): about 4.9 ms per process started with it (13.3 before). Not changed in this
round (decided 11-08); releasing the lock after registering in the alive agent set would remove it.

## 3. Steady state: AOT code against JIT code

Output cost per call (`output-threads.jsonl`, output-dims M1 settings, 3 x 6 s, ns per call per
thread, median; new/old):

| program | 1 thread | 6 threads |
|---|---|---|
| out-control (global data, no output) | 224 -> 225 (1.004) | 4,750 -> 4,671 (0.983) |
| out-perf-8 | 613 -> 604 (0.985) | 4,672 -> 4,713 (1.009) |
| out-rbres-8 | 391 -> 383 (0.982) | 4,917 -> 5,050 (1.027) |

Real tools on 1/2/4/6 threads (`realtools-mt.jsonl`, measure_mt.py, 3 rounds x 10 s): every 1- and
6-thread cell within 1.5% of old except funclatency-libbpf on one thread (-2.2%, 1.49 -> 1.46 M
ops/s, steady) and bpftrace hist on six (-2.5%); the 2- and 4-thread cells jump between two levels
on both builds (CPU cluster placement), medians not comparable. `aot-ab/` (5 rounds, old / new /
new with the cache off, `summary.txt`): funclatency-libbpf 1 thread 1.492 / 1.460 / 1.440, 6 threads
1.230 / 1.229 / 1.220; bpftrace hist 1 thread 1.348 / 1.345 / 1.340, 6 threads 1.262 / 1.247 /
1.246. The same binaries with the cache off are not faster, so the AOT objects are not slower than
JIT code; funclatency-libbpf's 2% on one thread is between the two builds (with the JIT too, 3.5%),
not attributed (the hot path's source is unchanged; relinked layout, and `bpftime_prog` gained a
member). Accepted 11-08.

## 4. Regression (`regression/`, dev c9d4d974, `build-probecheck`, checks on, 23:22-23:45)

Every runtime test case as root in a process of its own (`each.sh`): 279 cases (273 + the 6
`[aot-cache]` cases), the same 3 map usage counter cases skipped in this build as before, the rest
passed; `[kernel_user]` 2/2 and the daemon tests 10/10 as root; transformer 54 passed and the 1
expected failure; syscall trace 7/7 (the daily filter); Frida attach 26/26 in 5 of 5 runs (the
detach case that crashes in about 1 run in 4-5, before this work too, did not this time); the daily
smoke: funclatency 5,892,000 calls counted exactly, memleak 8 outstanding allocations, a SIGUSR1
detach under load with the workload running to its end. Root-owned test segments removed
afterwards. Earlier in the day (not in `regression/`): `[aot-cache]` with seven mutations (commit
messages), and the Frida suite 16 runs with 3 failing, the same rate with the pre-change Frida
impl (5 of 20: the crash predates this work; investigated in its own session).

## 5. x86 (NUC, 23:44-23:47, NUC session told before and after; `x86/`)

dev c9d4d974 built with the daily's settings (checks off, LLVM 19, gcc-13; the four new commits
taken there as a git bundle on top of the pushed 8a0afd36): the whole runtime suite in one
process (console reporter) 278 cases, 271 passed, 6 skipped, 1 failed (the user-to-kernel tail call,
which needs root, as on 9c088441); `[aot-cache]` 3/3; the array JIT and JIT load groups 43 cases;
Frida 3/3 (26 cases). The x86 files go to the x86-nuc branch (NUC session).
