
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-12T17:24:06.741308  

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
| Baseline (small) | small | 27.53 | - | - |
| Baseline (large) | large | 31.55 | - | - |
| Baseline (xlarge) | xlarge | 159.28 | - | - |
| Baseline (minimal) | minimal | 39.45 | - | - |
| Empty probe (tiny) | tiny | 32.42 | 26.82 | 1.21x (+20.9%) |
| Empty probe (xlarge) | xlarge | 175.32 | 159.28 | 1.10x (+10.1%) |
| Global timer (xlarge) | xlarge | 178.08 | 159.28 | 1.12x (+11.8%) |
| GPU Ringbuf (small) | small | 43.18 | 27.53 | 1.57x (+56.8%) |
| GPU Ringbuf (xlarge) | xlarge | 215.51 | 159.28 | 1.35x (+35.3%) |
| Per-GPU-thread array (small) | small | 38.63 | 27.53 | 1.40x (+40.3%) |
| Memtrace (small) | small | 33.10 | 27.53 | 1.20x (+20.2%) |
| Memtrace (large) | large | 172.96 | 31.55 | 5.48x (+448.2%) |
| GPU Array map update (small) | small | 35.22 | 27.53 | 1.28x (+27.9%) |
| GPU Array map update (large) | large | 229.32 | 31.55 | 7.27x (+626.8%) |
| GPU Array map lookup (small) | small | 36.06 | 27.53 | 1.31x (+31.0%) |
| GPU Array map lookup (large) | large | 70.96 | 31.55 | 2.25x (+124.9%) |
| CPU Array map update (minimal) | minimal | 44.78 | 26.82 | 1.67x (+67.0%) |
| CPU Array map lookup (minimal) | minimal | 40.43 | 26.82 | 1.51x (+50.7%) |
| CUDA Counter (minimal) | minimal | 2939.91 | 39.45 | 74.52x (+7352.2%) |
| Kernel Retsnoop (minimal) | minimal | 43.99 | 39.45 | 1.12x (+11.5%) |
| Thread Histogram (minimal) | minimal | 40.10 | 39.45 | 1.02x (+1.6%) |
| Memory Trace (minimal) | minimal | 56.90 | 39.45 | 1.44x (+44.2%) |

