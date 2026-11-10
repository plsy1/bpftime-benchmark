
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T15:59:27.479590  

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
| Baseline (tiny) | tiny | 26.99 | - | - |
| Baseline (small) | small | 27.60 | - | - |
| Baseline (medium) | medium | 28.18 | - | - |
| Baseline (large) | large | 31.24 | - | - |
| Baseline (xlarge) | xlarge | 159.02 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 38.57 | 27.60 | 1.40x (+39.7%) |
| Entry+Exit (small) | small | 38.26 | 27.60 | 1.39x (+38.6%) |
| Entry+Exit (large) | large | 51.77 | 31.24 | 1.66x (+65.7%) |
| Empty probe (large) | large | 51.64 | 31.24 | 1.65x (+65.3%) |
| Empty probe (xlarge) | xlarge | 182.40 | 159.02 | 1.15x (+14.7%) |
| Entry probe (xlarge) | xlarge | 178.90 | 159.02 | 1.13x (+12.5%) |
| Memtrace (tiny) | tiny | 37.48 | 26.99 | 1.39x (+38.9%) |
| Memtrace (large) | large | 182.17 | 31.24 | 5.83x (+483.1%) |
| GPU Array map lookup (xlarge) | xlarge | 453.03 | 159.02 | 2.85x (+184.9%) |
| Per-GPU-thread array (tiny) | tiny | 44.80 | 26.99 | 1.66x (+66.0%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 47.59 | 27.60 | 1.72x (+72.4%) |
| GPU Array map update (small_64x16) | small_64x16 | 49.80 | 27.60 | 1.80x (+80.4%) |
| GPU Array map update (small) | small | 50.21 | 27.60 | 1.82x (+81.9%) |
| GPU Array map update (large) | large | 240.96 | 31.24 | 7.71x (+671.3%) |
| GPU Array map update (xlarge) | xlarge | 1502.56 | 159.02 | 9.45x (+844.9%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 56.26 | 27.60 | 2.04x (+103.8%) |
| GPU Ringbuf (small) | small | 57.98 | 27.60 | 2.10x (+110.1%) |
| GPU Ringbuf (medium) | medium | 56.24 | 28.18 | 2.00x (+99.6%) |

