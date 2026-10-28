# BPFtime Syscall Benchmark Results

Benchmark run at: **2026-10-29 00:27:09**

## System Information
- OS: Linux orin-nano 6.8.12-1021-tegra-btf #2 SMP PREEMPT Sat Oct 24 23:02:32 JST 2026 aarch64
- Hostname: orin-nano
- Number of runs per configuration: 10

## Benchmark Results

### Native (No tracing)
| Metric | Value |
| ------ | ----- |
| Average time usage (mean) | **353.45 ns** |
| Average time usage (median) | 353.58 ns |
| Standard deviation | 0.38 |
| Min | 352.83 ns |
| Max | 353.88 ns |

**Individual runs (ns):**
`353.16, 353.16, 353.88, 353.71, 353.84, 353.65, 353.5, 352.98, 353.79, 352.83`


### Kernel Tracepoint Syscall
| Metric | Value |
| ------ | ----- |
| Average time usage (mean) | **438.22 ns** |
| Average time usage (median) | 438.20 ns |
| Standard deviation | 1.05 |
| Min | 436.52 ns |
| Max | 440.03 ns |

**Individual runs (ns):**
`436.52, 438.27, 438.64, 440.03, 437.11, 438.13, 438.45, 439.53, 438.12, 437.4`


### Userspace BPF Syscall
| Metric | Value |
| ------ | ----- |
| Average time usage (mean) | **640.00 ns** |
| Average time usage (median) | 641.06 ns |
| Standard deviation | 4.19 |
| Min | 632.63 ns |
| Max | 645.09 ns |

**Individual runs (ns):**
`633.28, 638.81, 632.63, 641.63, 639.42, 641.83, 640.49, 645.09, 642.58, 644.28`

## Comparison Results

### Overhead Compared to Native
| Configuration | Overhead |
| ------------ | -------- |
| Kernel Tracepoint Syscall | **23.98%** |
| Userspace BPF Syscall | **81.07%** |

### Userspace BPF vs Kernel Tracepoint
Userspace BPF syscall has **46.05%** more overhead than kernel tracepoint

## Summary
This benchmark compares three configurations for syscall handling:
1. **Native**: No tracing or interception
2. **Kernel Tracepoint**: Traditional kernel-based syscall tracking
3. **Userspace BPF**: BPFtime's userspace syscall interception

Each configuration was run multiple times to ensure statistical significance. Lower numbers represent better performance.