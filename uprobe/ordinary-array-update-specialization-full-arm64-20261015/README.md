# Ordinary array update specialization — full uprobe benchmark

Date: 2026-10-15

Base commit: `bc5150e5fed5e7903f276025577c644534de2e86`

This run measures the uncommitted ordinary-array update helper specialization
on top of the accepted ordinary-array lookup optimization. The benchmark used
MAXN_SUPER, `jetson_clocks`, CPU 5, and 10 samples per case.

## Validation

- Exit status: 0.
- Duration: 1,896.58 seconds.
- All kernel, userspace, and embedded cases contain 10 finite samples.
- CPU 5 remained locked at 1,728,000 kHz before and after the run.
- The focused runtime suite passed 17,917 assertions in 5 test cases.
- The matched A/B result for ordinary array update was 31.63 to 15.86
  ns/helper, a 49.86% reduction.

## Ten-operation comparison

Times are average nanoseconds per benchmark invocation. `Previous` is the
immediately preceding ordinary-array-lookup full run. Positive values in
`vs previous` mean the current candidate is faster.

| Operation | Initial | Previous | Current | Same-run kernel | vs previous | Current / kernel |
|---|---:|---:|---:|---:|---:|---:|
| ordinary array lookup | 15,937 | 10,715 | 10,715 | 3,811 | +0.00% | 2.81x |
| ordinary array update | 23,510 | 31,956 | 16,047 | 13,040 | +49.79% | 1.23x |
| ordinary hash lookup | 58,473 | 63,803 | 63,372 | 28,224 | +0.68% | 2.25x |
| ordinary hash update | 63,182 | 70,389 | 70,791 | 93,149 | -0.57% | 0.76x |
| ordinary hash delete | 42,639 | 60,445 | 60,463 | 108,026 | -0.03% | 0.56x |
| per-CPU array lookup | 27,728 | 14,353 | 14,381 | 4,218 | -0.20% | 3.41x |
| per-CPU array update | 75,929 | 35,686 | 35,729 | 16,634 | -0.12% | 2.15x |
| per-CPU hash lookup | 141,245 | 72,347 | 72,294 | 29,252 | +0.07% | 2.47x |
| per-CPU hash update | 220,961 | 79,999 | 79,979 | 67,386 | +0.03% | 1.19x |
| per-CPU hash delete | 986,684 | 117,380 | 118,325 | 149,128 | -0.80% | 0.79x |

## Conclusion

The full run confirms the focused A/B result. Ordinary array update improves
by 49.79% relative to the immediately preceding full run and moves from 2.42x
kernel to 1.23x kernel. It is also 31.75% faster than the original research
baseline.

The other nine userspace operations move by at most 0.80% in the full run.
The matched A/B run is stricter for reachable regressions and measured the
three unrelated array cases within 0.33% of baseline.
