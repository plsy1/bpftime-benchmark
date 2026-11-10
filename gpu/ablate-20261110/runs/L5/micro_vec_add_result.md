
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T16:58:53.238194  

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
| Baseline (tiny) | tiny | 27.21 | - | - |
| Baseline (small) | small | 27.22 | - | - |
| Baseline (medium) | medium | 27.41 | - | - |
| Baseline (large) | large | 31.31 | - | - |
| Baseline (xlarge) | xlarge | 158.78 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 35.20 | 27.22 | 1.29x (+29.3%) |
| Entry+Exit (small) | small | 39.92 | 27.22 | 1.47x (+46.7%) |
| Entry+Exit (large) | large | 45.22 | 31.31 | 1.44x (+44.4%) |
| Empty probe (large) | large | 52.00 | 31.31 | 1.66x (+66.1%) |
| Empty probe (xlarge) | xlarge | 184.35 | 158.78 | 1.16x (+16.1%) |
| Entry probe (xlarge) | xlarge | 178.74 | 158.78 | 1.13x (+12.6%) |
| Memtrace (tiny) | tiny | 42.40 | 27.21 | 1.56x (+55.8%) |
| Memtrace (large) | large | 184.54 | 31.31 | 5.89x (+489.4%) |
| GPU Array map lookup (xlarge) | xlarge | 455.50 | 158.78 | 2.87x (+186.9%) |
| Per-GPU-thread array (tiny) | tiny | 46.00 | 27.21 | 1.69x (+69.1%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 47.13 | 27.22 | 1.73x (+73.1%) |
| GPU Array map update (small_64x16) | small_64x16 | 47.10 | 27.22 | 1.73x (+73.0%) |
| GPU Array map update (small) | small | 43.15 | 27.22 | 1.59x (+58.5%) |
| GPU Array map update (large) | large | 245.70 | 31.31 | 7.85x (+684.7%) |
| GPU Array map update (xlarge) | xlarge | 1499.74 | 158.78 | 9.45x (+844.5%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 54.92 | 27.22 | 2.02x (+101.8%) |
| GPU Ringbuf (small) | small | 59.91 | 27.22 | 2.20x (+120.1%) |
| GPU Ringbuf (medium) | medium | 57.25 | 27.41 | 2.09x (+108.9%) |

