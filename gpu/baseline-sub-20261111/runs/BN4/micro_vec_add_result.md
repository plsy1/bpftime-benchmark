
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T14:17:43.269044  

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
| Baseline (tiny) | tiny | 26.17 | - | - |
| Baseline (small) | small | 27.76 | - | - |
| Baseline (large) | large | 32.54 | - | - |
| Baseline (xlarge) | xlarge | 159.42 | - | - |
| Baseline (minimal) | minimal | 41.86 | - | - |
| Empty probe (tiny) | tiny | 34.15 | 26.17 | 1.30x (+30.5%) |
| Empty probe (xlarge) | xlarge | 173.25 | 159.42 | 1.09x (+8.7%) |
| Global timer (xlarge) | xlarge | 178.11 | 159.42 | 1.12x (+11.7%) |
| GPU Ringbuf (small) | small | 40.08 | 27.76 | 1.44x (+44.4%) |
| GPU Ringbuf (xlarge) | xlarge | 216.16 | 159.42 | 1.36x (+35.6%) |
| Per-GPU-thread array (small) | small | 38.43 | 27.76 | 1.38x (+38.4%) |
| Memtrace (small) | small | 32.62 | 27.76 | 1.18x (+17.5%) |
| Memtrace (large) | large | 174.69 | 32.54 | 5.37x (+436.8%) |
| GPU Array map update (small) | small | 37.21 | 27.76 | 1.34x (+34.0%) |
| GPU Array map update (large) | large | 250.35 | 32.54 | 7.69x (+669.4%) |
| GPU Array map lookup (small) | small | 31.20 | 27.76 | 1.12x (+12.4%) |
| GPU Array map lookup (large) | large | 67.52 | 32.54 | 2.07x (+107.5%) |
| CPU Array map update (minimal) | minimal | 51.23 | 26.17 | 1.96x (+95.8%) |
| CPU Array map lookup (minimal) | minimal | 56.13 | 26.17 | 2.14x (+114.5%) |
| CUDA Counter (minimal) | minimal | 2951.66 | 41.86 | 70.51x (+6951.3%) |
| Kernel Retsnoop (minimal) | minimal | 49.28 | 41.86 | 1.18x (+17.7%) |
| Thread Histogram (minimal) | minimal | 48.43 | 41.86 | 1.16x (+15.7%) |
| Memory Trace (minimal) | minimal | 58.36 | 41.86 | 1.39x (+39.4%) |

