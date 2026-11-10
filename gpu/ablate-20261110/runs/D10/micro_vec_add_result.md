
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T18:49:47.940458  

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
| Baseline (tiny) | tiny | 27.43 | - | - |
| Baseline (small) | small | 27.44 | - | - |
| Baseline (medium) | medium | 28.05 | - | - |
| Baseline (large) | large | 31.46 | - | - |
| Baseline (xlarge) | xlarge | 159.78 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 35.96 | 27.44 | 1.31x (+31.0%) |
| Entry+Exit (small) | small | 34.61 | 27.44 | 1.26x (+26.1%) |
| Entry+Exit (large) | large | 39.41 | 31.46 | 1.25x (+25.3%) |
| Empty probe (large) | large | 38.19 | 31.46 | 1.21x (+21.4%) |
| Empty probe (xlarge) | xlarge | 168.49 | 159.78 | 1.05x (+5.5%) |
| Entry probe (xlarge) | xlarge | 170.95 | 159.78 | 1.07x (+7.0%) |
| Memtrace (tiny) | tiny | 32.43 | 27.43 | 1.18x (+18.2%) |
| Memtrace (large) | large | 170.84 | 31.46 | 5.43x (+443.0%) |
| GPU Array map lookup (xlarge) | xlarge | 441.64 | 159.78 | 2.76x (+176.4%) |
| Per-GPU-thread array (tiny) | tiny | 33.96 | 27.43 | 1.24x (+23.8%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 37.47 | 27.44 | 1.37x (+36.6%) |
| GPU Array map update (small_64x16) | small_64x16 | 36.08 | 27.44 | 1.31x (+31.5%) |
| GPU Array map update (small) | small | 34.82 | 27.44 | 1.27x (+26.9%) |
| GPU Array map update (large) | large | 247.08 | 31.46 | 7.85x (+685.4%) |
| GPU Array map update (xlarge) | xlarge | 1512.97 | 159.78 | 9.47x (+846.9%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 41.93 | 27.44 | 1.53x (+52.8%) |
| GPU Ringbuf (small) | small | 41.16 | 27.44 | 1.50x (+50.0%) |
| GPU Ringbuf (medium) | medium | 40.68 | 28.05 | 1.45x (+45.0%) |

