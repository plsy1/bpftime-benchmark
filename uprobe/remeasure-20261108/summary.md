## Output cost per call (96 runs, 0 not ok)

| path | thr | bpf total / out ns | bpftime now total / out ns | out now / bpf | 11-06 out / bpf(11-06) | bpf deliv | now deliv |
|---|---:|---:|---:|---:|---:|---:|---:|
| perf-8 | 1 | 2613 / 1366 | 620 / 396 | 0.29 | 0.16 | 100.00% | 100.00% |
| perf-8 | 2 | 4185 / 2368 | 688 / 404 | 0.17 | 0.15 | 100.00% | 100.00% |
| perf-8 | 4 | 5304 / 1962 | 2788 / 280 | 0.14 | 0.14 | 100.00% | 100.00% |
| perf-8 | 6 | 5041 / 963 | 4611 / -164 | -0.17 | 0.06 | 100.00% | 100.00% |
| rbout-8 | 1 | 1961 / 715 | 397 / 173 | 0.24 | 0.60 | 100.00% | 100.00% |
| rbout-8 | 2 | 2340 / 523 | 746 / 462 | 0.88 | 1.47 | 100.00% | 100.00% |
| rbout-8 | 4 | 4171 / 829 | 2917 / 409 | 0.49 | 2.22 | 100.00% | 100.00% |
| rbout-8 | 6 | 4315 / 237 | 4982 / 207 | 0.87 | 7.14 | 99.64% | 98.28% |
| rbres-8 | 1 | 1954 / 708 | 389 / 164 | 0.23 | 0.64 | 100.00% | 100.00% |
| rbres-8 | 2 | 2344 / 527 | 759 / 475 | 0.90 | 1.54 | 100.00% | 100.00% |
| rbres-8 | 4 | 4210 / 869 | 2967 / 459 | 0.53 | 2.49 | 100.00% | 100.00% |
| rbres-8 | 6 | 4306 / 228 | 5033 / 259 | 1.14 | 8.96 | 99.89% | 98.14% |
| control (probe only) | 1 | 1246 | 224 | | | | |
| control (probe only) | 2 | 1817 | 284 | | | | |
| control (probe only) | 4 | 3342 | 2508 | | | | |
| control (probe only) | 6 | 4078 | 4774 | | | | |

## Real tools on threads (ops/s summed, median of 3)

| tool | thr | bpf | bpftime now | now / bpf | 11-04 dev-ff5515d8 / bpf(11-04) | events reported/expected (now) |
|---|---:|---:|---:|---:|---:|---|
| bpftrace-uretprobe-hist | 1 | 469k | 1348k | 2.87 | 2.93 | 13561000/13561000, 13473000/13473000, 13651000/13651000 |
| bpftrace-uretprobe-hist | 2 | 653k | 2216k | 3.39 | 3.38 | 22295997/22296000, 22663999/22664000, 17785998/17786000 |
| bpftrace-uretprobe-hist | 4 | 792k | 2277k | 2.87 | 2.87 | 22953000/22953000, 19465000/19465000, 27104000/27104000 |
| bpftrace-uretprobe-hist | 6 | 890k | 1221k | 1.37 | 1.38 | 12226998/12227000, 12521997/12522000, 12300998/12301000 |
| funclatency-bpftime-example | 1 | 512k | 1756k | 3.43 | 3.58 | 17709000/17709000, 17511000/17511000, 17668000/17668000 |
| funclatency-bpftime-example | 2 | 706k | 2767k | 3.92 | 3.92 | 27876531/27888000, 27817437/27833000, 27042536/27310000 |
| funclatency-bpftime-example | 4 | 777k | 1405k | 1.81 | 1.76 | 14137220/14182000, 13515529/13527000, 13999571/14138000 |
| funclatency-bpftime-example | 6 | 919k | 1235k | 1.34 | 1.36 | 12434101/12477000, 12478494/12545000, 12391888/12419000 |
| funclatency-libbpf | 1 | 484k | 1490k | 3.08 | 3.13 | 14995000/14995000, 15009000/15009000, 14988000/14988000 |
| funclatency-libbpf | 2 | 660k | 2406k | 3.64 | 3.67 | 24201000/24201000, 18743000/18743000, 24196000/24196000 |
| funclatency-libbpf | 4 | 785k | 1947k | 2.48 | 2.02 | 16719000/16719000, 19622000/19622000, 19575000/19575000 |
| funclatency-libbpf | 6 | 898k | 1241k | 1.38 | 1.38 | 13174000/13174000, 12426000/12426000, 12495000/12495000 |
| malloc-example | 1 | 534k | 520k | 0.97 | 0.96 | 5230004/5229000, 5267000/5266000, 5198003/5197000 |
| malloc-example | 2 | 800k | 322k | 0.40 | 0.40 | 3235325/3237000, 3266954/3265000, 3207659/3208000 |
| malloc-example | 4 | 973k | 260k | 0.27 | 0.27 | 2500193/2619000, 2505170/2640000, 2488473/2625000 |
| malloc-example | 6 | 1338k | 269k | 0.20 | 0.20 | 2532932/2688000, 2568110/2712000, 2566592/2724000 |
| memleak | 1 | 332k | 643k | 1.94 | 1.30 | 3/~1024 outstanding 64-byte blocks, 3/~1024 outstanding 64-byte blocks, 3/~1024 outstanding 64-byte blocks rc [None, None, None, None, None, None] |
| memleak | 2 | 479k | 827k | 1.73 | 1.26 | 5/~1024 outstanding 64-byte blocks, 5/~1024 outstanding 64-byte blocks, 5/~1024 outstanding 64-byte blocks rc [None, None, None, None, None, None] |
| memleak | 4 | 575k | 678k | 1.18 | 0.78 | 9/~1024 outstanding 64-byte blocks, 9/~1024 outstanding 64-byte blocks, 9/~1024 outstanding 64-byte blocks rc [None, None, None, None, None, None] |
| memleak | 6 | 681k | 505k | 0.74 | 0.78 | 9/~1024 outstanding 64-byte blocks, 9/~1024 outstanding 64-byte blocks, 9/~1024 outstanding 64-byte blocks rc [None, None, None, None, None, None] |

