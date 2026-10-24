# Ordinary array lookup specialization — full uprobe benchmark

Date: 2026-10-15

Base commit: `05eac852dc7b61bd4d8b041eaa737375dcf57dc7`

This run measures the uncommitted ordinary-array lookup helper specialization
on top of the accepted official-8da optimization stack. The benchmark used
MAXN_SUPER, `jetson_clocks`, CPU 5, and 10 samples per case.

## Validation

- Exit status: 0.
- Duration: 1,920.65 seconds.
- All kernel, userspace, and embedded cases contain 10 finite samples.
- CPU 5 remained locked at 1,728,000 kHz before and after the run.
- The focused A/B runtime suite passed 17,917 assertions in 5 test cases.
- The matched A/B result for ordinary array lookup was 23.43 to 9.72
  ns/helper, a 58.51% reduction.

## Ten-operation comparison

Times are average nanoseconds per benchmark invocation. `Previous` is the
immediately preceding clean-leaf full run and is the regression check. A
positive value in `vs previous` means the candidate is faster.

| Operation | Initial | Previous | Current | Same-run kernel | vs previous | Current / kernel |
|---|---:|---:|---:|---:|---:|---:|
| ordinary array lookup | 15,937 | 24,387 | 10,715 | 3,793 | +56.06% | 2.82x |
| ordinary array update | 23,510 | 32,272 | 31,956 | 13,209 | +0.98% | 2.42x |
| ordinary hash lookup | 58,473 | 63,766 | 63,803 | 28,629 | -0.06% | 2.23x |
| ordinary hash update | 63,182 | 67,979 | 70,389 | 94,287 | -3.55% | 0.75x |
| ordinary hash delete | 42,639 | 60,450 | 60,445 | 106,623 | +0.01% | 0.57x |
| per-CPU array lookup | 27,728 | 14,411 | 14,353 | 4,407 | +0.40% | 3.26x |
| per-CPU array update | 75,929 | 35,813 | 35,686 | 16,582 | +0.36% | 2.15x |
| per-CPU hash lookup | 141,245 | 72,355 | 72,347 | 29,384 | +0.01% | 2.46x |
| per-CPU hash update | 220,961 | 80,160 | 79,999 | 68,449 | +0.20% | 1.17x |
| per-CPU hash delete | 986,684 | 118,335 | 117,380 | 152,311 | +0.81% | 0.77x |

## Conclusion

The full run confirms the focused A/B result: ordinary array lookup improves
by 56.06% relative to the immediately preceding full run and moves from 6.39x
kernel to 2.82x kernel. The patch does not make bpftime faster than kernel for
this operation, but removes most of the bpftime-specific generic helper
dispatch overhead.

All other lookup/update/delete paths are unchanged by construction. Their
full-run movements are cross-run variation; the 3.55% ordinary-hash-update
movement is not reachable from the new array-only helper selection. The
matched A/B run is the regression authority and showed unrelated movements
within 0.3%.
