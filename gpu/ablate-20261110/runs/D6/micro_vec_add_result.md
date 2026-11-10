
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T17:13:57.269220  

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
| Baseline (small) | small | 26.60 | - | - |
| Baseline (medium) | medium | 27.76 | - | - |
| Baseline (large) | large | 31.77 | - | - |
| Baseline (xlarge) | xlarge | 158.89 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 36.40 | 26.60 | 1.37x (+36.8%) |
| Entry+Exit (small) | small | 33.47 | 26.60 | 1.26x (+25.8%) |
| Entry+Exit (large) | large | 34.56 | 31.77 | 1.09x (+8.8%) |
| Empty probe (large) | large | 42.96 | 31.77 | 1.35x (+35.2%) |
| Empty probe (xlarge) | xlarge | 174.02 | 158.89 | 1.10x (+9.5%) |
| Entry probe (xlarge) | xlarge | 171.21 | 158.89 | 1.08x (+7.8%) |
| Memtrace (tiny) | tiny | 38.60 | 27.15 | 1.42x (+42.2%) |
| Memtrace (large) | large | 173.73 | 31.77 | 5.47x (+446.8%) |
| GPU Array map lookup (xlarge) | xlarge | 442.88 | 158.89 | 2.79x (+178.7%) |
| Per-GPU-thread array (tiny) | tiny | 38.07 | 27.15 | 1.40x (+40.2%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 35.72 | 26.60 | 1.34x (+34.3%) |
| GPU Array map update (small_64x16) | small_64x16 | 34.68 | 26.60 | 1.30x (+30.4%) |
| GPU Array map update (small) | small | 37.63 | 26.60 | 1.41x (+41.5%) |
| GPU Array map update (large) | large | 242.62 | 31.77 | 7.64x (+663.7%) |
| GPU Array map update (xlarge) | xlarge | 1505.48 | 158.89 | 9.47x (+847.5%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 49.07 | 26.60 | 1.84x (+84.5%) |
| GPU Ringbuf (small) | small | 42.43 | 26.60 | 1.60x (+59.5%) |
| GPU Ringbuf (medium) | medium | 44.53 | 27.76 | 1.60x (+60.4%) |

