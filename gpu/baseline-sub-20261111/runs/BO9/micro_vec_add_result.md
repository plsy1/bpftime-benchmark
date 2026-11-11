
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T14:26:15.512099  

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
| Baseline (tiny) | tiny | 27.50 | - | - |
| Baseline (small) | small | 27.48 | - | - |
| Baseline (large) | large | 31.13 | - | - |
| Baseline (xlarge) | xlarge | 160.33 | - | - |
| Baseline (minimal) | minimal | 38.24 | - | - |
| Empty probe (tiny) | tiny | 35.07 | 27.50 | 1.28x (+27.5%) |
| Empty probe (xlarge) | xlarge | 179.17 | 160.33 | 1.12x (+11.8%) |
| Global timer (xlarge) | xlarge | 181.31 | 160.33 | 1.13x (+13.1%) |
| GPU Ringbuf (small) | small | 36.55 | 27.48 | 1.33x (+33.0%) |
| GPU Ringbuf (xlarge) | xlarge | 205.85 | 160.33 | 1.28x (+28.4%) |
| Per-GPU-thread array (small) | small | 39.09 | 27.48 | 1.42x (+42.2%) |
| Memtrace (small) | small | 38.86 | 27.48 | 1.41x (+41.4%) |
| Memtrace (large) | large | 175.31 | 31.13 | 5.63x (+463.2%) |
| GPU Array map update (small) | small | 40.59 | 27.48 | 1.48x (+47.7%) |
| GPU Array map update (large) | large | 219.92 | 31.13 | 7.06x (+606.5%) |
| GPU Array map lookup (small) | small | 37.45 | 27.48 | 1.36x (+36.3%) |
| GPU Array map lookup (large) | large | 65.55 | 31.13 | 2.11x (+110.6%) |
| CPU Array map update (minimal) | minimal | 33695.88 | 27.50 | 1225.30x (+122430.5%) |
| CPU Array map lookup (minimal) | minimal | 33705.40 | 27.50 | 1225.65x (+122465.1%) |
| CUDA Counter (minimal) | minimal | 337651.25 | 38.24 | 8829.79x (+882879.2%) |
| Kernel Retsnoop (minimal) | minimal | 54.14 | 38.24 | 1.42x (+41.6%) |
| Thread Histogram (minimal) | minimal | 44.97 | 38.24 | 1.18x (+17.6%) |
| Memory Trace (minimal) | minimal | 63.42 | 38.24 | 1.66x (+65.8%) |

