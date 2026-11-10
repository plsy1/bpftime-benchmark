
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T18:12:33.430064  

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
| Baseline (tiny) | tiny | 27.60 | - | - |
| Baseline (small) | small | 27.34 | - | - |
| Baseline (medium) | medium | 27.54 | - | - |
| Baseline (large) | large | 31.75 | - | - |
| Baseline (xlarge) | xlarge | 159.32 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 36.62 | 27.34 | 1.34x (+33.9%) |
| Entry+Exit (small) | small | 32.58 | 27.34 | 1.19x (+19.2%) |
| Entry+Exit (large) | large | 49.84 | 31.75 | 1.57x (+57.0%) |
| Empty probe (large) | large | 50.93 | 31.75 | 1.60x (+60.4%) |
| Empty probe (xlarge) | xlarge | 184.96 | 159.32 | 1.16x (+16.1%) |
| Entry probe (xlarge) | xlarge | 180.49 | 159.32 | 1.13x (+13.3%) |
| Memtrace (tiny) | tiny | 43.05 | 27.60 | 1.56x (+56.0%) |
| Memtrace (large) | large | 183.14 | 31.75 | 5.77x (+476.8%) |
| GPU Array map lookup (xlarge) | xlarge | 457.87 | 159.32 | 2.87x (+187.4%) |
| Per-GPU-thread array (tiny) | tiny | 47.02 | 27.60 | 1.70x (+70.4%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 47.36 | 27.34 | 1.73x (+73.2%) |
| GPU Array map update (small_64x16) | small_64x16 | 48.37 | 27.34 | 1.77x (+76.9%) |
| GPU Array map update (small) | small | 44.87 | 27.34 | 1.64x (+64.1%) |
| GPU Array map update (large) | large | 244.40 | 31.75 | 7.70x (+669.8%) |
| GPU Array map update (xlarge) | xlarge | 1496.50 | 159.32 | 9.39x (+839.3%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 54.97 | 27.34 | 2.01x (+101.1%) |
| GPU Ringbuf (small) | small | 54.33 | 27.34 | 1.99x (+98.7%) |
| GPU Ringbuf (medium) | medium | 56.08 | 27.54 | 2.04x (+103.6%) |

