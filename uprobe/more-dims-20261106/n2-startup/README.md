# Start-up of a traced process (2026-11-06)

Item 2 of the extra dimensions (`../README.md`). Kernel `6.8.12-1021-tegra-btf`, bpftime
`fix-final` (dev d46ceb8f). cpu5 at 1728000 before and after; 14:34-14:37 (`../go-rest.log`);
no other runs; every cell checked (no failed start; hits == starts for "hit", 0 for "nohit").

## Setup (`src/`, `run.py`)

- `shortlived [hit]`: calls `count_event` once (`hit`) or never, and exits.
- `spawnloop N CMD...`: starts N children one after another (`posix_spawn`, then `waitpid`) and
  reports the wall time from before the spawn to the reaped exit. It is never traced itself:
  children get `LD_PRELOAD` from `CHILD_PRELOAD`.
- Cells: `none` (no tool); `kernel` (outattach with the object on `count_event`; the kernel arms
  the probe when the binary is mapped); `fix-final` (the tool under the syscall server, each
  child with the agent); `agent-noshm` (children with the agent and no tool running). Objects:
  `out-control`, `out-rbres-256`, `out-perf-256` of `output-dims-20261106`. N = 200 (10 for
  `agent-noshm`), 2 reps.

## Findings

Wall time per process, p50 (p99) in ms, two reps:

| backend, program | nohit | hit |
|---|---|---|
| none | 0.49 (0.63-0.67) | 0.49 (0.58-0.74) |
| kernel, control | 0.50 (0.68-0.85) | 0.51 (0.62) |
| kernel, ringbuf 256 B | 0.50 (0.58-0.61) | 0.53 (0.66-0.74) |
| kernel, perf 256 B | 0.50 (0.59-0.75) | 0.53 (0.76-0.80) |
| fix-final, control | 22.6-22.7 (23.2-23.5) | 22.5-22.8 (23.2-26.1) |
| fix-final, ringbuf 256 B | 24.1 (24.7-24.9) | 24.0-24.2 (25.0-25.4) |
| fix-final, perf 256 B | 27.2-27.3 (28.1) | 27.2-27.4 (27.9-28.1) |
| agent, no tool running | 3,016.5-3,017.4 | 3,016.4-3,016.5 |

**1. Every traced process pays 22-27 ms before `main`; the kernel adds nothing measurable.**
The cost does not depend on whether the probe is ever hit: it is the agent's start (load the
114 MB `libbpftime-agent.so` with LLVM inside, map the segment, compile the program, hook the
function), done in every new process. About 16 ms of it is loading and initializing the agent
alone (the `agent-noshm` time minus its 3 s wait, below), 6 ms the program and the hook for the
control, 1.5 ms more with a ring buffer and 4.7 ms more with a perf buffer. A workload that
starts many short processes (make -j, shell scripts, CGI, a CI job) pays it for each one: here
a process that lives 0.5 ms takes 46-56x as long. The kernel arms uprobes when the binary is
mapped, once per mapping, with no per-process compilation.

**2. With the agent preloaded and no tool running, every process waits 3 s.** The agent retries
opening the shared memory 60 times, 50 ms apart (`bpftime_agent_main`, `runtime/agent/agent.cpp`,
upstream since fac864e5 "Support dynamic attach (#542)", 2026-05), before `main` runs. Anything
that keeps `LD_PRELOAD` after the tool exits (a shell that exported it, children of a traced
service, a container's environment) starts each process 3 s late, and a process that execs
others pays it again for each.

## For the plan

- Start-up: what of the 16 ms is relocation and constructors of the agent (LLVM's static
  initializers), and what is avoidable (load the JIT lazily; reuse compiled code across
  processes, as AOT objects in the segment).
- No-tool case: fail fast when the segment does not exist (one check, no retry) unless a loader
  announces it is starting.

Raw: `startup.jsonl`, `startup.log`. Binaries are rebuilt by `src/build.sh` (not kept).
