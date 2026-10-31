# Fixes for the third review's findings (2026-10-31)

The findings are in `../rereview2/README.md` (dev 9aeb2863). The fixes are on
dev branch `dev/uprobe-official-177a1dc465ff`, commits 9aeb2863..f73a45f1.
Every fix has a test that fails with the fix removed; each commit message
names its mutation.

## Commits

| Commit | Finding | Fix |
|---|---|---|
| d93a0f00 | 2, creation details | Per-CPU hash keys counted once in the size bound. Creation rules checked against Linux v6.8 sources: hashes have no 512-byte key limit and E2BIG at key + value >= 4 MiB - 48; arrays take values up to INT_MAX and require 4-byte keys; queues and stacks take no key; a per-CPU value over 32 KiB is ENOMEM. bf1af650 had used an older kernel's rules. |
| 06fdaa07 | 3, 4 | Boost's container length_error (not a std::length_error) and any other exception no longer leave a handler without its map. Unsupported types return EINVAL, not errno 0. |
| d1abdc50 | 5 | A rejected creation closes the descriptor the shm opened for it. |
| 6ae782c8 | 6, errno notes | Lookup and delete: a null key or value is EFAULT and nothing is removed; any flag is EINVAL; unsupported types return ENOTSUPP (524). |
| 2839d98a | 8 | A perf shard whose data area did not fit is removed, so the next output retries. |
| a646180d | 9, 10, minor | Ringbuf submit reads its slot before publishing, checks the slot still holds its record, and publishes and releases with CAS. A consumed record's slot is free again. The identity is read before the reserve lock. |
| b8af155a | 11 | A dead producer's ringbuf record is recovered before the process is reaped (Z/X state in /proc). |
| ce11e384 | 12 | Fork children do not use the thread's cached perf shards (fork generation). |
| 58622f72 | 13 | An unknown pid namespace (no /proc) is a random id with the top bit set, not 0. |
| 2555b56d | 1, 10 | The clone hook makes no system call: clone3's flags are read from the caller's stack window, otherwise the epoch is bumped. Fork-like clones (no CLONE_THREAD) bump too. |
| ae90254b | 7 | On btrfs a mapped file is recognized by its inode. |
| 59fecdf4 | 14 | Every call that changes mappings forgets lazy mode's cached mapping. Special calls are now a bitmap. |
| 19a872c1 | 15, 16a | Our SIGSYS handler takes the application's SA_RESTART. The application's action is published whole. |
| ca54fa89 | 16b | A fork from a forwarded SIGSYS handler that interrupted a rescan no longer deadlocks. |
| f73a45f1 | documentation | CLAUDE.md: each fix, the statements that contradicted the code, and the remaining limits. |

Not done as planned:
- **16a** (a concurrent sigaction against a reading handler) has no deterministic test. It is covered only by the forwarding tests that use the new storage.
- **Lookup and delete flags** are checked through the syscall server with the review's experiment, not by a unit test.
- **16c** (the refused-execve report) is documented only.

## Verification

- Full build at f73a45f1.
- Candidate gate:
  - runtime focused: 153 cases, 152 passed, 1 skipped;
  - transformer: 47 cases, 46 passed, 1 expected failure;
  - syscall trace: 7 passed.
- Runtime cases each in its own process as root: 229 cases. All passed except the 3 map-usage-counter cases, which are not built.
- The third review's reproductions (`repro/`) rerun on the final build:
  - **maps**: 256- and 64-byte per-CPU hash keys are created; unsupported types give EINVAL; 200 rejections leak no descriptor; a hash just over the segment gives ENOMEM, keeps no handler and leaks no descriptor; lookup and delete gives 524, flags give EINVAL, null pointers give EFAULT.
  - **shm**: a zombie owner is recovered before the reap. A header-only shard is no longer kept: the output gives ENOSPC with 0 shards, and succeeds once memory is freed.
    - `rawclone` still prints "NO". It links libruntime.a without the transformer, which is the documented residual: without the hook there is no epoch.
  - **syscall**:
    - `exp_pvr`: epoch 1 and 2 with no filter, with an ERRNO filter and with a KILL filter; the KILL case used to die.
    - `exp_mremap`: the file stays intact, shared_skipped is 1.
    - `exp_restart`: lazy mode reads 1 for both the SA_RESTART handler and SIG_IGN; it used to return EINTR.
- The second round's reproductions (`repro/round2.txt`):
  - deadlock: 20 s, 10,779 forks, 20,078 dlopens;
  - appfilter: 159 in every mode.
- `clone-e2e/`: statsnoop with a parent, 2 vfork children and 2 raw clone(SIGCHLD) children under the syscall-trace agent. 1,601 events, every pid right (`summary.txt`).

## Cost

All runs had cpu5 at 1728000 before and after. Medians of 5.

- **Hook** (`hook-perf/`, 10 alternating rounds per write, 9aeb2863's transformer against f73a45f1's, built from source):

  | Leg | 9aeb2863 | f73a45f1 | Change |
  |---|---|---|---|
  | rewrite | 397.1 ns | 397.3 ns | unchanged |
  | agent-other | 424.0 ns | 426.1 ns | +2.1 ns |
  | agent-exit | 526.8 ns | 519.0 ns | −7.8 ns |

  The special-call bitmap costs nothing measurable.
- **Output** (`output-ab/`, uprobe on a counting loop, same harness as `../refixes/output-ab`). Against 9aeb2863's code (build `opt-identity2`), in `summary.txt`:
  - control (pid_tgid only): −0.3% with 1 thread, +1.5% with 6;
  - perf output: −3.5% with 1 thread, +1.3% with 6;
  - ringbuf, 6 threads: −2.4% to −4.7% across runs;
  - ringbuf, 1 thread: worse, see below.
- **Ringbuf, 1 thread** (`ringbuf-diagnosis.txt`, perf stat per call):
  - **Extra instructions**: the new code runs 41 more user instructions per call (1661 → 1702).
    - With the tool pinned to cpu4, in the producer's cluster: 426.8 → 437.0 ns (+2.4%) every run.
  - **Cross-cluster slow mode**: with the tool on the other cluster (cpu0, or unpinned), some runs take 720–770 ns instead of about 665 ns.
    - Same instruction count, about 130–160 more cycles.
    - About 40% of runs; 9aeb2863's code never did this in about 60 runs.
  - **Where it comes from**: the agent side. A new agent with the old server shows it; an old agent with the new server does not. 9aeb2863's ringbuf_map.cpp in the f73a45f1 tree shows it in 0 of 8 runs; that file with only the new `submit` (build `mix-s1`) shows it in 3 of 8.
  - **Ruled out**: code layout (the old code plus the new functions, never called: 0 of 8), the /proc probe, and the identity read inside or outside the lock.
  - **Changes that did not remove it**: a single atomic publish (fetch_and or exchange), a plain-store slot release, relaxed loads, and a relaxed slot store in claim. Some of these were cumulative working-tree states, not single changes (perfstat2, 4, 5, 6).
  - **Status**: none of these changes is kept; the code is as committed. The cross-cluster cost of the new submit is not explained yet.
- **fuse**: not rerun; this round has no change on its path.

`run_perfstat.py` and `run_pinned.py` need the versioned perf
(`/usr/lib/linux-tools/6.8.0-1020-nvidia-tegra/perf`): the BTF kernel has no
matching linux-tools package. Run 1 of the output A/B (`out-run1.jsonl`) had a
compile running during part of it; `out.jsonl` is the clean rerun.
