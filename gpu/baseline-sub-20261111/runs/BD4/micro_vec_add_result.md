
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T14:17:19.548640  

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
| Baseline (tiny) | tiny | 26.12 | - | - |
| Baseline (small) | small | 27.48 | - | - |
| Baseline (large) | large | 30.50 | - | - |
| Baseline (xlarge) | xlarge | 161.07 | - | - |
| Baseline (minimal) | minimal | 39.21 | - | - |
| Empty probe (tiny) | tiny | 34.43 | 26.12 | 1.32x (+31.8%) |
| Empty probe (xlarge) | xlarge | 174.21 | 161.07 | 1.08x (+8.2%) |
| Global timer (xlarge) | xlarge | 183.19 | 161.07 | 1.14x (+13.7%) |
| GPU Ringbuf (small) | small | 45.00 | 27.48 | 1.64x (+63.8%) |
| GPU Ringbuf (xlarge) | xlarge | 211.73 | 161.07 | 1.31x (+31.5%) |
| Per-GPU-thread array (small) | small | 33.72 | 27.48 | 1.23x (+22.7%) |
| Memtrace (small) | small | 31.44 | 27.48 | 1.14x (+14.4%) |
| Memtrace (large) | large | 172.66 | 30.50 | 5.66x (+466.1%) |
| GPU Array map update (small) | small | 40.09 | 27.48 | 1.46x (+45.9%) |
| GPU Array map update (large) | large | 245.45 | 30.50 | 8.05x (+704.8%) |
| GPU Array map lookup (small) | small | 38.20 | 27.48 | 1.39x (+39.0%) |
| GPU Array map lookup (large) | large | 66.09 | 30.50 | 2.17x (+116.7%) |
| CPU Array map update (minimal) | minimal | 333.59 | 26.12 | 12.77x (+1177.1%) |
| CPU Array map lookup (minimal) | minimal | 373.15 | 26.12 | 14.29x (+1328.6%) |
| CUDA Counter (minimal) | minimal | 3485.17 | 39.21 | 88.88x (+8788.5%) |
| Kernel Retsnoop (minimal) | minimal | 47.93 | 39.21 | 1.22x (+22.2%) |
| Thread Histogram (minimal) | minimal | 51.20 | 39.21 | 1.31x (+30.6%) |
| Memory Trace (minimal) | minimal | 57.85 | 39.21 | 1.48x (+47.5%) |

