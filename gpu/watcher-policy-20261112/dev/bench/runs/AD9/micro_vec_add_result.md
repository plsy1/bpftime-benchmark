
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-12T17:28:05.189756  

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
| Baseline (tiny) | tiny | 27.22 | - | - |
| Baseline (small) | small | 28.03 | - | - |
| Baseline (large) | large | 31.89 | - | - |
| Baseline (xlarge) | xlarge | 160.65 | - | - |
| Baseline (minimal) | minimal | 40.73 | - | - |
| Empty probe (tiny) | tiny | 30.56 | 27.22 | 1.12x (+12.3%) |
| Empty probe (xlarge) | xlarge | 172.26 | 160.65 | 1.07x (+7.2%) |
| Global timer (xlarge) | xlarge | 179.98 | 160.65 | 1.12x (+12.0%) |
| GPU Ringbuf (small) | small | 49.12 | 28.03 | 1.75x (+75.2%) |
| GPU Ringbuf (xlarge) | xlarge | 212.66 | 160.65 | 1.32x (+32.4%) |
| Per-GPU-thread array (small) | small | 35.66 | 28.03 | 1.27x (+27.2%) |
| Memtrace (small) | small | 35.90 | 28.03 | 1.28x (+28.1%) |
| Memtrace (large) | large | 174.81 | 31.89 | 5.48x (+448.2%) |
| GPU Array map update (small) | small | 39.00 | 28.03 | 1.39x (+39.1%) |
| GPU Array map update (large) | large | 239.29 | 31.89 | 7.50x (+650.4%) |
| GPU Array map lookup (small) | small | 35.97 | 28.03 | 1.28x (+28.3%) |
| GPU Array map lookup (large) | large | 65.16 | 31.89 | 2.04x (+104.3%) |
| CPU Array map update (minimal) | minimal | 41.52 | 27.22 | 1.53x (+52.5%) |
| CPU Array map lookup (minimal) | minimal | 41.12 | 27.22 | 1.51x (+51.1%) |
| CUDA Counter (minimal) | minimal | 2983.69 | 40.73 | 73.26x (+7225.5%) |
| Kernel Retsnoop (minimal) | minimal | 50.30 | 40.73 | 1.23x (+23.5%) |
| Thread Histogram (minimal) | minimal | 42.81 | 40.73 | 1.05x (+5.1%) |
| Memory Trace (minimal) | minimal | 62.53 | 40.73 | 1.54x (+53.5%) |

