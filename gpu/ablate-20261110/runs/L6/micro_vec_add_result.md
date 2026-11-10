
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T17:17:31.264301  

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
| Baseline (tiny) | tiny | 27.09 | - | - |
| Baseline (small) | small | 27.51 | - | - |
| Baseline (medium) | medium | 27.49 | - | - |
| Baseline (large) | large | 32.37 | - | - |
| Baseline (xlarge) | xlarge | 158.73 | - | - |
| Entry+Exit (small_64x16) | small_64x16 | 31.12 | 27.51 | 1.13x (+13.1%) |
| Entry+Exit (small) | small | 40.43 | 27.51 | 1.47x (+47.0%) |
| Entry+Exit (large) | large | 50.53 | 32.37 | 1.56x (+56.1%) |
| Empty probe (large) | large | 51.27 | 32.37 | 1.58x (+58.4%) |
| Empty probe (xlarge) | xlarge | 182.72 | 158.73 | 1.15x (+15.1%) |
| Entry probe (xlarge) | xlarge | 182.01 | 158.73 | 1.15x (+14.7%) |
| Memtrace (tiny) | tiny | 38.66 | 27.09 | 1.43x (+42.7%) |
| Memtrace (large) | large | 184.48 | 32.37 | 5.70x (+469.9%) |
| GPU Array map lookup (xlarge) | xlarge | 457.73 | 158.73 | 2.88x (+188.4%) |
| Per-GPU-thread array (tiny) | tiny | 46.01 | 27.09 | 1.70x (+69.8%) |
| Per-GPU-thread array (small_64x16) | small_64x16 | 47.47 | 27.51 | 1.73x (+72.6%) |
| GPU Array map update (small_64x16) | small_64x16 | 47.34 | 27.51 | 1.72x (+72.1%) |
| GPU Array map update (small) | small | 45.16 | 27.51 | 1.64x (+64.2%) |
| GPU Array map update (large) | large | 238.26 | 32.37 | 7.36x (+636.1%) |
| GPU Array map update (xlarge) | xlarge | 1477.68 | 158.73 | 9.31x (+830.9%) |
| GPU Ringbuf (small_64x16) | small_64x16 | 55.88 | 27.51 | 2.03x (+103.1%) |
| GPU Ringbuf (small) | small | 52.90 | 27.51 | 1.92x (+92.3%) |
| GPU Ringbuf (medium) | medium | 54.07 | 27.49 | 1.97x (+96.7%) |

