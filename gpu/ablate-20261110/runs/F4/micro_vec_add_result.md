
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T16:21:40.260913  

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
| Baseline (tiny) | tiny | 27.35 | - | - |
| Baseline (small) | small | 27.70 | - | - |
| Baseline (medium) | medium | 27.84 | - | - |
| Baseline (large) | large | 31.24 | - | - |
| Baseline (xlarge) | xlarge | 160.73 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 32.58 | 27.70 | 1.18x (+17.6%) |
| Entry+Exit (small) | small | 35.78 | 27.70 | 1.29x (+29.2%) |
| Entry+Exit (large) | large | 38.28 | 31.24 | 1.23x (+22.5%) |
| Empty probe (large) | large | 36.25 | 31.24 | 1.16x (+16.0%) |
| Empty probe (xlarge) | xlarge | 172.95 | 160.73 | 1.08x (+7.6%) |
| Entry probe (xlarge) | xlarge | 168.67 | 160.73 | 1.05x (+4.9%) |
| Memtrace (tiny) | tiny | 33.35 | 27.35 | 1.22x (+21.9%) |
| Memtrace (large) | large | 178.91 | 31.24 | 5.73x (+472.7%) |
| GPU Array map lookup (xlarge) | xlarge | 445.12 | 160.73 | 2.77x (+176.9%) |
| Per-GPU-thread array (tiny) | tiny | 34.88 | 27.35 | 1.28x (+27.5%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 32.71 | 27.70 | 1.18x (+18.1%) |
| GPU Array map update (small_64x16) | small_64x16 | 38.56 | 27.70 | 1.39x (+39.2%) |
| GPU Array map update (small) | small | 38.61 | 27.70 | 1.39x (+39.4%) |
| GPU Array map update (large) | large | 245.48 | 31.24 | 7.86x (+685.8%) |
| GPU Array map update (xlarge) | xlarge | 1524.21 | 160.73 | 9.48x (+848.3%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 41.63 | 27.70 | 1.50x (+50.3%) |
| GPU Ringbuf (small) | small | 48.28 | 27.70 | 1.74x (+74.3%) |
| GPU Ringbuf (medium) | medium | 41.50 | 27.84 | 1.49x (+49.1%) |

