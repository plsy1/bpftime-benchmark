
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T16:25:14.373725  

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
| Baseline (tiny) | tiny | 27.86 | - | - |
| Baseline (small) | small | 27.52 | - | - |
| Baseline (medium) | medium | 27.75 | - | - |
| Baseline (large) | large | 31.84 | - | - |
| Baseline (xlarge) | xlarge | 159.30 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 36.23 | 27.52 | 1.32x (+31.6%) |
| Entry+Exit (small) | small | 31.05 | 27.52 | 1.13x (+12.8%) |
| Entry+Exit (large) | large | 42.96 | 31.84 | 1.35x (+34.9%) |
| Empty probe (large) | large | 39.89 | 31.84 | 1.25x (+25.3%) |
| Empty probe (xlarge) | xlarge | 172.66 | 159.30 | 1.08x (+8.4%) |
| Entry probe (xlarge) | xlarge | 171.19 | 159.30 | 1.07x (+7.5%) |
| Memtrace (tiny) | tiny | 29.32 | 27.86 | 1.05x (+5.2%) |
| Memtrace (large) | large | 168.07 | 31.84 | 5.28x (+427.9%) |
| GPU Array map lookup (xlarge) | xlarge | 454.06 | 159.30 | 2.85x (+185.0%) |
| Per-GPU-thread array (tiny) | tiny | 31.39 | 27.86 | 1.13x (+12.7%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 31.30 | 27.52 | 1.14x (+13.7%) |
| GPU Array map update (small_64x16) | small_64x16 | 37.08 | 27.52 | 1.35x (+34.7%) |
| GPU Array map update (small) | small | 32.31 | 27.52 | 1.17x (+17.4%) |
| GPU Array map update (large) | large | 235.30 | 31.84 | 7.39x (+639.0%) |
| GPU Array map update (xlarge) | xlarge | 1617.58 | 159.30 | 10.15x (+915.4%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 47.07 | 27.52 | 1.71x (+71.0%) |
| GPU Ringbuf (small) | small | 45.42 | 27.52 | 1.65x (+65.0%) |
| GPU Ringbuf (medium) | medium | 43.50 | 27.75 | 1.57x (+56.8%) |

