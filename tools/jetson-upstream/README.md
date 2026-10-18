# Jetson official-upstream tracker

This directory keeps an exact mirror of official bpftime in this fork, runs a
reproducible ARM64 uprobe benchmark when the official SHA changes, and attempts
to replay the accepted local patch stack in a separate worktree.

## Branch model

- `official/master`: remote-tracking ref fetched from
  `https://github.com/eunomia-bpf/bpftime.git`.
- `official-latest`: exact local mirror of `official/master`; never add local
  commits to this branch.
- `automation/jetson-daily`: this automation and its configuration.
- `codex/uprobe-official-<SHA>`: generated candidate branch for one official
  snapshot.

The official mirror is updated only by fast-forward. A rewritten official
history stops the job instead of silently replacing the branch.

## Commands

Run from the automation worktree:

```sh
python3 tools/jetson-upstream/tracker.py status
python3 tools/jetson-upstream/tracker.py sync --push
python3 tools/jetson-upstream/tracker.py migrate
python3 tools/jetson-upstream/tracker.py daily --push
```

`daily` does the following only when the official SHA changed:

1. updates and optionally pushes `official-latest`;
2. creates a detached official worktree;
3. initializes its submodules;
4. builds with GCC 13, clang/LLVM 15, RelWithDebInfo, LTO enabled, and probe
   checks disabled; on this Jetson it generates a build-local ARM64
   `vmlinux.h` with CO-RE relocations disabled because the host kernel does not
   expose `/sys/kernel/btf/vmlinux`;
5. runs the per-CPU array/hash, shared hash, and four-byte fast-path runtime
   tests that cover the maintained patch stack;
6. applies `jetson_clocks`, pins the suite to CPU 5, and runs the official
   uprobe benchmark for 10 rounds;
7. validates every raw result has 10 samples and archives metadata;
8. attempts the configured patch stack in a separate candidate worktree;
9. builds and tests a conflict-free candidate, then pushes its dated branch.

Results are written below `../results/uprobe/official-daily`. Worktrees,
migration reports, and the lock file are written below
`../bpftime-daily-state`. Both roots are configurable in `config.json` and are
resolved relative to the automation worktree's parent directory.

The upstream `benchmark.py` assumes both a repository directory named
`bpftime` and a build directory named `build`. The tracker creates a temporary
runner view below the state directory that provides those names as symlinks;
it does not modify the detached official worktree. The temporary runner also
guards cleanup of an already-exited loader and treats a missing
`Successfully started!` signal as a hard failure. This prevents an unloaded
probe from being recorded as an implausibly fast successful benchmark.

If migration conflicts, the candidate worktree and a JSON report are retained.
Resolve the conflict there, test it, and then update the patch stack rather
than editing `official-latest`.

The former SIGSEGV-handler installation patch is not replayed: current
official bpftime replaced that implementation with `process_vm_readv` and
`process_vm_writev`, so replaying the old signal-based path would be a semantic
regression rather than a migration.

The configured commit IDs are the rebased/adapted commits from
`codex/uprobe-official-8da867d541f8`, not the original historical commit IDs.
This makes the patch stack portable to another Jetson without relying on a
machine-local `rerere` cache.

The maintained stack now includes all accepted optimizations through
`df934cd13027` (2026-10-18), including ordinary hash lookup key dispatch and
per-CPU hash bucket-local iteration. The focused test filter also includes
`[maps][hash]` for fast modulo, generic keys, wrapped probe chains and full-table
misses. The latest complete ten-round run has eight of ten map operations
below same-run kernel BPF; ordinary and per-CPU array lookup remain slower.

The per-CPU array clean production leaf is included in the maintained patch
stack as commit `05eac852dc7b`. On the official 8da base, the same-base full
benchmark reduced per-CPU array lookup from 15,923.76 ns to 14,410.72 ns per
invocation, a 9.50% improvement; this reproduced the older-base 9.48% A/B
result.

The ordinary array lookup load-time specialization is included as commit
`bc5150e5fed5`. Its matched A/B reduced the helper cost from 23.43 ns to 9.72
ns, a 58.51% improvement. The complete 10-run uprobe benchmark reduced the
operation from 24,386.66 ns to 10,715.45 ns per invocation, leaving it at
2.82x the same-run kernel result.

## Timer

The checked-in user service runs at 03:30 local time and is persistent. Install
it with:

```sh
tools/jetson-upstream/install-user-timer.sh
```

The service uses non-interactive `sudo` for `jetson_clocks` and the kernel BPF
loader. Verify `sudo -n true` works before enabling it. The lock file prevents
two tracker jobs from running concurrently.

The current official bpftool bootstrap also requires the `libssl-dev` package.
It is installed on this Jetson; install the same package when moving the
automation to another machine.

The complete upstream runtime suite is not used as the daily gate because the
official `Test attach replace with ebpf` case currently SIGSEGVs on this Jetson
before reaching the map tests. The configured focused suite passes 24,550
assertions in 14 test cases and directly covers the maintained changes.
