
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T17:32:34.845732  

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
| Baseline (tiny) | tiny | 27.06 | - | - |
| Baseline (small) | small | 26.80 | - | - |
| Baseline (medium) | medium | 28.13 | - | - |
| Baseline (large) | large | 31.31 | - | - |
| Baseline (xlarge) | xlarge | 160.88 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 35.89 | 26.80 | 1.34x (+33.9%) |
| Entry+Exit (small) | small | 31.46 | 26.80 | 1.17x (+17.4%) |
| Entry+Exit (large) | large | 38.71 | 31.31 | 1.24x (+23.6%) |
| Empty probe (large) | large | 37.81 | 31.31 | 1.21x (+20.8%) |
| Empty probe (xlarge) | xlarge | 169.19 | 160.88 | 1.05x (+5.2%) |
| Entry probe (xlarge) | xlarge | 167.74 | 160.88 | 1.04x (+4.3%) |
| Memtrace (tiny) | tiny | 30.55 | 27.06 | 1.13x (+12.9%) |
| Memtrace (large) | large | 172.65 | 31.31 | 5.51x (+451.4%) |
| GPU Array map lookup (xlarge) | xlarge | 447.03 | 160.88 | 2.78x (+177.9%) |
| Per-GPU-thread array (tiny) | tiny | 36.10 | 27.06 | 1.33x (+33.4%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 37.52 | 26.80 | 1.40x (+40.0%) |
| GPU Array map update (small_64x16) | small_64x16 | 39.40 | 26.80 | 1.47x (+47.0%) |
| GPU Array map update (small) | small | 36.27 | 26.80 | 1.35x (+35.3%) |
| GPU Array map update (large) | large | 251.09 | 31.31 | 8.02x (+701.9%) |
| GPU Array map update (xlarge) | xlarge | 1498.59 | 160.88 | 9.31x (+831.5%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 47.93 | 26.80 | 1.79x (+78.8%) |
| GPU Ringbuf (small) | small | 42.20 | 26.80 | 1.57x (+57.5%) |
| GPU Ringbuf (medium) | medium | 44.02 | 28.13 | 1.56x (+56.5%) |

