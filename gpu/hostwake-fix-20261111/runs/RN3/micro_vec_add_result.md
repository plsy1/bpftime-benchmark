
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T15:45:18.231413  

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
| Baseline (tiny) | tiny | 25.87 | - | - |
| Baseline (small) | small | 27.60 | - | - |
| Baseline (large) | large | 31.44 | - | - |
| Baseline (xlarge) | xlarge | 159.08 | - | - |
| Baseline (minimal) | minimal | 40.59 | - | - |
| Empty probe (tiny) | tiny | 33.10 | 25.87 | 1.28x (+27.9%) |
| Empty probe (xlarge) | xlarge | 171.73 | 159.08 | 1.08x (+8.0%) |
| Global timer (xlarge) | xlarge | 173.93 | 159.08 | 1.09x (+9.3%) |
| GPU Ringbuf (small) | small | 40.44 | 27.60 | 1.47x (+46.5%) |
| GPU Ringbuf (xlarge) | xlarge | 212.74 | 159.08 | 1.34x (+33.7%) |
| Per-GPU-thread array (small) | small | 37.80 | 27.60 | 1.37x (+37.0%) |
| Memtrace (small) | small | 32.15 | 27.60 | 1.16x (+16.5%) |
| Memtrace (large) | large | 174.48 | 31.44 | 5.55x (+455.0%) |
| GPU Array map update (small) | small | 39.54 | 27.60 | 1.43x (+43.3%) |
| GPU Array map update (large) | large | 243.03 | 31.44 | 7.73x (+673.0%) |
| GPU Array map lookup (small) | small | 35.06 | 27.60 | 1.27x (+27.0%) |
| GPU Array map lookup (large) | large | 70.92 | 31.44 | 2.26x (+125.6%) |
| CPU Array map update (minimal) | minimal | 45.38 | 25.87 | 1.75x (+75.4%) |
| CPU Array map lookup (minimal) | minimal | 51.94 | 25.87 | 2.01x (+100.8%) |
| CUDA Counter (minimal) | minimal | 2926.45 | 40.59 | 72.10x (+7109.8%) |
| Kernel Retsnoop (minimal) | minimal | 47.48 | 40.59 | 1.17x (+17.0%) |
| Thread Histogram (minimal) | minimal | 48.55 | 40.59 | 1.20x (+19.6%) |
| Memory Trace (minimal) | minimal | 55.74 | 40.59 | 1.37x (+37.3%) |

