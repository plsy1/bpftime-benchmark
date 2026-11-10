
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T17:43:16.918373  

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
| Baseline (tiny) | tiny | 25.95 | - | - |
| Baseline (small) | small | 26.30 | - | - |
| Baseline (medium) | medium | 25.66 | - | - |
| Baseline (large) | large | 33.03 | - | - |
| Baseline (xlarge) | xlarge | 160.63 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 33.14 | 26.30 | 1.26x (+26.0%) |
| Entry+Exit (small) | small | 33.65 | 26.30 | 1.28x (+27.9%) |
| Entry+Exit (large) | large | 35.84 | 33.03 | 1.09x (+8.5%) |
| Empty probe (large) | large | 40.72 | 33.03 | 1.23x (+23.3%) |
| Empty probe (xlarge) | xlarge | 176.14 | 160.63 | 1.10x (+9.7%) |
| Entry probe (xlarge) | xlarge | 166.19 | 160.63 | 1.03x (+3.5%) |
| Memtrace (tiny) | tiny | 37.29 | 25.95 | 1.44x (+43.7%) |
| Memtrace (large) | large | 174.87 | 33.03 | 5.29x (+429.4%) |
| GPU Array map lookup (xlarge) | xlarge | 454.60 | 160.63 | 2.83x (+183.0%) |
| Per-GPU-thread array (tiny) | tiny | 35.83 | 25.95 | 1.38x (+38.1%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 33.27 | 26.30 | 1.27x (+26.5%) |
| GPU Array map update (small_64x16) | small_64x16 | 37.80 | 26.30 | 1.44x (+43.7%) |
| GPU Array map update (small) | small | 35.76 | 26.30 | 1.36x (+36.0%) |
| GPU Array map update (large) | large | 235.43 | 33.03 | 7.13x (+612.8%) |
| GPU Array map update (xlarge) | xlarge | 1580.66 | 160.63 | 9.84x (+884.0%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 41.21 | 26.30 | 1.57x (+56.7%) |
| GPU Ringbuf (small) | small | 47.26 | 26.30 | 1.80x (+79.7%) |
| GPU Ringbuf (medium) | medium | 47.08 | 25.66 | 1.83x (+83.5%) |

