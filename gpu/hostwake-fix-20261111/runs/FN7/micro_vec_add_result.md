
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T16:18:12.584342  

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
| Baseline (tiny) | tiny | 27.98 | - | - |
| Baseline (small) | small | 27.57 | - | - |
| Baseline (large) | large | 30.92 | - | - |
| Baseline (xlarge) | xlarge | 159.24 | - | - |
| Baseline (minimal) | minimal | 40.92 | - | - |
| Empty probe (tiny) | tiny | 34.90 | 27.98 | 1.25x (+24.7%) |
| Empty probe (xlarge) | xlarge | 169.47 | 159.24 | 1.06x (+6.4%) |
| Global timer (xlarge) | xlarge | 177.26 | 159.24 | 1.11x (+11.3%) |
| GPU Ringbuf (small) | small | 41.38 | 27.57 | 1.50x (+50.1%) |
| GPU Ringbuf (xlarge) | xlarge | 214.29 | 159.24 | 1.35x (+34.6%) |
| Per-GPU-thread array (small) | small | 33.46 | 27.57 | 1.21x (+21.4%) |
| Memtrace (small) | small | 32.17 | 27.57 | 1.17x (+16.7%) |
| Memtrace (large) | large | 171.29 | 30.92 | 5.54x (+454.0%) |
| GPU Array map update (small) | small | 37.53 | 27.57 | 1.36x (+36.1%) |
| GPU Array map update (large) | large | 249.50 | 30.92 | 8.07x (+706.9%) |
| GPU Array map lookup (small) | small | 36.18 | 27.57 | 1.31x (+31.2%) |
| GPU Array map lookup (large) | large | 69.47 | 30.92 | 2.25x (+124.7%) |
| CPU Array map update (minimal) | minimal | 54.12 | 27.98 | 1.93x (+93.4%) |
| CPU Array map lookup (minimal) | minimal | 50.28 | 27.98 | 1.80x (+79.7%) |
| CUDA Counter (minimal) | minimal | 2970.69 | 40.92 | 72.60x (+7159.8%) |
| Kernel Retsnoop (minimal) | minimal | 49.21 | 40.92 | 1.20x (+20.3%) |
| Thread Histogram (minimal) | minimal | 51.81 | 40.92 | 1.27x (+26.6%) |
| Memory Trace (minimal) | minimal | 63.92 | 40.92 | 1.56x (+56.2%) |

