
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T17:50:23.091163  

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
| Baseline (tiny) | tiny | 27.46 | - | - |
| Baseline (small) | small | 27.69 | - | - |
| Baseline (medium) | medium | 27.08 | - | - |
| Baseline (large) | large | 32.42 | - | - |
| Baseline (xlarge) | xlarge | 159.21 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 34.18 | 27.69 | 1.23x (+23.4%) |
| Entry+Exit (small) | small | 29.14 | 27.69 | 1.05x (+5.2%) |
| Entry+Exit (large) | large | 34.88 | 32.42 | 1.08x (+7.6%) |
| Empty probe (large) | large | 37.58 | 32.42 | 1.16x (+15.9%) |
| Empty probe (xlarge) | xlarge | 171.44 | 159.21 | 1.08x (+7.7%) |
| Entry probe (xlarge) | xlarge | 172.78 | 159.21 | 1.09x (+8.5%) |
| Memtrace (tiny) | tiny | 31.39 | 27.46 | 1.14x (+14.3%) |
| Memtrace (large) | large | 173.67 | 32.42 | 5.36x (+435.7%) |
| GPU Array map lookup (xlarge) | xlarge | 460.52 | 159.21 | 2.89x (+189.3%) |
| Per-GPU-thread array (tiny) | tiny | 31.28 | 27.46 | 1.14x (+13.9%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 36.63 | 27.69 | 1.32x (+32.3%) |
| GPU Array map update (small_64x16) | small_64x16 | 35.52 | 27.69 | 1.28x (+28.3%) |
| GPU Array map update (small) | small | 34.96 | 27.69 | 1.26x (+26.3%) |
| GPU Array map update (large) | large | 242.58 | 32.42 | 7.48x (+648.2%) |
| GPU Array map update (xlarge) | xlarge | 1477.75 | 159.21 | 9.28x (+828.2%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 48.38 | 27.69 | 1.75x (+74.7%) |
| GPU Ringbuf (small) | small | 41.92 | 27.69 | 1.51x (+51.4%) |
| GPU Ringbuf (medium) | medium | 41.56 | 27.08 | 1.53x (+53.5%) |

