# Jetson E14 and E3 Opc/Dpc (complete ABBA), 2026-11-24

## E14 real-tool throughput (measure.py, 5 rounds × 10 s, median ops/s; none = workload without a tool)

| tool | none | K | O | D | D/O | D/K | O/K |
|---|---:|---:|---:|---:|---:|---:|---:|
| gethostlatency | 112,326 | 62,707 | 56,181 | 88,849 | 1.58 | 1.42 | 0.90 |
| malloc example | 42,471,548 | 539,618 | 1,909,330 | 546,224 | 0.29 | 1.01 | 3.54 |
| funclatency example | 90,235,104 | 511,176 | 1,676,032 | 1,779,358 | 1.06 | 3.48 | 3.28 |
| funclatency (libbpf-tools) | 90,262,234 | 482,462 | SIGSEGV | 1,529,478 | — | 3.17 | — |
| memleak | 42,512,210 | 331,660 | no result | 644,564 | — | 1.94 | — |
| bpftrace uprobe-count | 90,227,129 | 796,971 | 3,903,593 | 4,085,960 | 1.05 | 5.13 | 4.90 |
| bpftrace uretprobe-hist | 89,761,606 | 471,194 | 1,169,951 | 1,353,230 | 1.16 | 2.87 | 2.48 |
| bpftrace gethostlatency | 112,843 | 62,025 | 88,241 | 69,950 | 0.79 | 1.13 | 1.42 |

- Audit: (reported + lost) / expected ≥ 0.9999 for D in every row. For O: rows ≥ 0.9986; funclatency (libbpf-tools)
  SIGSEGV in every round.
- D is faster than K on every tool (1.01–5.13×).
- D is slower than O on two:
  - **malloc example** (0.29×). D runs at K's speed. O is 3.5× K, but O's counts are wrong in 2 of 3 E12 runs
    (over-count).
  - **bpftrace gethostlatency** (0.79×). The libbpf-tools gethostlatency is the opposite (D 1.58× O).
  - Neither cause has been investigated.

## E3 Opc / Dpc, ABBA complete (run 1 Opc, run 2 Dpc; second session: run 3 Dpc, run 4 Opc)

Median ns per call, pooled over the two runs of each arm. Each run's median is in brackets.

| case | Opc | Dpc | Dpc/Opc |
|---|---:|---:|---:|
| uprobe | 195.8 (196.4, 195.2) | 210.9 (210.7, 211.3) | 1.077 |
| uretprobe | 265.8 (265.7, 266.0) | 302.2 (302.2, 302.3) | 1.137 |
| uprobe + uretprobe | 319.5 (319.7, 319.4) | 346.6 (346.6, 346.9) | 1.085 |
| probe read (1,000 ops) | 862,573 (859,971, 866,315) | 882,170 (890,305, 879,534) | 1.023 |
| probe write (1,000 ops) | 865,126 (864,819, 865,360) | 885,643 (894,911, 879,977) | 1.024 |
| hash update / lookup / delete | 62,096 / 51,912 / 47,607 | 31,981 / 17,985 / 35,516 | 0.52 / 0.35 / 0.75 |
| array update / lookup / delete | 25,404 / 20,058 / 20,363 | 11,569 / 1,186 / 19,848 | 0.46 / 0.06 / 0.98 |
| per-CPU hash update / lookup / delete | 213,228 / 128,696 / 257,224 | 34,445 / 24,473 / 66,792 | 0.16 / 0.19 / 0.26 |
| per-CPU array update / lookup / delete | 70,506 / 28,799 / 20,418 | 14,958 / 4,127 / 19,922 | 0.21 / 0.14 / 0.98 |

- Kernel bookends (uprobe / uretprobe / read, ns): 1,239.7 / 1,661.4 / 17,568 at bookend 0, then
  1,242.3 / 1,663.2 / 17,562 at bookend 2 of session 1, 1,241.8 / 1,660.8 / 17,611 at bookend 2 of session 2,
  and 1,242.1 / 1,663.9 / 17,613 at bookend 4. The machine was stable across both sessions (≤ 0.3 %).
- With the checks on, a checked access costs about 862 ns for Opc and 882 ns for Dpc (one process_vm_readv each).
  Dpc is 2.3 % slower, and its two runs differ by 1.2 % (run 2 is slower).
- The hook and map ratios with the checks on match the unchecked O/D ratios in E3. Without checks D/O was:
  - uprobe 1.07, uretprobe 1.13;
  - array lookup 1,185 vs 20,023 ns.
