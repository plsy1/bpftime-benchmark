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
| Native (No tracing) | 25,140.70 | - | - |
| Kernel syscount (targeting nginx) | 23,523.47 | -6.43% | - |
| Kernel syscount (not targeting nginx) | 24,209.89 | -3.70% | - |
| UserBPF syscount (targeting nginx) | 24,593.65 | -2.18% | +4.55% |
| UserBPF syscount (not targeting nginx) | 23,634.00 | -5.99% | -2.38% |

## Key Findings

1. **Performance Comparison with Native Baseline**
   - There's a mixed performance impact when comparing with the native baseline.

2. **UserBPF vs Kernel-based syscount**
   - When targeting nginx specifically, UserBPF showed 4.55% better performance than the kernel equivalent.
   - When not targeting nginx, UserBPF showed 2.38% worse performance than the kernel equivalent.

3. **Targeted vs. Untargeted Performance**
   - For kernel-based tracing, the untargeted mode performed 2.92% better than targeted mode.
   - For UserBPF, the targeted mode performed 3.90% better than untargeted mode.

## Conclusion

The benchmark results demonstrate that bpftime's userspace BPF implementation provides performance characteristics that differ from traditional kernel-based syscount for syscall tracing.

This data suggests that userspace BPF may offer benefits for observability tools that need to monitor production systems with minimal overhead.

## Recommendations

1. **Extend testing with more runs**: Multiple benchmark runs with different loads would provide more statistical confidence.

2. **Profile resource usage**: Adding CPU, memory, and I/O metrics would provide deeper insights into the efficiency differences.

3. **Test with varied workloads**: Different nginx configurations and request patterns could reveal performance characteristics under various conditions.

## Raw Data

The raw benchmark data is available in the JSON file: `benchmark_results_20261029_054739.json`

![Benchmark Results](benchmark_chart.png)