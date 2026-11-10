
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T18:45:26.675702  

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
| Baseline (tiny) | tiny | 27.51 | - | - |
| Baseline (small) | small | 27.64 | - | - |
| Baseline (medium) | medium | 27.25 | - | - |
| Baseline (large) | large | 31.18 | - | - |
| Baseline (xlarge) | xlarge | 158.47 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 34.91 | 27.64 | 1.26x (+26.3%) |
| Entry+Exit (small) | small | 31.84 | 27.64 | 1.15x (+15.2%) |
| Entry+Exit (large) | large | 45.76 | 31.18 | 1.47x (+46.8%) |
| Empty probe (large) | large | 44.94 | 31.18 | 1.44x (+44.1%) |
| Empty probe (xlarge) | xlarge | 174.41 | 158.47 | 1.10x (+10.1%) |
| Entry probe (xlarge) | xlarge | 174.35 | 158.47 | 1.10x (+10.0%) |
| Memtrace (tiny) | tiny | 30.99 | 27.51 | 1.13x (+12.6%) |
| Memtrace (large) | large | 177.76 | 31.18 | 5.70x (+470.1%) |
| GPU Array map lookup (xlarge) | xlarge | 456.89 | 158.47 | 2.88x (+188.3%) |
| Per-GPU-thread array (tiny) | tiny | 40.05 | 27.51 | 1.46x (+45.6%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 39.94 | 27.64 | 1.45x (+44.5%) |
| GPU Array map update (small_64x16) | small_64x16 | 42.45 | 27.64 | 1.54x (+53.6%) |
| GPU Array map update (small) | small | 40.35 | 27.64 | 1.46x (+46.0%) |
| GPU Array map update (large) | large | 219.89 | 31.18 | 7.05x (+605.2%) |
| GPU Array map update (xlarge) | xlarge | 1704.48 | 158.47 | 10.76x (+975.6%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 40.41 | 27.64 | 1.46x (+46.2%) |
| GPU Ringbuf (small) | small | 39.19 | 27.64 | 1.42x (+41.8%) |
| GPU Ringbuf (medium) | medium | 38.51 | 27.25 | 1.41x (+41.3%) |

