
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-12T17:23:19.740919  

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
| Baseline (tiny) | tiny | 26.88 | - | - |
| Baseline (small) | small | 26.02 | - | - |
| Baseline (large) | large | 31.44 | - | - |
| Baseline (xlarge) | xlarge | 161.17 | - | - |
| Baseline (minimal) | minimal | 38.28 | - | - |
| Empty probe (tiny) | tiny | 27.84 | 26.88 | 1.04x (+3.6%) |
| Empty probe (xlarge) | xlarge | 177.08 | 161.17 | 1.10x (+9.9%) |
| Global timer (xlarge) | xlarge | 179.48 | 161.17 | 1.11x (+11.4%) |
| GPU Ringbuf (small) | small | 44.70 | 26.02 | 1.72x (+71.8%) |
| GPU Ringbuf (xlarge) | xlarge | 211.85 | 161.17 | 1.31x (+31.4%) |
| Per-GPU-thread array (small) | small | 36.54 | 26.02 | 1.40x (+40.4%) |
| Memtrace (small) | small | 35.54 | 26.02 | 1.37x (+36.6%) |
| Memtrace (large) | large | 171.57 | 31.44 | 5.46x (+445.7%) |
| GPU Array map update (small) | small | 37.92 | 26.02 | 1.46x (+45.7%) |
| GPU Array map update (large) | large | 232.24 | 31.44 | 7.39x (+638.7%) |
| GPU Array map lookup (small) | small | 35.24 | 26.02 | 1.35x (+35.4%) |
| GPU Array map lookup (large) | large | 66.14 | 31.44 | 2.10x (+110.4%) |
| CPU Array map update (minimal) | minimal | 39.99 | 26.88 | 1.49x (+48.8%) |
| CPU Array map lookup (minimal) | minimal | 47.42 | 26.88 | 1.76x (+76.4%) |
| CUDA Counter (minimal) | minimal | 2891.99 | 38.28 | 75.55x (+7454.8%) |
| Kernel Retsnoop (minimal) | minimal | 48.16 | 38.28 | 1.26x (+25.8%) |
| Thread Histogram (minimal) | minimal | 50.01 | 38.28 | 1.31x (+30.6%) |
| Memory Trace (minimal) | minimal | 62.75 | 38.28 | 1.64x (+63.9%) |

