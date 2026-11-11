
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T14:24:42.562847  

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
| Baseline (tiny) | tiny | 27.26 | - | - |
| Baseline (small) | small | 27.52 | - | - |
| Baseline (large) | large | 31.17 | - | - |
| Baseline (xlarge) | xlarge | 159.51 | - | - |
| Baseline (minimal) | minimal | 36.87 | - | - |
| Empty probe (tiny) | tiny | 32.12 | 27.26 | 1.18x (+17.8%) |
| Empty probe (xlarge) | xlarge | 172.61 | 159.51 | 1.08x (+8.2%) |
| Global timer (xlarge) | xlarge | 177.64 | 159.51 | 1.11x (+11.4%) |
| GPU Ringbuf (small) | small | 35.96 | 27.52 | 1.31x (+30.7%) |
| GPU Ringbuf (xlarge) | xlarge | 206.42 | 159.51 | 1.29x (+29.4%) |
| Per-GPU-thread array (small) | small | 37.22 | 27.52 | 1.35x (+35.2%) |
| Memtrace (small) | small | 40.87 | 27.52 | 1.49x (+48.5%) |
| Memtrace (large) | large | 175.46 | 31.17 | 5.63x (+462.9%) |
| GPU Array map update (small) | small | 41.21 | 27.52 | 1.50x (+49.7%) |
| GPU Array map update (large) | large | 219.58 | 31.17 | 7.04x (+604.5%) |
| GPU Array map lookup (small) | small | 38.55 | 27.52 | 1.40x (+40.1%) |
| GPU Array map lookup (large) | large | 66.99 | 31.17 | 2.15x (+114.9%) |
| CPU Array map update (minimal) | minimal | 33713.99 | 27.26 | 1236.76x (+123575.7%) |
| CPU Array map lookup (minimal) | minimal | 33701.16 | 27.26 | 1236.29x (+123528.6%) |
| CUDA Counter (minimal) | minimal | 337388.35 | 36.87 | 9150.76x (+914975.5%) |
| Kernel Retsnoop (minimal) | minimal | 61.67 | 36.87 | 1.67x (+67.3%) |
| Thread Histogram (minimal) | minimal | 44.30 | 36.87 | 1.20x (+20.2%) |
| Memory Trace (minimal) | minimal | 62.80 | 36.87 | 1.70x (+70.3%) |

