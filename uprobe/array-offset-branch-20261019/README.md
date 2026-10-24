# Local array implementation offset-pointer decoding experiment

Baseline: `fefe2f6edf276e1b2957711203b5022dd1be8d7c`.
Immutable accepted baseline: `/tmp/bpftime-array-lookup-cold-20261019.8N6hqp/cold-errors`.
Candidate snapshot: `/tmp/bpftime-array-offset-branch-20261019.z1JKDF/branch`.

The preceding load-order candidate was not adopted (six paired target wins,
1.12% median gain, below its 2% screening threshold). Its source and tests are
archived separately. Its production leaf change has been reverted exactly.

This independent candidate adds a local `offset_ptr_get_branch` template,
using Boost's public bool/get interfaces rather than hardcoded offsets or
manual integer/pointer reconstruction. Null returns nullptr; non-null uses
Boost get(). Only `bpf_map_handler::map_lookup_array` selects this accessor.
No changes to Boost, layout, allocation, caches, fd/type/key checks, capacity
source, errno behavior or per-CPU implementation. No inline assembly or
nonnull assumptions. Existing null-implementation dereference preconditions
are not expanded into a new error policy.

Inspect actual production helper assembly first. A meaningful change should
remove the map_impl pointer's csetm/and dependency, keep helper success paths
call-free and frameless and retain original checks/fallback. If the compiler
folds away the null branch after inlining into a dereference, record that
explicitly; the standalone accessor must still return nullptr for null.

Tests cover null, pointers before/after the offset pointer, a self pointer,
copy and assignment, unchanged errno, and rebasing through two simultaneously
mapped views of the same shared-memory segment. Existing array/hash tests
run before performance measurement.

tmux: `uprobe-array-offset-branch-20261019`. Six-round balanced array A/B,
fixed CPU 5 / 1728000 kHz, GCC 13 / LTO on / checks off. Gate: >=2% target
median gain and >=5/6 paired wins, other array medians <=3% regression. Only
passing candidates enter a hash regression screen; full ten-round acceptance
requires separate review. Short timings are control-subtracted and are not
the raw same-run-kernel ten-operation table.

## Short-screen review

Both suites completed successfully, with 18 test cases / 106,592 assertions
passing before timing. Control-subtracted six-round medians:

| Operation | Baseline ns/helper | Candidate ns/helper | Candidate change |
| --- | ---: | ---: | ---: |
| Ordinary array lookup | 6.46245 | 5.77077 | -10.70% |
| Ordinary array update | 11.35782 | 11.32465 | -0.29% |
| Per-CPU array lookup | 9.69197 | 9.73490 | +0.44% |
| Per-CPU array update | 14.21247 | 14.17203 | -0.28% |
| Ordinary hash lookup | 26.27126 | 26.33529 | +0.24% |
| Ordinary hash update | 35.59631 | 35.65015 | +0.15% |
| Per-CPU hash lookup | 26.73541 | 26.74744 | +0.04% |
| Per-CPU hash update | 44.31065 | 44.24606 | -0.15% |

The target wins all six paired rounds and passes the >=2% gate. All screened
other operations remain below the 3% regression gate. Small changes are not
declared zero regression; delete operations are not covered by this short
harness and must be checked in the full suite.

Actual assembly removes one csetm/and pair (three -> two each). It still has
no calls or stack references. It adds a predicted null branch and a cold
null-dereference trap block; a null implementation was already invalid for
calling the array leaf, while the standalone accessor's null result remains
nullptr and is directly tested. Static helper size rises from 47 to 50
instructions including that cold block; fewer total instructions is not the
reason for the measured gain. fd/type/key bounds and fallback are retained.

The full ten-round suite is scheduled in the `full10` tmux window of the same
session. It first verifies source and frozen binaries and rebuilds the embedded
executable, then runs kernel/userspace/embed cases and generates the ten-item
comparison against the original and immediately previous accepted baselines.
This candidate is not yet accepted, committed, pushed, or added to daily replay.

## Full validation and precommit recheck

Full ten rounds completed successfully at 2026-10-19 21:46:00 JST in 1490.35 s.
All 17 kernel, 17 userspace and one embedded cases have ten finite positive
samples; frozen-artifact checks passed. Ordinary array lookup: previous
7.46729008 -> 6.767880288 ns/helper (-9.37%), same-run kernel approximately
3.74 ns/helper, ratio 1.81x (previous 1.96x). All ten new target samples are
below all ten previous samples. Eight of ten operations remain faster than
same-run kernel. The full table remains in
[full10/ten-operation-comparison.md](full10/ten-operation-comparison.md).

Per-CPU array update's full-run mean increased 14.40442864 -> 14.527824064
ns/helper (+0.86%); its median changed 14.38635136 -> 14.44771696 (+0.43%).
This prompted an independent precommit recheck rather than declaring noise.
No production code or binaries were changed or rebuilt for that recheck.

Precommit recheck: six paired rounds, 100,000 invocations per measured case
(twice the original short-screen length), 3,000 warmup, same CPU/frequency.
The archived driver reverses the original screen's initial build ordering,
then alternates it; physical baseline/candidate labels remain unchanged.
Both builds and all user-owned diagnostic files passed before/after checks.

| Operation | Baseline net ns | Candidate net ns | Median change | Paired wins |
| --- | ---: | ---: | ---: | ---: |
| Ordinary array lookup | 6.469169 | 5.776494 | -10.707% | 6/6 |
| Ordinary array update | 11.331768 | 11.334420 | +0.023% | 4/6 |
| Per-CPU array lookup | 9.696309 | 9.713408 | +0.176% | 3/6 |
| Per-CPU array update | 14.172411 | 14.188028 | +0.110% | 3/6 |

Raw (not control-subtracted) median changes are respectively -9.230%,
+0.057%, +0.199%, and +0.125%, supporting the same interpretation. The target
benefit reproduces; the approximately 0.9% per-CPU update slowdown does not
reproduce consistently across the two short screens and this full run. This
does not establish exactly zero regression or exclude smaller effects.

Review recommendation: the candidate is suitable to retain; independent
precommit recheck passed with no observed persistent per-CPU update regression
of the previously suspected magnitude. No commit, push, or daily replay-list
update has yet been performed. See [recheck-summary.md](recheck-summary.md)
and the complete raw data in `precommit-recheck-ab6/`. Recheck figures do not
replace or selectively rewrite the completed full-ten comparison table.
