
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T16:28:48.256686  

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
| Baseline (small) | small | 27.20 | - | - |
| Baseline (medium) | medium | 27.57 | - | - |
| Baseline (large) | large | 31.20 | - | - |
| Baseline (xlarge) | xlarge | 159.75 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 29.18 | 27.20 | 1.07x (+7.3%) |
| Entry+Exit (small) | small | 33.06 | 27.20 | 1.22x (+21.5%) |
| Entry+Exit (large) | large | 37.89 | 31.20 | 1.21x (+21.4%) |
| Empty probe (large) | large | 35.89 | 31.20 | 1.15x (+15.0%) |
| Empty probe (xlarge) | xlarge | 172.23 | 159.75 | 1.08x (+7.8%) |
| Entry probe (xlarge) | xlarge | 166.53 | 159.75 | 1.04x (+4.2%) |
| Memtrace (tiny) | tiny | 30.17 | 27.08 | 1.11x (+11.4%) |
| Memtrace (large) | large | 174.69 | 31.20 | 5.60x (+459.9%) |
| GPU Array map lookup (xlarge) | xlarge | 442.61 | 159.75 | 2.77x (+177.1%) |
| Per-GPU-thread array (tiny) | tiny | 30.73 | 27.08 | 1.13x (+13.5%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 34.89 | 27.20 | 1.28x (+28.3%) |
| GPU Array map update (small_64x16) | small_64x16 | 37.23 | 27.20 | 1.37x (+36.9%) |
| GPU Array map update (small) | small | 32.14 | 27.20 | 1.18x (+18.2%) |
| GPU Array map update (large) | large | 231.57 | 31.20 | 7.42x (+642.2%) |
| GPU Array map update (xlarge) | xlarge | 1440.53 | 159.75 | 9.02x (+801.7%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 37.27 | 27.20 | 1.37x (+37.0%) |
| GPU Ringbuf (small) | small | 31.46 | 27.20 | 1.16x (+15.7%) |
| GPU Ringbuf (medium) | medium | 40.84 | 27.57 | 1.48x (+48.1%) |

