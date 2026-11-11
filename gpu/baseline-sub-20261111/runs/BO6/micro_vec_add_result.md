
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T14:20:26.952443  

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
| Baseline (tiny) | tiny | 26.82 | - | - |
| Baseline (small) | small | 26.50 | - | - |
| Baseline (large) | large | 31.02 | - | - |
| Baseline (xlarge) | xlarge | 158.04 | - | - |
| Baseline (minimal) | minimal | 39.98 | - | - |
| Empty probe (tiny) | tiny | 34.37 | 26.82 | 1.28x (+28.2%) |
| Empty probe (xlarge) | xlarge | 177.07 | 158.04 | 1.12x (+12.0%) |
| Global timer (xlarge) | xlarge | 175.15 | 158.04 | 1.11x (+10.8%) |
| GPU Ringbuf (small) | small | 37.02 | 26.50 | 1.40x (+39.7%) |
| GPU Ringbuf (xlarge) | xlarge | 210.40 | 158.04 | 1.33x (+33.1%) |
| Per-GPU-thread array (small) | small | 39.91 | 26.50 | 1.51x (+50.6%) |
| Memtrace (small) | small | 39.12 | 26.50 | 1.48x (+47.6%) |
| Memtrace (large) | large | 179.80 | 31.02 | 5.80x (+479.6%) |
| GPU Array map update (small) | small | 42.23 | 26.50 | 1.59x (+59.4%) |
| GPU Array map update (large) | large | 224.63 | 31.02 | 7.24x (+624.1%) |
| GPU Array map lookup (small) | small | 35.09 | 26.50 | 1.32x (+32.4%) |
| GPU Array map lookup (large) | large | 69.85 | 31.02 | 2.25x (+125.2%) |
| CPU Array map update (minimal) | minimal | 33687.58 | 26.82 | 1256.06x (+125506.2%) |
| CPU Array map lookup (minimal) | minimal | 33701.61 | 26.82 | 1256.59x (+125558.5%) |
| CUDA Counter (minimal) | minimal | 337322.61 | 39.98 | 8437.28x (+843628.4%) |
| Kernel Retsnoop (minimal) | minimal | 60.66 | 39.98 | 1.52x (+51.7%) |
| Thread Histogram (minimal) | minimal | 43.41 | 39.98 | 1.09x (+8.6%) |
| Memory Trace (minimal) | minimal | 66.01 | 39.98 | 1.65x (+65.1%) |

