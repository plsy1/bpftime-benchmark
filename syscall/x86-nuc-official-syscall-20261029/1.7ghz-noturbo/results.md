# BPFtime Syscall Benchmark Results

Benchmark run at: **2026-10-29 00:53:59**

## System Information
- OS: Linux debian-ichi-nuc 6.12.113+deb13-amd64 #1 SMP PREEMPT_DYNAMIC Debian 6.12.113-1 (2026-10-15) x86_64
- Hostname: debian-ichi-nuc
- Number of runs per configuration: 10

## Benchmark Results

### Native (No tracing)
| Metric | Value |
| ------ | ----- |
| Average time usage (mean) | **235.74 ns** |
| Average time usage (median) | 234.99 ns |
| Standard deviation | 1.75 |
| Min | 234.46 ns |
| Max | 239.45 ns |

**Individual runs (ns):**
`234.92, 235.6, 235.31, 238.5, 235.07, 239.45, 234.68, 234.75, 234.65, 234.46`


### Kernel Tracepoint Syscall
| Metric | Value |
| ------ | ----- |
| Average time usage (mean) | **293.57 ns** |
| Average time usage (median) | 289.15 ns |
| Standard deviation | 14.41 |
| Min | 288.05 ns |
| Max | 334.54 ns |

**Individual runs (ns):**
`334.54, 289.14, 290.39, 288.53, 288.22, 288.05, 289.61, 288.71, 289.34, 289.16`


### Userspace BPF Syscall
| Metric | Value |
| ------ | ----- |
| Average time usage (mean) | **459.95 ns** |
| Average time usage (median) | 459.94 ns |
| Standard deviation | 1.05 |
| Min | 458.75 ns |
| Max | 461.47 ns |

**Individual runs (ns):**
`460.77, 461.47, 458.84, 461.34, 458.75, 459.7, 460.51, 458.92, 459.01, 460.18`

## Comparison Results

### Overhead Compared to Native
| Configuration | Overhead |
| ------------ | -------- |
| Kernel Tracepoint Syscall | **24.53%** |
| Userspace BPF Syscall | **95.11%** |

### Userspace BPF vs Kernel Tracepoint
Userspace BPF syscall has **56.67%** more overhead than kernel tracepoint

## Summary
This benchmark compares three configurations for syscall handling:
1. **Native**: No tracing or interception
2. **Kernel Tracepoint**: Traditional kernel-based syscall tracking
3. **Userspace BPF**: BPFtime's userspace syscall interception

Each configuration was run multiple times to ensure statistical significance. Lower numbers represent better performance.