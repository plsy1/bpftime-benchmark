# BPFtime Syscall Benchmark Results

Benchmark run at: **2026-10-29 14:22:55**

## System Information
- OS: Linux debian-ichi-nuc 6.12.113+deb13-amd64 #1 SMP PREEMPT_DYNAMIC Debian 6.12.113-1 (2026-10-15) x86_64
- Hostname: debian-ichi-nuc
- Number of runs per configuration: 10

## Benchmark Results

### Native (No tracing)
| Metric | Value |
| ------ | ----- |
| Average time usage (mean) | **234.70 ns** |
| Average time usage (median) | 234.82 ns |
| Standard deviation | 0.45 |
| Min | 233.73 ns |
| Max | 235.20 ns |

**Individual runs (ns):**
`234.8, 234.48, 234.93, 234.5, 235.1, 234.31, 233.73, 234.84, 235.2, 235.1`


### Kernel Tracepoint Syscall
| Metric | Value |
| ------ | ----- |
| Average time usage (mean) | **288.47 ns** |
| Average time usage (median) | 288.44 ns |
| Standard deviation | 0.64 |
| Min | 287.55 ns |
| Max | 289.58 ns |

**Individual runs (ns):**
`289.03, 288.6, 288.28, 288.95, 288.03, 288.28, 288.79, 289.58, 287.55, 287.61`


### Userspace BPF Syscall
| Metric | Value |
| ------ | ----- |
| Average time usage (mean) | **387.35 ns** |
| Average time usage (median) | 387.08 ns |
| Standard deviation | 1.34 |
| Min | 385.96 ns |
| Max | 390.53 ns |

**Individual runs (ns):**
`386.46, 386.62, 388.0, 390.53, 387.21, 386.94, 386.11, 387.77, 387.95, 385.96`

## Comparison Results

### Overhead Compared to Native
| Configuration | Overhead |
| ------------ | -------- |
| Kernel Tracepoint Syscall | **22.91%** |
| Userspace BPF Syscall | **65.04%** |

### Userspace BPF vs Kernel Tracepoint
Userspace BPF syscall has **34.28%** more overhead than kernel tracepoint

## Summary
This benchmark compares three configurations for syscall handling:
1. **Native**: No tracing or interception
2. **Kernel Tracepoint**: Traditional kernel-based syscall tracking
3. **Userspace BPF**: BPFtime's userspace syscall interception

Each configuration was run multiple times to ensure statistical significance. Lower numbers represent better performance.