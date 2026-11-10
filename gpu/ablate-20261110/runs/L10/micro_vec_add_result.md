
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T18:53:21.808038  

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
| Baseline (tiny) | tiny | 27.70 | - | - |
| Baseline (small) | small | 27.58 | - | - |
| Baseline (medium) | medium | 27.47 | - | - |
| Baseline (large) | large | 31.39 | - | - |
| Baseline (xlarge) | xlarge | 159.88 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 37.00 | 27.58 | 1.34x (+34.2%) |
| Entry+Exit (small) | small | 29.40 | 27.58 | 1.07x (+6.6%) |
| Entry+Exit (large) | large | 50.63 | 31.39 | 1.61x (+61.3%) |
| Empty probe (large) | large | 51.84 | 31.39 | 1.65x (+65.1%) |
| Empty probe (xlarge) | xlarge | 184.39 | 159.88 | 1.15x (+15.3%) |
| Entry probe (xlarge) | xlarge | 179.15 | 159.88 | 1.12x (+12.1%) |
| Memtrace (tiny) | tiny | 42.30 | 27.70 | 1.53x (+52.7%) |
| Memtrace (large) | large | 180.42 | 31.39 | 5.75x (+474.8%) |
| GPU Array map lookup (xlarge) | xlarge | 453.54 | 159.88 | 2.84x (+183.7%) |
| Per-GPU-thread array (tiny) | tiny | 45.59 | 27.70 | 1.65x (+64.6%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 46.77 | 27.58 | 1.70x (+69.6%) |
| GPU Array map update (small_64x16) | small_64x16 | 49.84 | 27.58 | 1.81x (+80.7%) |
| GPU Array map update (small) | small | 49.55 | 27.58 | 1.80x (+79.7%) |
| GPU Array map update (large) | large | 245.14 | 31.39 | 7.81x (+680.9%) |
| GPU Array map update (xlarge) | xlarge | 1495.74 | 159.88 | 9.36x (+835.5%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 54.02 | 27.58 | 1.96x (+95.9%) |
| GPU Ringbuf (small) | small | 57.31 | 27.58 | 2.08x (+107.8%) |
| GPU Ringbuf (medium) | medium | 50.78 | 27.47 | 1.85x (+84.9%) |

