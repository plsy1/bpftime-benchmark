
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T16:55:19.519476  

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
| Baseline (tiny) | tiny | 27.00 | - | - |
| Baseline (small) | small | 27.12 | - | - |
| Baseline (medium) | medium | 26.33 | - | - |
| Baseline (large) | large | 31.21 | - | - |
| Baseline (xlarge) | xlarge | 159.65 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 32.96 | 27.12 | 1.22x (+21.5%) |
| Entry+Exit (small) | small | 36.27 | 27.12 | 1.34x (+33.7%) |
| Entry+Exit (large) | large | 36.89 | 31.21 | 1.18x (+18.2%) |
| Empty probe (large) | large | 39.15 | 31.21 | 1.25x (+25.4%) |
| Empty probe (xlarge) | xlarge | 173.27 | 159.65 | 1.09x (+8.5%) |
| Entry probe (xlarge) | xlarge | 170.15 | 159.65 | 1.07x (+6.6%) |
| Memtrace (tiny) | tiny | 31.44 | 27.00 | 1.16x (+16.4%) |
| Memtrace (large) | large | 170.82 | 31.21 | 5.47x (+447.3%) |
| GPU Array map lookup (xlarge) | xlarge | 438.47 | 159.65 | 2.75x (+174.6%) |
| Per-GPU-thread array (tiny) | tiny | 35.90 | 27.00 | 1.33x (+33.0%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 35.74 | 27.12 | 1.32x (+31.8%) |
| GPU Array map update (small_64x16) | small_64x16 | 35.67 | 27.12 | 1.32x (+31.5%) |
| GPU Array map update (small) | small | 36.32 | 27.12 | 1.34x (+33.9%) |
| GPU Array map update (large) | large | 246.91 | 31.21 | 7.91x (+691.1%) |
| GPU Array map update (xlarge) | xlarge | 1470.49 | 159.65 | 9.21x (+821.1%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 42.60 | 27.12 | 1.57x (+57.1%) |
| GPU Ringbuf (small) | small | 38.07 | 27.12 | 1.40x (+40.4%) |
| GPU Ringbuf (medium) | medium | 49.53 | 26.33 | 1.88x (+88.1%) |

