# per-CPU array lookup load-time specialization

Date: 2026-10-14

Source baseline commit: `f4025239e09cee96151313dbf57955cd186adb51`

Candidate build: `build-uprobe-percpu-array-fastpath-opt-20261014`

## Conclusion

Programs whose map relocations resolve exclusively to per-CPU arrays bind a
specialized lookup helper when the program is loaded. The helper retains fd,
handler-kind, and map-type checks, but bypasses the generic map-type dispatch
for the normal per-CPU array path.

The matched 5-round A/B test reduced the median per-CPU array lookup cost from
20.2553 ns/helper to 11.7266 ns/helper, a 42.11% reduction. Ordinary array
lookup changed by +0.21%; ordinary array update and per-CPU array update moved
by -0.48% and -0.43%, respectively. Those non-target differences are treated
as measurement noise.

The independent full benchmark improved per-CPU array lookup from the previous
21,341 ns/invocation to 12,908 ns/invocation, a 39.52% reduction. Each map
benchmark invocation performs 1,000 helper calls.

## Full 10-operation comparison

Positive percentages mean faster. Times are average ns/invocation from the
10-run full benchmark.

| Operation | Same-run Kernel | Initial BPFtime | Previous BPFtime | Current BPFtime | Initial to current | Previous to current | Current / Kernel |
|---|---:|---:|---:|---:|---:|---:|---:|
| ordinary array lookup | 3,681 | 15,937 | 15,866 | 15,809 | +0.81% | +0.36% | 4.29x |
| ordinary array update | 13,028 | 23,510 | 23,485 | 23,498 | +0.05% | -0.05% | 1.80x |
| ordinary hash lookup | 28,596 | 58,473 | 57,292 | 56,865 | +2.75% | +0.74% | 1.99x |
| ordinary hash update | 94,104 | 63,182 | 62,651 | 62,492 | +1.09% | +0.25% | 0.66x |
| ordinary hash delete | 108,837 | 42,639 | 42,394 | 42,137 | +1.18% | +0.61% | 0.39x |
| per-CPU array lookup | 4,400 | 27,728 | 21,341 | 12,908 | +53.45% | +39.52% | 2.93x |
| per-CPU array update | 16,618 | 75,929 | 27,087 | 27,134 | +64.26% | -0.17% | 1.63x |
| per-CPU hash lookup | 28,714 | 141,245 | 62,095 | 61,806 | +56.24% | +0.47% | 2.15x |
| per-CPU hash update | 67,327 | 220,961 | 72,552 | 72,168 | +67.34% | +0.53% | 1.07x |
| per-CPU hash delete | 151,276 | 986,684 | 109,646 | 109,595 | +88.89% | +0.05% | 0.72x |

Initial baseline:
`uprobe-top-arm64-20261007-strict-repro-official-no-btf`, commit `1a1dc0d`.

Previous accepted result:
`percpu-hash-node-pool-full-regression-arm64-20261014`, commit `f402523`.

## Validation

- Full benchmark status: success.
- Full benchmark groups: 17 kernel, 17 userspace, and 1 embedded case.
- Every full benchmark case contains 10 raw samples.
- CPU frequency samples stayed at 1,728,000 kHz.
- `[percpu-array]`: 1,761 assertions in 2 test cases passed.
- `test_arraymap_percpu*`: 16,011 assertions in 2 test cases passed.

