
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T16:32:21.630061  

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
| Baseline (tiny) | tiny | 26.25 | - | - |
| Baseline (small) | small | 27.50 | - | - |
| Baseline (medium) | medium | 27.55 | - | - |
| Baseline (large) | large | 31.10 | - | - |
| Baseline (xlarge) | xlarge | 159.88 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 34.47 | 27.50 | 1.25x (+25.3%) |
| Entry+Exit (small) | small | 30.06 | 27.50 | 1.09x (+9.3%) |
| Entry+Exit (large) | large | 42.26 | 31.10 | 1.36x (+35.9%) |
| Empty probe (large) | large | 42.59 | 31.10 | 1.37x (+36.9%) |
| Empty probe (xlarge) | xlarge | 179.31 | 159.88 | 1.12x (+12.2%) |
| Entry probe (xlarge) | xlarge | 172.41 | 159.88 | 1.08x (+7.8%) |
| Memtrace (tiny) | tiny | 30.49 | 26.25 | 1.16x (+16.2%) |
| Memtrace (large) | large | 179.61 | 31.10 | 5.78x (+477.5%) |
| GPU Array map lookup (xlarge) | xlarge | 458.41 | 159.88 | 2.87x (+186.7%) |
| Per-GPU-thread array (tiny) | tiny | 37.96 | 26.25 | 1.45x (+44.6%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 39.50 | 27.50 | 1.44x (+43.6%) |
| GPU Array map update (small_64x16) | small_64x16 | 40.62 | 27.50 | 1.48x (+47.7%) |
| GPU Array map update (small) | small | 40.40 | 27.50 | 1.47x (+46.9%) |
| GPU Array map update (large) | large | 223.51 | 31.10 | 7.19x (+618.7%) |
| GPU Array map update (xlarge) | xlarge | 1641.39 | 159.88 | 10.27x (+926.6%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 36.50 | 27.50 | 1.33x (+32.7%) |
| GPU Ringbuf (small) | small | 39.65 | 27.50 | 1.44x (+44.2%) |
| GPU Ringbuf (medium) | medium | 38.90 | 27.55 | 1.41x (+41.2%) |

