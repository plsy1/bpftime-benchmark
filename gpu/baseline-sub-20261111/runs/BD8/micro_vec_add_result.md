
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T14:23:55.576729  

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
| Baseline (tiny) | tiny | 26.88 | - | - |
| Baseline (small) | small | 26.71 | - | - |
| Baseline (large) | large | 30.63 | - | - |
| Baseline (xlarge) | xlarge | 157.50 | - | - |
| Baseline (minimal) | minimal | 40.83 | - | - |
| Empty probe (tiny) | tiny | 33.16 | 26.88 | 1.23x (+23.4%) |
| Empty probe (xlarge) | xlarge | 174.30 | 157.50 | 1.11x (+10.7%) |
| Global timer (xlarge) | xlarge | 176.74 | 157.50 | 1.12x (+12.2%) |
| GPU Ringbuf (small) | small | 40.50 | 26.71 | 1.52x (+51.6%) |
| GPU Ringbuf (xlarge) | xlarge | 212.05 | 157.50 | 1.35x (+34.6%) |
| Per-GPU-thread array (small) | small | 37.39 | 26.71 | 1.40x (+40.0%) |
| Memtrace (small) | small | 35.87 | 26.71 | 1.34x (+34.3%) |
| Memtrace (large) | large | 176.64 | 30.63 | 5.77x (+476.7%) |
| GPU Array map update (small) | small | 39.10 | 26.71 | 1.46x (+46.4%) |
| GPU Array map update (large) | large | 235.22 | 30.63 | 7.68x (+667.9%) |
| GPU Array map lookup (small) | small | 38.37 | 26.71 | 1.44x (+43.7%) |
| GPU Array map lookup (large) | large | 64.48 | 30.63 | 2.11x (+110.5%) |
| CPU Array map update (minimal) | minimal | 319.29 | 26.88 | 11.88x (+1087.8%) |
| CPU Array map lookup (minimal) | minimal | 373.76 | 26.88 | 13.90x (+1290.5%) |
| CUDA Counter (minimal) | minimal | 3489.47 | 40.83 | 85.46x (+8446.3%) |
| Kernel Retsnoop (minimal) | minimal | 47.55 | 40.83 | 1.16x (+16.5%) |
| Thread Histogram (minimal) | minimal | 47.07 | 40.83 | 1.15x (+15.3%) |
| Memory Trace (minimal) | minimal | 57.77 | 40.83 | 1.41x (+41.5%) |

