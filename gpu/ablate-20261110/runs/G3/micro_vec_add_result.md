
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T16:06:36.176054  

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
| Baseline (tiny) | tiny | 27.39 | - | - |
| Baseline (small) | small | 28.50 | - | - |
| Baseline (medium) | medium | 27.18 | - | - |
| Baseline (large) | large | 31.14 | - | - |
| Baseline (xlarge) | xlarge | 161.95 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 32.68 | 28.50 | 1.15x (+14.7%) |
| Entry+Exit (small) | small | 34.40 | 28.50 | 1.21x (+20.7%) |
| Entry+Exit (large) | large | 38.80 | 31.14 | 1.25x (+24.6%) |
| Empty probe (large) | large | 40.76 | 31.14 | 1.31x (+30.9%) |
| Empty probe (xlarge) | xlarge | 171.08 | 161.95 | 1.06x (+5.6%) |
| Entry probe (xlarge) | xlarge | 171.66 | 161.95 | 1.06x (+6.0%) |
| Memtrace (tiny) | tiny | 31.47 | 27.39 | 1.15x (+14.9%) |
| Memtrace (large) | large | 176.66 | 31.14 | 5.67x (+467.3%) |
| GPU Array map lookup (xlarge) | xlarge | 458.13 | 161.95 | 2.83x (+182.9%) |
| Per-GPU-thread array (tiny) | tiny | 37.94 | 27.39 | 1.39x (+38.5%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 31.98 | 28.50 | 1.12x (+12.2%) |
| GPU Array map update (small_64x16) | small_64x16 | 36.36 | 28.50 | 1.28x (+27.6%) |
| GPU Array map update (small) | small | 33.69 | 28.50 | 1.18x (+18.2%) |
| GPU Array map update (large) | large | 231.42 | 31.14 | 7.43x (+643.2%) |
| GPU Array map update (xlarge) | xlarge | 1531.70 | 161.95 | 9.46x (+845.8%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 43.87 | 28.50 | 1.54x (+53.9%) |
| GPU Ringbuf (small) | small | 45.52 | 28.50 | 1.60x (+59.7%) |
| GPU Ringbuf (medium) | medium | 40.29 | 27.18 | 1.48x (+48.2%) |

