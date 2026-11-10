
# CUDA Benchmark Results

**Device:** Orin (nvgpu)  
**Timestamp:** 2026-11-10T10:49:59.422843  

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
| Baseline (minimal) | minimal | 38.60 | - | - |
| E empty (minimal) | minimal | 42.60 | 38.60 | 1.10x (+10.4%) |
| E entry (minimal) | minimal | 47.48 | 38.60 | 1.23x (+23.0%) |
| E exit (minimal) | minimal | 39.18 | 38.60 | 1.02x (+1.5%) |
| E both (minimal) | minimal | 50.63 | 38.60 | 1.31x (+31.2%) |
| E ringbuf (minimal) | minimal | 64.47 | 38.60 | 1.67x (+67.0%) |
| E globaltimer (minimal) | minimal | 35.71 | 38.60 | 0.92x (-7.5%) |
| E pergputd-array-lookup (minimal) | minimal | 53.53 | 38.60 | 1.39x (+38.7%) |
| E memtrace (minimal) | minimal | 58.64 | 38.60 | 1.52x (+51.9%) |
| E gpu-array-update (minimal) | minimal | 79.69 | 38.60 | 2.06x (+106.5%) |
| E gpu-array-lookup (minimal) | minimal | 50.94 | 38.60 | 1.32x (+32.0%) |
| E array-update (minimal) | minimal | 33846.72 | 38.60 | 876.86x (+87585.8%) |
| E array-lookup (minimal) | minimal | 33898.70 | 38.60 | 878.20x (+87720.5%) |
| E hash-update (minimal) | minimal | 33854.15 | 38.60 | 877.05x (+87605.1%) |
| E hash-lookup (minimal) | minimal | 33876.84 | 38.60 | 877.64x (+87663.8%) |
| E hash-delete (minimal) | minimal | 33879.63 | 38.60 | 877.71x (+87671.1%) |
| E CUDA Counter (minimal) | minimal | 338921.23 | 38.60 | 8780.34x (+877934.3%) |
| E Kernel Retsnoop (minimal) | minimal | 84.62 | 38.60 | 2.19x (+119.2%) |

