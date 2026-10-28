# BPFtime Syscall Benchmark Results

Benchmark run at: **2026-10-29 00:48:05**

## System Information
- OS: Linux debian-ichi-nuc 6.12.113+deb13-amd64 #1 SMP PREEMPT_DYNAMIC Debian 6.12.113-1 (2026-10-15) x86_64
- Hostname: debian-ichi-nuc
- Number of runs per configuration: 10

## Benchmark Results

### Native (No tracing)
| Metric | Value |
| ------ | ----- |
| Average time usage (mean) | **92.14 ns** |
| Average time usage (median) | 91.74 ns |
| Standard deviation | 1.34 |
| Min | 90.78 ns |
| Max | 95.02 ns |

**Individual runs (ns):**
`92.39, 93.69, 90.93, 90.78, 91.65, 91.84, 95.02, 91.4, 91.13, 92.6`


### Kernel Tracepoint Syscall
| Metric | Value |
| ------ | ----- |
| Average time usage (mean) | **225.33 ns** |
| Average time usage (median) | 225.30 ns |
| Standard deviation | 0.48 |
| Min | 224.79 ns |
| Max | 226.42 ns |

**Individual runs (ns):**
`225.13, 225.3, 224.86, 225.74, 224.79, 224.98, 225.35, 226.42, 225.42, 225.3`


### Userspace BPF Syscall
| Metric | Value |
| ------ | ----- |
| Average time usage (mean) | **358.35 ns** |
| Average time usage (median) | 357.79 ns |
| Standard deviation | 4.32 |
| Min | 354.58 ns |
| Max | 369.01 ns |

**Individual runs (ns):**
`357.0, 355.27, 359.23, 354.58, 360.32, 359.61, 369.01, 358.57, 354.86, 355.1`

## Comparison Results

### Overhead Compared to Native
| Configuration | Overhead |
| ------------ | -------- |
| Kernel Tracepoint Syscall | **144.54%** |
| Userspace BPF Syscall | **288.91%** |

### Userspace BPF vs Kernel Tracepoint
Userspace BPF syscall has **59.04%** more overhead than kernel tracepoint

## Summary
This benchmark compares three configurations for syscall handling:
1. **Native**: No tracing or interception
2. **Kernel Tracepoint**: Traditional kernel-based syscall tracking
3. **Userspace BPF**: BPFtime's userspace syscall interception

Each configuration was run multiple times to ensure statistical significance. Lower numbers represent better performance.