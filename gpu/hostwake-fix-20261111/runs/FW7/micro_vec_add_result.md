
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T16:18:36.006641  

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
| Baseline (tiny) | tiny | 27.25 | - | - |
| Baseline (small) | small | 26.18 | - | - |
| Baseline (large) | large | 31.20 | - | - |
| Baseline (xlarge) | xlarge | 158.39 | - | - |
| Baseline (minimal) | minimal | 40.44 | - | - |
| Empty probe (tiny) | tiny | 27.33 | 27.25 | 1.00x (+0.3%) |
| Empty probe (xlarge) | xlarge | 174.55 | 158.39 | 1.10x (+10.2%) |
| Global timer (xlarge) | xlarge | 182.60 | 158.39 | 1.15x (+15.3%) |
| GPU Ringbuf (small) | small | 43.10 | 26.18 | 1.65x (+64.6%) |
| GPU Ringbuf (xlarge) | xlarge | 220.37 | 158.39 | 1.39x (+39.1%) |
| Per-GPU-thread array (small) | small | 38.51 | 26.18 | 1.47x (+47.1%) |
| Memtrace (small) | small | 34.26 | 26.18 | 1.31x (+30.9%) |
| Memtrace (large) | large | 173.13 | 31.20 | 5.55x (+454.9%) |
| GPU Array map update (small) | small | 37.96 | 26.18 | 1.45x (+45.0%) |
| GPU Array map update (large) | large | 219.80 | 31.20 | 7.04x (+604.5%) |
| GPU Array map lookup (small) | small | 33.42 | 26.18 | 1.28x (+27.7%) |
| GPU Array map lookup (large) | large | 69.88 | 31.20 | 2.24x (+124.0%) |
| CPU Array map update (minimal) | minimal | 46.23 | 27.25 | 1.70x (+69.7%) |
| CPU Array map lookup (minimal) | minimal | 46.96 | 27.25 | 1.72x (+72.3%) |
| CUDA Counter (minimal) | minimal | 2865.00 | 40.44 | 70.85x (+6984.6%) |
| Kernel Retsnoop (minimal) | minimal | 46.70 | 40.44 | 1.15x (+15.5%) |
| Thread Histogram (minimal) | minimal | 43.12 | 40.44 | 1.07x (+6.6%) |
| Memory Trace (minimal) | minimal | 61.51 | 40.44 | 1.52x (+52.1%) |

