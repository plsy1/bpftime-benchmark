
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T16:13:45.483828  

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
| Baseline (tiny) | tiny | 27.14 | - | - |
| Baseline (small) | small | 28.15 | - | - |
| Baseline (medium) | medium | 27.58 | - | - |
| Baseline (large) | large | 31.91 | - | - |
| Baseline (xlarge) | xlarge | 159.03 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 33.95 | 28.15 | 1.21x (+20.6%) |
| Entry+Exit (small) | small | 30.39 | 28.15 | 1.08x (+8.0%) |
| Entry+Exit (large) | large | 42.89 | 31.91 | 1.34x (+34.4%) |
| Empty probe (large) | large | 45.34 | 31.91 | 1.42x (+42.1%) |
| Empty probe (xlarge) | xlarge | 176.48 | 159.03 | 1.11x (+11.0%) |
| Entry probe (xlarge) | xlarge | 172.45 | 159.03 | 1.08x (+8.4%) |
| Memtrace (tiny) | tiny | 32.41 | 27.14 | 1.19x (+19.4%) |
| Memtrace (large) | large | 174.54 | 31.91 | 5.47x (+447.0%) |
| GPU Array map lookup (xlarge) | xlarge | 463.07 | 159.03 | 2.91x (+191.2%) |
| Per-GPU-thread array (tiny) | tiny | 39.23 | 27.14 | 1.45x (+44.5%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 40.34 | 28.15 | 1.43x (+43.3%) |
| GPU Array map update (small_64x16) | small_64x16 | 40.76 | 28.15 | 1.45x (+44.8%) |
| GPU Array map update (small) | small | 43.17 | 28.15 | 1.53x (+53.4%) |
| GPU Array map update (large) | large | 221.19 | 31.91 | 6.93x (+593.2%) |
| GPU Array map update (xlarge) | xlarge | 1686.80 | 159.03 | 10.61x (+960.7%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 38.30 | 28.15 | 1.36x (+36.1%) |
| GPU Ringbuf (small) | small | 34.29 | 28.15 | 1.22x (+21.8%) |
| GPU Ringbuf (medium) | medium | 37.11 | 27.58 | 1.35x (+34.6%) |

