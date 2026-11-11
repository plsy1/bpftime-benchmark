
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-11T14:11:51.433730  

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
| Baseline (tiny) | tiny | 25.60 | - | - |
| Baseline (small) | small | 26.77 | - | - |
| Baseline (large) | large | 31.11 | - | - |
| Baseline (xlarge) | xlarge | 159.96 | - | - |
| Baseline (minimal) | minimal | 37.19 | - | - |
| Empty probe (tiny) | tiny | 32.14 | 25.60 | 1.26x (+25.5%) |
| Empty probe (xlarge) | xlarge | 173.13 | 159.96 | 1.08x (+8.2%) |
| Global timer (xlarge) | xlarge | 179.90 | 159.96 | 1.12x (+12.5%) |
| GPU Ringbuf (small) | small | 44.00 | 26.77 | 1.64x (+64.4%) |
| GPU Ringbuf (xlarge) | xlarge | 212.00 | 159.96 | 1.33x (+32.5%) |
| Per-GPU-thread array (small) | small | 33.80 | 26.77 | 1.26x (+26.3%) |
| Memtrace (small) | small | 32.92 | 26.77 | 1.23x (+23.0%) |
| Memtrace (large) | large | 174.77 | 31.11 | 5.62x (+461.8%) |
| GPU Array map update (small) | small | 34.83 | 26.77 | 1.30x (+30.1%) |
| GPU Array map update (large) | large | 236.23 | 31.11 | 7.59x (+659.3%) |
| GPU Array map lookup (small) | small | 36.23 | 26.77 | 1.35x (+35.3%) |
| GPU Array map lookup (large) | large | 64.62 | 31.11 | 2.08x (+107.7%) |
| CPU Array map update (minimal) | minimal | 48.51 | 25.60 | 1.89x (+89.5%) |
| CPU Array map lookup (minimal) | minimal | 49.13 | 25.60 | 1.92x (+91.9%) |
| CUDA Counter (minimal) | minimal | 2973.14 | 37.19 | 79.94x (+7894.5%) |
| Kernel Retsnoop (minimal) | minimal | 49.38 | 37.19 | 1.33x (+32.8%) |
| Thread Histogram (minimal) | minimal | 46.72 | 37.19 | 1.26x (+25.6%) |
| Memory Trace (minimal) | minimal | 55.74 | 37.19 | 1.50x (+49.9%) |

