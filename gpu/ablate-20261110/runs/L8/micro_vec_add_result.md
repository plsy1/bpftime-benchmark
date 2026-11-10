
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T17:53:56.653238  

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
| Baseline (tiny) | tiny | 27.58 | - | - |
| Baseline (small) | small | 26.80 | - | - |
| Baseline (medium) | medium | 28.93 | - | - |
| Baseline (large) | large | 31.34 | - | - |
| Baseline (xlarge) | xlarge | 158.53 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 37.27 | 26.80 | 1.39x (+39.1%) |
| Entry+Exit (small) | small | 31.86 | 26.80 | 1.19x (+18.9%) |
| Entry+Exit (large) | large | 49.89 | 31.34 | 1.59x (+59.2%) |
| Empty probe (large) | large | 49.74 | 31.34 | 1.59x (+58.7%) |
| Empty probe (xlarge) | xlarge | 182.92 | 158.53 | 1.15x (+15.4%) |
| Entry probe (xlarge) | xlarge | 179.29 | 158.53 | 1.13x (+13.1%) |
| Memtrace (tiny) | tiny | 41.47 | 27.58 | 1.50x (+50.4%) |
| Memtrace (large) | large | 182.25 | 31.34 | 5.82x (+481.5%) |
| GPU Array map lookup (xlarge) | xlarge | 456.75 | 158.53 | 2.88x (+188.1%) |
| Per-GPU-thread array (tiny) | tiny | 46.64 | 27.58 | 1.69x (+69.1%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 48.76 | 26.80 | 1.82x (+81.9%) |
| GPU Array map update (small_64x16) | small_64x16 | 48.42 | 26.80 | 1.81x (+80.7%) |
| GPU Array map update (small) | small | 47.61 | 26.80 | 1.78x (+77.6%) |
| GPU Array map update (large) | large | 240.04 | 31.34 | 7.66x (+665.9%) |
| GPU Array map update (xlarge) | xlarge | 1477.36 | 158.53 | 9.32x (+831.9%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 49.97 | 26.80 | 1.86x (+86.5%) |
| GPU Ringbuf (small) | small | 52.15 | 26.80 | 1.95x (+94.6%) |
| GPU Ringbuf (medium) | medium | 57.44 | 28.93 | 1.99x (+98.5%) |

