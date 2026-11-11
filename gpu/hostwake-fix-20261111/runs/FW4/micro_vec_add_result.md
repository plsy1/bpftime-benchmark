
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T16:15:52.175447  

## Workload Configuration

| Workload | Binary | Elements | Iterations | Threads | Blocks |
|----------|--------|----------|------------|---------|--------|
| large | /home/jetson/src/bpftime-gpu-work/compare/victims/vec_add_legacy | 100000 | 1000 | 512 | 196 |
| medium | /home/jetson/src/bpftime-gpu-work/compare/victims/vec_add_legacy | 10000 | 10000 | 256 | 40 |
| minimal | /home/jetson/src/bpftime-gpu-work/compare/victims/vec_add_legacy | 32 | 3 | 32 | 1 |
| small | /home/jetson/src/bpftime-gpu-work/compare/victims/vec_add_legacy | 1000 | 10000 | 256 | 4 |
| small_64x16 | /home/jetson/src/bpftime-gpu-work/compare/victims/vec_add_legacy | 1000 | 10000 | 64 | 16 |
| tiny | /home/jetson/src/bpftime-gpu-work/compare/victims/vec_add_legacy | 32 | 10000 | 32 | 1 |
| xlarge | /home/jetson/src/bpftime-gpu-work/compare/victims/vec_add_legacy | 1000000 | 1000 | 512 | 1954 |

## Benchmark Results

| Test Name | Workload | Avg Time (μs) | vs Baseline | Overhead |
|-----------|----------|---------------|-------------|----------|
| Baseline (tiny) | tiny | 27.18 | - | - |
| Baseline (small) | small | 27.14 | - | - |
| Baseline (large) | large | 31.27 | - | - |
| Baseline (xlarge) | xlarge | 160.45 | - | - |
| Baseline (minimal) | minimal | 43.01 | - | - |
| Empty probe (tiny) | tiny | 28.25 | 27.18 | 1.04x (+3.9%) |
| Empty probe (xlarge) | xlarge | 172.59 | 160.45 | 1.08x (+7.6%) |
| Global timer (xlarge) | xlarge | 179.37 | 160.45 | 1.12x (+11.8%) |
| GPU Ringbuf (small) | small | 48.90 | 27.14 | 1.80x (+80.2%) |
| GPU Ringbuf (xlarge) | xlarge | 218.43 | 160.45 | 1.36x (+36.1%) |
| Per-GPU-thread array (small) | small | 38.92 | 27.14 | 1.43x (+43.4%) |
| Memtrace (small) | small | 36.08 | 27.14 | 1.33x (+32.9%) |
| Memtrace (large) | large | 173.38 | 31.27 | 5.54x (+454.5%) |
| GPU Array map update (small) | small | 37.90 | 27.14 | 1.40x (+39.6%) |
| GPU Array map update (large) | large | 226.10 | 31.27 | 7.23x (+623.1%) |
| GPU Array map lookup (small) | small | 34.93 | 27.14 | 1.29x (+28.7%) |
| GPU Array map lookup (large) | large | 70.23 | 31.27 | 2.25x (+124.6%) |
| CPU Array map update (minimal) | minimal | 49.52 | 27.18 | 1.82x (+82.2%) |
| CPU Array map lookup (minimal) | minimal | 39.61 | 27.18 | 1.46x (+45.7%) |
| CUDA Counter (minimal) | minimal | 2899.09 | 43.01 | 67.41x (+6640.5%) |
| Kernel Retsnoop (minimal) | minimal | 48.00 | 43.01 | 1.12x (+11.6%) |
| Thread Histogram (minimal) | minimal | 39.01 | 43.01 | 0.90x (-9.3%) |
| Memory Trace (minimal) | minimal | 60.72 | 43.01 | 1.41x (+41.2%) |

