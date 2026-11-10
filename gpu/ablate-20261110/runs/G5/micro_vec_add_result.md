
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T16:43:51.472992  

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
| Baseline (tiny) | tiny | 27.91 | - | - |
| Baseline (small) | small | 26.63 | - | - |
| Baseline (medium) | medium | 27.88 | - | - |
| Baseline (large) | large | 31.40 | - | - |
| Baseline (xlarge) | xlarge | 159.45 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 29.88 | 26.63 | 1.12x (+12.2%) |
| Entry+Exit (small) | small | 34.98 | 26.63 | 1.31x (+31.4%) |
| Entry+Exit (large) | large | 38.12 | 31.40 | 1.21x (+21.4%) |
| Empty probe (large) | large | 40.66 | 31.40 | 1.29x (+29.5%) |
| Empty probe (xlarge) | xlarge | 173.83 | 159.45 | 1.09x (+9.0%) |
| Entry probe (xlarge) | xlarge | 170.03 | 159.45 | 1.07x (+6.6%) |
| Memtrace (tiny) | tiny | 36.49 | 27.91 | 1.31x (+30.7%) |
| Memtrace (large) | large | 168.97 | 31.40 | 5.38x (+438.1%) |
| GPU Array map lookup (xlarge) | xlarge | 458.86 | 159.45 | 2.88x (+187.8%) |
| Per-GPU-thread array (tiny) | tiny | 34.04 | 27.91 | 1.22x (+22.0%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 39.76 | 26.63 | 1.49x (+49.3%) |
| GPU Array map update (small_64x16) | small_64x16 | 33.28 | 26.63 | 1.25x (+25.0%) |
| GPU Array map update (small) | small | 34.56 | 26.63 | 1.30x (+29.8%) |
| GPU Array map update (large) | large | 233.16 | 31.40 | 7.43x (+642.5%) |
| GPU Array map update (xlarge) | xlarge | 1554.49 | 159.45 | 9.75x (+874.9%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 41.52 | 26.63 | 1.56x (+55.9%) |
| GPU Ringbuf (small) | small | 41.79 | 26.63 | 1.57x (+56.9%) |
| GPU Ringbuf (medium) | medium | 42.37 | 27.88 | 1.52x (+52.0%) |

