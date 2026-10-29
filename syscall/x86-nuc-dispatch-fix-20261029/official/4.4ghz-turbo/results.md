# BPFtime Syscall Benchmark Results

Benchmark run at: **2026-10-29 14:25:16**

## System Information
- OS: Linux debian-ichi-nuc 6.12.113+deb13-amd64 #1 SMP PREEMPT_DYNAMIC Debian 6.12.113-1 (2026-10-15) x86_64
- Hostname: debian-ichi-nuc
- Number of runs per configuration: 10

## Benchmark Results

### Native (No tracing)
| Metric | Value |
| ------ | ----- |
| Average time usage (mean) | **91.08 ns** |
| Average time usage (median) | 90.95 ns |
| Standard deviation | 0.43 |
| Min | 90.65 ns |
| Max | 91.96 ns |

**Individual runs (ns):**
`91.0, 91.57, 91.1, 90.67, 90.86, 90.91, 90.65, 91.96, 91.35, 90.71`


### Kernel Tracepoint Syscall
| Metric | Value |
| ------ | ----- |
| Average time usage (mean) | **112.05 ns** |
| Average time usage (median) | 111.91 ns |
| Standard deviation | 0.62 |
| Min | 111.41 ns |
| Max | 113.31 ns |

**Individual runs (ns):**
`111.8, 111.43, 113.31, 111.66, 111.41, 112.5, 112.34, 112.55, 112.02, 111.52`


### Userspace BPF Syscall
| Metric | Value |
| ------ | ----- |
| Average time usage (mean) | **151.98 ns** |
| Average time usage (median) | 151.44 ns |
| Standard deviation | 1.87 |
| Min | 150.36 ns |
| Max | 156.21 ns |

**Individual runs (ns):**
`151.4, 150.49, 156.21, 151.63, 151.26, 150.36, 152.14, 150.54, 154.32, 151.49`

## Comparison Results

### Overhead Compared to Native
| Configuration | Overhead |
| ------------ | -------- |
| Kernel Tracepoint Syscall | **23.03%** |
| Userspace BPF Syscall | **66.87%** |

### Userspace BPF vs Kernel Tracepoint
Userspace BPF syscall has **35.63%** more overhead than kernel tracepoint

## Summary
This benchmark compares three configurations for syscall handling:
1. **Native**: No tracing or interception
2. **Kernel Tracepoint**: Traditional kernel-based syscall tracking
3. **Userspace BPF**: BPFtime's userspace syscall interception

Each configuration was run multiple times to ensure statistical significance. Lower numbers represent better performance.