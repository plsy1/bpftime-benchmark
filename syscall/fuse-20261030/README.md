# fuse benchmark on the Jetson (2026-10-30)

First run of `benchmark/fuse` on ARM64, unblocked by the aarch64 system call
rewriting. The benchmark is bpftime-only by design: its tracepoint programs
call `bpf_override_return` and bpftime's helper 1003 (`bpftime_get_abs_path`),
which the kernel does not allow there, so there is no kernel leg. It compares a
FUSE passthrough mount without bpftime against the same mount with fs-cache's
programs answering some calls in the process (a getdents64 cache and an open
deny list).

Dev worktree at 2ecc32cb plus the fixes below; cpu5 at 1728000 before and after; no other runs.

## It did not run as shipped: three problems

1. **fs-cache's loader exits on ARM64** (benchmark): it attaches
   `syscalls/sys_{enter,exit}_open`, which do not exist on aarch64 (no
   `open(2)`): "failed to attach BPF programs".
2. **bpftime's syscall table misses aarch64 system calls** (bpftime,
   aarch64): `generate_syscall_id_table.sh` keeps only `#define __NR_x <number>`,
   but asm-generic defines `fcntl`, `newfstatat`, `statfs`, `fstat`, `lseek`,
   `mmap`, `truncate`, `ftruncate`, `sendfile`, `fstatfs` (missing) and
   `fadvise64` (wrong) as `__NR3264_*`. Their `sys_enter_*`/`sys_exit_*` programs fail to attach
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
attempt).

## Fixes (dev, after this directory's first commit)

- (1) fs-cache's loader skips the two `open` programs when the tracepoint is
  absent (`fs-cache-no-open.patch`).
- (2) `generate_syscall_id_table.sh` lets cpp expand every `__NR_` name; the
  aarch64 table gains the 10 missing calls and `fadvise64 223` (it had
  `fadvise64 __NR3264_fadvise64`: the old filter took "64" for a number). The
  x86 output on the NUC is byte for byte the old one. The header now depends
  on the script, so existing build directories regenerate it.
- (3) The `BPFTIME_ENABLE_FS_HELPER` guard is removed (before 2bcf61f3 the
  helpers were unconditional).
- Tests: "System call names map to their numbers" and `[helpers]` (a JIT-ed
  program calling 1003 and 1004); mutations (old script; guard back) fail
  them. No agent log errors in the runs below (root's `runtime.log`).

## Results (`run.sh`): ns per call, medians of 5 alternating rounds

100,000 calls each (`filebench`), filebench on cpu5, the FUSE daemon on cpu4,
fs-cache's loader on cpu0 (it busy-loops), printk output off
(`BPFTIME_TRACE_PRINTK_OUTPUT=none`; with it on the bpftime log, open was
22412 vs 21905 ns in the experimental build).

Fixed dev build (`build-jetson-daily`: LTO, probe read/write checks off),
`runs-fixed.jsonl`:

| Workload | native | bpftime | native / bpftime |
|---|---:|---:|---:|
| getdents64 at EOF, 20-entry directory (cache) | 8091 | 714 | **11.3x faster** |
| open + close, path on the deny list | 13067 | 896 | **14.6x faster** |
| open + close, allowed path | 13084 | 17387 | 0.75x (+4.3 us) |
| fstatat (program only looks up a map) | 877 | 1188 | 0.74x (+310 ns) |

Spread within 2% everywhere.

Earlier experimental build (`build-fuse-exp`, `runs.jsonl`): bpftime's CMake
defaults (no LTO, `ENABLE_PROBE_READ_CHECK`/`ENABLE_PROBE_WRITE_CHECK` on: every
`bpf_probe_read*` first checks the memory with `process_vm_readv`/`writev`),
`-DBPFTIME_ENABLE_FS_HELPER=1` and the missing table entries added by hand to
its generated header, which did not take: the `fcntl`/`newfstatat`/`statfs`
programs still failed to attach there, so its fstatat row ran no program.

| Workload | native | bpftime | native / bpftime |
|---|---:|---:|---:|
| getdents64 (cache) | 8077 | 1314 | 6.15x faster |
| open + close, deny list | 13063 | 2872 | 4.55x faster |
| open + close, allowed | 13116 | 21905 | 0.60x (+8.8 us) |
| fstatat (no program attached) | 876 | 1030 | 0.85x |

- The two cases the benchmark is built for work: a cached getdents64 and a
  denied open are answered in the process, without the FUSE round trip.
- Correctness: the cached listing equals the native one
  (`work/{native,bpftime}-listing.log`); every denied open was seen by the
  program (100,000 `sys_enter_openat` printk lines in a check run).
- The native "denied" leg is FUSE allowing the open (the C passthrough has no
  deny list); a FUSE daemon refusing it costs the same round trip.
- Allowed opens get slower because bpftime's helpers make system calls the
  kernel's do not. Per open/close (strace; native makes 2):
  - fixed build (`strace-open-fixed.txt`): 3 more: `getuid` and `getgid`
    (`bpf_get_current_uid_gid`), `getcwd` (helper 1003,
    `std::filesystem::absolute`);
  - with the probe checks on (`strace-open.txt`, `perf-open-syscalls.txt`:
    7.0 per iteration): also `process_vm_readv` and `process_vm_writev`
    (`bpf_probe_read_str`). Profile: `perf-open-report.txt`.
- The probe checks make `bpf_probe_read` fault-safe like the kernel's (a bad
  pointer returns -EFAULT instead of crashing the host); the daily build turns
  them off, bpftime's default turns them on.

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
- `run-fixed.log`, `runs-fixed.jsonl`: the fixed build's table.
- `profile-open.sh`, `count-open.sh`, `strace-open.sh` (`BUILD`, `STRACE_OUT`)
  and their outputs: the allowed-open cost.
- `fs-cache-no-open.patch`: the loader fix used here.
