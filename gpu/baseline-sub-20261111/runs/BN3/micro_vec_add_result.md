
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T14:14:12.367855  

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
| Baseline (tiny) | tiny | 28.03 | - | - |
| Baseline (small) | small | 26.78 | - | - |
| Baseline (large) | large | 31.78 | - | - |
| Baseline (xlarge) | xlarge | 159.70 | - | - |
| Baseline (minimal) | minimal | 40.20 | - | - |
| Empty probe (tiny) | tiny | 34.98 | 28.03 | 1.25x (+24.8%) |
| Empty probe (xlarge) | xlarge | 171.13 | 159.70 | 1.07x (+7.2%) |
| Global timer (xlarge) | xlarge | 179.21 | 159.70 | 1.12x (+12.2%) |
| GPU Ringbuf (small) | small | 40.94 | 26.78 | 1.53x (+52.9%) |
| GPU Ringbuf (xlarge) | xlarge | 214.32 | 159.70 | 1.34x (+34.2%) |
| Per-GPU-thread array (small) | small | 37.79 | 26.78 | 1.41x (+41.1%) |
| Memtrace (small) | small | 35.72 | 26.78 | 1.33x (+33.4%) |
| Memtrace (large) | large | 173.81 | 31.78 | 5.47x (+446.9%) |
| GPU Array map update (small) | small | 36.38 | 26.78 | 1.36x (+35.8%) |
| GPU Array map update (large) | large | 243.50 | 31.78 | 7.66x (+666.2%) |
| GPU Array map lookup (small) | small | 40.28 | 26.78 | 1.50x (+50.4%) |
| GPU Array map lookup (large) | large | 64.14 | 31.78 | 2.02x (+101.8%) |
| CPU Array map update (minimal) | minimal | 55.51 | 28.03 | 1.98x (+98.0%) |
| CPU Array map lookup (minimal) | minimal | 48.36 | 28.03 | 1.73x (+72.5%) |
| CUDA Counter (minimal) | minimal | 2927.32 | 40.20 | 72.82x (+7181.9%) |
| Kernel Retsnoop (minimal) | minimal | 49.12 | 40.20 | 1.22x (+22.2%) |
| Thread Histogram (minimal) | minimal | 52.62 | 40.20 | 1.31x (+30.9%) |
| Memory Trace (minimal) | minimal | 56.17 | 40.20 | 1.40x (+39.7%) |

