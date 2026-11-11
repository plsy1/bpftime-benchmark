
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T14:28:57.922845  

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
| Baseline (tiny) | tiny | 27.37 | - | - |
| Baseline (small) | small | 27.64 | - | - |
| Baseline (large) | large | 31.37 | - | - |
| Baseline (xlarge) | xlarge | 159.02 | - | - |
| Baseline (minimal) | minimal | 38.05 | - | - |
| Empty probe (tiny) | tiny | 30.28 | 27.37 | 1.11x (+10.6%) |
| Empty probe (xlarge) | xlarge | 172.20 | 159.02 | 1.08x (+8.3%) |
| Global timer (xlarge) | xlarge | 177.67 | 159.02 | 1.12x (+11.7%) |
| GPU Ringbuf (small) | small | 40.56 | 27.64 | 1.47x (+46.7%) |
| GPU Ringbuf (xlarge) | xlarge | 212.54 | 159.02 | 1.34x (+33.7%) |
| Per-GPU-thread array (small) | small | 38.83 | 27.64 | 1.40x (+40.5%) |
| Memtrace (small) | small | 31.68 | 27.64 | 1.15x (+14.6%) |
| Memtrace (large) | large | 174.08 | 31.37 | 5.55x (+454.9%) |
| GPU Array map update (small) | small | 33.79 | 27.64 | 1.22x (+22.3%) |
| GPU Array map update (large) | large | 252.76 | 31.37 | 8.06x (+705.7%) |
| GPU Array map lookup (small) | small | 39.21 | 27.64 | 1.42x (+41.9%) |
| GPU Array map lookup (large) | large | 68.44 | 31.37 | 2.18x (+118.2%) |
| CPU Array map update (minimal) | minimal | 318.20 | 27.37 | 11.63x (+1062.6%) |
| CPU Array map lookup (minimal) | minimal | 366.55 | 27.37 | 13.39x (+1239.2%) |
| CUDA Counter (minimal) | minimal | 3458.30 | 38.05 | 90.89x (+8988.8%) |
| Kernel Retsnoop (minimal) | minimal | 49.08 | 38.05 | 1.29x (+29.0%) |
| Thread Histogram (minimal) | minimal | 47.72 | 38.05 | 1.25x (+25.4%) |
| Memory Trace (minimal) | minimal | 54.90 | 38.05 | 1.44x (+44.3%) |

