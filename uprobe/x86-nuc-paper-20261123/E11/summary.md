# x86 E11: map churn on 1/2/4/6 threads (insert at entry, lookup + delete at exit; 4-byte key, 8-byte value)

Session E11-churn-20261123-try1: run_churn.py (from benchmark-results/jetson uprobe/map-threads-20261028, rebuilt for x86 with clang-19 and the official tree's libbpf), backends none / kernel / official 4708f409 / dev 9ccda7e8, workload threads on P cores 0,2,4,6 at 3.5 GHz (threads 5-6 share those cores), 8 s per run, 3 reps, order rotated. 84 runs, every traced run counted exactly the workload's calls with no misses.

Time per traced call per thread (us; median of 3), = threads / total ops per second:

| threads | none | kernel | official | dev | control program (no maps): kernel / official / dev |
|---:|---:|---:|---:|---:|---|
| 1 | 0.007 | 1.790 | 0.369 | 0.357 | 1.696 / 0.301 / 0.313 |
| 2 | 0.007 | 2.069 | 0.543 | 0.410 | 1.948 / 0.370 / 0.368 |
| 4 | 0.006 | 2.137 | 1.653 | 0.421 | 2.043 / 0.382 / 0.387 |
| 6 | 0.010 | 2.970 | 2.853 | 0.578 | 2.834 / 0.547 / 0.531 |

- Official's hash map serializes threads: its map part (churn minus control) grows from 0.07 us at 1 thread to 1.27 us at 4 and 2.31 us at 6, so at 6 threads official costs as much as the kernel per call.
- dev stays within 0.05 us of its control program at every thread count (no serialization).
- The x86 syscall-layer part of E11 was not run: on x86 bpftime's syscall rewriting is zpoline's technique (prior work) and needs vm.mmap_min_addr=0; the paper's syscall contribution is the aarch64 trampoline, measured on the Jetson.
