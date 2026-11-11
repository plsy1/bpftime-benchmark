
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T16:17:49.244081  

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
| Baseline (tiny) | tiny | 26.94 | - | - |
| Baseline (small) | small | 26.75 | - | - |
| Baseline (large) | large | 31.59 | - | - |
| Baseline (xlarge) | xlarge | 159.41 | - | - |
| Baseline (minimal) | minimal | 40.19 | - | - |
| Empty probe (tiny) | tiny | 34.12 | 26.94 | 1.27x (+26.7%) |
| Empty probe (xlarge) | xlarge | 173.93 | 159.41 | 1.09x (+9.1%) |
| Global timer (xlarge) | xlarge | 179.09 | 159.41 | 1.12x (+12.3%) |
| GPU Ringbuf (small) | small | 41.16 | 26.75 | 1.54x (+53.9%) |
| GPU Ringbuf (xlarge) | xlarge | 214.88 | 159.41 | 1.35x (+34.8%) |
| Per-GPU-thread array (small) | small | 32.32 | 26.75 | 1.21x (+20.8%) |
| Memtrace (small) | small | 31.20 | 26.75 | 1.17x (+16.6%) |
| Memtrace (large) | large | 174.03 | 31.59 | 5.51x (+450.9%) |
| GPU Array map update (small) | small | 38.72 | 26.75 | 1.45x (+44.7%) |
| GPU Array map update (large) | large | 250.00 | 31.59 | 7.91x (+691.4%) |
| GPU Array map lookup (small) | small | 31.95 | 26.75 | 1.19x (+19.4%) |
| GPU Array map lookup (large) | large | 66.47 | 31.59 | 2.10x (+110.4%) |
| CPU Array map update (minimal) | minimal | 51.92 | 26.94 | 1.93x (+92.7%) |
| CPU Array map lookup (minimal) | minimal | 50.94 | 26.94 | 1.89x (+89.1%) |
| CUDA Counter (minimal) | minimal | 2962.21 | 40.19 | 73.71x (+7270.5%) |
| Kernel Retsnoop (minimal) | minimal | 48.94 | 40.19 | 1.22x (+21.8%) |
| Thread Histogram (minimal) | minimal | 51.36 | 40.19 | 1.28x (+27.8%) |
| Memory Trace (minimal) | minimal | 56.34 | 40.19 | 1.40x (+40.2%) |

