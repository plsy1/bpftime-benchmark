# Per-CPU array lookup: address-dependency experiment

Accepted baseline: `fefe2f6edf276e1b2957711203b5022dd1be8d7c`.
Frozen builds: `/tmp/bpftime-percpu-array-address-20261019.S1mHZP`.

This is separate from the rejected cold-error outlining experiment. Leave
error handling inline. Validate the key and compute its CPU-0 entry base
and value width before querying the current CPU; after the query return
`entry_base + cpu * value_size`. Keep only base and width live across the
query instead of the map/key and dependent metadata loads afterward.

Each lookup still queries CPU once, including invalid/null keys. There are
separate source call sites for valid and invalid paths, not multiple queries
on one execution. Invalid paths query CPU before setting ENOENT so that
query-side errno changes cannot replace the required final error. The order
of key validation relative to the CPU query intentionally changes; CPU ID is
never cached, and storage layout/checks/fallback/return values are unchanged.

Correctness: use the real runtime's affinity and error tests plus an isolated
standalone test that replaces only its own `my_sched_getcpu` with a counting
stub. It switches simulated CPUs between calls, checks both lookup methods,
null/out-of-range keys, four value widths and errno behavior, including a
query that changes errno. The stub is never built into production binaries.

Use GCC 13, LTO on, diagnostics/probe checks off, CPU 5 at 1728000 kHz.
Freeze accepted baseline and candidate; run correctness and inspect generated
assembly before six balanced array A/B rounds. Require >=2% target median
improvement, >=5/6 paired wins and <=3% median regressions elsewhere. Then
run six hash rounds with the same <=3% gate for all operations. Only then
start the complete ten-round suite in tmux. A failed gate stops for review.

Short tests subtract control overhead; complete ten-operation tables use raw
average ns/invocation divided by 1000 and compare the original baseline,
previous accepted version and same-run kernel. No candidate commit is made
before review of results.

Decision: **all three variants rejected** on 2026-10-19. Active phase is
`restore-baseline` (see `active-phase.txt`). All experimental source/test
changes are reverted; rebuilding and retesting accepted `fefe2f6` is tracked
in `restore-*` artifacts. Each rejected phase retains its frozen libraries,
source snapshots, logs and status files; `pipeline-exit-status.txt` is only
the first phase's status, not an infrastructure failure of the entire task.

## Correctness gates

The independent counting-stub test passes 10,248 assertions in one case.
Both public lookup paths perform exactly one CPU query for successful,
out-of-range and null-key cases; simulated CPU changes and query-side errno
changes produce the expected results. This test is never linked or preloaded
into the production agent/server.

The current production runtime's selected array/hash tests pass 106,610
assertions in 17 cases, including actual affinity changes and errno checks.
All four production/test targets build successfully.

The first candidate's assembly shows an important tradeoff: metadata reads
move before CPU selection, but GCC reassociates the address expression and
keeps four callee-saved registers rather than two (48-byte versus 32-byte
frame). Therefore the intended full two-value live-range reduction has not
been achieved by source spelling alone. Measure the frozen build rather
than claiming this is already a successful optimization.

## First spelling rejected

| Operation | Baseline median ns/helper | First candidate median ns/helper | Change |
|---|---:|---:|---:|
| Ordinary array lookup | 6.4633 | 6.4577 | -0.09% |
| Ordinary array update | 11.3151 | 11.3245 | +0.08% |
| Per-CPU array lookup | 9.7239 | 11.2273 | **+15.46%** |
| Per-CPU array update | 14.0753 | 14.1580 | +0.59% |

All six target pairs lose. Hash and full ten-round testing were not started
for this phase. More callee-saved registers and a larger stack frame are
observed, but the exact contribution of spills versus layout/scheduling is
not isolated by these measurements.

## Second spelling: explicit CPU-slot function boundary

Use a private static, non-inlined `lookup_cpu_slot(entry_base, cpu_stride)`
method so that the selected key's base address must be a completed argument
before the slot function queries CPU. No class fields or shared-memory layout
change; CPU selection and both lookup error paths retain their results and
one query per invocation. The specialized helper should tail-branch to this
leaf, not add an ordinary call/return pair. Verify this and the leaf's two
saved values in assembly before comparing timing.

The independent query-count/slot/errno test also passes for this spelling
(10,248 assertions). Its test-only selector is not part of production code.
The same accepted baseline, six-round balance, frequency and timing gates
are used, with no rejected cold-error outlining added to the candidate.

## Second spelling rejected; final combination

The second spelling passes both independent query-count tests and production
correctness tests, and actually produces the intended two-value CPU-slot leaf.
It still adds a 16-byte outer helper frame before tail-branching to the
32-byte CPU-slot leaf. In six balanced pairs, target median increases from
9.70288 to 11.85975 ns/helper (+22.23%, six losses). No hash or full testing
starts for it. Other median changes: ordinary lookup +0.22%, ordinary update
-0.29%, per-CPU array update -0.56%.

The final `frameless-slot` spelling tests that remaining overhead explicitly.
Keep the same non-inlined CPU-slot leaf but move invalid-key CPU query/errno
handling to a private cold function, and reuse the already accepted array
ENOENT helper for invalid handles. Thus the outer adapter should contain no
stack frame/calls and only tail-branches. This is a separately measured
combination, not a claim that previously rejected cold outlining works alone.
Neither shared-memory fields nor CPU-query count/return semantics change.
All earlier phase binaries remain frozen. Require the frameless outer helper
in assembly and repeat the same correctness and timing gates. If this final
spelling also lacks stable benefit, restore the accepted baseline rather
than continue adding transformations to this experiment.

## Final spelling rejected; accepted baseline restored in source

The third spelling passes 10,248 independent query-count assertions and
106,610 production assertions in 17 array/hash cases. Assembly confirms
the outer specialized helper has no stack references or calls and tail-
branches to the two-value CPU-slot function. Nevertheless the target still
loses all six paired timings: 9.69785 to 10.93678 ns/helper (+12.78%).
Other median changes are ordinary lookup +0.35%, ordinary update -0.37%,
and per-CPU array update -0.60%. All binary/frequency guards pass.

| Variant | Target old median ns/helper | Target new median ns/helper | Change | Paired wins |
|---|---:|---:|---:|---:|
| Inline address preparation | 9.72388 | 11.22726 | +15.46% | 0/6 |
| Explicit CPU-slot function | 9.70288 | 11.85975 | +22.23% | 0/6 |
| Frameless outer adapter + slot function | 9.69785 | 10.93678 | +12.78% | 0/6 |

All three phase exit statuses are 1 because the predeclared performance
gate rejects the target regressions; correctness and short-measurement
harnesses themselves succeed. No hash screen or full ten-round benchmark
is started for any variant. No new production/kernel ratio is claimed from
control-subtracted short-test data. The accepted ten-operation results remain
the [previous full suite](../ordinary-array-lookup-cold-20261019/full10/ten-operation-comparison.md):
ordinary array lookup 7.47 ns/helper (1.96x kernel) and per-CPU array lookup
10.76 ns/helper (2.45x kernel), eight of ten maps faster than kernel.

The tests establish that these three concrete C++ rewrites are slower, not
that all possible implementation-level optimizations are exhausted. Metadata
movement, arithmetic reassociation, stack costs and code layout vary; their
individual causal contributions are not isolated by this screen. Do not
assume fewer post-query instructions must mean a shorter total critical path.
No architecture, CPU caching, map layout or synchronization change is used.

All four runtime/source test files return byte-for-byte to accepted HEAD.
`restore-baseline.sh` restores the exact frozen production libraries/tool
before rebuilding all targets and re-running baseline tests. The existing
unrelated benchmark diagnostic edits are preserved. These rejected variants
are not committed, pushed, or added to the daily patch list.
