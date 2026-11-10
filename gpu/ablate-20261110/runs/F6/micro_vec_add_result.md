
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T17:21:06.533463  

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
| Baseline (tiny) | tiny | 27.44 | - | - |
| Baseline (small) | small | 27.30 | - | - |
| Baseline (medium) | medium | 27.29 | - | - |
| Baseline (large) | large | 32.44 | - | - |
| Baseline (xlarge) | xlarge | 160.04 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 32.50 | 27.30 | 1.19x (+19.0%) |
| Entry+Exit (small) | small | 34.78 | 27.30 | 1.27x (+27.4%) |
| Entry+Exit (large) | large | 40.91 | 32.44 | 1.26x (+26.1%) |
| Empty probe (large) | large | 39.34 | 32.44 | 1.21x (+21.3%) |
| Empty probe (xlarge) | xlarge | 174.39 | 160.04 | 1.09x (+9.0%) |
| Entry probe (xlarge) | xlarge | 168.89 | 160.04 | 1.06x (+5.5%) |
| Memtrace (tiny) | tiny | 38.18 | 27.44 | 1.39x (+39.1%) |
| Memtrace (large) | large | 178.03 | 32.44 | 5.49x (+448.8%) |
| GPU Array map lookup (xlarge) | xlarge | 447.68 | 160.04 | 2.80x (+179.7%) |
| Per-GPU-thread array (tiny) | tiny | 33.21 | 27.44 | 1.21x (+21.0%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 37.22 | 27.30 | 1.36x (+36.3%) |
| GPU Array map update (small_64x16) | small_64x16 | 34.79 | 27.30 | 1.27x (+27.4%) |
| GPU Array map update (small) | small | 39.13 | 27.30 | 1.43x (+43.3%) |
| GPU Array map update (large) | large | 243.42 | 32.44 | 7.50x (+650.4%) |
| GPU Array map update (xlarge) | xlarge | 1378.36 | 160.04 | 8.61x (+761.3%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 46.15 | 27.30 | 1.69x (+69.0%) |
| GPU Ringbuf (small) | small | 42.11 | 27.30 | 1.54x (+54.2%) |
| GPU Ringbuf (medium) | medium | 42.91 | 27.29 | 1.57x (+57.2%) |

