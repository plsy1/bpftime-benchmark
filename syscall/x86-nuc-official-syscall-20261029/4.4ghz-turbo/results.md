# BPFtime Syscall Benchmark Results

Benchmark run at: **2026-10-29 00:51:13**

## System Information
- OS: Linux debian-ichi-nuc 6.12.113+deb13-amd64 #1 SMP PREEMPT_DYNAMIC Debian 6.12.113-1 (2026-10-15) x86_64
- Hostname: debian-ichi-nuc
- Number of runs per configuration: 10

## Benchmark Results

### Native (No tracing)
| Metric | Value |
| ------ | ----- |
| Average time usage (mean) | **91.60 ns** |
| Average time usage (median) | 91.36 ns |
| Standard deviation | 0.65 |
| Min | 91.07 ns |
| Max | 93.19 ns |

**Individual runs (ns):**
`91.2, 92.16, 91.07, 91.62, 91.11, 91.26, 93.19, 91.62, 91.42, 91.3`


### Kernel Tracepoint Syscall
| Metric | Value |
| ------ | ----- |
| Average time usage (mean) | **112.52 ns** |
| Average time usage (median) | 112.49 ns |
| Standard deviation | 0.66 |
| Min | 111.68 ns |
| Max | 113.69 ns |

**Individual runs (ns):**
`112.34, 112.87, 111.98, 112.64, 111.79, 113.69, 112.16, 113.32, 112.7, 111.68`


### Userspace BPF Syscall
| Metric | Value |
| ------ | ----- |
| Average time usage (mean) | **178.99 ns** |
| Average time usage (median) | 179.14 ns |
| Standard deviation | 0.79 |
| Min | 177.95 ns |
| Max | 180.43 ns |

**Individual runs (ns):**
`180.43, 179.55, 179.53, 178.97, 178.16, 179.32, 178.46, 179.4, 177.95, 178.19`

## Comparison Results

### Overhead Compared to Native
| Configuration | Overhead |
| ------------ | -------- |
| Kernel Tracepoint Syscall | **22.84%** |
| Userspace BPF Syscall | **95.42%** |

### Userspace BPF vs Kernel Tracepoint
Userspace BPF syscall has **59.08%** more overhead than kernel tracepoint

## Summary
This benchmark compares three configurations for syscall handling:
1. **Native**: No tracing or interception
2. **Kernel Tracepoint**: Traditional kernel-based syscall tracking
3. **Userspace BPF**: BPFtime's userspace syscall interception

Each configuration was run multiple times to ensure statistical significance. Lower numbers represent better performance.