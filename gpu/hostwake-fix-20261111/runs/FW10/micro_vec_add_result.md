
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T16:20:33.731647  

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
| Baseline (tiny) | tiny | 27.34 | - | - |
| Baseline (small) | small | 27.26 | - | - |
| Baseline (large) | large | 31.32 | - | - |
| Baseline (xlarge) | xlarge | 158.98 | - | - |
| Baseline (minimal) | minimal | 35.77 | - | - |
| Empty probe (tiny) | tiny | 29.91 | 27.34 | 1.09x (+9.4%) |
| Empty probe (xlarge) | xlarge | 175.39 | 158.98 | 1.10x (+10.3%) |
| Global timer (xlarge) | xlarge | 180.21 | 158.98 | 1.13x (+13.4%) |
| GPU Ringbuf (small) | small | 45.90 | 27.26 | 1.68x (+68.4%) |
| GPU Ringbuf (xlarge) | xlarge | 214.51 | 158.98 | 1.35x (+34.9%) |
| Per-GPU-thread array (small) | small | 36.42 | 27.26 | 1.34x (+33.6%) |
| Memtrace (small) | small | 34.27 | 27.26 | 1.26x (+25.7%) |
| Memtrace (large) | large | 181.28 | 31.32 | 5.79x (+478.8%) |
| GPU Array map update (small) | small | 39.78 | 27.26 | 1.46x (+45.9%) |
| GPU Array map update (large) | large | 223.81 | 31.32 | 7.15x (+614.6%) |
| GPU Array map lookup (small) | small | 35.72 | 27.26 | 1.31x (+31.0%) |
| GPU Array map lookup (large) | large | 67.92 | 31.32 | 2.17x (+116.9%) |
| CPU Array map update (minimal) | minimal | 38.44 | 27.34 | 1.41x (+40.6%) |
| CPU Array map lookup (minimal) | minimal | 40.04 | 27.34 | 1.46x (+46.5%) |
| CUDA Counter (minimal) | minimal | 2870.57 | 35.77 | 80.25x (+7925.1%) |
| Kernel Retsnoop (minimal) | minimal | 48.32 | 35.77 | 1.35x (+35.1%) |
| Thread Histogram (minimal) | minimal | 37.12 | 35.77 | 1.04x (+3.8%) |
| Memory Trace (minimal) | minimal | 59.12 | 35.77 | 1.65x (+65.3%) |

