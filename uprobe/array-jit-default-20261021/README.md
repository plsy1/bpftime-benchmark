# Automatic per-call-site array lookup JIT

LATEST STATUS (2026-10-21): this directory preserves the original regressed
default-on experiment. The user-approved repair is now applied to primary and
has passed correctness, short tests and full ten-round verification; full
hash delete is 36.64 ns, 0.34x same-run kernel. See
[formal repair results](../array-jit-default-delete-fix-20261021/RESULT.md) and
the current handoff at
`/home/jetson/src/summry/uprobe/bpftime-uprobe-handoff-20261021.md`.
Source repair is now committed/pushed as c0939bd on
codex/uprobe-official-8da867d541f8; no daily patch-stack update. Statements below about
unchanged primary or pending follow-up describe this earlier diagnostic stage.

Goal: remove the explicit enable switch, automatically inline eligible LLVM
ordinary/per-CPU array lookup sites, and retain original helpers elsewhere.
This is a compatibility/generalization change, not another claimed latency win.
Implementation is uncommitted and unpushed. Correctness and consuming short
tests passed. Full 35-case x 10 completed 13:58:44 JST with sample/SHA/default
activation checks passing, BUT ordinary hash delete slowed approximately 17.9%.
The isolated 12-pair AB/BA recheck reproduced +18.02% (37.0534 -> 43.7309 ns/op),
12/12 pairs slower. Candidate is NOT accepted as regression-free; see
hash-delete-recheck/RESULT.md. Follow-up isolated investigation localized a
GCC/LTO outlining change caused by the added bound-array erase caller. A
diagnostic-only direct array erase restores native inlining and 36.26 ns/op
(12 balanced ABC blocks); primary is unchanged and non-target performance on
that diagnostic build is not yet screened. See hash-delete-investigation/RESULT.md.

Default-on tests: 5 cases / 138 assertions passed both with the enable variable
absent and with its obsolete value 0. Main screen: 42 cases / 110422 assertions.
Host contract: 18 cases / 201 assertions. Standalone native VM passed without
runtime. All six balanced rounds passed checksums, frequency and binary hashes.

| Extra consuming workload | Previous INLINE ns | Current DEFAULT ns | Kernel ns | Current/kernel |
|---|---:|---:|---:|---:|
| Ordinary, all hits | 3.0950 | 3.0926 | 5.8852 | 0.53x |
| Ordinary, approximately half misses | 9.6531 | 8.4546 | 12.3200 | 0.69x |
| Per-CPU, all hits | 5.1479 | 5.1471 | 6.8556 | 0.75x |
| Per-CPU, approximately half misses | 11.5074 | 10.6503 | 12.7440 | 0.84x |

These are six-round medians in additional value-consuming diagnostic workloads,
not historical unused-result latency. All-hit results are stable; mixed-miss
improvements are not claimed as the purpose of this compatibility change.

## Implementation

- Runtime no longer reads BPFTIME_ARRAY_JIT_INLINE. Known standard lookup
  pointers are deferred until load and considered automatically for LLVM JIT.
- Each bound descriptor has explicit ordinary/per-CPU kind. Mixed arrays,
  hash maps, update/delete and unrelated helpers no longer disqualify a program.
  LLVM specializes individual constant-map call sites; unknown/non-array calls
  retain the exact original helper and its arguments.
- Both inlined hits and standard lookup/update/delete use the same owned array
  identity after FD close/reuse. Existing map-reference/mapping/fork protections
  remain. Known unbound handles avoid dynamic-binding acquisition attempts.
- Custom helpers are selected BEFORE compilation, not registered after code is
  already published. Their implementations are never silently bypassed.
- Missing/custom CPU helpers exclude per-CPU descriptors without excluding
  ordinary arrays. CPU querying, failure callbacks and bounds remain per lookup.
- Interpreter/AOT and unsupported relocation/backend/diagnostic cases retain
  original paths. Bound programs still reject signal/AOT use and inherited
  execution after fork. No new hotplug or preemption guarantees are claimed.
- LLVM source/header overlays are parent-owned checked patches; the official
  submodule is clean. CLI, examples, tests and wrapper must all compile with
  the generated descriptor header. Shared-memory map layouts are unchanged.

## Evidence plan

Default tests do NOT request opt-in and run with the enable variable absent.
They check mixed arrays/hash/update/custom unrelated calls with actual value
reads, pinned update identity after reuse, custom lookup/CPU fallback, per-CPU
mutations, equal-stride single-CPU descriptor kind, and AOT original helpers.
Existing CPU-injection/query-count/failure tests and lifetime/fork tests remain.

Consuming six-round balanced A/B compares the exact frozen previous INLINE
version (percpu-array-jit-direct-cpu-20261020/artifacts, flag=1) against current
DEFAULT-ON (no enable variable), and same-run kernel. Runtime keys, actual
values, checksums, hit/miss counts, artifact hashes and CPU frequency are
checked. All-hit and additional mixed-half-miss workloads are diagnostic tests,
not unchanged historical uprobe cases. Unused-result timings are NOT gain
metrics and must not be compared with historical C++ lookup rows.

Full 35-case x 10 suite starts in tmux only after correctness, standalone and
consuming <=3% non-regression gates. It freezes binaries, verifies all samples
and hashes, and audits default activation separately from timing samples.
Full summary retains eight historically comparable rows; the two array lookup
rows refer to the separate consuming tests rather than dead-result gains.

Sources: source/ and source-sha256.json, checked against primary and warm build.
Earlier failed/interrupted build attempts are retained with notes; they are
not successful test results. tmux: uprobe-array-jit-default-20261021.
