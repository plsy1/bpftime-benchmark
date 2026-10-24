# per-CPU array lookup clean production leaf

Date: 2026-10-14

Source baseline commit: `b9fee864bf542b07cf83eb5c990201bd830519f5`

Candidate build: `build-uprobe-percpu-array-clean-leaf-opt-20261014`

## Conclusion

The load-time-specialized per-CPU array lookup helper now calls a production
leaf that does not inspect the diagnostic `BPFTIME_MAP_PRODUCTION_AB` mode on
every lookup. Diagnostic modes still use the generic helper, so their behavior
is preserved.

The matched 5-round A/B test reduced the median per-CPU array lookup cost from
11.7707 ns/helper to 10.6546 ns/helper, a 9.48% reduction. Ordinary array
lookup changed by -0.70%; ordinary array update and per-CPU array update
improved by 0.25% and 0.60%, respectively. These control movements are treated
as measurement noise.

The independent full benchmark improved per-CPU array lookup from the previous
12,908 ns/invocation to 11,708 ns/invocation, a 9.30% reduction. Relative to
the fixed initial unoptimized result, the cumulative reduction is 57.78%.
Each map benchmark invocation performs 1,000 helper calls.

## Full 10-operation comparison

Positive percentages mean faster. Times are average ns/invocation from the
10-run full benchmark.

| Operation | Same-run Kernel | Initial BPFtime | Previous BPFtime | Current BPFtime | Initial to current | Previous to current | Current / Kernel |
|---|---:|---:|---:|---:|---:|---:|---:|
| ordinary array lookup | 3,762 | 15,937 | 15,809 | 15,887 | +0.32% | -0.49% | 4.22x |
| ordinary array update | 13,105 | 23,510 | 23,498 | 23,446 | +0.27% | +0.22% | 1.79x |
| ordinary hash lookup | 28,388 | 58,473 | 56,865 | 57,032 | +2.46% | -0.29% | 2.01x |
| ordinary hash update | 94,248 | 63,182 | 62,492 | 63,720 | -0.85% | -1.96% | 0.68x |
| ordinary hash delete | 107,266 | 42,639 | 42,137 | 41,044 | +3.74% | +2.59% | 0.38x |
| per-CPU array lookup | 4,406 | 27,728 | 12,908 | 11,708 | +57.78% | +9.30% | 2.66x |
| per-CPU array update | 16,604 | 75,929 | 27,134 | 26,969 | +64.48% | +0.61% | 1.62x |
| per-CPU hash lookup | 29,309 | 141,245 | 61,806 | 63,688 | +54.91% | -3.04% | 2.17x |
| per-CPU hash update | 67,811 | 220,961 | 72,168 | 71,386 | +67.69% | +1.08% | 1.05x |
| per-CPU hash delete | 151,643 | 986,684 | 109,595 | 108,885 | +88.96% | +0.65% | 0.72x |

Initial baseline:
`uprobe-top-arm64-20261007-strict-repro-official-no-btf`, commit `1a1dc0d`.

Previous accepted result:
`percpu-array-load-specialization-full-regression-arm64-20261014`, commit
`b9fee86`.

The current raw per-CPU array lookup ratio is 2.66x kernel. After subtracting
the same-run empty-uprobe overhead from both sides, the helper-path ratio is
3.59x (11,389 ns BPFtime versus 3,176 ns kernel per invocation).

The per-CPU hash lookup cross-run change is -3.04%, but this patch is gated to
programs whose lookup maps are exclusively per-CPU arrays and cannot select
the new leaf for a per-CPU hash map. It is therefore recorded as cross-run
variation, not an effect of the candidate path.

## Validation

- Full benchmark status: success; duration 1,665.12 seconds.
- Full benchmark groups: 17 kernel, 17 userspace, and 1 embedded case.
- Every full benchmark case contains 10 raw samples.
- All 31 recorded CPU-frequency samples stayed at 1,728,000 kHz.
- `[percpu-array]`: 1,761 assertions in 2 test cases passed.
- `test_arraymap_percpu*`: 16,011 assertions in 2 test cases passed.
- `git diff --check` passed.

