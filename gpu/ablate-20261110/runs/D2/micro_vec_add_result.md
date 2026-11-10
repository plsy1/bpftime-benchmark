
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T15:37:17.143762  

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
| Baseline (tiny) | tiny | 27.15 | - | - |
| Baseline (small) | small | 27.05 | - | - |
| Baseline (medium) | medium | 27.68 | - | - |
| Baseline (large) | large | 31.39 | - | - |
| Baseline (xlarge) | xlarge | 158.44 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 36.45 | 27.05 | 1.35x (+34.8%) |
| Entry+Exit (small) | small | 35.62 | 27.05 | 1.32x (+31.7%) |
| Entry+Exit (large) | large | 38.72 | 31.39 | 1.23x (+23.4%) |
| Empty probe (large) | large | 38.17 | 31.39 | 1.22x (+21.6%) |
| Empty probe (xlarge) | xlarge | 171.33 | 158.44 | 1.08x (+8.1%) |
| Entry probe (xlarge) | xlarge | 168.25 | 158.44 | 1.06x (+6.2%) |
| Memtrace (tiny) | tiny | 34.25 | 27.15 | 1.26x (+26.2%) |
| Memtrace (large) | large | 174.23 | 31.39 | 5.55x (+455.0%) |
| GPU Array map lookup (xlarge) | xlarge | 445.08 | 158.44 | 2.81x (+180.9%) |
| Per-GPU-thread array (tiny) | tiny | 34.99 | 27.15 | 1.29x (+28.9%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 33.80 | 27.05 | 1.25x (+25.0%) |
| GPU Array map update (small_64x16) | small_64x16 | 33.93 | 27.05 | 1.25x (+25.4%) |
| GPU Array map update (small) | small | 37.68 | 27.05 | 1.39x (+39.3%) |
| GPU Array map update (large) | large | 252.10 | 31.39 | 8.03x (+703.1%) |
| GPU Array map update (xlarge) | xlarge | 1538.18 | 158.44 | 9.71x (+870.8%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 44.18 | 27.05 | 1.63x (+63.3%) |
| GPU Ringbuf (small) | small | 40.80 | 27.05 | 1.51x (+50.8%) |
| GPU Ringbuf (medium) | medium | 46.56 | 27.68 | 1.68x (+68.2%) |

