
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T18:31:11.279455  

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
| Baseline (tiny) | tiny | 27.38 | - | - |
| Baseline (small) | small | 27.92 | - | - |
| Baseline (medium) | medium | 27.21 | - | - |
| Baseline (large) | large | 31.94 | - | - |
| Baseline (xlarge) | xlarge | 160.36 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 31.41 | 27.92 | 1.12x (+12.5%) |
| Entry+Exit (small) | small | 29.71 | 27.92 | 1.06x (+6.4%) |
| Entry+Exit (large) | large | 36.85 | 31.94 | 1.15x (+15.4%) |
| Empty probe (large) | large | 39.24 | 31.94 | 1.23x (+22.9%) |
| Empty probe (xlarge) | xlarge | 174.78 | 160.36 | 1.09x (+9.0%) |
| Entry probe (xlarge) | xlarge | 169.46 | 160.36 | 1.06x (+5.7%) |
| Memtrace (tiny) | tiny | 36.92 | 27.38 | 1.35x (+34.8%) |
| Memtrace (large) | large | 169.86 | 31.94 | 5.32x (+431.8%) |
| GPU Array map lookup (xlarge) | xlarge | 445.75 | 160.36 | 2.78x (+178.0%) |
| Per-GPU-thread array (tiny) | tiny | 38.29 | 27.38 | 1.40x (+39.8%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 40.22 | 27.92 | 1.44x (+44.1%) |
| GPU Array map update (small_64x16) | small_64x16 | 34.00 | 27.92 | 1.22x (+21.8%) |
| GPU Array map update (small) | small | 35.27 | 27.92 | 1.26x (+26.3%) |
| GPU Array map update (large) | large | 248.93 | 31.94 | 7.79x (+679.4%) |
| GPU Array map update (xlarge) | xlarge | 1500.67 | 160.36 | 9.36x (+835.8%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 46.26 | 27.92 | 1.66x (+65.7%) |
| GPU Ringbuf (small) | small | 44.10 | 27.92 | 1.58x (+58.0%) |
| GPU Ringbuf (medium) | medium | 42.43 | 27.21 | 1.56x (+55.9%) |

