
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T18:16:08.631555  

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
| Baseline (tiny) | tiny | 27.31 | - | - |
| Baseline (small) | small | 27.52 | - | - |
| Baseline (medium) | medium | 27.32 | - | - |
| Baseline (large) | large | 31.25 | - | - |
| Baseline (xlarge) | xlarge | 159.77 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 32.80 | 27.52 | 1.19x (+19.2%) |
| Entry+Exit (small) | small | 34.38 | 27.52 | 1.25x (+24.9%) |
| Entry+Exit (large) | large | 37.77 | 31.25 | 1.21x (+20.9%) |
| Empty probe (large) | large | 38.08 | 31.25 | 1.22x (+21.9%) |
| Empty probe (xlarge) | xlarge | 170.20 | 159.77 | 1.07x (+6.5%) |
| Entry probe (xlarge) | xlarge | 170.73 | 159.77 | 1.07x (+6.9%) |
| Memtrace (tiny) | tiny | 33.18 | 27.31 | 1.21x (+21.5%) |
| Memtrace (large) | large | 172.55 | 31.25 | 5.52x (+452.2%) |
| GPU Array map lookup (xlarge) | xlarge | 439.22 | 159.77 | 2.75x (+174.9%) |
| Per-GPU-thread array (tiny) | tiny | 34.15 | 27.31 | 1.25x (+25.0%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 34.64 | 27.52 | 1.26x (+25.9%) |
| GPU Array map update (small_64x16) | small_64x16 | 37.66 | 27.52 | 1.37x (+36.8%) |
| GPU Array map update (small) | small | 35.31 | 27.52 | 1.28x (+28.3%) |
| GPU Array map update (large) | large | 246.69 | 31.25 | 7.89x (+689.4%) |
| GPU Array map update (xlarge) | xlarge | 1474.49 | 159.77 | 9.23x (+822.9%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 46.09 | 27.52 | 1.67x (+67.5%) |
| GPU Ringbuf (small) | small | 42.79 | 27.52 | 1.55x (+55.5%) |
| GPU Ringbuf (medium) | medium | 44.55 | 27.32 | 1.63x (+63.1%) |

