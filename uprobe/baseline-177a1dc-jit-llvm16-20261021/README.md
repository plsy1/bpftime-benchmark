# 177a1dc development baseline (array JIT, LLVM 16)

New reference point for further uprobe map optimization on
`codex/uprobe-official-177a1dc465ff` at `5466f0d10db3` (official `177a1dc` +
the 41-commit daily patch stack, ending with the default ordinary/per-CPU
array lookup JIT and the test-only fix `2eea622`). Future "previous" numbers
should come from this directory, not from the 8da867d541f8 experiments.

Run 2026-10-21 20:37–21:11 JST, serially in tmux session
`uprobe-baseline-177a1dc-20261021` (`run-all.sh`), no overlap with the daily
timer. Exit status 0 for every stage.

## Setup

- Source: detached clean worktree `/home/jetson/src/bpftime-baseline-177a1dc-20261021`
  at `5466f0d`; `vm/llvm-jit` clean at `87bac39`; see `source-state.txt`.
- Build `build-jit`: GCC 13, LLVM 16 (`/usr/lib/llvm-16/cmake`), BPF clang 15,
  RelWithDebInfo, LTO ON, probe read/write checks OFF, verifier OFF, daemon/AOT CLI OFF.
  All recorded CMake options are identical to the archived
  `array-jit-default-delete-fix-20261021` build (`cmake-options.diff` is empty).
- Workloads rebuilt from this tree: `benchmark/test`, `benchmark/uprobe/uprobe`
  with the daily tracker's no-BTF `vmlinux.h`, and both consuming array
  workloads with the archived `build-diagnostic.sh` commands.
- Measured runtime artifacts are frozen copies in `artifacts/`; hashes for
  them and all workloads are in `artifact-sha256.txt` and were rechecked before
  and after the full run.
- MAXN_SUPER, CPU 5 fixed at 1.728 GHz (checked before every consuming process
  and before/after the full run).

## Results

| Check | Result |
|---|---|
| Correctness, complete daily filter (`test-filter.txt`) | 66 cases / 128293 assertions passed |
| Ordinary consuming, 6 rounds, kernel / c0939bd / 5466f0d | all checksums and hit/miss counts passed |
| Per-CPU consuming, 6 rounds | all checksums and hit/miss counts passed |
| Full uprobe suite | 17 kernel + 17 userspace + 1 embedded, 10 samples each, 1484.51 s |
| Default activation audit | passed, 17 cases, inline logs present, no enable variable |

See `full10/ten-operation-comparison.md`. Eight historically comparable
operations remain below same-run kernel (0.34x–0.95x), all within ±1% of the
8da867d541f8 c0939bd full run. In the consuming runs the frozen c0939bd
artifacts were measured as a third arm in the same run; the 177a1dc build is
within +0.2% on every case (ordinary hit 3.10 ns, 0.53x kernel; mixed 8.46 ns,
0.69x; per-CPU hit 5.18 ns, 0.76x; mixed 10.65 ns, 0.83x). The move to the
newer official base therefore did not change performance.

The historical array lookup rows do not consume the returned value and stay
excluded from gain claims; use the consuming tables.

Publication: scripts, logs, CMake option records, hashes, consuming
raw/summary/environment data and the full ten-round JSON/table. `artifacts/`
(340 MB of binaries) and the runner symlinks remain local only.
