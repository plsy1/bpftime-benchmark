# BPFtime userspace map path: x64 reproduction and ARM64 comparison

## Technical summary

This directory reproduces the Jetson map-path diagnostics on x64 at BPFtime
commit `8ed291e130fe3f15f99955b0d259eb119efdaa7d`. The primary end-to-end estimate
is the median of five independent LLVM JIT real-minus-noop process pairs. Direct
L0-L3 measurements are used for path attribution.

ARM64 is **not** absolutely heavier. Array lookup is effectively equal
(ARM64 10.063 ns/helper, x64 9.984 ns/helper), while x64 is heavier for array
update (+4.929 ns), hash lookup (+34.122 ns), and hash update (+31.778 ns).
The large hash differences are concentrated in the fixed-size hash map
implementation and handler. Shared-memory/fd dispatch is small. The final
helper contributes materially to the x64 hash-lookup direct result, but not to
hash update.

## Key findings

| Operation | ARM64 BPFtime net | x64 BPFtime net | x64 - ARM64 | ARM64 kernel | x64 kernel |
|---|---:|---:|---:|---:|---:|
| array lookup | 10.063 ns, 17.374 cyc, 85.000 ins | 9.984 ns, 21.775 cyc, 81.073 ins | -0.079 ns, +4.401 cyc, -3.926 ins | 1.384 ns | 0.515 ns |
| array update | 14.507 ns, 25.047 cyc, 134.000 ins | 19.435 ns, 42.391 cyc, 143.139 ins | +4.929 ns, +17.345 cyc, +9.138 ins | 10.952 ns | 18.791 ns |
| hash lookup hit | 41.558 ns, 71.754 cyc, 270.905 ins | 75.680 ns, 165.073 cyc, 261.805 ins | +34.122 ns, +93.319 cyc, -9.100 ins | 26.769 ns | 25.920 ns |
| hash update existing | 52.543 ns, 90.725 cyc, 316.908 ins | 84.321 ns, 183.933 cyc, 322.914 ins | +31.778 ns, +93.208 cyc, +6.006 ins | 90.823 ns | 75.992 ns |

The x64 hash lookup executes slightly fewer retired instructions than ARM64 but
takes 2.30x the cycles. This rejects an explanation based on extra generic
dispatch instructions alone; the fixed hash implementation's lock/memory
behavior and generated machine-code efficiency are the stronger candidates.

## End-to-end JIT A/B

Each x64 row below is the median of five independent `noop -> real` pairs. The
range is across pairwise real-minus-noop results.

| Operation | net ns/helper | range | cycles/helper | instructions/helper |
|---|---:|---:|---:|---:|
| array lookup | 9.984 | 9.943-10.042 | 21.775 | 81.073 |
| array update existing | 19.435 | 19.426-19.451 | 42.391 | 143.139 |
| hash lookup hit | 75.680 | 75.431-77.326 | 165.073 | 261.805 |
| hash update existing | 84.321 | 80.714-106.737 | 183.933 | 322.914 |

Hash update pair 2 is a visible high outlier (106.737 ns/helper). It is retained
in the raw data and range; the predeclared median-of-five estimator prevents it
from replacing the central result. Direct hash-update L3 is 84.700 ns/op, which
independently agrees with the 84.321 ns/helper median. No thermal throttle event
was recorded after the run.

## Direct cumulative paths

These are cumulative L3-minus-control costs. They provide an absolute
cross-check in ns/op, cycles/op, and instructions/op.

| Operation | ARM64 L3 net | x64 L3 net | x64 - ARM64 ns |
|---|---:|---:|---:|
| array lookup | 8.920 ns, 15.400 cyc, 89.000 ins | 10.626 ns, 23.178 cyc, 84.079 ins | +1.706 |
| array update | 13.178 ns, 22.753 cyc, 131.000 ins | 16.498 ns, 35.993 cyc, 128.113 ins | +3.321 |
| hash lookup hit | 41.261 ns, 71.250 cyc, 285.908 ins | 83.903 ns, 182.797 cyc, 280.994 ins | +42.642 |
| hash update existing | 53.199 ns, 91.863 cyc, 337.908 ins | 84.700 ns, 184.779 cyc, 342.303 ins | +31.501 |

## Layer attribution

Values are incremental costs between adjacent direct entry points. Negative
L3-L2 values are preserved: with runtime LTO enabled, separately compiled
diagnostic entry points do not form a strictly additive call stack.

| Path | Stage | ARM64 ns / cyc / ins | x64 ns / cyc / ins | x64 - ARM64 ns |
|---|---|---:|---:|---:|
| array lookup | map implementation | 0.228 / 0.393 / 14.000 | 0.923 / 2.010 / 14.022 | +0.694 |
| array lookup | handler | 4.760 / 8.219 / 50.000 | 6.450 / 14.071 / 44.029 | +1.690 |
| array lookup | shared-memory/fd | 4.959 / 8.562 / 34.000 | 3.687 / 8.044 / 40.025 | -1.272 |
| array lookup | final helper | -1.027 / -1.774 / -9.000 | -0.434 / -0.946 / -13.997 | +0.594 |
| array update | map implementation | 2.771 / 4.785 / 48.000 | 5.031 / 10.974 / 43.074 | +2.259 |
| array update | handler | 6.166 / 10.647 / 60.000 | 8.211 / 17.914 / 61.012 | +2.045 |
| array update | shared-memory/fd | 5.941 / 10.257 / 41.000 | 5.108 / 11.141 / 50.039 | -0.833 |
| array update | final helper | -1.701 / -2.936 / -18.000 | -1.851 / -4.035 / -26.012 | -0.150 |
| hash lookup | spin lock | 11.451 / 19.813 / 43.000 | 13.338 / 29.058 / 23.062 | +1.887 |
| hash lookup | map implementation after lock | 24.963 / 43.083 / 126.908 | 45.065 / 98.238 / 165.422 | +20.101 |
| hash lookup | handler | 3.557 / 6.130 / 79.000 | 13.284 / 29.093 / 48.653 | +9.727 |
| hash lookup | shared-memory/fd | 1.444 / 2.483 / 34.000 | 2.872 / 6.403 / 39.202 | +1.428 |
| hash lookup | final helper | -0.154 / -0.259 / 3.000 | 9.345 / 20.004 / 4.656 | +9.499 |
| hash update | spin lock | 14.445 / 24.945 / 43.000 | 12.878 / 28.056 / 23.035 | -1.567 |
| hash update | map implementation after lock | 23.374 / 40.366 / 160.908 | 49.345 / 107.657 / 197.884 | +25.971 |
| hash update | handler | 9.424 / 16.264 / 93.000 | 20.078 / 43.803 / 74.383 | +10.654 |
| hash update | shared-memory/fd | 5.386 / 9.302 / 41.000 | 5.974 / 13.029 / 49.970 | +0.588 |
| hash update | final helper | 0.570 / 0.986 / 0.000 | -3.575 / -7.767 / -2.970 | -4.145 |

For hash update, the direct cross-platform excess (+31.501 ns) closely matches
the JIT excess (+31.778 ns): map implementation (+25.971 ns) and handler
(+10.654 ns) dominate, partly offset by lock and final-helper differences. For
hash lookup, map implementation (+20.101 ns), handler (+9.727 ns), and final
helper (+9.499 ns) explain most of the +42.642 ns direct excess. The
shared-memory/fd stage is only +1.428 ns.

## Numerator versus kernel denominator

| Operation | ARM64 BPFtime / kernel | x64 BPFtime / kernel | Cross-platform driver |
|---|---:|---:|---|
| array lookup | 10.063 / 1.384 ns = 7.27x | 9.984 / 0.515 ns = 19.37x | Kernel denominator: BPFtime is flat, x64 kernel is 0.869 ns lower. |
| array update | 14.507 / 10.952 ns = 1.32x | 19.435 / 18.791 ns = 1.03x | Kernel denominator dominates ratio narrowing: x64 adds 7.839 ns kernel versus 4.929 ns BPFtime. |
| hash lookup | 41.558 / 26.769 ns = 1.55x | 75.680 / 25.920 ns = 2.92x | BPFtime numerator: kernel differs by only -0.849 ns, BPFtime adds 34.122 ns. |
| hash update | 52.543 / 90.823 ns = 0.58x | 84.321 / 75.992 ns = 1.11x | Both amplify the change, but numerator dominates: BPFtime +31.778 ns while kernel -14.831 ns. |

The kernel denominator archives contain wall-time only, so kernel-side
cycles/helper and instructions/helper cannot be reported without a new kernel
counter run. No counter values are inferred from frequency. Both architectures'
BPFtime userspace sides are reported above with measured ns, cycles, and
instructions.

## Scope and methodology

- Code: branch `codex/official-no-btf`, commit `8ed291e`, clean at measurement.
- Host execution only; no Docker and no full uprobe benchmark.
- CPU 5 pinned; SMT sibling CPU 11 temporarily offline.
- Performance governor active and Intel turbo disabled during the measurement;
  both were restored afterward.
- LLVM 15.0.7, BPFtime runtime LTO ON, diagnostic caller IPO OFF, probe
  read/write checks OFF, GCC 13, isolated Boost 1.83.
- JIT array: 1M-helper warm-up, 3 x 100M formal helpers per process, five
  independent noop/real pairs; perf denominator 301M helpers.
- JIT hash: 1M-helper warm-up, 3 x 20M formal helpers per process, five
  independent noop/real pairs; perf denominator 61M helpers.
- Direct array: 1M-op warm-up and 3 x 100M formal ops; perf denominator 301M.
- Direct hash: 1M-op warm-up and 3 x 10M formal ops; perf denominator 31M.
- Perf events were collected simultaneously with wall-time. Hardware events
  were multiplexed and scaled by `perf stat`; every event was counted.

## Environment differences and limitations

The x64 host necessarily differs in CPU and kernel. CMake is 4.2.3 rather than
the Jetson environment. The BPFtime code was built with GCC 13 and Boost 1.83,
but the local LLVM 15 libraries were built with GCC 12. One linked, unused uBPF
compatibility archive was built by its ExternalProject with system GCC 15; the
diagnostic explicitly selects LLVM JIT, and runtime hot-path objects are GCC 13.
These facts are captured in `environment.txt`.

Direct layers are separately timed entry points, not nested subtraction in one
binary execution. Consequently, L3-L2 can be negative under LTO. The x64 hash
lookup direct L3 result (83.903 ns) is 8.223 ns above the JIT A/B result
(75.680 ns), whereas hash update agrees within 0.379 ns. The hash-lookup layer
split should therefore be treated as directional attribution; the JIT A/B
median remains the end-to-end numerator.

## Artifacts

- `jit-helper-ab.csv`: all five pairs plus median/min/max summaries.
- `direct-layers.csv`: absolute and control/previous-layer deltas.
- `jit-cross-architecture.csv`: ARM64/x64 BPFtime and kernel comparison.
- `layer-cross-architecture.csv`: reproducible adjacent-layer attribution.
- `raw-stdout/` and `raw-perf/`: 62 paired raw outputs.
- `run-experiment.sh`, `summarize_results.py`, and
  `compare_architectures.py`: execution and derivation pipeline.
- `environment.txt`: commits, dirty state, host, toolchain, CMake flags, CPU
  isolation, governor, turbo, and exact denominators.

## Next steps and open question

The remaining uncertainty is the x64 hash-lookup JIT/direct gap. A focused
follow-up could collect call-graph profiles or assembly for only the two
diagnostic entry paths, without modifying runtime or changing loop semantics.
It is not required to answer the present cross-architecture questions.
