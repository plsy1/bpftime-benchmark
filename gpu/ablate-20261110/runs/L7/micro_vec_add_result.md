
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T17:36:08.437492  

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
| Baseline (tiny) | tiny | 27.73 | - | - |
| Baseline (small) | small | 27.22 | - | - |
| Baseline (medium) | medium | 28.32 | - | - |
| Baseline (large) | large | 31.83 | - | - |
| Baseline (xlarge) | xlarge | 160.26 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 39.75 | 27.22 | 1.46x (+46.0%) |
| Entry+Exit (small) | small | 37.84 | 27.22 | 1.39x (+39.0%) |
| Entry+Exit (large) | large | 51.90 | 31.83 | 1.63x (+63.1%) |
| Empty probe (large) | large | 47.60 | 31.83 | 1.50x (+49.5%) |
| Empty probe (xlarge) | xlarge | 183.16 | 160.26 | 1.14x (+14.3%) |
| Entry probe (xlarge) | xlarge | 177.77 | 160.26 | 1.11x (+10.9%) |
| Memtrace (tiny) | tiny | 40.90 | 27.73 | 1.47x (+47.5%) |
| Memtrace (large) | large | 185.67 | 31.83 | 5.83x (+483.3%) |
| GPU Array map lookup (xlarge) | xlarge | 452.37 | 160.26 | 2.82x (+182.3%) |
| Per-GPU-thread array (tiny) | tiny | 43.33 | 27.73 | 1.56x (+56.3%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 45.09 | 27.22 | 1.66x (+65.7%) |
| GPU Array map update (small_64x16) | small_64x16 | 50.69 | 27.22 | 1.86x (+86.2%) |
| GPU Array map update (small) | small | 47.13 | 27.22 | 1.73x (+73.1%) |
| GPU Array map update (large) | large | 237.11 | 31.83 | 7.45x (+644.9%) |
| GPU Array map update (xlarge) | xlarge | 1474.21 | 160.26 | 9.20x (+819.9%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 52.26 | 27.22 | 1.92x (+92.0%) |
| GPU Ringbuf (small) | small | 53.98 | 27.22 | 1.98x (+98.3%) |
| GPU Ringbuf (medium) | medium | 54.75 | 28.32 | 1.93x (+93.3%) |

