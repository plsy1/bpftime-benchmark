
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T17:06:02.404219  

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
| Baseline (tiny) | tiny | 27.61 | - | - |
| Baseline (small) | small | 27.57 | - | - |
| Baseline (medium) | medium | 27.72 | - | - |
| Baseline (large) | large | 31.44 | - | - |
| Baseline (xlarge) | xlarge | 158.70 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 36.21 | 27.57 | 1.31x (+31.3%) |
| Entry+Exit (small) | small | 36.47 | 27.57 | 1.32x (+32.3%) |
| Entry+Exit (large) | large | 38.52 | 31.44 | 1.23x (+22.5%) |
| Empty probe (large) | large | 36.41 | 31.44 | 1.16x (+15.8%) |
| Empty probe (xlarge) | xlarge | 174.06 | 158.70 | 1.10x (+9.7%) |
| Entry probe (xlarge) | xlarge | 167.72 | 158.70 | 1.06x (+5.7%) |
| Memtrace (tiny) | tiny | 30.53 | 27.61 | 1.11x (+10.6%) |
| Memtrace (large) | large | 176.62 | 31.44 | 5.62x (+461.8%) |
| GPU Array map lookup (xlarge) | xlarge | 454.01 | 158.70 | 2.86x (+186.1%) |
| Per-GPU-thread array (tiny) | tiny | 38.55 | 27.61 | 1.40x (+39.6%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 36.29 | 27.57 | 1.32x (+31.6%) |
| GPU Array map update (small_64x16) | small_64x16 | 34.78 | 27.57 | 1.26x (+26.2%) |
| GPU Array map update (small) | small | 34.81 | 27.57 | 1.26x (+26.3%) |
| GPU Array map update (large) | large | 230.36 | 31.44 | 7.33x (+632.7%) |
| GPU Array map update (xlarge) | xlarge | 1434.36 | 158.70 | 9.04x (+803.8%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 32.75 | 27.57 | 1.19x (+18.8%) |
| GPU Ringbuf (small) | small | 30.78 | 27.57 | 1.12x (+11.6%) |
| GPU Ringbuf (medium) | medium | 36.65 | 27.72 | 1.32x (+32.2%) |

