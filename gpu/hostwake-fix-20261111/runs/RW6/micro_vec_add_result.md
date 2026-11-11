
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T15:47:40.594308  

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
| Baseline (tiny) | tiny | 26.77 | - | - |
| Baseline (small) | small | 27.73 | - | - |
| Baseline (large) | large | 32.47 | - | - |
| Baseline (xlarge) | xlarge | 160.28 | - | - |
| Baseline (minimal) | minimal | 40.75 | - | - |
| Empty probe (tiny) | tiny | 30.46 | 26.77 | 1.14x (+13.8%) |
| Empty probe (xlarge) | xlarge | 176.63 | 160.28 | 1.10x (+10.2%) |
| Global timer (xlarge) | xlarge | 180.04 | 160.28 | 1.12x (+12.3%) |
| GPU Ringbuf (small) | small | 40.38 | 27.73 | 1.46x (+45.6%) |
| GPU Ringbuf (xlarge) | xlarge | 219.49 | 160.28 | 1.37x (+36.9%) |
| Per-GPU-thread array (small) | small | 37.74 | 27.73 | 1.36x (+36.1%) |
| Memtrace (small) | small | 37.67 | 27.73 | 1.36x (+35.8%) |
| Memtrace (large) | large | 172.65 | 32.47 | 5.32x (+431.7%) |
| GPU Array map update (small) | small | 36.93 | 27.73 | 1.33x (+33.2%) |
| GPU Array map update (large) | large | 241.32 | 32.47 | 7.43x (+643.2%) |
| GPU Array map lookup (small) | small | 32.55 | 27.73 | 1.17x (+17.4%) |
| GPU Array map lookup (large) | large | 68.08 | 32.47 | 2.10x (+109.7%) |
| CPU Array map update (minimal) | minimal | 50.80 | 26.77 | 1.90x (+89.8%) |
| CPU Array map lookup (minimal) | minimal | 48.59 | 26.77 | 1.82x (+81.5%) |
| CUDA Counter (minimal) | minimal | 2885.75 | 40.75 | 70.82x (+6981.6%) |
| Kernel Retsnoop (minimal) | minimal | 44.73 | 40.75 | 1.10x (+9.8%) |
| Thread Histogram (minimal) | minimal | 54.50 | 40.75 | 1.34x (+33.7%) |
| Memory Trace (minimal) | minimal | 62.95 | 40.75 | 1.54x (+54.5%) |

