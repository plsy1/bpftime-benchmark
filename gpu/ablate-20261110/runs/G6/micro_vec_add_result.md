
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T17:24:40.704023  

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
| Baseline (tiny) | tiny | 27.59 | - | - |
| Baseline (small) | small | 27.43 | - | - |
| Baseline (medium) | medium | 27.51 | - | - |
| Baseline (large) | large | 32.31 | - | - |
| Baseline (xlarge) | xlarge | 159.82 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 31.76 | 27.43 | 1.16x (+15.8%) |
| Entry+Exit (small) | small | 35.75 | 27.43 | 1.30x (+30.3%) |
| Entry+Exit (large) | large | 37.31 | 32.31 | 1.15x (+15.5%) |
| Empty probe (large) | large | 42.62 | 32.31 | 1.32x (+31.9%) |
| Empty probe (xlarge) | xlarge | 172.24 | 159.82 | 1.08x (+7.8%) |
| Entry probe (xlarge) | xlarge | 168.24 | 159.82 | 1.05x (+5.3%) |
| Memtrace (tiny) | tiny | 34.88 | 27.59 | 1.26x (+26.4%) |
| Memtrace (large) | large | 172.02 | 32.31 | 5.32x (+432.4%) |
| GPU Array map lookup (xlarge) | xlarge | 462.00 | 159.82 | 2.89x (+189.1%) |
| Per-GPU-thread array (tiny) | tiny | 38.49 | 27.59 | 1.40x (+39.5%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 37.20 | 27.43 | 1.36x (+35.6%) |
| GPU Array map update (small_64x16) | small_64x16 | 33.02 | 27.43 | 1.20x (+20.4%) |
| GPU Array map update (small) | small | 38.44 | 27.43 | 1.40x (+40.1%) |
| GPU Array map update (large) | large | 236.46 | 32.31 | 7.32x (+631.8%) |
| GPU Array map update (xlarge) | xlarge | 1601.27 | 159.82 | 10.02x (+901.9%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 45.96 | 27.43 | 1.68x (+67.6%) |
| GPU Ringbuf (small) | small | 37.60 | 27.43 | 1.37x (+37.1%) |
| GPU Ringbuf (medium) | medium | 41.69 | 27.51 | 1.52x (+51.5%) |

