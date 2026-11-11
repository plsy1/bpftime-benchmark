
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T14:18:53.607065  

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
| Baseline (tiny) | tiny | 27.29 | - | - |
| Baseline (small) | small | 27.23 | - | - |
| Baseline (large) | large | 30.94 | - | - |
| Baseline (xlarge) | xlarge | 159.03 | - | - |
| Baseline (minimal) | minimal | 38.48 | - | - |
| Empty probe (tiny) | tiny | 30.41 | 27.29 | 1.11x (+11.4%) |
| Empty probe (xlarge) | xlarge | 178.11 | 159.03 | 1.12x (+12.0%) |
| Global timer (xlarge) | xlarge | 182.62 | 159.03 | 1.15x (+14.8%) |
| GPU Ringbuf (small) | small | 37.54 | 27.23 | 1.38x (+37.9%) |
| GPU Ringbuf (xlarge) | xlarge | 207.55 | 159.03 | 1.31x (+30.5%) |
| Per-GPU-thread array (small) | small | 41.83 | 27.23 | 1.54x (+53.6%) |
| Memtrace (small) | small | 39.62 | 27.23 | 1.46x (+45.5%) |
| Memtrace (large) | large | 175.04 | 30.94 | 5.66x (+465.7%) |
| GPU Array map update (small) | small | 43.28 | 27.23 | 1.59x (+58.9%) |
| GPU Array map update (large) | large | 221.07 | 30.94 | 7.15x (+614.5%) |
| GPU Array map lookup (small) | small | 33.10 | 27.23 | 1.22x (+21.6%) |
| GPU Array map lookup (large) | large | 66.86 | 30.94 | 2.16x (+116.1%) |
| CPU Array map update (minimal) | minimal | 33702.10 | 27.29 | 1234.96x (+123396.2%) |
| CPU Array map lookup (minimal) | minimal | 33744.62 | 27.29 | 1236.52x (+123552.0%) |
| CUDA Counter (minimal) | minimal | 337320.04 | 38.48 | 8766.11x (+876511.3%) |
| Kernel Retsnoop (minimal) | minimal | 62.85 | 38.48 | 1.63x (+63.3%) |
| Thread Histogram (minimal) | minimal | 49.05 | 38.48 | 1.27x (+27.5%) |
| Memory Trace (minimal) | minimal | 68.39 | 38.48 | 1.78x (+77.7%) |

