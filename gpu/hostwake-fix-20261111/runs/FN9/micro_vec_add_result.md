
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T16:19:46.694607  

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
| Baseline (tiny) | tiny | 27.58 | - | - |
| Baseline (small) | small | 27.55 | - | - |
| Baseline (large) | large | 31.08 | - | - |
| Baseline (xlarge) | xlarge | 158.82 | - | - |
| Baseline (minimal) | minimal | 38.40 | - | - |
| Empty probe (tiny) | tiny | 34.10 | 27.58 | 1.24x (+23.6%) |
| Empty probe (xlarge) | xlarge | 168.38 | 158.82 | 1.06x (+6.0%) |
| Global timer (xlarge) | xlarge | 177.95 | 158.82 | 1.12x (+12.0%) |
| GPU Ringbuf (small) | small | 41.14 | 27.55 | 1.49x (+49.3%) |
| GPU Ringbuf (xlarge) | xlarge | 214.04 | 158.82 | 1.35x (+34.8%) |
| Per-GPU-thread array (small) | small | 36.81 | 27.55 | 1.34x (+33.6%) |
| Memtrace (small) | small | 34.55 | 27.55 | 1.25x (+25.4%) |
| Memtrace (large) | large | 174.65 | 31.08 | 5.62x (+461.9%) |
| GPU Array map update (small) | small | 37.19 | 27.55 | 1.35x (+35.0%) |
| GPU Array map update (large) | large | 245.75 | 31.08 | 7.91x (+690.7%) |
| GPU Array map lookup (small) | small | 33.34 | 27.55 | 1.21x (+21.0%) |
| GPU Array map lookup (large) | large | 63.93 | 31.08 | 2.06x (+105.7%) |
| CPU Array map update (minimal) | minimal | 51.69 | 27.58 | 1.87x (+87.4%) |
| CPU Array map lookup (minimal) | minimal | 49.05 | 27.58 | 1.78x (+77.8%) |
| CUDA Counter (minimal) | minimal | 2948.23 | 38.40 | 76.78x (+7577.7%) |
| Kernel Retsnoop (minimal) | minimal | 46.91 | 38.40 | 1.22x (+22.2%) |
| Thread Histogram (minimal) | minimal | 44.73 | 38.40 | 1.16x (+16.5%) |
| Memory Trace (minimal) | minimal | 60.82 | 38.40 | 1.58x (+58.4%) |

