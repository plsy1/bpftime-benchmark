
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T15:46:29.564907  

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
| Baseline (tiny) | tiny | 27.25 | - | - |
| Baseline (small) | small | 27.78 | - | - |
| Baseline (large) | large | 31.30 | - | - |
| Baseline (xlarge) | xlarge | 160.17 | - | - |
| Baseline (minimal) | minimal | 38.84 | - | - |
| Empty probe (tiny) | tiny | 35.39 | 27.25 | 1.30x (+29.9%) |
| Empty probe (xlarge) | xlarge | 168.53 | 160.17 | 1.05x (+5.2%) |
| Global timer (xlarge) | xlarge | 181.24 | 160.17 | 1.13x (+13.2%) |
| GPU Ringbuf (small) | small | 40.33 | 27.78 | 1.45x (+45.2%) |
| GPU Ringbuf (xlarge) | xlarge | 217.40 | 160.17 | 1.36x (+35.7%) |
| Per-GPU-thread array (small) | small | 32.59 | 27.78 | 1.17x (+17.3%) |
| Memtrace (small) | small | 37.76 | 27.78 | 1.36x (+35.9%) |
| Memtrace (large) | large | 174.72 | 31.30 | 5.58x (+458.2%) |
| GPU Array map update (small) | small | 39.34 | 27.78 | 1.42x (+41.6%) |
| GPU Array map update (large) | large | 244.51 | 31.30 | 7.81x (+681.2%) |
| GPU Array map lookup (small) | small | 34.59 | 27.78 | 1.25x (+24.5%) |
| GPU Array map lookup (large) | large | 67.95 | 31.30 | 2.17x (+117.1%) |
| CPU Array map update (minimal) | minimal | 52.03 | 27.25 | 1.91x (+90.9%) |
| CPU Array map lookup (minimal) | minimal | 47.78 | 27.25 | 1.75x (+75.3%) |
| CUDA Counter (minimal) | minimal | 2866.34 | 38.84 | 73.80x (+7279.9%) |
| Kernel Retsnoop (minimal) | minimal | 46.42 | 38.84 | 1.20x (+19.5%) |
| Thread Histogram (minimal) | minimal | 47.01 | 38.84 | 1.21x (+21.0%) |
| Memory Trace (minimal) | minimal | 54.27 | 38.84 | 1.40x (+39.7%) |

