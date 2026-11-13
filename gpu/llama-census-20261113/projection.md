# Projected direction-1 and direction-2 volume on llama.cpp (TinyLlama Q4_K_M, Orin)

Census runs: llama-bench --no-warmup -r 1, CUDA graphs off, nsys 2026.3.1; nothing measured with bpftime.

## Census totals

| run | launches | thread executions | max threads in one launch | min | GPU kernel time |
|---|---:|---:|---:|---:|---:|
| one 32-token prefill | 488 | 17,630,720 | 4,096,000 | 512 | 23.8 ms |
| one 128-token prefill | 488 | 52,209,152 | 4,096,000 | 512 | 54.0 ms |
| one 512-token prefill | 444 | 192,560,640 | 4,194,304 | 512 | 204.6 ms |
| 128 decode tokens | 54,272 | 5,037,359,104 | 4,096,000 | 256 | 2036.9 ms |

## Probe on: every kernel

| run | tool | D1 ops | D2 ops (host calls) | host-call time, serialized | x GPU time of the run | D2 if aggregated per warp |
|---|---|---:|---:|---:|---:|---:|
| one 32-token prefill | threadhist (bpftime) | 17.63M | 0 | 0 | 0 | - |
| one 32-token prefill | kernelretsnoop (bpftime) | 17.63M | 0 | 0 | 0 | - |
| one 32-token prefill | kernel_trace (bpftime) | 488 | 0 | 0 | 0 | - |
| one 32-token prefill | kernelretsnoop (gpu_ext, warp leader) | 551.0k | 0 | 0 | 0 | - |
| one 32-token prefill | launchlate (gpu_ext, thread 0) | 0 | 0 | 0 | 0 | - |
| one 32-token prefill | launchlate (bpftime, printk per thread) | 0 | 17.63M | 2.9 min-4.6 min | 7,268-11,569x | 551.0k (5.4 s-8.6 s) |
| one 32-token prefill | threadscheduling (bpftime, CPU maps) | 0 | 52.89M | 8.6 min-13.8 min | 21,804-34,708x | 1.65M (16.2 s-25.8 s) |
| one 32-token prefill | cuda-counter (bpftime, CPU maps + printk) | 0 | 141.05M | 23.0 min-36.7 min | 58,144-92,556x | 4.41M (43.2 s-1.1 min) |
| one 128-token prefill | threadhist (bpftime) | 52.21M | 0 | 0 | 0 | - |
| one 128-token prefill | kernelretsnoop (bpftime) | 52.21M | 0 | 0 | 0 | - |
| one 128-token prefill | kernel_trace (bpftime) | 488 | 0 | 0 | 0 | - |
| one 128-token prefill | kernelretsnoop (gpu_ext, warp leader) | 1.63M | 0 | 0 | 0 | - |
| one 128-token prefill | launchlate (gpu_ext, thread 0) | 0 | 0 | 0 | 0 | - |
| one 128-token prefill | launchlate (bpftime, printk per thread) | 0 | 52.21M | 8.5 min-13.6 min | 9,468-15,071x | 1.63M (16.0 s-25.5 s) |
| one 128-token prefill | threadscheduling (bpftime, CPU maps) | 0 | 156.63M | 25.6 min-40.7 min | 28,403-45,213x | 4.89M (48.0 s-1.3 min) |
| one 128-token prefill | cuda-counter (bpftime, CPU maps + printk) | 0 | 417.67M | 1.1 h-1.8 h | 75,742-120,568x | 13.05M (2.1 min-3.4 min) |
| one 512-token prefill | threadhist (bpftime) | 192.56M | 0 | 0 | 0 | - |
| one 512-token prefill | kernelretsnoop (bpftime) | 192.56M | 0 | 0 | 0 | - |
| one 512-token prefill | kernel_trace (bpftime) | 444 | 0 | 0 | 0 | - |
| one 512-token prefill | kernelretsnoop (gpu_ext, warp leader) | 6.02M | 0 | 0 | 0 | - |
| one 512-token prefill | launchlate (gpu_ext, thread 0) | 0 | 0 | 0 | 0 | - |
| one 512-token prefill | launchlate (bpftime, printk per thread) | 0 | 192.56M | 31.5 min-50.1 min | 9,222-14,680x | 6.02M (59.0 s-1.6 min) |
| one 512-token prefill | threadscheduling (bpftime, CPU maps) | 0 | 577.68M | 1.6 h-2.5 h | 27,665-44,039x | 18.05M (2.9 min-4.7 min) |
| one 512-token prefill | cuda-counter (bpftime, CPU maps + printk) | 0 | 1.54G | 4.2 h-6.7 h | 73,774-117,437x | 48.14M (7.9 min-12.5 min) |
| 128 decode tokens | threadhist (bpftime) | 5.04G | 0 | 0 | 0 | - |
| 128 decode tokens | kernelretsnoop (bpftime) | 5.04G | 0 | 0 | 0 | - |
| 128 decode tokens | kernel_trace (bpftime) | 54.3k | 0 | 0 | 0 | - |
| 128 decode tokens | kernelretsnoop (gpu_ext, warp leader) | 157.42M | 0 | 0 | 0 | - |
| 128 decode tokens | launchlate (gpu_ext, thread 0) | 0 | 0 | 0 | 0 | - |
| 128 decode tokens | launchlate (bpftime, printk per thread) | 0 | 5.04G | 13.7 h-21.8 h | 24,236-38,579x | 157.42M (25.7 min-40.9 min) |
| 128 decode tokens | threadscheduling (bpftime, CPU maps) | 0 | 15.11G | 41.1 h-65.5 h | 72,707-115,737x | 472.25M (1.3 h-2.0 h) |
| 128 decode tokens | cuda-counter (bpftime, CPU maps + printk) | 0 | 40.30G | 109.7 h-174.6 h | 193,885-308,633x | 1.26G (3.4 h-5.5 h) |

Per-thread storage (direction 1), probe on every kernel:

| run | slots a launch needs (max threads per launch) | slots written per launch, mean, as share of that | records lost with the default 1,024 slots | threadhist array at max | kernelretsnoop rings at max (16 x 1,032 B per thread) |
|---|---:|---:|---:|---:|---:|
| one 32-token prefill | 4,096,000 | 0.9% | at least 97.2% | 31.2 MiB | 63.2 GiB (segment 128 MiB) |
| one 128-token prefill | 4,096,000 | 2.6% | at least 99.0% | 31.2 MiB | 63.2 GiB (segment 128 MiB) |
| one 512-token prefill | 4,194,304 | 10.3% | at least 99.8% | 32.0 MiB | 64.8 GiB (segment 128 MiB) |
| 128 decode tokens | 4,096,000 | 2.3% | at least 99.0% | 31.2 MiB | 63.2 GiB (segment 128 MiB) |

## Probe on: rope_norm, K (gpu_ext target)

| run | tool | D1 ops | D2 ops (host calls) | host-call time, serialized | x GPU time of the run | D2 if aggregated per warp |
|---|---|---:|---:|---:|---:|---:|
| one 32-token prefill | threadhist (bpftime) | 720.9k | 0 | 0 | 0 | - |
| one 32-token prefill | kernelretsnoop (bpftime) | 720.9k | 0 | 0 | 0 | - |
| one 32-token prefill | kernel_trace (bpftime) | 22 | 0 | 0 | 0 | - |
| one 32-token prefill | kernelretsnoop (gpu_ext, warp leader) | 22.5k | 0 | 0 | 0 | - |
| one 32-token prefill | launchlate (gpu_ext, thread 0) | 0 | 0 | 0 | 0 | - |
| one 32-token prefill | launchlate (bpftime, printk per thread) | 0 | 720.9k | 7.1 s-11.2 s | 297-473x | 22.5k (220.8 ms-351.4 ms) |
| one 32-token prefill | threadscheduling (bpftime, CPU maps) | 0 | 2.16M | 21.2 s-33.7 s | 892-1,419x | 67.6k (662.3 ms-1.1 s) |
| one 32-token prefill | cuda-counter (bpftime, CPU maps + printk) | 0 | 5.77M | 56.5 s-1.5 min | 2,377-3,784x | 180.2k (1.8 s-2.8 s) |
| one 128-token prefill | threadhist (bpftime) | 2.88M | 0 | 0 | 0 | - |
| one 128-token prefill | kernelretsnoop (bpftime) | 2.88M | 0 | 0 | 0 | - |
| one 128-token prefill | kernel_trace (bpftime) | 22 | 0 | 0 | 0 | - |
| one 128-token prefill | kernelretsnoop (gpu_ext, warp leader) | 90.1k | 0 | 0 | 0 | - |
| one 128-token prefill | launchlate (gpu_ext, thread 0) | 0 | 0 | 0 | 0 | - |
| one 128-token prefill | launchlate (bpftime, printk per thread) | 0 | 2.88M | 28.3 s-45.0 s | 523-832x | 90.1k (883.1 ms-1.4 s) |
| one 128-token prefill | threadscheduling (bpftime, CPU maps) | 0 | 8.65M | 1.4 min-2.2 min | 1,569-2,497x | 270.3k (2.6 s-4.2 s) |
| one 128-token prefill | cuda-counter (bpftime, CPU maps + printk) | 0 | 23.07M | 3.8 min-6.0 min | 4,183-6,659x | 720.9k (7.1 s-11.2 s) |
| one 512-token prefill | threadhist (bpftime) | 11.53M | 0 | 0 | 0 | - |
| one 512-token prefill | kernelretsnoop (bpftime) | 11.53M | 0 | 0 | 0 | - |
| one 512-token prefill | kernel_trace (bpftime) | 22 | 0 | 0 | 0 | - |
| one 512-token prefill | kernelretsnoop (gpu_ext, warp leader) | 360.4k | 0 | 0 | 0 | - |
| one 512-token prefill | launchlate (gpu_ext, thread 0) | 0 | 0 | 0 | 0 | - |
| one 512-token prefill | launchlate (bpftime, printk per thread) | 0 | 11.53M | 1.9 min-3.0 min | 552-879x | 360.4k (3.5 s-5.6 s) |
| one 512-token prefill | threadscheduling (bpftime, CPU maps) | 0 | 34.60M | 5.7 min-9.0 min | 1,657-2,638x | 1.08M (10.6 s-16.9 s) |
| one 512-token prefill | cuda-counter (bpftime, CPU maps + printk) | 0 | 92.27M | 15.1 min-24.0 min | 4,419-7,034x | 2.88M (28.3 s-45.0 s) |
| 128 decode tokens | threadhist (bpftime) | 2.88M | 0 | 0 | 0 | - |
| 128 decode tokens | kernelretsnoop (bpftime) | 2.88M | 0 | 0 | 0 | - |
| 128 decode tokens | kernel_trace (bpftime) | 2.8k | 0 | 0 | 0 | - |
| 128 decode tokens | kernelretsnoop (gpu_ext, warp leader) | 90.1k | 0 | 0 | 0 | - |
| 128 decode tokens | launchlate (gpu_ext, thread 0) | 0 | 0 | 0 | 0 | - |
| 128 decode tokens | launchlate (bpftime, printk per thread) | 0 | 2.88M | 28.3 s-45.0 s | 14-22x | 90.1k (883.1 ms-1.4 s) |
| 128 decode tokens | threadscheduling (bpftime, CPU maps) | 0 | 8.65M | 1.4 min-2.2 min | 42-66x | 270.3k (2.6 s-4.2 s) |
| 128 decode tokens | cuda-counter (bpftime, CPU maps + printk) | 0 | 23.07M | 3.8 min-6.0 min | 111-177x | 720.9k (7.1 s-11.2 s) |

Per-thread storage (direction 1), probe on rope_norm, K (gpu_ext target):

| run | slots a launch needs (max threads per launch) | slots written per launch, mean, as share of that | records lost with the default 1,024 slots | threadhist array at max | kernelretsnoop rings at max (16 x 1,032 B per thread) |
|---|---:|---:|---:|---:|---:|
| one 32-token prefill | 32,768 | 100.0% | at least 96.9% | 0.2 MiB | 0.5 GiB (segment 128 MiB) |
| one 128-token prefill | 131,072 | 100.0% | at least 99.2% | 1.0 MiB | 2.0 GiB (segment 128 MiB) |
| one 512-token prefill | 524,288 | 100.0% | at least 99.8% | 4.0 MiB | 8.1 GiB (segment 128 MiB) |
| 128 decode tokens | 1,024 | 100.0% | at least 0.0% | 0.0 MiB | 0.0 GiB (segment 128 MiB) |

## Probe on: rms_norm_f32 (smoke target)

| run | tool | D1 ops | D2 ops (host calls) | host-call time, serialized | x GPU time of the run | D2 if aggregated per warp |
|---|---|---:|---:|---:|---:|---:|
| one 32-token prefill | threadhist (bpftime) | 705.5k | 0 | 0 | 0 | - |
| one 32-token prefill | kernelretsnoop (bpftime) | 705.5k | 0 | 0 | 0 | - |
| one 32-token prefill | kernel_trace (bpftime) | 45 | 0 | 0 | 0 | - |
| one 32-token prefill | kernelretsnoop (gpu_ext, warp leader) | 22.0k | 0 | 0 | 0 | - |
| one 32-token prefill | launchlate (gpu_ext, thread 0) | 0 | 0 | 0 | 0 | - |
| one 32-token prefill | launchlate (bpftime, printk per thread) | 0 | 705.5k | 6.9 s-11.0 s | 291-463x | 22.0k (216.1 ms-343.9 ms) |
| one 32-token prefill | threadscheduling (bpftime, CPU maps) | 0 | 2.12M | 20.7 s-33.0 s | 873-1,389x | 66.1k (648.2 ms-1.0 s) |
| one 32-token prefill | cuda-counter (bpftime, CPU maps + printk) | 0 | 5.64M | 55.3 s-1.5 min | 2,327-3,704x | 176.4k (1.7 s-2.8 s) |
| one 128-token prefill | threadhist (bpftime) | 2.82M | 0 | 0 | 0 | - |
| one 128-token prefill | kernelretsnoop (bpftime) | 2.82M | 0 | 0 | 0 | - |
| one 128-token prefill | kernel_trace (bpftime) | 45 | 0 | 0 | 0 | - |
| one 128-token prefill | kernelretsnoop (gpu_ext, warp leader) | 88.1k | 0 | 0 | 0 | - |
| one 128-token prefill | launchlate (gpu_ext, thread 0) | 0 | 0 | 0 | 0 | - |
| one 128-token prefill | launchlate (bpftime, printk per thread) | 0 | 2.82M | 27.6 s-44.0 s | 511-814x | 88.1k (863.3 ms-1.4 s) |
| one 128-token prefill | threadscheduling (bpftime, CPU maps) | 0 | 8.46M | 1.4 min-2.2 min | 1,534-2,441x | 264.3k (2.6 s-4.1 s) |
| one 128-token prefill | cuda-counter (bpftime, CPU maps + printk) | 0 | 22.55M | 3.7 min-5.9 min | 4,090-6,510x | 704.8k (6.9 s-11.0 s) |
| one 512-token prefill | threadhist (bpftime) | 11.27M | 0 | 0 | 0 | - |
| one 512-token prefill | kernelretsnoop (bpftime) | 11.27M | 0 | 0 | 0 | - |
| one 512-token prefill | kernel_trace (bpftime) | 45 | 0 | 0 | 0 | - |
| one 512-token prefill | kernelretsnoop (gpu_ext, warp leader) | 352.3k | 0 | 0 | 0 | - |
| one 512-token prefill | launchlate (gpu_ext, thread 0) | 0 | 0 | 0 | 0 | - |
| one 512-token prefill | launchlate (bpftime, printk per thread) | 0 | 11.27M | 1.8 min-2.9 min | 540-859x | 352.3k (3.5 s-5.5 s) |
| one 512-token prefill | threadscheduling (bpftime, CPU maps) | 0 | 33.82M | 5.5 min-8.8 min | 1,620-2,578x | 1.06M (10.4 s-16.5 s) |
| one 512-token prefill | cuda-counter (bpftime, CPU maps + printk) | 0 | 90.19M | 14.7 min-23.4 min | 4,319-6,875x | 2.82M (27.6 s-44.0 s) |
| 128 decode tokens | threadhist (bpftime) | 2.95M | 0 | 0 | 0 | - |
| 128 decode tokens | kernelretsnoop (bpftime) | 2.95M | 0 | 0 | 0 | - |
| 128 decode tokens | kernel_trace (bpftime) | 5.8k | 0 | 0 | 0 | - |
| 128 decode tokens | kernelretsnoop (gpu_ext, warp leader) | 92.2k | 0 | 0 | 0 | - |
| 128 decode tokens | launchlate (gpu_ext, thread 0) | 0 | 0 | 0 | 0 | - |
| 128 decode tokens | launchlate (bpftime, printk per thread) | 0 | 2.95M | 28.9 s-46.0 s | 14-23x | 92.2k (903.2 ms-1.4 s) |
| 128 decode tokens | threadscheduling (bpftime, CPU maps) | 0 | 8.85M | 1.4 min-2.3 min | 43-68x | 276.5k (2.7 s-4.3 s) |
| 128 decode tokens | cuda-counter (bpftime, CPU maps + printk) | 0 | 23.59M | 3.9 min-6.1 min | 114-181x | 737.3k (7.2 s-11.5 s) |

Per-thread storage (direction 1), probe on rms_norm_f32 (smoke target):

| run | slots a launch needs (max threads per launch) | slots written per launch, mean, as share of that | records lost with the default 1,024 slots | threadhist array at max | kernelretsnoop rings at max (16 x 1,032 B per thread) |
|---|---:|---:|---:|---:|---:|
| one 32-token prefill | 16,384 | 95.7% | at least 93.5% | 0.1 MiB | 0.3 GiB (segment 128 MiB) |
| one 128-token prefill | 65,536 | 95.6% | at least 98.4% | 0.5 MiB | 1.0 GiB (segment 128 MiB) |
| one 512-token prefill | 262,144 | 95.6% | at least 99.6% | 2.0 MiB | 4.0 GiB (segment 128 MiB) |
| 128 decode tokens | 512 | 100.0% | at least 0.0% | 0.0 MiB | 0.0 GiB (segment 128 MiB) |

