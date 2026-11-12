
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-12T17:21:44.108203  

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
| Baseline (tiny) | tiny | 27.54 | - | - |
| Baseline (small) | small | 27.37 | - | - |
| Baseline (large) | large | 30.95 | - | - |
| Baseline (xlarge) | xlarge | 160.48 | - | - |
| Baseline (minimal) | minimal | 38.34 | - | - |
| Empty probe (tiny) | tiny | 28.74 | 27.54 | 1.04x (+4.4%) |
| Empty probe (xlarge) | xlarge | 176.72 | 160.48 | 1.10x (+10.1%) |
| Global timer (xlarge) | xlarge | 174.43 | 160.48 | 1.09x (+8.7%) |
| GPU Ringbuf (small) | small | 42.52 | 27.37 | 1.55x (+55.4%) |
| GPU Ringbuf (xlarge) | xlarge | 213.19 | 160.48 | 1.33x (+32.8%) |
| Per-GPU-thread array (small) | small | 34.84 | 27.37 | 1.27x (+27.3%) |
| Memtrace (small) | small | 34.79 | 27.37 | 1.27x (+27.1%) |
| Memtrace (large) | large | 176.27 | 30.95 | 5.70x (+469.5%) |
| GPU Array map update (small) | small | 40.55 | 27.37 | 1.48x (+48.2%) |
| GPU Array map update (large) | large | 238.38 | 30.95 | 7.70x (+670.2%) |
| GPU Array map lookup (small) | small | 31.77 | 27.37 | 1.16x (+16.1%) |
| GPU Array map lookup (large) | large | 71.10 | 30.95 | 2.30x (+129.7%) |
| CPU Array map update (minimal) | minimal | 40.10 | 27.54 | 1.46x (+45.6%) |
| CPU Array map lookup (minimal) | minimal | 50.58 | 27.54 | 1.84x (+83.7%) |
| CUDA Counter (minimal) | minimal | 2955.24 | 38.34 | 77.08x (+7608.0%) |
| Kernel Retsnoop (minimal) | minimal | 46.24 | 38.34 | 1.21x (+20.6%) |
| Thread Histogram (minimal) | minimal | 44.62 | 38.34 | 1.16x (+16.4%) |
| Memory Trace (minimal) | minimal | 61.35 | 38.34 | 1.60x (+60.0%) |

