
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T17:46:50.228541  

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
| Baseline (tiny) | tiny | 27.71 | - | - |
| Baseline (small) | small | 27.06 | - | - |
| Baseline (medium) | medium | 27.85 | - | - |
| Baseline (large) | large | 31.61 | - | - |
| Baseline (xlarge) | xlarge | 159.39 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 36.15 | 27.06 | 1.34x (+33.6%) |
| Entry+Exit (small) | small | 35.21 | 27.06 | 1.30x (+30.1%) |
| Entry+Exit (large) | large | 39.34 | 31.61 | 1.24x (+24.5%) |
| Empty probe (large) | large | 40.56 | 31.61 | 1.28x (+28.3%) |
| Empty probe (xlarge) | xlarge | 167.96 | 159.39 | 1.05x (+5.4%) |
| Entry probe (xlarge) | xlarge | 168.64 | 159.39 | 1.06x (+5.8%) |
| Memtrace (tiny) | tiny | 32.55 | 27.71 | 1.17x (+17.5%) |
| Memtrace (large) | large | 170.10 | 31.61 | 5.38x (+438.1%) |
| GPU Array map lookup (xlarge) | xlarge | 438.90 | 159.39 | 2.75x (+175.4%) |
| Per-GPU-thread array (tiny) | tiny | 38.75 | 27.71 | 1.40x (+39.8%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 31.27 | 27.06 | 1.16x (+15.6%) |
| GPU Array map update (small_64x16) | small_64x16 | 35.21 | 27.06 | 1.30x (+30.1%) |
| GPU Array map update (small) | small | 36.65 | 27.06 | 1.35x (+35.4%) |
| GPU Array map update (large) | large | 231.79 | 31.61 | 7.33x (+633.3%) |
| GPU Array map update (xlarge) | xlarge | 1398.37 | 159.39 | 8.77x (+777.3%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 30.86 | 27.06 | 1.14x (+14.0%) |
| GPU Ringbuf (small) | small | 33.81 | 27.06 | 1.25x (+24.9%) |
| GPU Ringbuf (medium) | medium | 37.35 | 27.85 | 1.34x (+34.1%) |

