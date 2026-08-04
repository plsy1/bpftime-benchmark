# x64 per-CPU hash delete fix smoke test

## Result

This is a single-run smoke test of commit
`c6e2737aabddb23a9915f5eec2fe890f2c3fba07`. It is not a five-run formal
mean or median.

The repaired BPFtime path now erases the complete per-CPU hash entry. The
official out-of-timing setup repopulates keys 0 through 999 before every timed
sample, so all 1000 measured helper calls are delete-hit operations.

Net cost is:

```text
(per-CPU hash delete ns/invocation - empty uprobe ns/invocation) / 1000
```

| Item | kernel | BPFtime |
|---|---:|---:|
| Raw delete ns/invocation | 113329.091120 | 1096194.080950 |
| Empty-uprobe ns/invocation | 3356.617140 | 442.486860 |
| Net ns/helper | 109.972473980 | 1095.751594090 |

The post-fix single-run BPFtime/kernel ratio is **9.963871x**. The direction
matches the Jetson single-run reference, where kernel is 150.214513 ns/helper,
BPFtime is 981.381322 ns/helper, and the ratio is 6.533x. These are smoke
results from different hosts, not formal cross-architecture averages.

## Interpretation

The previous fixed-frequency x64 archive reported a BPFtime median near
260.901 ns/helper, but that code cleared one per-CPU value slot instead of
deleting the shared hash entry. It did not implement the same delete semantics
and must not be retained as a delete-performance baseline. The repaired path
is approximately 1.096 us/helper in this run and takes the expected slower
direction after real shared-memory hash erase work is restored.

The kernel smoke result, 109.972 ns/helper, remains close to the prior x64
kernel median of 108.661 ns/helper. This is a useful control: the source change
affects the BPFtime userspace per-CPU hash implementation, not the kernel BPF
map path.

## Conditions

- Intel Core i7-8750H, Linux `7.0.0-27-generic`.
- Source branch `codex/official-no-btf`, exact commit `c6e2737`.
- RelWithDebInfo, LLVM JIT ON, runtime LTO ON.
- Probe read/write checks OFF, Boost 1.83, GCC/G++ 13.4.
- BPF compiler Clang 15.0.7, LLVM commit `8dfdcc7...`.
- CPU5 pinned at 2.2 GHz, CPU11 offline, performance governor, turbo off.
- Root loader and victim, `ITER=100000`, complete unmodified 17-case victim.
- Kernel and BPFtime loaders each waited five seconds before the victim.
- BPFtime loader started the syscall server; victim reported
  `shm_open_type 1`; no kernel BPF links were present in the BPFtime phase.

The handoff example used `/usr/lib/llvm-15/bin/clang`, which is absent on this
host. The build used the exact Clang 15.0.7 compiler recorded by the earlier
formal x64 experiment at
`/home/y1/src/llvm-project-15.0.7/build-kmr-gcc12/bin/clang`. This is a path
difference, not a compiler-version substitution.

## Validation

- Both victim outputs contain all 17 official cases.
- Both loader logs contain the per-CPU hash delete setup attachment.
- Kernel loader created BPF links; all links were gone before BPFtime.
- BPFtime used shared-memory runtime and created no kernel links.
- Both turbostat files report CPU5 `Bzy_MHz=2200`.
- Kernel victim elapsed time: 2:14.16, exit status 0.
- BPFtime victim elapsed time: 7:04.36, exit status 0.
- Final BPF link count is zero and `/dev/shm/bpftime_maps_shm` is absent.
- CPU11, governor, turbo, and pstate limits were restored.
- Source worktree is clean after the run.

## Files

- `summary.csv`: exact raw, empty-subtracted, and ratio values.
- `environment.txt`: commits, build configuration, hashes, and cleanup state.
- `run.sh`: fixed-frequency single-run orchestration and validation.
- `fixed-frequency-state.txt`: formal CPU controls before victim execution.
- `raw/kernel-loader.txt`, `raw/bpftime-loader.txt`: complete loader logs.
- `raw/*-run-1.txt`: complete official victim output.
- `raw/*-run-1.time.txt`: process resource and exit-status records.
- `raw/*-run-1.turbostat.txt`: fixed-frequency evidence.
