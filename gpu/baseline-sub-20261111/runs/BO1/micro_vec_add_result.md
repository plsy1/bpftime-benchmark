
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T14:10:17.698334  

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
| Baseline (tiny) | tiny | 25.64 | - | - |
| Baseline (small) | small | 27.49 | - | - |
| Baseline (large) | large | 31.27 | - | - |
| Baseline (xlarge) | xlarge | 159.60 | - | - |
| Baseline (minimal) | minimal | 37.28 | - | - |
| Empty probe (tiny) | tiny | 33.34 | 25.64 | 1.30x (+30.0%) |
| Empty probe (xlarge) | xlarge | 174.57 | 159.60 | 1.09x (+9.4%) |
| Global timer (xlarge) | xlarge | 177.61 | 159.60 | 1.11x (+11.3%) |
| GPU Ringbuf (small) | small | 38.51 | 27.49 | 1.40x (+40.1%) |
| GPU Ringbuf (xlarge) | xlarge | 210.66 | 159.60 | 1.32x (+32.0%) |
| Per-GPU-thread array (small) | small | 38.57 | 27.49 | 1.40x (+40.3%) |
| Memtrace (small) | small | 37.23 | 27.49 | 1.35x (+35.4%) |
| Memtrace (large) | large | 183.97 | 31.27 | 5.88x (+488.3%) |
| GPU Array map update (small) | small | 40.42 | 27.49 | 1.47x (+47.0%) |
| GPU Array map update (large) | large | 223.69 | 31.27 | 7.15x (+615.4%) |
| GPU Array map lookup (small) | small | 37.08 | 27.49 | 1.35x (+34.9%) |
| GPU Array map lookup (large) | large | 66.39 | 31.27 | 2.12x (+112.3%) |
| CPU Array map update (minimal) | minimal | 33746.80 | 25.64 | 1316.18x (+131517.8%) |
| CPU Array map lookup (minimal) | minimal | 33743.10 | 25.64 | 1316.03x (+131503.4%) |
| CUDA Counter (minimal) | minimal | 337277.75 | 37.28 | 9047.15x (+904615.0%) |
| Kernel Retsnoop (minimal) | minimal | 61.93 | 37.28 | 1.66x (+66.1%) |
| Thread Histogram (minimal) | minimal | 49.79 | 37.28 | 1.34x (+33.6%) |
| Memory Trace (minimal) | minimal | 64.27 | 37.28 | 1.72x (+72.4%) |

