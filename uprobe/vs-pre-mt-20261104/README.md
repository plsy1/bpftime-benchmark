# Real tools before and after the multi-thread map work (2026-11-04)

Before: `exp-c98951a4-gumfix` = c98951a4 (10-28; its map code is the same as
1aa74c4c, the parent of 9c3e0345 "hash map: lookups take no lock", where the
multi-thread work starts) with the ff5515d8 memleak fix cherry-picked so
memleak runs on threads (no CLAUDE.md; built in the jit-stack-validation-12
worktree with the daily cmake arguments). Now: `dev-ff5515d8`. bpf on the
same workloads. Frozen builds in `uprobe/ssl-nginx-2x2-20261023/builds/`.

`run.sh`: `measure_mt.py`, 5 tools, 1/2/4/6 threads (thread i on CPU 5 - i,
4 and 6 cross the clusters), 3 rounds x 10 s, backends rotated, 180 runs, no
errors. cpu5 at 1728000 before and after; 13:35-14:33, no daily job.
`sum.py` -> `summary.md` (ops/s summed over threads, median of 3):

| tool | thr | bpf | before | now | before/bpf | now/bpf | now/before |
|---|--:|--:|--:|--:|--:|--:|--:|
| bpftrace-uretprobe-hist | 1 | 467k | 1.22M | 1.37M | 2.61 | 2.93 | 1.12 |
| | 2 | 652k | 1.50M | 2.20M | 2.30 | 3.38 | 1.47 |
| | 4 | 789k | 311k | 2.26M | 0.39 | 2.87 | 7.27 |
| | 6 | 896k | 273k | 1.24M | 0.31 | 1.38 | 4.53 |
| funclatency-bpftime-example | 1 | 510k | 1.85M | 1.83M | 3.63 | 3.58 | 0.99 |
| | 2 | 703k | 2.48M | 2.75M | 3.53 | 3.92 | 1.11 |
| | 4 | 778k | 644k | 1.37M | 0.83 | 1.76 | 2.13 |
| | 6 | 921k | 536k | 1.25M | 0.58 | 1.36 | 2.34 |
| funclatency-libbpf | 1 | 481k | 1.53M | 1.51M | 3.17 | 3.13 | 0.99 |
| | 2 | 669k | 1.99M | 2.45M | 2.97 | 3.67 | 1.23 |
| | 4 | 788k | 444k | 1.59M | 0.56 | 2.02 | 3.58 |
| | 6 | 883k | 371k | 1.22M | 0.42 | 1.38 | 3.29 |
| malloc-example | 1 | 532k | 515k | 509k | 0.97 | 0.96 | 0.99 |
| | 2 | 804k | 318k | 322k | 0.40 | 0.40 | 1.01 |
| | 4 | 970k | 260k | 263k | 0.27 | 0.27 | 1.01 |
| | 6 | 1.33M | 269k | 270k | 0.20 | 0.20 | 1.00 |
| memleak | 1 | 329k | 434k | 428k | 1.32 | 1.30 | 0.99 |
| | 2 | 476k | 628k | 600k | 1.32 | 1.26 | 0.95 |
| | 4 | 572k | 438k | 445k | 0.77 | 0.78 | 1.02 |
| | 6 | 701k | 551k | 547k | 0.79 | 0.78 | 0.99 |

- The hash-map tools (funclatency: a start-time hash per thread, updated,
  looked up and deleted on every call; bpftrace's hist map) were below bpf at
  4 and 6 threads before (0.31-0.83x) and are above it now (1.36-2.87x):
  2.1-7.3x faster. At 2 threads 1.11-1.47x; at 1 thread equal (0.99), except
  uretprobe-hist 1.12x.
- malloc-example is bound by bpf_printk (spdlog, one logger lock): unchanged.
- memleak is unchanged (0.95-1.02x) and stays below bpf at 4 and 6 threads.
  Profiles at 6 threads (`profile/`, perf.data kept on the Jetson, not
  committed): the stack trace map's lock in `bpftime_get_stackid` takes
  41.7% of the cycles now (`bpftime_spin_lock_outlined`, plus
  `__aarch64_swp4_acq` 24.6%), and 41.0% + 8.5% before (`pthread_spin_lock`,
  `__aarch64_cas4_acq`). The hash map lock was 3.6% of memleak before. memleak
  calls `bpf_get_stackid` on every allocation, and every allocation here comes
  from the same stack, so all threads take one lock. New problem for the plan:
  the stack trace map is one lock (bpf's stackmap does not serialize lookups
  of an existing stack).
