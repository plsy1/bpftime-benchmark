# fuse benchmark on the Jetson (2026-10-30)

First run of `benchmark/fuse` on ARM64, unblocked by the aarch64 system call
rewriting. The benchmark is bpftime-only by design: its tracepoint programs
call `bpf_override_return` and bpftime's helper 1003 (`bpftime_get_abs_path`),
which the kernel does not allow there, so there is no kernel leg. It compares a
FUSE passthrough mount without bpftime against the same mount with fs-cache's
programs answering some calls in the process (a getdents64 cache and an open
deny list).

Dev worktree at 2ecc32cb; cpu5 at 1728000 before and after; no other runs.

## It did not run as shipped: three problems

1. **fs-cache's loader exits on ARM64** (benchmark): it attaches
   `syscalls/sys_{enter,exit}_open`, which do not exist on aarch64 (no
   `open(2)`): "failed to attach BPF programs". Fixed locally, uncommitted, the
   way libbpf-tools' opensnoop does it: skip those two programs when the
   tracepoint is absent (`fs-cache-no-open.patch`).
2. **bpftime's syscall table misses 11 aarch64 system calls** (bpftime,
   aarch64): `generate_syscall_id_table.sh` keeps only `#define __NR_x <number>`,
   but asm-generic defines `fcntl`, `newfstatat`, `statfs`, `fstat`, `lseek`,
   `mmap`, `truncate`, `ftruncate`, `sendfile`, `fstatfs`, `fadvise64` as
   `__NR3264_*`. Their `sys_enter_*`/`sys_exit_*` programs fail to attach
   ("Unable to lookup sys nr for syscall tracepoint sys_enter_fcntl"); the
   others attach, so a tool silently loses those events.
3. **Helpers 1003/1004 are compiled out on every architecture** (bpftime):
   `bpftime_get_abs_path`/`bpftime_path_join` sit under
   `#if BPFTIME_ENABLE_FS_HELPER`, a macro nothing defines (since 2bcf61f3,
   2023-12). The LLVM JIT then rejects `sys_exit_openat` ("Ext func not found:
   _bpf_helper_ext_1003; the program is disabled"), no directory fd is ever
   recorded, and the getdents64 cache never fills.

With only (1) fixed, the bpftime leg ran but cached nothing: getdents64 8498 ns
vs 8199 native (`check.log`; that run also had the server dead in its first
attempt). The numbers below use an experimental build `build-fuse-exp` (agent,
syscall server, transformer only): `-DBPFTIME_ENABLE_FS_HELPER=1` and the 11
missing entries added to its generated `syscall_id_list.h`. No source change
for (2) and (3) yet.

## Results (`run.sh`, `runs.jsonl`): ns per call, medians of 5 alternating rounds

100,000 calls each (`filebench`), filebench on cpu5, the FUSE daemon on cpu4,
fs-cache's loader on cpu0 (it busy-loops), printk output off
(`BPFTIME_TRACE_PRINTK_OUTPUT=none`; with it on the bpftime log, open was
22412 vs 21905 ns).

| Workload | native | bpftime | native / bpftime |
|---|---:|---:|---:|
| getdents64 at EOF, 20-entry directory (cache) | 8077 | 1314 | **6.15x faster** |
| open + close, path on the deny list | 13063 | 2872 | **4.55x faster** |
| open + close, allowed path | 13116 | 21905 | 0.60x (+8.8 us) |
| fstatat (program only looks up a map) | 876 | 1030 | 0.85x (+154 ns) |

Spread within 1-2% everywhere (`runs.jsonl`).

- The two cases the benchmark is built for work: a cached getdents64 and a
  denied open are answered in the process, without the FUSE round trip.
- Correctness: the cached listing equals the native one
  (`work/{native,bpftime}-listing.log`); every denied open was seen by the
  program (100,000 `sys_enter_openat` printk lines in a check run).
- The native "denied" leg is FUSE allowing the open (the C passthrough has no
  deny list); a FUSE daemon refusing it costs the same round trip.
- Allowed opens get 8.8 us slower. strace (`strace-open.txt`) and
  `perf stat` (`perf-open-syscalls.txt`: 7.0 system calls per iteration vs 2):
  each open/close makes five more system calls inside bpftime's helpers:
  - `bpf_probe_read_str` with bpftime's fault check: `process_vm_readv` and
    `process_vm_writev`;
  - `bpf_get_current_uid_gid`: `getuid` and `getgid`;
  - helper 1003 (`std::filesystem::absolute`): `getcwd`.
  The kernel's versions of the first two make no system call. Profile:
  `perf-open-report.txt`.

## Benchmark design notes (not changed)

- The getdents64 cache keeps the first 2048 bytes of a directory and is never
  invalidated (the README's unlink kprobe is not in the code); it alternates
  the cached listing and 0 per thread, so the timed loop (getdents64 at EOF)
  is not the same call natively (0 from FUSE every time).
- `sys_exit_fcntl` looks its arguments up in `getdents64_args_start` (wrong
  map), so `F_DUPFD` copies are never tracked.
- fs-cache's `main` busy-loops a CPU while attached.
- `benchmark.py` hard-codes `/root/...` paths, needs LoggedFS (not installed),
  and its `find -exec cat` workload execs a process per file. The filebench
  workload name is `newfsstatat` (sic).

## Files

- `check.sh`, `check.log`: first functional checks (shipped build).
- `debug.sh`, `work/agent-debug.log`, `work/server-debug.log`: which programs
  fire; the agent log errors are in root's `~/.bpftime/runtime.log`.
- `run.sh`, `run.log`, `runs.jsonl`: the table; `work/*listing.log`,
  `work/fuse-*.log`: correctness checks and FUSE operation counts.
- `profile-open.sh`, `count-open.sh`, `strace-open.sh` and their outputs: the
  allowed-open cost.
- `fs-cache-no-open.patch`: the loader fix used here.
