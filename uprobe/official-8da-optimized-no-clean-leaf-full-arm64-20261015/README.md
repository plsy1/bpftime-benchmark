# Official 8da optimized candidate without clean-leaf

Date: 2026-10-15

Commit: `d330894031a7c72c046400f985240f9b9bff87db`

This is the same-base control run for the migrated per-CPU array clean-leaf
experiment. It contains the migrated optimization stack but not the four-file
clean-leaf working-tree change.

## Validation

- Exit status: 0.
- Benchmark duration: 1,938.45 seconds.
- Kernel cases: 17, all with 10 samples.
- Userspace cases: 17, all with 10 samples.
- Embedded cases: 1, with 10 samples.
- Focused runtime tests passed 24,550 assertions in 14 test cases.
- CPU 5 remained locked at 1,728,000 kHz before and after the run.

## Clean-leaf isolation

| Operation | Without clean-leaf | With clean-leaf | Change |
|---|---:|---:|---:|
| per-CPU array lookup | 15,924 | 14,411 | +9.50% |
| ordinary array lookup | 24,169 | 24,387 | -0.90% |
| ordinary array update | 32,140 | 32,272 | -0.41% |
| per-CPU array update | 35,882 | 35,813 | +0.19% |

The target improvement reproduces the older-base matched A/B result of 9.48%.
This supports accepting clean-leaf. Unrelated map movements are cross-run
variation because those programs cannot select the specialized clean-leaf
helper.
