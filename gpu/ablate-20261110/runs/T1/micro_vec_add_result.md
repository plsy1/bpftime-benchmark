
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T15:33:44.124473  

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
| Baseline (tiny) | tiny | 27.41 | - | - |
| Baseline (small) | small | 27.84 | - | - |
| Baseline (medium) | medium | 27.11 | - | - |
| Baseline (large) | large | 31.24 | - | - |
| Baseline (xlarge) | xlarge | 160.48 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 28.74 | 27.84 | 1.03x (+3.2%) |
| Entry+Exit (small) | small | 31.60 | 27.84 | 1.14x (+13.5%) |
| Entry+Exit (large) | large | 40.39 | 31.24 | 1.29x (+29.3%) |
| Empty probe (large) | large | 38.73 | 31.24 | 1.24x (+24.0%) |
| Empty probe (xlarge) | xlarge | 174.30 | 160.48 | 1.09x (+8.6%) |
| Entry probe (xlarge) | xlarge | 171.14 | 160.48 | 1.07x (+6.6%) |
| Memtrace (tiny) | tiny | 28.91 | 27.41 | 1.05x (+5.5%) |
| Memtrace (large) | large | 173.49 | 31.24 | 5.55x (+455.3%) |
| GPU Array map lookup (xlarge) | xlarge | 440.59 | 160.48 | 2.75x (+174.5%) |
| Per-GPU-thread array (tiny) | tiny | 33.23 | 27.41 | 1.21x (+21.2%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 35.01 | 27.84 | 1.26x (+25.8%) |
| GPU Array map update (small_64x16) | small_64x16 | 37.08 | 27.84 | 1.33x (+33.2%) |
| GPU Array map update (small) | small | 36.71 | 27.84 | 1.32x (+31.9%) |
| GPU Array map update (large) | large | 232.89 | 31.24 | 7.45x (+645.5%) |
| GPU Array map update (xlarge) | xlarge | 1422.18 | 160.48 | 8.86x (+786.2%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 35.49 | 27.84 | 1.27x (+27.5%) |
| GPU Ringbuf (small) | small | 38.82 | 27.84 | 1.39x (+39.4%) |
| GPU Ringbuf (medium) | medium | 38.91 | 27.11 | 1.44x (+43.5%) |

