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
- `dev/uprobe-official-<SHA>`: generated candidate branch for one official
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
`archive/uprobe-official-8da867d541f8` (formerly `codex/uprobe-official-8da867d541f8`),
not the original historical commit IDs. Candidate branches were renamed from
`codex/` to `dev/` on 2026-10-26; the commit IDs are unchanged.
This makes the patch stack portable to another Jetson without relying on a
machine-local `rerere` cache.

The maintained stack now ends with the default array lookup JIT (accepted
2026-10-21): `9ffbffba8325` adds the owned ordinary array JIT binding,
`c0939bdf8e3e` makes ordinary and per-CPU array lookup specialization the
default for eligible LLVM JIT programs (with the bound array erase fix for the
GCC 13/LTO ordinary hash delete regression), and `2eea622f9768` makes the
map-name collision test create the segment-wide map-binding mutex before its
baseline, as `bpftime_shm` does. The JIT changes keep `vm/llvm-jit` clean: the
llvmbpf changes are parent-owned patches applied at configure time, and a
patch conflict after an upstream llvmbpf update fails configuration and needs
manual review. The JIT was measured with an LLVM 16 build; the daily job
builds and tests it with LLVM 15, so daily candidate builds are not the same
toolchain as the archived JIT measurements. The focused filter adds
`[array-jit-lifetime]`, `[array-jit-poc]`, `[array-offset-branch]` and the
injected-agent host-contract cases. `9ffbffb` still passes the obsolete
`BPFTIME_ARRAY_JIT_INLINE` variable to the userspace benchmark; the runtime no
longer reads it.

`bdd98355868b` (accepted 2026-10-21) lets programs with libbpf global data
(`.rodata`/`.data`/`.bss`/`.kconfig` direct map-value loads) use the array
lookup JIT: such single-element arrays are bound and owned, and the loaded copy
of the program uses their retained addresses instead of `map_val(fd)`. A
consuming workload with a `.rodata` setting and a `.bss` counter improves by
67-86% versus `5466f0d`; the original uprobe suite is unchanged (12-round AB/BA
screen within +/-0.6%). Its tests are tagged `[array-jit-global]` and are
already covered by `[array-jit-lifetime]`. Replayed onto official
`177a1dc465ff` with LLVM 15: 69 cases / 128,422 assertions on the candidate.

Five commits accepted 2026-10-22 follow: `f24dc9bf93f9` (hash variants of the
consuming diagnostic), `baf0cc85eba4` (constant-time dispatch of non-inlined
map calls in bound programs), `c3090b598241` (typed hash helpers next to arrays
and global data), `1cb208076d74` (the ordinary hash map allocates twice
`max_entries` buckets, releases deleted runs that end at an empty bucket and
bounds probing by the largest displacement; the map's memory doubles) and
`f26cc335f797` (bound programs call the original map helpers directly for
constant non-array handles; overlay patches and a private helper slot header).
Hash lookup misses drop from 8.6-19x to about 0.8-0.95x kernel on the
consuming workloads; tests are covered by `[bpftime_hash_map]` and
`[array-jit-lifetime]`. Replayed onto official `177a1dc465ff` with LLVM 15:
74 cases / 239,322 assertions on the candidate, 26 cases on official.

Three commits accepted 2026-10-23 follow: `af10949b47a2` (the hash map probe
bound shrinks when the farthest elements are deleted, so misses stay fast
after long insert/delete churn: 1.62x -> 1.01x kernel on the churn workload),
`dffb2fce8064` (skip the bound recomputation in the common delete case) and
`72aebadd6d51` (typed hash delete helper selected at load time like the
typed lookup/update helpers; official ordinary hash delete 36.9 -> 28.2 ns).
Replayed onto official `177a1dc465ff` with LLVM 15 (round 5).

Two commits accepted 2026-10-23 follow: `20870a03f73a` removes the per-event
CPU affinity pinning from `bpf_perf_event_output` (the July ssl-nginx fix, which
the official tree still lacks: +21.6% ssl-nginx throughput at 16 B over official
177a1dc) and `a90fa447abf3` lets fork children execute inherited array JIT
programs. Since `c0939bdf8e3e` a program bound to arrays returned ENOTSUP in
fork children, so forking servers (nginx master/worker) silently recorded no
events; the atfork prepare handler now takes a shared map reference per live
binding for the child. Replayed onto official `177a1dc465ff` with LLVM 15
(round 6).

`db3d475f329d` (accepted 2026-10-23) makes bpftime's software perf buffer
report dropped samples kernel-style (`PERF_RECORD_LOST`, `-ENOSPC`) and wakes
the emulated `epoll_wait` consumer once a producer shard is half full. Before
it, sslsniff on ssl-nginx with 256 KB responses silently lost about half the
events. The focused filter now also runs `[perf_event],[epoll]` (present in the
official tree as well). Replayed onto official `177a1dc465ff` with LLVM 15
(round 7).

`7ed339ad` (accepted 2026-10-24) caches `bpf_get_current_uid_gid`: the agent
interposes libc's eight uid/gid setters, each invalidating the cache, and the
cache is enabled only when all eight are bound (LD_PRELOAD); fork children
invalidate. The helper's two system calls per call were half of bpftime's
helper/map stage in the sslsniff ablation; ssl-nginx 16 B goes from +8.0% to
+16.5% over kernel. The focused filter adds `[uid_gid]`. Replayed onto official
`177a1dc465ff` with LLVM 15 (round 8).

`bca2ebce` (accepted 2026-10-24) caches the producer pid and tid that every
software `bpf_perf_event_output` used to fetch with `getpid()` and `gettid`
(two system calls per sample, found with perf in the nginx worker); an
atfork child handler clears the cache. ssl-nginx 16 B: +14.8% -> +17.4% over
kernel. Replayed onto official `177a1dc465ff` with LLVM 15 (round 9).

Before the JIT, the stack included all accepted optimizations through
`fefe2f6edf27` (accepted 2026-10-19), including ordinary hash lookup key dispatch,
per-CPU hash bucket-local iteration, per-CPU array production lookup leaf
inlining and ordinary array lookup error-path outlining. The focused test filter
includes `[array-lookup-cold]` for the new boundary/errno regression tests and
`[maps][hash]` for fast modulo, generic keys, wrapped probe chains and full-table
misses. The complete ten-round run at that point had eight of ten map
operations below same-run kernel BPF; ordinary and per-CPU array lookup were
slower. With the JIT, the historical array lookup workload ignores the
returned value and can simplify after inlining, so array lookup gains are
measured with separate consuming workloads instead (archived under
`results/uprobe/array-jit-default-delete-fix-20261021`).

The latest ordinary array lookup change moves ENOENT handling into a shared
cold, non-inlined function. The specialized helper's successful path no longer
uses a stack frame, register spills or calls; all checks and generic fallback
remain. Six-round A/B improves its median by 6.75% (six of six paired wins).
The full ten-round suite reduces cost from 7.93 to 7.47 ns/helper (-5.8%),
leaving a 1.96x same-run kernel ratio. The per-CPU array lookup result is
10.76 ns/helper, 2.45x kernel; the other eight map operations remain below
same-run kernel. The archived table retains the initial and previous baselines.

The preceding per-CPU array lookup change reduces full-suite cost from 11.64 to
10.92 ns/helper (-6.2%, 2.48x same-run kernel). Two additional balanced six-round
A/B tests reproduce roughly 7% lookup gains. They do not consistently reproduce
the original +0.82% per-CPU array update difference (-0.19% and +0.45% median
changes); the unchanged update instruction sequence and remaining uncertainty
are documented in the experiment archive. CPU selection is still performed
on every lookup, with affinity-change correctness coverage.

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
before reaching the map tests. With the JIT commits replayed onto official
`177a1dc465ff`, the configured focused suite passes 128,422 assertions in 69
test cases on the LLVM 15 candidate build and 17,913 assertions in 26 test
cases on the official build; candidate-only tags match nothing there and do not
fail the gate. Experiments on the maintained stack should run this whole
filter, not only their own tags: the map-name collision case was missed by the
JIT experiments' narrower filters until it was replayed here.
