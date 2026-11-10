
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T15:44:25.595779  

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
| Baseline (tiny) | tiny | 27.10 | - | - |
| Baseline (small) | small | 27.35 | - | - |
| Baseline (medium) | medium | 27.18 | - | - |
| Baseline (large) | large | 31.33 | - | - |
| Baseline (xlarge) | xlarge | 161.22 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 31.85 | 27.35 | 1.16x (+16.5%) |
| Entry+Exit (small) | small | 32.26 | 27.35 | 1.18x (+18.0%) |
| Entry+Exit (large) | large | 36.36 | 31.33 | 1.16x (+16.1%) |
| Empty probe (large) | large | 34.35 | 31.33 | 1.10x (+9.6%) |
| Empty probe (xlarge) | xlarge | 169.63 | 161.22 | 1.05x (+5.2%) |
| Entry probe (xlarge) | xlarge | 167.28 | 161.22 | 1.04x (+3.8%) |
| Memtrace (tiny) | tiny | 37.63 | 27.10 | 1.39x (+38.9%) |
| Memtrace (large) | large | 174.45 | 31.33 | 5.57x (+456.8%) |
| GPU Array map lookup (xlarge) | xlarge | 446.17 | 161.22 | 2.77x (+176.7%) |
| Per-GPU-thread array (tiny) | tiny | 34.63 | 27.10 | 1.28x (+27.8%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 40.07 | 27.35 | 1.47x (+46.5%) |
| GPU Array map update (small_64x16) | small_64x16 | 38.68 | 27.35 | 1.41x (+41.4%) |
| GPU Array map update (small) | small | 33.50 | 27.35 | 1.22x (+22.5%) |
| GPU Array map update (large) | large | 248.04 | 31.33 | 7.92x (+691.7%) |
| GPU Array map update (xlarge) | xlarge | 1531.25 | 161.22 | 9.50x (+849.8%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 45.19 | 27.35 | 1.65x (+65.2%) |
| GPU Ringbuf (small) | small | 43.85 | 27.35 | 1.60x (+60.3%) |
| GPU Ringbuf (medium) | medium | 45.69 | 27.18 | 1.68x (+68.1%) |

