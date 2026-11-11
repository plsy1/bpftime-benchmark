
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T15:45:42.147429  

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
| Baseline (tiny) | tiny | 25.60 | - | - |
| Baseline (small) | small | 27.44 | - | - |
| Baseline (large) | large | 31.14 | - | - |
| Baseline (xlarge) | xlarge | 159.80 | - | - |
| Baseline (minimal) | minimal | 40.19 | - | - |
| Empty probe (tiny) | tiny | 28.09 | 25.60 | 1.10x (+9.7%) |
| Empty probe (xlarge) | xlarge | 174.19 | 159.80 | 1.09x (+9.0%) |
| Global timer (xlarge) | xlarge | 176.57 | 159.80 | 1.10x (+10.5%) |
| GPU Ringbuf (small) | small | 45.58 | 27.44 | 1.66x (+66.1%) |
| GPU Ringbuf (xlarge) | xlarge | 214.41 | 159.80 | 1.34x (+34.2%) |
| Per-GPU-thread array (small) | small | 36.42 | 27.44 | 1.33x (+32.7%) |
| Memtrace (small) | small | 36.17 | 27.44 | 1.32x (+31.8%) |
| Memtrace (large) | large | 173.36 | 31.14 | 5.57x (+456.7%) |
| GPU Array map update (small) | small | 37.15 | 27.44 | 1.35x (+35.4%) |
| GPU Array map update (large) | large | 229.69 | 31.14 | 7.38x (+637.6%) |
| GPU Array map lookup (small) | small | 33.40 | 27.44 | 1.22x (+21.7%) |
| GPU Array map lookup (large) | large | 69.97 | 31.14 | 2.25x (+124.7%) |
| CPU Array map update (minimal) | minimal | 39.40 | 25.60 | 1.54x (+53.9%) |
| CPU Array map lookup (minimal) | minimal | 38.48 | 25.60 | 1.50x (+50.3%) |
| CUDA Counter (minimal) | minimal | 2948.73 | 40.19 | 73.37x (+7237.0%) |
| Kernel Retsnoop (minimal) | minimal | 57.64 | 40.19 | 1.43x (+43.4%) |
| Thread Histogram (minimal) | minimal | 48.74 | 40.19 | 1.21x (+21.3%) |
| Memory Trace (minimal) | minimal | 62.04 | 40.19 | 1.54x (+54.4%) |

