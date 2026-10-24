# Migrated clean-leaf full uprobe benchmark

Date: 2026-10-15

Official base: `8da867d541f8322aa7bc56c05727769c2aa0622b`

Candidate base commit: `d330894031a7c72c046400f985240f9b9bff87db`

The candidate contains the complete migrated optimization stack plus the
uncommitted per-CPU array clean production leaf. The run used MAXN_SUPER,
`jetson_clocks`, CPU 5, and 10 samples per benchmark case.

## Validation

- Exit status: 0.
- Duration: 1,924.09 seconds.
- Kernel cases: 17, all with 10 samples.
- Userspace cases: 17, all with 10 samples.
- Embedded cases: 1, with 10 samples.
- Focused runtime tests passed 24,550 assertions in 14 test cases before the run.
- CPU 5 remained locked at 1,728,000 kHz before and after the run.

## Ten-operation comparison

Times are average nanoseconds per benchmark invocation. Positive percentages
mean the current candidate is faster. `Initial` and `previous` use the older
research source base and are continuity checks; the scientifically matched
comparison for this migration is `official 8da` versus `current 8da`.

| Operation | Same-run kernel | Initial | Previous clean-leaf | Official 8da | Current 8da | vs official 8da | vs previous | Current / kernel |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| ordinary array lookup | 3,815 | 15,937 | 15,887 | 20,029 | 24,387 | -21.76% | -53.50% | 6.39x |
| ordinary array update | 13,213 | 23,510 | 23,446 | 25,459 | 32,272 | -26.76% | -37.64% | 2.44x |
| ordinary hash lookup | 28,273 | 58,473 | 57,032 | 52,398 | 63,766 | -21.70% | -11.81% | 2.26x |
| ordinary hash update | 92,990 | 63,182 | 63,720 | 61,997 | 67,979 | -9.65% | -6.68% | 0.73x |
| ordinary hash delete | 107,127 | 42,639 | 41,044 | 150,949 | 60,450 | +59.95% | -47.28% | 0.56x |
| per-CPU array lookup | 4,407 | 27,728 | 11,708 | 28,870 | 14,411 | +50.08% | -23.09% | 3.27x |
| per-CPU array update | 16,600 | 75,929 | 26,969 | 74,088 | 35,813 | +51.66% | -32.80% | 2.16x |
| per-CPU hash lookup | 29,231 | 141,245 | 63,688 | 124,995 | 72,355 | +42.11% | -13.61% | 2.48x |
| per-CPU hash update | 67,856 | 220,961 | 71,386 | 211,485 | 80,160 | +62.10% | -12.29% | 1.18x |
| per-CPU hash delete | 149,864 | 986,684 | 108,885 | 256,233 | 118,335 | +53.82% | -8.68% | 0.79x |

The official 8da delete numbers use the upstream workload, while the candidate
contains the migrated delete-hit setup fix, so the delete rows are not a pure
implementation-only comparison.

## Conclusion

The migrated stack still substantially improves all five per-CPU operations
relative to the matching official 8da baseline. Per-CPU array lookup improves
from 28,870 ns to 14,411 ns, but its ratio is 3.27x kernel rather than the
2.66x observed on the older research base. The current candidate is also
slower than official 8da for ordinary array lookup/update and ordinary hash
lookup/update.

The follow-up run built and measured unmodified `d330894` on the same official
8da base. Per-CPU array lookup was 15,923.76 ns without clean-leaf and
14,410.72 ns with clean-leaf, so the isolated improvement is 9.50%. This
reproduces the 9.48% matched A/B improvement measured on the older research
base.

| Operation | Without clean-leaf | With clean-leaf | Change |
|---|---:|---:|---:|
| per-CPU array lookup | 15,924 | 14,411 | +9.50% |
| ordinary array lookup | 24,169 | 24,387 | -0.90% |
| ordinary array update | 32,140 | 32,272 | -0.41% |
| per-CPU array update | 35,882 | 35,813 | +0.19% |

The clean-leaf selection is restricted to lookup programs whose maps are all
per-CPU arrays. Movements in unrelated hash cases are therefore cross-run
variation, not reachable effects of this patch. The same-base result supports
accepting and committing clean-leaf. The broader ordinary-map regressions
relative to official 8da belong to the complete migrated patch stack and
should be investigated separately.
