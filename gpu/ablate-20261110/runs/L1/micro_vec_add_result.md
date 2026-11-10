
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T15:23:02.474685  

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
| Baseline (tiny) | tiny | 27.08 | - | - |
| Baseline (small) | small | 27.42 | - | - |
| Baseline (medium) | medium | 27.50 | - | - |
| Baseline (large) | large | 31.64 | - | - |
| Baseline (xlarge) | xlarge | 157.71 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 36.90 | 27.42 | 1.35x (+34.6%) |
| Entry+Exit (small) | small | 37.43 | 27.42 | 1.37x (+36.5%) |
| Entry+Exit (large) | large | 51.64 | 31.64 | 1.63x (+63.2%) |
| Empty probe (large) | large | 51.92 | 31.64 | 1.64x (+64.1%) |
| Empty probe (xlarge) | xlarge | 182.37 | 157.71 | 1.16x (+15.6%) |
| Entry probe (xlarge) | xlarge | 179.26 | 157.71 | 1.14x (+13.7%) |
| Memtrace (tiny) | tiny | 41.78 | 27.08 | 1.54x (+54.3%) |
| Memtrace (large) | large | 184.05 | 31.64 | 5.82x (+481.7%) |
| GPU Array map lookup (xlarge) | xlarge | 458.65 | 157.71 | 2.91x (+190.8%) |
| Per-GPU-thread array (tiny) | tiny | 46.66 | 27.08 | 1.72x (+72.3%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 48.57 | 27.42 | 1.77x (+77.1%) |
| GPU Array map update (small_64x16) | small_64x16 | 47.33 | 27.42 | 1.73x (+72.6%) |
| GPU Array map update (small) | small | 48.42 | 27.42 | 1.77x (+76.6%) |
| GPU Array map update (large) | large | 245.40 | 31.64 | 7.76x (+675.6%) |
| GPU Array map update (xlarge) | xlarge | 1515.51 | 157.71 | 9.61x (+860.9%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 55.00 | 27.42 | 2.01x (+100.6%) |
| GPU Ringbuf (small) | small | 53.82 | 27.42 | 1.96x (+96.3%) |
| GPU Ringbuf (medium) | medium | 56.80 | 27.50 | 2.07x (+106.5%) |

