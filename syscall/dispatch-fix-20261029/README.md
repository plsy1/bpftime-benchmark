# System call dispatch fix: remeasured (2026-10-29, Jetson)

Dev branch `2e850c45` ("syscall trace: pass system calls without programs straight through"), on top of `7a7d29b8`.
Same measurements as `../breakdown-20261029` (before the fix) and `../official-syscall-20261029`; build
`build-jetson-daily`, cpu5 at 1728000 before and after every run.

## The change

`dispatch_syscall` runs for every system call of a rewritten process. It used to set the thread-local
override-return callback (`std::optional<std::function>`) twice per call before checking whether any program was
attached: two heap allocations (the lambda captured three references, 24 B, more than the 16-byte local buffer) and
dynamic TLS accesses in the dlmopen-ed agent. Now:

1. The per-syscall and global lists are checked first; without programs the call is issued directly.
2. With programs, the callback is set only around them and captures one pointer: no allocation.
3. Numbers outside the 512-entry lists (`syscall(-1)`, x32) run only global programs. Indexing with them read other
   members of the attacher (-1 ran global exit programs at entry) or past the object (600 crashed).

Tests (`bpftime_syscall_trace_attach_tests`, 4 new): no allocation and no callback without programs; no allocation
with programs; out-of-range numbers (-1, 600, 0x40000001; the large ones in forked children); override at entry and
exit (regression guard, passes before and after). Mutations: setting the callback on every call (the old behaviour)
fails the two allocation tests (100 allocations per 100 calls), a callback capturing 24 B fails the second (200),
no range check fails the third. Daily gate green, transformer tests 3 × 6/6. Two older cases of this binary fail on
aarch64 before and after (they look up `fork`, which arm64 does not have); they are not in the daily suite.

## Layer breakdown (`run_layers.py --rounds 5`, medians, `run2.jsonl`, `summary-run2.md`)

| Leg | Before (run1) | After (run2) | Step before | Step after |
|---|---:|---:|---:|---:|
| none | 352.6 | 352.4 | – | – |
| rewrite | 390.1 | 389.9 | +37.5 | +37.5 |
| passthrough | 390.5 | 391.0 | +0.4 | +1.1 |
| agent-other (dispatch, no program for write) | 592.6 | **418.8** | **+202.1** | **+27.8** |
| agent-exit (one empty sys_exit_write program) | 634.8 | **498.5** | +42.2 | +79.7 |
| agent-count (enter + exit counting programs) | 681.8 | **600.0** | +47.0 | +101.5 |
| kernel-other / exit / count | 367.4 / 436.7 / 523.6 | 367.8 / 438.1 / 524.3 | | |

Over no tracing: bpftime with one empty exit program +282.2 → **+146.1 ns** (kernel +85.7); with two counting programs
+329.2 → **+247.6 ns** (kernel +171.9). All 45 runs valid, counts equal N.

The steps of agent-exit and agent-count grew because setting the callback (TLS) now happens only when a program
runs; before, every call paid for it in the agent-other step. Their totals dropped by the two heap allocations
(about 41 ns each).

## Official benchmark/syscall (10 runs, `official/`)

| | Before | After |
|---|---:|---:|
| No tracing | 353.45 ns | 353.14 ns |
| Kernel tracepoint | 438.22 ns (+84.8) | 437.76 ns (+84.6) |
| bpftime | 640.00 ns (+286.6) | **515.51 ns (+162.4)** |
| bpftime overhead / kernel overhead | 3.4× | **1.9×** |

Rerun 15:50 with no other process using CPU (`official-clean/`; the run above and every measurement since 10-28
night ran beside a leftover process spinning on one CPU): none 353.24, kernel 438.31, bpftime 515.99 ns. Within
0.6 ns of the run above.

## perf (`profile.sh`, 20M writes, `perf/`)

| Leg | cycles / write | instructions / write |
|---|---:|---:|
| rewrite (before, `../breakdown-20261029/perf`) | 674 | 1207 |
| agent-other before | 1032 | 2176 |
| agent-other after | **725** | **1345** |
| agent-exit after | 890 | 1744 |

- agent-other: the dispatch costs 51 cycles and 138 instructions over the trampoline; malloc/free, `std::function` and
  TLS are gone from the profile (`syscall_callback` 4.6%).
- agent-exit: what remains with a program is the thread-local callback: `_dl_tlsdesc_dynamic` 4.3% and the TLS init
  function of `curr_thread_override_return_callback` 2.9%, beside `bpftime_prog_exec` 2.4%. Making the thread-local a
  trivially initialized pointer (initial-exec) would remove that; it is shared with the Frida uprobe path, so it is
  a separate change.
