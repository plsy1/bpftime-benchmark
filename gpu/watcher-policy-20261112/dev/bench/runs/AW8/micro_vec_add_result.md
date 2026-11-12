
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-12T17:27:17.568675  

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
| Baseline (tiny) | tiny | 27.23 | - | - |
| Baseline (small) | small | 26.37 | - | - |
| Baseline (large) | large | 32.41 | - | - |
| Baseline (xlarge) | xlarge | 159.13 | - | - |
| Baseline (minimal) | minimal | 35.58 | - | - |
| Empty probe (tiny) | tiny | 28.90 | 27.23 | 1.06x (+6.1%) |
| Empty probe (xlarge) | xlarge | 173.24 | 159.13 | 1.09x (+8.9%) |
| Global timer (xlarge) | xlarge | 179.32 | 159.13 | 1.13x (+12.7%) |
| GPU Ringbuf (small) | small | 44.48 | 26.37 | 1.69x (+68.7%) |
| GPU Ringbuf (xlarge) | xlarge | 219.88 | 159.13 | 1.38x (+38.2%) |
| Per-GPU-thread array (small) | small | 38.36 | 26.37 | 1.45x (+45.5%) |
| Memtrace (small) | small | 33.94 | 26.37 | 1.29x (+28.7%) |
| Memtrace (large) | large | 173.57 | 32.41 | 5.36x (+435.5%) |
| GPU Array map update (small) | small | 38.79 | 26.37 | 1.47x (+47.1%) |
| GPU Array map update (large) | large | 234.43 | 32.41 | 7.23x (+623.3%) |
| GPU Array map lookup (small) | small | 31.99 | 26.37 | 1.21x (+21.3%) |
| GPU Array map lookup (large) | large | 64.83 | 32.41 | 2.00x (+100.0%) |
| CPU Array map update (minimal) | minimal | 43.14 | 27.23 | 1.58x (+58.4%) |
| CPU Array map lookup (minimal) | minimal | 37.69 | 27.23 | 1.38x (+38.4%) |
| CUDA Counter (minimal) | minimal | 2849.03 | 35.58 | 80.07x (+7907.4%) |
| Kernel Retsnoop (minimal) | minimal | 47.75 | 35.58 | 1.34x (+34.2%) |
| Thread Histogram (minimal) | minimal | 41.90 | 35.58 | 1.18x (+17.8%) |
| Memory Trace (minimal) | minimal | 57.89 | 35.58 | 1.63x (+62.7%) |

