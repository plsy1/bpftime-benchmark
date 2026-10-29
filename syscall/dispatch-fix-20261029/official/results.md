# BPFtime Syscall Benchmark Results

Benchmark run at: **2026-10-29 13:54:09**

## System Information
- OS: Linux orin-nano 6.8.12-1021-tegra-btf #2 SMP PREEMPT Sat Oct 24 23:02:32 JST 2026 aarch64
- Hostname: orin-nano
- Number of runs per configuration: 10

## Benchmark Results

### Native (No tracing)
| Metric | Value |
| ------ | ----- |
| Average time usage (mean) | **353.14 ns** |
| Average time usage (median) | 353.10 ns |
| Standard deviation | 0.27 |
| Min | 352.79 ns |
| Max | 353.68 ns |

**Individual runs (ns):**
`353.14, 352.99, 353.68, 353.05, 352.96, 353.14, 352.87, 353.45, 352.79, 353.31`


### Kernel Tracepoint Syscall
| Metric | Value |
| ------ | ----- |
| Average time usage (mean) | **437.76 ns** |
| Average time usage (median) | 437.34 ns |
| Standard deviation | 1.33 |
| Min | 436.56 ns |
| Max | 440.95 ns |

**Individual runs (ns):**
`440.95, 438.01, 439.04, 437.26, 437.71, 436.56, 437.06, 436.92, 437.43, 436.7`


### Userspace BPF Syscall
| Metric | Value |
| ------ | ----- |
| Average time usage (mean) | **515.51 ns** |
| Average time usage (median) | 517.54 ns |
| Standard deviation | 8.41 |
| Min | 495.84 ns |
| Max | 524.64 ns |

**Individual runs (ns):**
`514.76, 523.33, 518.63, 495.84, 524.64, 517.42, 517.65, 506.94, 518.87, 516.97`

## Comparison Results

### Overhead Compared to Native
| Configuration | Overhead |
| ------------ | -------- |
| Kernel Tracepoint Syscall | **23.96%** |
| Userspace BPF Syscall | **45.98%** |

### Userspace BPF vs Kernel Tracepoint
Userspace BPF syscall has **17.76%** more overhead than kernel tracepoint

## Summary
This benchmark compares three configurations for syscall handling:
1. **Native**: No tracing or interception
2. **Kernel Tracepoint**: Traditional kernel-based syscall tracking
3. **Userspace BPF**: BPFtime's userspace syscall interception

Each configuration was run multiple times to ensure statistical significance. Lower numbers represent better performance.