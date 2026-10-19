# Ordinary array lookup cold-error-path experiment

Accepted baseline: `eb09b972d59ca7845e3e1251a2ad96845b93feaf`.
Frozen builds: `/tmp/bpftime-array-lookup-cold-20261019.8N6hqp`.

Accepted on 2026-10-19 as source commit
`fefe2f6edf276e1b2957711203b5022dd1be8d7c`. The daily workflow includes this
commit in its explicit replay list and selects the `[array-lookup-cold]`
regression test. The accepted source matches the frozen full-run snapshot.

Move ordinary-array ENOENT handling into one cold, non-inlined C++ function,
used by both the specialized helper's invalid-handler checks and the array
leaf's out-of-bounds check. Keep all original checks, fallback dispatch,
return values, errno behavior, map layout and CPU-selection semantics.
Do not add null-key checks to ordinary array lookup (not part of this change).

The accepted baseline's ordinary helper already has its array leaf inlined,
but saves/restores a stack frame and a callee-saved register on successful
lookups because it also contains an error-only errno call. First verify
whether error outlining removes these instructions from the actual production
helper; do not infer success from C++ source alone.

Correctness: new array lookup tests cover first/last keys, invalid keys,
multiple value sizes, direct value access and unchanged errno on success.
Run the current runtime's array/hash tests before measuring. Freeze binaries
and source snapshots before the six-round alternating production A/B.

If ordinary lookup median improves by at least 2%, with at least five of six
paired wins and no >3% median regression in other array operations, run a
six-round hash regression screen. Known ordinary hash-update bimodality is
reported separately; the full-run preflight still holds for manual review
if even that operation's median increases by more than 3%.
Only then start the full ten-round suite. Every benchmark is pinned to CPU 5
at 1728000 kHz with GCC 13, LTO on, diagnostics and probe checks off.

The short A/B subtracts control overhead. Full-suite tables use raw average
ns per invocation divided by 1000 and retain the initial baseline, previous
accepted version and same-run kernel comparison.

## Build and correctness result

The agent, syscall server, management tool and current runtime tests compiled
successfully. The selected array/hash tests passed 106,562 assertions in 17
test cases, including the new ordinary-array cold-path regression test.

Production assembly confirms the specialized ordinary-array helper shrinks
from 64 to 47 static instructions (including error blocks). Its hot function
contains no stack references, no calls and no inline errno handling. The
baseline has a 32-byte stack frame, callee-saved register spills and two
errno calls. Invalid cases now tail-branch to the shared cold error function;
the generic map-type fallback remains present. Static instruction counts are
not a substitute for measured timing and do not count only the executed path.

## Six-round array A/B

| Operation | Baseline median ns/helper | Candidate median ns/helper | Change |
|---|---:|---:|---:|
| Ordinary array lookup | 6.9296 | 6.4616 | -6.75% |
| Ordinary array update | 11.3391 | 11.3566 | +0.15% |
| Per-CPU array lookup | 9.8766 | 9.6765 | -2.03% |
| Per-CPU array update | 14.1574 | 14.2127 | +0.39% |

The target improves in all six paired rounds. The other array medians pass
the regression screen, but their small differences remain explicitly recorded;
passing a 3% gate does not establish the absence of smaller side effects.
The per-CPU lookup source is unchanged, so its apparent gain is not evidence
of a direct CPU-selection or map-algorithm improvement. Frozen binaries and
frequency checks passed. The hash screen runs next before full validation.

## Six-round hash screen and full-suite launch

| Operation | Baseline median ns/helper | Candidate median ns/helper | Change |
|---|---:|---:|---:|
| Ordinary hash lookup | 26.0051 | 26.1858 | +0.69% |
| Ordinary hash update | 34.2866 | 34.5816 | +0.86% |
| Per-CPU hash lookup | 26.7991 | 26.7668 | -0.12% |
| Per-CPU hash update | 44.3340 | 44.3095 | -0.06% |

All four operations passed the final <=3% median-increase gate, including
ordinary hash update. Smaller differences are retained for full-suite review;
this screen alone is not a claim of zero regression.

The complete ten-round suite started at 2026-10-19 00:35:56 JST in tmux
session `uprobe-array-lookup-cold-20261019`. It runs both kernel and userspace
cases, followed by embedded execution, using the frozen candidate libraries.
At launch the kernel loader reported successful startup. The following result
was reviewed before accepting and committing the candidate.

## Complete ten-round result and acceptance

Completed successfully at 2026-10-19 01:00:53 JST in 1495.71 seconds
(approximately 25 minutes). Pipeline and full-run exit statuses are both 0.
The independent review verifies 17 kernel cases, 17 userspace cases and one
embedded case, with ten finite positive samples each. Frozen agent and syscall
server SHA-256 checks pass; the committed four source files match the tested
snapshot exactly.

Ordinary array lookup falls from 7.93049 to 7.46729 ns/helper (-5.84%). Its
same-run kernel cost is approximately 3.81 ns/helper, leaving a 1.96x ratio
(previously 2.09x). Current samples span 7.45815–7.49093 ns; the previous
accepted samples span 7.91859–7.94808 ns. Every new sample is below every
previous sample, corroborating the six-round matched A/B benefit.

The other map operations have no observed increase above approximately 0.3%
in this full run; smaller side effects are not ruled out. Per-CPU array lookup
is 10.76 ns/helper (2.45x kernel). Eight of ten map operations remain below
same-run kernel; only the two array lookups remain slower. No map design,
CPU selection or synchronization semantics were changed.

See [the complete ten-operation comparison](full10/ten-operation-comparison.md)
and [the full benchmark report](full10/results.md). The table includes the
original unoptimized baseline, its kernel ratio, the immediately preceding
accepted version, and current same-run kernel results.

Acceptance-time rerun of `[array-lookup-cold],[percpu-array]` also passed
(6,103 assertions in four tagged test cases); the earlier full selected
array/hash correctness run passed 106,562 assertions in 17 cases.
