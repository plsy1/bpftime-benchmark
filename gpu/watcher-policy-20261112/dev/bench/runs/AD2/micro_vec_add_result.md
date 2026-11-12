
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-12T17:22:08.323716  

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
| Baseline (tiny) | tiny | 27.62 | - | - |
| Baseline (small) | small | 25.80 | - | - |
| Baseline (large) | large | 30.99 | - | - |
| Baseline (xlarge) | xlarge | 159.37 | - | - |
| Baseline (minimal) | minimal | 43.12 | - | - |
| Empty probe (tiny) | tiny | 29.65 | 27.62 | 1.07x (+7.3%) |
| Empty probe (xlarge) | xlarge | 177.75 | 159.37 | 1.12x (+11.5%) |
| Global timer (xlarge) | xlarge | 178.60 | 159.37 | 1.12x (+12.1%) |
| GPU Ringbuf (small) | small | 42.96 | 25.80 | 1.67x (+66.5%) |
| GPU Ringbuf (xlarge) | xlarge | 211.99 | 159.37 | 1.33x (+33.0%) |
| Per-GPU-thread array (small) | small | 38.62 | 25.80 | 1.50x (+49.7%) |
| Memtrace (small) | small | 36.35 | 25.80 | 1.41x (+40.9%) |
| Memtrace (large) | large | 174.54 | 30.99 | 5.63x (+463.2%) |
| GPU Array map update (small) | small | 36.99 | 25.80 | 1.43x (+43.4%) |
| GPU Array map update (large) | large | 231.95 | 30.99 | 7.48x (+648.5%) |
| GPU Array map lookup (small) | small | 35.30 | 25.80 | 1.37x (+36.8%) |
| GPU Array map lookup (large) | large | 70.27 | 30.99 | 2.27x (+126.8%) |
| CPU Array map update (minimal) | minimal | 46.09 | 27.62 | 1.67x (+66.9%) |
| CPU Array map lookup (minimal) | minimal | 45.04 | 27.62 | 1.63x (+63.1%) |
| CUDA Counter (minimal) | minimal | 2951.20 | 43.12 | 68.44x (+6744.2%) |
| Kernel Retsnoop (minimal) | minimal | 49.18 | 43.12 | 1.14x (+14.1%) |
| Thread Histogram (minimal) | minimal | 45.54 | 43.12 | 1.06x (+5.6%) |
| Memory Trace (minimal) | minimal | 57.04 | 43.12 | 1.32x (+32.3%) |

