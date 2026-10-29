# BPFtime Syscall Benchmark Results

Benchmark run at: **2026-10-29 15:50:38**

## System Information
- OS: Linux orin-nano 6.8.12-1021-tegra-btf #2 SMP PREEMPT Sat Oct 24 23:02:32 JST 2026 aarch64
- Hostname: orin-nano
- Number of runs per configuration: 10

## Benchmark Results

### Native (No tracing)
| Metric | Value |
| ------ | ----- |
| Average time usage (mean) | **353.24 ns** |
| Average time usage (median) | 353.25 ns |
| Standard deviation | 0.33 |
| Min | 352.78 ns |
| Max | 353.92 ns |

**Individual runs (ns):**
`352.94, 353.29, 353.92, 353.17, 353.33, 352.78, 352.91, 353.28, 353.52, 353.22`


### Kernel Tracepoint Syscall
| Metric | Value |
| ------ | ----- |
| Average time usage (mean) | **438.31 ns** |
| Average time usage (median) | 437.74 ns |
| Standard deviation | 2.34 |
| Min | 436.34 ns |
| Max | 444.59 ns |

**Individual runs (ns):**
`438.11, 438.64, 438.73, 444.59, 436.82, 437.02, 437.42, 438.06, 436.34, 437.41`


### Userspace BPF Syscall
| Metric | Value |
| ------ | ----- |
| Average time usage (mean) | **515.99 ns** |
| Average time usage (median) | 513.99 ns |
| Standard deviation | 6.36 |
| Min | 505.20 ns |
| Max | 526.96 ns |

**Individual runs (ns):**
`523.37, 516.13, 526.96, 513.79, 514.18, 521.72, 513.43, 513.35, 505.2, 511.75`

## Comparison Results

### Overhead Compared to Native
| Configuration | Overhead |
| ------------ | -------- |
| Kernel Tracepoint Syscall | **24.09%** |
| Userspace BPF Syscall | **46.07%** |

### Userspace BPF vs Kernel Tracepoint
Userspace BPF syscall has **17.72%** more overhead than kernel tracepoint

## Summary
This benchmark compares three configurations for syscall handling:
1. **Native**: No tracing or interception
2. **Kernel Tracepoint**: Traditional kernel-based syscall tracking
3. **Userspace BPF**: BPFtime's userspace syscall interception

Each configuration was run multiple times to ensure statistical significance. Lower numbers represent better performance.