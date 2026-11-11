
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T14:11:27.583581  

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
| Baseline (tiny) | tiny | 27.02 | - | - |
| Baseline (small) | small | 27.70 | - | - |
| Baseline (large) | large | 31.13 | - | - |
| Baseline (xlarge) | xlarge | 159.48 | - | - |
| Baseline (minimal) | minimal | 40.88 | - | - |
| Empty probe (tiny) | tiny | 35.75 | 27.02 | 1.32x (+32.3%) |
| Empty probe (xlarge) | xlarge | 173.16 | 159.48 | 1.09x (+8.6%) |
| Global timer (xlarge) | xlarge | 181.92 | 159.48 | 1.14x (+14.1%) |
| GPU Ringbuf (small) | small | 44.34 | 27.70 | 1.60x (+60.1%) |
| GPU Ringbuf (xlarge) | xlarge | 217.18 | 159.48 | 1.36x (+36.2%) |
| Per-GPU-thread array (small) | small | 36.80 | 27.70 | 1.33x (+32.9%) |
| Memtrace (small) | small | 37.04 | 27.70 | 1.34x (+33.7%) |
| Memtrace (large) | large | 176.50 | 31.13 | 5.67x (+467.0%) |
| GPU Array map update (small) | small | 35.08 | 27.70 | 1.27x (+26.6%) |
| GPU Array map update (large) | large | 239.57 | 31.13 | 7.70x (+669.6%) |
| GPU Array map lookup (small) | small | 33.27 | 27.70 | 1.20x (+20.1%) |
| GPU Array map lookup (large) | large | 69.70 | 31.13 | 2.24x (+123.9%) |
| CPU Array map update (minimal) | minimal | 320.84 | 27.02 | 11.87x (+1087.4%) |
| CPU Array map lookup (minimal) | minimal | 369.14 | 27.02 | 13.66x (+1266.2%) |
| CUDA Counter (minimal) | minimal | 3547.39 | 40.88 | 86.78x (+8577.6%) |
| Kernel Retsnoop (minimal) | minimal | 48.43 | 40.88 | 1.18x (+18.5%) |
| Thread Histogram (minimal) | minimal | 47.12 | 40.88 | 1.15x (+15.3%) |
| Memory Trace (minimal) | minimal | 55.61 | 40.88 | 1.36x (+36.0%) |

