# Where bpftime is still slower than bpf (2026-11-08)

Before the next fix plan: the slow items that had not been measured on the current dev, measured
against bpf, and the start-up and memory items taken apart. Build `dev-9c088441-probecheck` (dev
9c088441, probe read/write checks on; dev 8a0afd36 adds a test only). Jetson Orin, kernel
6.8.12-1021-tegra-btf, cpu5 at 1728000 before and after every batch, no leftovers, nothing else
running; 10:59-12:00 JST.

| part | directory / files | time |
|---|---|---|
| 1 output cost at 1/2/4/6 threads | `output-threads.jsonl`, `go.sh` | 10:59-11:12 |
| 2 real tools at 1/2/4/6 threads | `realtools-mt.jsonl`, `realtools-mt/` | 11:12-11:50 |
| 3 compile vs load an AOT object, per process | `aot-link/` | 11:55-11:56 |
| 4 a process traced by memleak, exec to main | `memleak-start/` | 11:57-11:59 |
| 5 the agent's size, relocations and memory | `agent-load/` | 11:59-12:00 |

`sum.py` makes `summary.md` (parts 1 and 2).

## 1. Output cost per call (output-dims M1's settings: steady, 3 x 6 s, 8-byte records)

ns per call per producer thread, median of 3; "out" = minus the control program's (the probe
alone), output-dims' method. Columns on the right: the 11-06 ratio (fix-final, before the
improvement round) for comparison.

| path | thr | bpf total / out | bpftime total / out | out bpftime / bpf | 11-06 | delivered bpf / bpftime |
|---|---:|---:|---:|---:|---:|---:|
| perf | 1 | 2,613 / 1,366 | 620 / 396 | 0.29 | 0.16 | 100 / 100% |
| perf | 2 | 4,185 / 2,368 | 688 / 404 | 0.17 | 0.15 | 100 / 100% |
| perf | 4 | 5,304 / 1,962 | 2,788 / 280 | 0.14 | 0.14 | 100 / 100% |
| perf | 6 | 5,041 / 963 | 4,611 / (-164) | - | 0.06 | 100 / 100% |
| ringbuf output | 1 | 1,961 / 715 | 397 / 173 | 0.24 | 0.60 | 100 / 100% |
| ringbuf output | 2 | 2,340 / 523 | 746 / 462 | 0.88 | 1.47 | 100 / 100% |
| ringbuf output | 4 | 4,171 / 829 | 2,917 / 409 | 0.49 | 2.22 | 100 / 100% |
| ringbuf output | 6 | 4,315 / 237 | 4,982 / 207 | 0.87 | 7.14 | 99.64 / 98.28% |
| ringbuf reserve+submit | 1 | 1,954 / 708 | 389 / 164 | 0.23 | 0.64 | 100 / 100% |
| ringbuf reserve+submit | 2 | 2,344 / 527 | 759 / 475 | 0.90 | 1.54 | 100 / 100% |
| ringbuf reserve+submit | 4 | 4,210 / 869 | 2,967 / 459 | 0.53 | 2.49 | 100 / 100% |
| ringbuf reserve+submit | 6 | 4,306 / 228 | 5,033 / 259 | 1.14 | 8.96 | 99.89 / 98.14% |
| control (the probe alone) | 1 / 2 / 4 / 6 | 1,246 / 1,817 / 3,342 / 4,078 | 224 / 284 / 2,508 / 4,774 | | | |

- The ring buffer's output part no longer grows with the producers (the reserve lock work of
  11-06/07): 2 threads 1.5x -> 0.9x bpf, 6 threads 7-9x -> 0.9-1.1x.
- At 6 threads every bpftime row is slower in total than bpf (4,982-5,033 against 4,306-4,315 ns),
  and so is the probe alone (4,774 against 4,078): six threads calling one probed function
  contend on Gum's per-function usage counter (`realtools-20261104`, profile: 80% of the cycles
  at 6 threads in `_gum_function_context_{begin,end}_invocation`'s LL/SC loop). At 4 threads the
  probe alone is still faster (2,508 against 3,342). perf at 6 threads: the output row ran faster
  than the control (negative difference: noise against a contended probe).
- Delivery with six producers on six cores: bpftime 98.1-98.3%, bpf 99.6-99.9% (both consumers
  starve; output-dims finding 4).

## 2. Real tools on threads (measure_mt.py, 3 rounds x 10 s, ops/s summed over threads)

| tool | 1 | 2 | 4 | 6 threads | 11-04 (6 thr) |
|---|---:|---:|---:|---:|---:|
| bpftrace uretprobe hist | 2.87x | 3.39x | 2.87x | 1.37x | 1.38x |
| funclatency (bpftime example) | 3.43x | 3.92x | 1.81x | 1.34x | 1.36x |
| funclatency (libbpf-tools) | 3.08x | 3.64x | 2.48x | 1.38x | 1.38x |
| malloc example (bpf_printk) | 0.97x | 0.40x | 0.27x | 0.20x | 0.20x |
| memleak | 1.94x | 1.73x | 1.18x | 0.74x | 0.78x |

(bpftime / bpf; 11-04 = dev-ff5515d8 in `vs-pre-mt-20261104`, before the stack trace map
fix.) Counts: bpftrace and libbpf-tools funclatency counted every call (within a few); the
bpftime example's funclatency 0.04-0.98% fewer calls than made (11-04: up to 2.5%; bpf 12-14%
fewer at 4 threads), not investigated, as before. memleak at 4 threads 0.78x -> 1.18x (efe08cbd, the lock-free stack trace
lookup); at 6 threads still 0.74x, the Gum counter. The malloc example is bound by `bpf_printk`:
its default sink is the bpftime log (spdlog, one lock). Its `reported` falls 5% short at 4-6
threads: the tool reads each pid's count and then deletes it, so increments in between are lost
(the tool's design; bpf loses 45% there, `expected` 9.5M, `reported` 5.0M).

## 3. Compile in every process, or load a precompiled object (`aot-link/`)

`bench_aot.cpp`, linked with the AOT tool's link line against build-probecheck: one fresh
process per measurement (the first LLJIT of a process pays LLVM's set-up, as a traced process
does), pinned to CPU 5, 20 per cell, modes alternated. JIT = `bpftime_prog_load(true)` (what
every traced process does now); load = `bpftime_prog::load_aot_object` on the object
`do_aot_compile` produced once (LLJIT + JITLink, helpers resolved by name in the process).

| programs | count | insns | JIT p50 (min-max) | AOT load p50 (min-max) |
|---|---:|---:|---:|---:|
| output-dims control | 1 | 15 | 11.83 ms (11.69-12.00) | 1.93 ms (1.90-2.12) |
| funclatency (kprobe + kretprobe) | 2 | 34 | 19.43 (18.99-19.70) | 3.28 (3.24-3.33) |
| opensnoop (openat enter + exit) | 2 | 259 | 23.81 (23.59-23.97) | 3.44 (3.42-3.51) |
| memleak (the 12 entry programs) | 12 | 626 | 112.37 (111.84-112.81) | 17.63 (17.36-18.38) |

A program costs 8-12 ms to compile and 1.5-2 ms to load. Both scale with the number of
programs, not with their size: 15 instructions and 259 cost the same per program (JIT 11.8 and
11.9 ms, load 1.9 and 1.7 ms), and memleak's 12 load in 17.6 ms, 12 x 1.47. Each program makes
its own LLJIT (LLVM's set-up for every instance); loading objects alone, one LLJIT each, would
still leave memleak's 22 programs at about 35 ms. One LLJIT per process for all its programs is
the part that would remove most of it (not measured).
Limits of the bench: programs come from `bpf_object__open` without libbpf's load, so no CO-RE
relocation and no subprograms (memleak's exit programs call `gen_alloc_exit2`, which libbpf
appends at load; the JIT refused them here, so only the entry programs are in the table); no
maps exist, so the array JIT binds nothing; helpers are the AOT tool's 1000 placeholders. The
bench sets a process-local runtime config first (`bpftime_set_runtime_config`): without a
segment, `bpftime_prog`'s constructor read an empty VM name and threw. Today the agent never
loads AOT code: `bpf_prog_handler::aot_insns` is written by `bpftime-aot compile/load` but read
nowhere in the agent, and `load_aot_object` refuses a program with bound arrays (global data),
which memleak and most libbpf-tools have.

## 4. A process traced by memleak (`memleak-start/`)

libbpf-tools memleak in uprobe mode (`-c`, the harness's way) starts a static wrapper
(`src/stampwrap.c`, copied to `/home/jetson/src/rp/stw` for memleak's 31-character limit),
which stamps CLOCK_MONOTONIC and execs `src/stampmain`, which stamps again on entering main.

| exec -> main | p50 | min-max |
|---|---:|---:|
| bpf | 0.73 ms | 0.70-0.90 |
| bpftime tool, child without the agent | 0.65 ms | 0.63-0.82 |
| bpftime (agent: segment, compile memleak's programs, attach) | 308.8 ms | 305.3-312.4 |

The 23 ms measured before was for one program; start-up grows with the programs a tool loads.
memleak loads 22 uprobe programs: at part 3's 8-12 ms each plus the agent's own 9 ms that is
185-273 ms, so most of the 309 ms is the compiles; the rest is not attributed (the bench's
programs had no maps, CO-RE relocations or subprograms, memleak's do).

## 5. The agent (`agent-load/`)

`libbpftime-agent.so` is 114 MB, of which 27 MB debug info; mapped: text 26.9 MB, rodata 10.1,
eh_frame 3.5, rela.dyn 4.6, data.rel.ro 3.3, bss 3.9. By symbol owner, LLVM is 22.8 of the
27.1 MB of text and 3.7 MB of data. Loading it applies 202,114 relative and about 2,100 symbol
relocations in every process (`LD_DEBUG=statistics`; ld.so on aarch64 prints no times).

Memory of a traced process (`smaps-traced.txt`: burstloop with outattach + out-control, 128 MB
segment) against the same process untraced (`smaps-plain.txt`), KB:

| | traced | untraced |
|---|---:|---:|
| VmRSS | 42,132 | 2,384 |
| private | 8,172 | 1,140 |
| of it: the agent's relocated data (private dirty pages of the .so) | 3,460 | - |
| anonymous (JIT, Gum, LLVM allocations) | 3,256 | 1,072 |
| heap | 1,244 | - |
| shared: the agent's code and read-only data (page cache, one copy for all) | 29,596 | - |
| the segment | 1,020 | - |

So a traced process costs about 7 MB of its own and 30 MB of code shared by every traced
process. The tool (outattach under the syscall server): VmRSS 21 MB (13.4 MB of it the server's
shared code), and its 128 MB segment committed in /dev/shm from the start (bpf: about 2 MB).

## The slow items now

| # | what | bpftime | bpf | cause |
|---|---|---|---|---|
| 1 | a traced process's start | 23-28 ms with one program; 309 ms under memleak | 0.5-0.7 ms | each program compiled in every process (8-12 ms each); the agent's own load about 9 ms (LLVM: 84% of its code, 202k relocations) |
| 2 | memory | +7 MB per traced process, +30 MB shared code; tool 21 MB + a 128 MB segment | about 0; tool 2 MB | the agent's relocated data, JIT and Gum; the segment committed in full |
| 3 | perf event delay to the tool | p50 90 us, p99 170 us | 19, 41-45 us | the consumer naps 100 us between batches (waking per sample cost ssl-nginx 10%) |
| 4 | one function probed from 6 threads at once | probe 4,774 ns (1.17x); memleak 0.74x | 4,078 ns | Gum's per-function usage counter (LL/SC); inside Frida/Gum, which this work leaves unchanged |
| 5 | bpf_printk from several threads | 0.40x (2) - 0.20x (6) | | the default sink is the spdlog log, one lock |
| 6 | checked probe reads of large data | 4 KB read 1,556 ns, 1 KB string 2,411 ns per probe | 1,439, 1,603 | a system call per read (the daily build has the checks off: fast, but a bad pointer kills the process) |
| 7 | delivery, 6 producers on 6 cores; 4-thread ring buffer bursts | 98.1-98.3%; 92-98% | 99.6-99.9%; 98.6-99.6% | the consumer gets less CPU |

Item 1 also covers processes starting at the same time (section 6): each start holds a lock
for about 13 ms, so 16 starting at once wait up to 211 ms (bpf 2.9 ms).

Decided 11-08: only item 1 is to be fixed. Items 3, 4 and 6 stay as they are (a deliberate
trade-off, inside Frida/Gum, the remaining gap is the system call itself); 2 waits for item 1
(most of it is LLVM in the agent); 5 and 7 are low priority.

## 6. Processes starting at the same time, and where a start goes (`concurrent-start/`, `startup-profile/`)

`concurrent-start/run.py`: `src/spawnburst.c` starts K copies of n2's shortlived at once (20
rounds, two passes), one program attached (`summary.txt`). bpftime's children each use 22.6 ms
of CPU at every K, but their wall time grows by about 13.3 ms per process started with them:

| K at once | bpf child p50 / max | bpftime child p50 / max | bpftime round |
|---:|---|---|---:|
| 1 | 0.5 / 0.7 | 22.1 / 22.7 | 22.1 |
| 2 | 0.5 / 0.8 | 23.0 / 38.0 | 35.5 |
| 4 | 0.6 / 0.9 | 36.5 / 62.8 | 61.9 |
| 8 | 0.7 / 1.7 | 65.4 / 117.0 | 115.0 |
| 16 | 1.3 / 2.9 | 117.8 / 211.5 | 219.7 |

The starts run one after another: the agent holds `shm_lifecycle_lock` (an exclusive `flock`
on `/tmp/bpftime-shm-<name>.lock`, agent.cpp) from opening the segment until it has compiled
and attached, about 13 ms of its 22, and every other agent waits for it (6 CPUs would run 4
starts in about 22 ms; it takes 62).

`startup-profile/README.md`: in a process starting under memleak, compiling the 22 programs is
93% of its start-up, and nearly all of that is the O3 optimizer and code generation proper
(creating each LLJIT instance: under 1.5%), so one LLJIT per process would save little. Loading
a precompiled object with the agent's own 39 helpers: 0.42-0.85 ms per program against 6.3-9.1
ms to compile it (`aot-link/results-real-helpers.txt`; the 1.5-2 ms of section 3 was mostly
defining the bench's 1000 placeholder helpers). memleak's programs use global data (.rodata,
.bss), so they are bound arrays, which `load_aot_object` refuses (-ENOTSUP).

Faster than bpf: the probe itself (1 thread 224 against 1,246 ns), ring buffer and perf output
at 1-4 threads (equal at 6), hash maps, the real tools at 1-6 threads (1.3-3.9x), ssl-nginx
under sslsniff (+17-19% with checks off, +5-7% with checks on), 64 processes on 6 CPUs (equal).
