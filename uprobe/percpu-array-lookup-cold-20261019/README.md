# Per-CPU array lookup: cold-error-path-only experiment

Accepted baseline: `fefe2f6edf276e1b2957711203b5022dd1be8d7c`.
Frozen builds: `/tmp/bpftime-percpu-array-cold-20261019.YHrmOV`.

Decision: **rejected** after the six-round array screen on 2026-10-19.
The candidate is a repeatable slowdown, not an accepted optimization.
All four experimental source/test files have been restored to the accepted
`fefe2f6` baseline; candidate snapshots and measurements remain archived here.
Baseline rebuild and validation are recorded separately by `restore-baseline.sh`.

Outline only ENOENT handling in the specialized per-CPU array helper and
its production lookup leaf. Leave all checks, lookup-time current-CPU query
order/count, CPU-slot addressing, map layout and fallback dispatch unchanged.
No CPU caching, address precomputation or update-path change is included.
Extend existing affinity-change tests with success/error errno assertions.

Unlike ordinary array lookup, the successful per-CPU path still must call
`sched_getcpu`. Therefore cold-error outlining is not expected to eliminate
its entire stack frame; verify actual code generation before measuring.

Use GCC 13, LTO on, diagnostics/probe checks off, CPU 5 pinned at 1728000 kHz.
Freeze the previous accepted binaries and new candidate, run current array/hash
correctness tests, inspect both array helpers, then run six balanced A/B rounds.
All timing gates use control-subtracted helper cost, not raw full-suite cost.

Only proceed if the target median improves >=2% with >=5 of 6 paired wins
and no other array median regresses >3%. Then run a six-round hash screen;
all four hash median changes must also remain <=3%, including the known
bimodal ordinary hash update case (otherwise hold for manual review).
Only after those gates start the full ten-round benchmark in the same tmux
session. A failed gate stops the pipeline, without committing or accepting
the candidate. The full ten-operation table retains the original baseline,
previous accepted version and same-run kernel results.

## Build, correctness and assembly gate

The agent, syscall server, tool and current runtime tests built successfully.
The selected array/hash tests passed 106,610 assertions in 17 cases, including
CPU-affinity changes, null/out-of-range keys, value sizes and success/error
errno checks. No CPU query or address computation was moved in the source.

The specialized per-CPU helper shrinks from 69 to 60 static instructions
(including error blocks). Inline errno calls are gone; the normal lookup
still calls `sched_getcpu` exactly once and still uses a stack frame. The
static count includes cold/error paths and is not an executed-instruction
count or timing result. GCC merges the identical cold errno functions, so
the tail-branch may name the existing ordinary-array error function.

The ordinary-array helper remains 47 static instructions with no stack
references or calls. Frozen source checksums for both ordinary-array files
and the hash implementation files pass. The six-round production array A/B
starts only after these gates.

Before launch, one old global runtime shared-memory object had no users
according to `sudo fuser`; it was removed with the frozen management tool.
Historical experiment results were not removed or modified.

## Six-round result: rejected before long testing

| Operation | Baseline median ns/helper | Candidate median ns/helper | Change |
|---|---:|---:|---:|
| Ordinary array lookup | 6.4638 | 6.4653 | +0.02% |
| Ordinary array update | 11.3154 | 11.3427 | +0.24% |
| Per-CPU array lookup | 9.7113 | 10.1829 | **+4.86%** |
| Per-CPU array update | 14.1065 | 14.1816 | +0.53% |

The target loses all six paired comparisons. CPU-frequency and frozen-binary
checks pass. The pipeline exits 1 because the performance gate correctly
rejects this regression, not because of a build, correctness or harness error.
No hash screen or full ten-round suite was started; do not derive a new
production/kernel ratio by mixing these control-subtracted short-test numbers
with the preceding full-suite measurements.

The accepted full-suite baselines therefore remain unchanged: ordinary array
lookup 7.47 ns/helper (1.96x same-run kernel), per-CPU array lookup 10.76
ns/helper (2.45x kernel). See the [accepted ten-operation table](../ordinary-array-lookup-cold-20261019/full10/ten-operation-comparison.md).

Although cold outlining reduces static code size and some branches, the
normal per-CPU CPU-query call and stack usage remain. Different code layout
and instruction scheduling are visible, but the measurements alone do not
establish which microarchitectural effect causes the slowdown. No padding,
CPU caching, skipped checks or unrelated optimization is added to rescue
this candidate. A CPU-query/address-dependency experiment would be a separate
future change, not part of this rejected single-variable experiment.

## Accepted baseline restoration complete

Restore finished successfully at 2026-10-19 12:18:21 JST (exit 0). All four
targets were rebuilt from accepted source `fefe2f6`, and the restored
array/hash checks passed 106,562 assertions in 17 cases. Agent and syscall
server binaries have the exact same SHA-256 values as the frozen accepted
baseline. `git diff --exit-code -- runtime` passes; only pre-existing benchmark
diagnostic edits remain in the source worktree. The rejected candidate was
not committed, pushed or added to the daily replay stack.
