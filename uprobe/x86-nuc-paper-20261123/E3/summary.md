# x86 E3: official uprobe suite, kernel / official 4708f409 / dev 9ccda7e8

Session E3-full10-20261123-try1: CPU 2 at 4.4 GHz (turbo on, C6+ off), benchmark.py --iter 10, order bookend, O, D, bookend, D, O, bookend. Values are medians of the 10 samples per run, then the median over runs (ns per call; map rows are per 1,000 operations per call). Kernel = median of the four in-run kernel groups; bookends agree within 3.5% on every row.

| op | kernel | bookends | official | O/K | dev | D/K | cycles kernel / O / D @4.4 GHz |
|---|---:|---:|---:|---:|---:|---:|---|
| uprobe_uretprobe | 1344.1 | 1344.1 | 211.5 | 0.16 | 224.5 | 0.17 | 5914 / 931 / 988 |
| uretprobe | 1295.6 | 1295.2 | 182.6 | 0.14 | 197.0 | 0.15 | 5701 / 803 / 867 |
| uprobe | 1110.0 | 1107.8 | 122.8 | 0.11 | 131.2 | 0.12 | 4884 / 540 / 577 |
| read | 19335.5 | 19314.5 | 1694.4 | 0.09 | 2533.3 | 0.13 | 85076 / 7455 / 11147 |
| write | 19149.8 | 19127.7 | 1507.1 | 0.08 | 2719.6 | 0.14 | 84259 / 6631 / 11966 |
| hash_map_update | 34807.3 | 34521.0 | 23906.9 | 0.69 | 10801.2 | 0.31 |  |
| hash_map_lookup | 10899.2 | 10528.3 | 21143.1 | 1.94 | 4814.3 | 0.44 |  |
| hash_map_delete | 34319.8 | 34248.5 | 17773.1 | 0.52 | 14697.4 | 0.43 |  |
| array_map_update | 7424.3 | 7425.3 | 8920.7 | 1.20 | 4106.4 | 0.55 |  |
| array_map_lookup | 1818.1 | 1816.2 | 5776.9 | 3.18 | 473.4 | 0.26 |  |
| array_map_delete (EINVAL path, excluded) | 1747.8 | 1749.2 | 5975.3 | 3.42 | 6578.1 | 3.76 |  |
| per_cpu_hash_map_update | 25871.2 | 25505.6 | 70706.7 | 2.73 | 11808.6 | 0.46 |  |
| per_cpu_hash_map_lookup | 10871.1 | 10707.6 | 45147.8 | 4.15 | 7221.3 | 0.66 |  |
| per_cpu_hash_map_delete | 112352.7 | 112430.2 | 64535.3 | 0.57 | 25839.6 | 0.23 |  |
| per_cpu_array_map_update | 7470.9 | 7428.3 | 18177.1 | 2.43 | 5480.8 | 0.73 |  |
| per_cpu_array_map_lookup | 1821.2 | 1818.6 | 9867.5 | 5.42 | 1517.8 | 0.83 |  |
| per_cpu_array_map_delete (EINVAL path, excluded) | 1749.7 | 1744.3 | 5996.4 | 3.43 | 6593.7 | 3.77 |  |

Notes:
- array_map_lookup rows do not use the lookup result (not a valid speedup claim; plan D3/E10 consuming benchmark is Jetson-only).
- dev is slower than official on probe read/write on x86 (read 2533 vs 1694 ns per call of 1,000 reads; every sample of both dev runs 2441-2794 vs official 1635-1740). The 10-26 dev a7d54d7e measured 1507 ns, so this is a regression inside dev between a7d54d7e and 9ccda7e8 (probe checks are OFF in both builds). Recorded, not fixed (code is frozen). Cross-platform (Jetson E3, reported 2026-11-24): on the Orin dev/official is 1.002 for probe read and 1.003 for probe write, so the regression is x86-only. Checked reads cost about 855 ns each on the Orin (188x unchecked) against about 270 ns on x86.
- dev uprobe hit 131 ns vs official 123 ns.

## Probe checks on (session E3pc-full10-20261123-try1, Opc/Dpc, same order and bookends)

| op | kernel | official | Opc | dev | Dpc |
|---|---:|---:|---:|---:|---:|
| uprobe | 1109.2 | 122.8 (0.11x) | 122.8 (0.11x) | 131.2 (0.12x) | 129.8 (0.12x) |
| read (1,000 reads per call) | 19301.4 | 1694.4 (0.09x) | 271699.8 (14.08x) | 2533.3 (0.13x) | 271109.9 (14.05x) |
| write (1,000 writes per call) | 19180.5 | 1507.1 (0.08x) | 271435.0 (14.15x) | 2719.6 (0.14x) | 269157.9 (14.03x) |
| hash lookup | 10580.7 | 21143.1 | 20878.3 | 4814.3 | 4806.4 |
| per-CPU array lookup | 1821.3 | 9867.5 | 9836.2 | 1517.8 | 1516.2 |

With checks on, every bpf_probe_read/write_user is a process_vm_readv/writev system call: about 270 ns per read on x86 (vs about 19 ns for the kernel's in-kernel copy and 1.7-2.5 ns unchecked). The microbenchmark makes 1,000 reads per call, so the cost shows 160x there; sslsniff makes one read per event (E7: Dpc +2 process_vm_readv per request), which is why checks cost only 0.5 us per event end to end (E4). Map and hook costs are unaffected by the check flags.
