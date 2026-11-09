# base-4708f409-20261110: the development base moves to official 4708f409

2026-11-10, Orin Nano, checks on. User decision (11-10): develop on
`dev/uprobe-official-4708f4092d1e` (worktree `bpftime-daily-state/worktrees/4708f4092d1e/candidate`)
instead of `dev/uprobe-official-177a1dc465ff`, which stays at 0decdfab, not renamed.

The new branch is the daily's candidate for official 4708f409 at c115b33d: the 236-commit list
(`gpu/fixes-20261109/daily-list`). Official moved 5 commits from 177a1dc465ff to 4708f409; in
`attach/`, `runtime/`, `vm/` and `daemon/` the two branches differ by upstream #709 only (17 lines,
the CUDA late path); the rest is upstream's `tools/bpftimetool` and documentation. The daily makes a
new `dev/uprobe-official-<SHA>` only when official moves, so this branch is left to development;
new commits reach the daily through its list, as before.

## Baseline (`regression/`, build-probecheck, the old dev worktree's settings: CUDA off, LTO, checks on)

08:26-08:29, as `gpu/fixes-20261109/verification/regression` without the CUDA part: every runtime
test case as root in a process of its own (`each.sh`): 282 cases, the 3 map usage counter cases
failing (no counters compiled in), the rest passed; `[kernel_user]` 2/2 and the daemon tests 10/10
as root; transformer 54 passed and the 1 expected failure; syscall trace 7/7; Frida attach 26/26 in
5 of 5 runs; the daily smoke: funclatency 8,284,000 calls counted exactly, memleak 9 outstanding,
a SIGUSR1 detach under load with the workload running to its end. Root-owned test segments removed
afterwards (`shm-cleanup.txt`). The same results as on 177a1dc465ff's af4a1a8d this morning.

The CUDA side of c115b33d (its build-cuda): `gpu/fixes-20261109/daily-list` (matrix 19/19 default,
10/19 late). The daily candidate gate on c115b33d: same place.
