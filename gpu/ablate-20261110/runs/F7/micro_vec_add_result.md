
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T17:39:43.195956  

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
| Baseline (tiny) | tiny | 25.65 | - | - |
| Baseline (small) | small | 27.41 | - | - |
| Baseline (medium) | medium | 27.28 | - | - |
| Baseline (large) | large | 31.27 | - | - |
| Baseline (xlarge) | xlarge | 159.65 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 32.48 | 27.41 | 1.18x (+18.5%) |
| Entry+Exit (small) | small | 32.95 | 27.41 | 1.20x (+20.2%) |
| Entry+Exit (large) | large | 34.50 | 31.27 | 1.10x (+10.3%) |
| Empty probe (large) | large | 37.54 | 31.27 | 1.20x (+20.1%) |
| Empty probe (xlarge) | xlarge | 172.23 | 159.65 | 1.08x (+7.9%) |
| Entry probe (xlarge) | xlarge | 167.01 | 159.65 | 1.05x (+4.6%) |
| Memtrace (tiny) | tiny | 33.22 | 25.65 | 1.30x (+29.5%) |
| Memtrace (large) | large | 172.18 | 31.27 | 5.51x (+450.6%) |
| GPU Array map lookup (xlarge) | xlarge | 440.71 | 159.65 | 2.76x (+176.0%) |
| Per-GPU-thread array (tiny) | tiny | 35.36 | 25.65 | 1.38x (+37.9%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 34.82 | 27.41 | 1.27x (+27.0%) |
| GPU Array map update (small_64x16) | small_64x16 | 37.72 | 27.41 | 1.38x (+37.6%) |
| GPU Array map update (small) | small | 38.87 | 27.41 | 1.42x (+41.8%) |
| GPU Array map update (large) | large | 238.08 | 31.27 | 7.61x (+661.4%) |
| GPU Array map update (xlarge) | xlarge | 1478.39 | 159.65 | 9.26x (+826.0%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 44.32 | 27.41 | 1.62x (+61.7%) |
| GPU Ringbuf (small) | small | 42.48 | 27.41 | 1.55x (+55.0%) |
| GPU Ringbuf (medium) | medium | 44.92 | 27.28 | 1.65x (+64.7%) |

