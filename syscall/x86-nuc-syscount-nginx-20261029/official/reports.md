# Benchmark Report: syscount-nginx Performance Analysis

## Overview
This report analyzes the performance of nginx under different syscall counting methods, comparing native execution (no tracing), kernel-based syscount (both targeted and untargeted), and bpftime's userspace BPF implementation (both targeted and untargeted).

## Test Environment
- **Test Date**: 2026-10-29
- **Benchmark Tool**: wrk (`http://127.0.0.1:801/index.html`, concurrency: 100, duration: 10s)
- **Number of Runs**: 10

## Performance Results

| Configuration | Requests/sec | % vs Native | % vs Kernel (same targeting) |
|---------------|--------------|-------------|------------------------------|
| Native (No tracing) | 53,884.49 | - | - |
| Kernel syscount (targeting nginx) | 51,583.05 | -4.27% | - |
| Kernel syscount (not targeting nginx) | 52,311.81 | -2.92% | - |
| UserBPF syscount (targeting nginx) | 53,306.21 | -1.07% | +3.34% |
| UserBPF syscount (not targeting nginx) | 53,036.98 | -1.57% | +1.39% |

## Key Findings

1. **Performance Comparison with Native Baseline**
   - There's a mixed performance impact when comparing with the native baseline.

2. **UserBPF vs Kernel-based syscount**
   - When targeting nginx specifically, UserBPF showed 3.34% better performance than the kernel equivalent.
   - When not targeting nginx, UserBPF showed 1.39% better performance than the kernel equivalent.

3. **Targeted vs. Untargeted Performance**
   - For kernel-based tracing, the untargeted mode performed 1.41% better than targeted mode.
   - For UserBPF, the targeted mode performed 0.51% better than untargeted mode.

## Conclusion

The benchmark results demonstrate that bpftime's userspace BPF implementation provides performance characteristics that differ from traditional kernel-based syscount for syscall tracing.

This data suggests that userspace BPF may offer benefits for observability tools that need to monitor production systems with minimal overhead.

## Recommendations

1. **Extend testing with more runs**: Multiple benchmark runs with different loads would provide more statistical confidence.

2. **Profile resource usage**: Adding CPU, memory, and I/O metrics would provide deeper insights into the efficiency differences.

3. **Test with varied workloads**: Different nginx configurations and request patterns could reveal performance characteristics under various conditions.

## Raw Data

The raw benchmark data is available in the JSON file: `benchmark_results_20261029_090830.json`

![Benchmark Results](benchmark_chart.png)