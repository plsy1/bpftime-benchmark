
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T15:50:02.680561  

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
| Baseline (tiny) | tiny | 27.45 | - | - |
| Baseline (small) | small | 27.35 | - | - |
| Baseline (large) | large | 32.48 | - | - |
| Baseline (xlarge) | xlarge | 158.29 | - | - |
| Baseline (minimal) | minimal | 34.73 | - | - |
| Empty probe (tiny) | tiny | 32.40 | 27.45 | 1.18x (+18.0%) |
| Empty probe (xlarge) | xlarge | 173.73 | 158.29 | 1.10x (+9.8%) |
| Global timer (xlarge) | xlarge | 176.53 | 158.29 | 1.12x (+11.5%) |
| GPU Ringbuf (small) | small | 43.79 | 27.35 | 1.60x (+60.1%) |
| GPU Ringbuf (xlarge) | xlarge | 213.80 | 158.29 | 1.35x (+35.1%) |
| Per-GPU-thread array (small) | small | 33.03 | 27.35 | 1.21x (+20.8%) |
| Memtrace (small) | small | 35.60 | 27.35 | 1.30x (+30.2%) |
| Memtrace (large) | large | 173.69 | 32.48 | 5.35x (+434.8%) |
| GPU Array map update (small) | small | 36.27 | 27.35 | 1.33x (+32.6%) |
| GPU Array map update (large) | large | 244.84 | 32.48 | 7.54x (+653.8%) |
| GPU Array map lookup (small) | small | 35.35 | 27.35 | 1.29x (+29.3%) |
| GPU Array map lookup (large) | large | 66.80 | 32.48 | 2.06x (+105.7%) |
| CPU Array map update (minimal) | minimal | 51.25 | 27.45 | 1.87x (+86.7%) |
| CPU Array map lookup (minimal) | minimal | 52.63 | 27.45 | 1.92x (+91.7%) |
| CUDA Counter (minimal) | minimal | 2886.96 | 34.73 | 83.13x (+8212.6%) |
| Kernel Retsnoop (minimal) | minimal | 48.96 | 34.73 | 1.41x (+41.0%) |
| Thread Histogram (minimal) | minimal | 50.11 | 34.73 | 1.44x (+44.3%) |
| Memory Trace (minimal) | minimal | 60.40 | 34.73 | 1.74x (+73.9%) |

