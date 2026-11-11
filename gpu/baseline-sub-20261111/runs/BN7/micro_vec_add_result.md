
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T14:23:32.339623  

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
| Baseline (tiny) | tiny | 26.96 | - | - |
| Baseline (small) | small | 27.20 | - | - |
| Baseline (large) | large | 31.34 | - | - |
| Baseline (xlarge) | xlarge | 159.34 | - | - |
| Baseline (minimal) | minimal | 35.40 | - | - |
| Empty probe (tiny) | tiny | 34.34 | 26.96 | 1.27x (+27.4%) |
| Empty probe (xlarge) | xlarge | 173.36 | 159.34 | 1.09x (+8.8%) |
| Global timer (xlarge) | xlarge | 175.47 | 159.34 | 1.10x (+10.1%) |
| GPU Ringbuf (small) | small | 40.84 | 27.20 | 1.50x (+50.1%) |
| GPU Ringbuf (xlarge) | xlarge | 211.76 | 159.34 | 1.33x (+32.9%) |
| Per-GPU-thread array (small) | small | 41.35 | 27.20 | 1.52x (+52.0%) |
| Memtrace (small) | small | 37.68 | 27.20 | 1.39x (+38.5%) |
| Memtrace (large) | large | 172.29 | 31.34 | 5.50x (+449.7%) |
| GPU Array map update (small) | small | 34.51 | 27.20 | 1.27x (+26.9%) |
| GPU Array map update (large) | large | 247.27 | 31.34 | 7.89x (+689.0%) |
| GPU Array map lookup (small) | small | 35.21 | 27.20 | 1.29x (+29.4%) |
| GPU Array map lookup (large) | large | 71.20 | 31.34 | 2.27x (+127.2%) |
| CPU Array map update (minimal) | minimal | 52.34 | 26.96 | 1.94x (+94.1%) |
| CPU Array map lookup (minimal) | minimal | 50.40 | 26.96 | 1.87x (+86.9%) |
| CUDA Counter (minimal) | minimal | 2900.98 | 35.40 | 81.95x (+8094.9%) |
| Kernel Retsnoop (minimal) | minimal | 50.32 | 35.40 | 1.42x (+42.1%) |
| Thread Histogram (minimal) | minimal | 48.39 | 35.40 | 1.37x (+36.7%) |
| Memory Trace (minimal) | minimal | 55.34 | 35.40 | 1.56x (+56.3%) |

