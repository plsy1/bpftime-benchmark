
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T17:02:28.401809  

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
| Baseline (tiny) | tiny | 27.40 | - | - |
| Baseline (small) | small | 27.96 | - | - |
| Baseline (medium) | medium | 27.42 | - | - |
| Baseline (large) | large | 31.15 | - | - |
| Baseline (xlarge) | xlarge | 158.37 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 36.47 | 27.96 | 1.30x (+30.4%) |
| Entry+Exit (small) | small | 32.23 | 27.96 | 1.15x (+15.3%) |
| Entry+Exit (large) | large | 37.69 | 31.15 | 1.21x (+21.0%) |
| Empty probe (large) | large | 36.20 | 31.15 | 1.16x (+16.2%) |
| Empty probe (xlarge) | xlarge | 170.75 | 158.37 | 1.08x (+7.8%) |
| Entry probe (xlarge) | xlarge | 172.38 | 158.37 | 1.09x (+8.8%) |
| Memtrace (tiny) | tiny | 36.22 | 27.40 | 1.32x (+32.2%) |
| Memtrace (large) | large | 172.16 | 31.15 | 5.53x (+452.7%) |
| GPU Array map lookup (xlarge) | xlarge | 445.49 | 158.37 | 2.81x (+181.3%) |
| Per-GPU-thread array (tiny) | tiny | 38.45 | 27.40 | 1.40x (+40.3%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 35.73 | 27.96 | 1.28x (+27.8%) |
| GPU Array map update (small_64x16) | small_64x16 | 38.65 | 27.96 | 1.38x (+38.2%) |
| GPU Array map update (small) | small | 33.08 | 27.96 | 1.18x (+18.3%) |
| GPU Array map update (large) | large | 244.28 | 31.15 | 7.84x (+684.2%) |
| GPU Array map update (xlarge) | xlarge | 1339.04 | 158.37 | 8.46x (+745.5%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 41.41 | 27.96 | 1.48x (+48.1%) |
| GPU Ringbuf (small) | small | 44.77 | 27.96 | 1.60x (+60.1%) |
| GPU Ringbuf (medium) | medium | 41.09 | 27.42 | 1.50x (+49.9%) |

