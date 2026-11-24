# Jetson E10, E11, E12, E13, cpuset control, E7c (2026-11-24)

Arms: kernel = K, O = official 4708f409, D = dev 9ccda7e8, Dpc = D with probe read/write checks.
E14 and the E3 Opc/Dpc second half (E3pc2) were refused at start by the guard (man-db.timer 09:03) and rerun
after it; see DEVIATIONS.md. All sessions here: exit 0, end guard ok.

## E12 18-tool exact audit (3 runs, --secs 4; delivered / expected)

| tool | K | O | D |
|---|---|---|---|
| funclatency (libbpf-tools) | 1.0000 ×3 | workload SIGSEGV ×3 | 1.0000 ×3 |
| funclatency example | 1.0000 ×3 | 1.0000 ×3 | 1.0000 ×3 |
| gethostlatency | exact ×2; run 3 255,098 + 901 lost of 256,000 | 0.9985 / 1.0000 / 0.9980, 0 reported lost (silent) | exact ×2; run 1 366,523 + 976 lost of 367,500 |
| memleak (approximate) | 1,027 / 1,027 / 1,026 leaks | 0 leaks (stack errors ~1.0 M) | 1,026 ×3 |
| malloc example | 1.0005 ×3 | 1.75 / 1.0001 / 1.30 (over-count) | 1.0005 ×3 |
| bashreadline | 1.0000 ×3 | 1.0000 ×3 | 1.0000 ×3 |
| usdt_minimal | 8/8 ×3 | 7/8 ×3 | 8/8 ×3 |
| sslsniff example | 1.0000 ×3 | 1.0000 ×3 | 1.0000 ×3 |
| bashreadline example | 1.0000 ×3 | 1.0000 ×3 | 1.0000 ×3 |
| get_stack_id example (no exact answer) | 64, workload timeout | 1,304–1,306, workload SIGSEGV | 70, workload timeout |
| bpftrace uprobe-count, uretprobe-hist, ustack, usdt-jvm-gc, bashreadline, gethostlatency (6) | 1.0000 ×3 each | 1.0000 ×3 each | 1.0000 ×3 each |
| javagc | 1.0000 ×3 | 1.0000 ×3 | 1.0000 ×3 |
| opensnoop | 0.9985 / 0.9988 / 0.9995, 0 lost | not supported on aarch64 (rc 127) | 1.0000 / 1.0000 / 0.9986, 0 lost |

- D: 13 of 18 tools have reported == expected in all 3 runs. These are both funclatency variants, bashreadline ×2,
  usdt_minimal, the sslsniff example, the 6 bpftrace scripts and javagc. The other five:
  - malloc example: +1,017 / +1,017 / +1,019 over expected. K is +1,041 / +1,038 / +1,021, so the count includes about
    1,000 mallocs outside the loop. D equals K.
  - gethostlatency: exact in 2 of 3 runs. Run 1 has 976 reported and 1 unaccounted.
  - opensnoop: exact in 2 of 3 runs. Run 3 is −54, unreported. K is −19..−58, unreported, in every run.
  - memleak: equal to K (1,026; the agent cannot see allocations before `__libc_start_main`, known).
  - get_stack_id: no exact answer. K and D both report the victim timing out under `timeout`; K has 64, all
    `-EEXIST`, and D has 70.
  - G13 (D ≥ 17/18 excluding get_stack_id) is met only when "equal to K's behaviour" counts. Exact in every run is 13/18.
  - Correction: an earlier version of this file said 16/18, which counted gethostlatency and opensnoop by their best
    runs.
- gethostlatency: K run 3 and D run 1 both drop events and report them. delivered + reported = expected − 1 in both
  (off by one in the lost accounting, the same for K and D).
- opensnoop: K is never exact (19–58 opens missing, 0 reported), and D run 3 misses 54. The expected count counts opens
  the tool does not see at the edges of the window. D (2 of 3 exact) is at least as close as K.
- O: 11 of 18 exact in every run (the funclatency example, bashreadline ×2, sslsniff example, 6 bpftrace, javagc).
  gethostlatency is exact in 1 of 3, with silent loss in the other two. The other O failures:
  - funclatency (libbpf-tools) and get_stack_id: SIGSEGV.
  - memleak: 0 leaks found.
  - malloc example: over-counts.
  - usdt_minimal: 7 of 8.
  - opensnoop: not supported on aarch64. The transformer imports `syscall_addr`, which official defines only in x86-64 asm.
    The load error is "undefined symbol" for any program.

## E13 start-up (200 rounds × 2 reps; p50 / max ms of child wall time)

| | K=1 p50 | K=1 max | K=16 child p50 | K=16 round p50 | K=16 round max |
|---|---:|---:|---:|---:|---:|
| kernel | 0.51 | 1.28 | 1.28 | 2.63 | 3.81 |
| O | 21.97 | 26.46 | 115.1 | 214.9 | 224.3 |
| D (AOT) | 12.98 | 17.72 | 47.3 | 86.0 | 101.7 |
| D no AOT | 21.97 | 26.40 | 117.5 | 220.1 | 228.5 |
| Dpc | 13.12 | 17.48 | — | — | — |

- D starts a traced child in 13.0 ms vs O's 22.0 ms (−41 %). The gain is all AOT: D without AOT equals O.
- At a burst of 16, D's rounds take 86 ms vs O's 215 ms (−60 %).
- The probe checks add 0.14 ms (Dpc vs D).
- seq (N = 200, out-control.bpf.o):

  | | p50 |
  |---|---:|
  | none | 0.48 ms |
  | kernel | 0.49–0.51 ms |
  | O | 21.9 ms |
  | D | 12.9 ms |

  hit and nohit are equal. Agent without shm (N = 10): O 3,015 ms (a fixed 3 s wait), D 8.0 ms.
- n4-many (start p50 ms; ns per call):

  | M | kernel | O | D |
  |---|---:|---:|---:|
  | 1 | 0.50 | 22.2 | 13.1 |
  | 10 | 0.52 | 25.5 | 15.0 |
  | 100 | 0.78 | 58.0 | 33.1 |
  | 1000 | 3.39 | 391.5 | 214.7 |

  ns per call: K 1,239–1,310, O 204–280, D 222–291. RSS: O 38.6 MB, D 30.9–32.8 MB, K 1.5 MB.
  Kernel M=1000: the samples are valid, but outattach did not exit within 60 s after the run (flagged ok = false).
- memleak-start (10 reps, median):

  | | workload start | tool setup |
  |---|---:|---:|
  | kernel | 0.69 ms | 167 ms |
  | O | 363 ms | 424 ms |
  | D | 38.1 ms | 769 ms |
  | D no AOT | 311 ms | 455 ms |

  AOT moves compilation from the workload's start (−89 % vs O) into tool setup (+345 ms vs O).

## E10 consuming lookup (6 runs, median ns per element; loader/victim on cpu5; all 36 checksum checks passed)

| case | K | O | D | D vs O | D/K |
|---|---:|---:|---:|---:|---:|
| ordinary control_hit | 3.775 | 1.609 | 1.607 | −0.1 % | 0.43 |
| ordinary hit | 5.780 | 21.878 | 3.102 | −85.8 % | 0.54 |
| ordinary control_mixed | 9.311 | 2.513 | 2.511 | −0.1 % | 0.27 |
| ordinary mixed | 12.202 | 30.247 | 8.685 | −71.3 % | 0.71 |
| ordinary unused (not a gain metric) | 3.768 | 20.038 | 1.185 | −94.1 % | 0.31 |
| per-CPU control_hit | 3.773 | 1.626 | 1.612 | −0.9 % | 0.43 |
| per-CPU hit | 6.875 | 29.748 | 5.129 | −82.8 % | 0.75 |
| per-CPU control_mixed | 9.303 | 2.529 | 2.516 | −0.5 % | 0.27 |
| per-CPU mixed | 12.783 | 38.800 | 10.645 | −72.6 % | 0.83 |
| per-CPU unused (not a gain metric) | 4.522 | 28.833 | 4.122 | −85.7 % | 0.91 |

D wins in 5–6 of 6 paired runs in every case. The controls equal O, so the gain is all in the lookup.

## E11 map churn and syscall layers

Churn (Mops/s, median of 3; threads pinned 5, 4, 3, …; 4+ threads cross clusters):

| obj | threads | K | O | D | D/O |
|---|---:|---:|---:|---:|---:|
| churn-k4-v8 | 1 | 0.488 | 1.542 | 1.677 | 1.09 |
| churn-k4-v8 | 2 | 0.670 | 2.110 | 2.082 | 0.99 |
| churn-k4-v8 | 4 | 0.807 | 0.452 | 1.432 | 3.16 |
| churn-k4-v8 | 6 | 0.890 | 0.388 | 1.235 | 3.18 |
| control | 1 | 0.547 | 2.259 | 2.094 | 0.93 |
| control | 2 | 0.738 | 3.719 | 3.488 | 0.94 |
| control | 4 | 0.827 | 1.450 | 1.548 | 1.07 |
| control | 6 | 0.934 | 1.192 | 1.226 | 1.03 |

- With 4 or more threads, O's map churn falls below K (0.56 and 0.44 × K). D keeps 1.39–1.77 × K.
- D at 2 threads, k4-v8, spreads from 2.02 to 2.91 across reps.

Syscall layers (D, 5 rounds, ns per write(2), median; none = 353.7):

| leg | ns per write | vs none |
|---|---:|---:|
| rewrite | 404.8 | +51.1 |
| passthrough | 405.3 | +51.6 |
| agent, other syscall | 436.2 | +82.5 |
| agent, exit | 531.3 | +177.6 |
| agent, count | 615.5 | +261.8 |
| kernel, other | 368.0 | +14.3 |
| kernel, exit | 438.5 | +84.8 |
| kernel, count | 525.0 | +171.3 |

- Counts are exact: agent 1,000,000 ×3; kernel 1,000,000, 1,000,000 and 1,000,052 (other processes' writes).
- O: rc 127 on every leg, from the same undefined `syscall_addr` as the E12 opensnoop failure.
  `LD_PRELOAD=builds/O/libbpftime-agent-transformer.so /bin/true` gives the same error.

## cpuset control (pinbench on cpu4, host vs inside the measurement container, ABAB × 3; median cycles per op, n = 30)

| case | host | container | ratio |
|---|---:|---:|---:|
| pin sequence, single | 2,710 | 2,713 | 1.001 |
| pin sequence, pair 4,5 (policy M mask) | 5,414 | 4,858 | 0.897 |
| pin sequence, all 0–5 | 5,424 | 4,886 | 0.901 |
| getuid+getgid | 1,017 | 1,016 | 0.999 |
| getpid / getppid / gettid | 516 / 514 / 493 | 506 / 513 / 491 | 0.98–1.00 |
| readlinkat /proc/self/exe | 2,813 | 2,887 | 1.026 |

The container (and its cgroup) does not make pinning dearer. The pair mask is in fact 10 % cheaper there.
So the in-nginx ~12,500 cycles per pin sequence (E7) is not a cpuset effect.

## E7c cache/TLB per request (nginx worker, user+kernel, 16 s window, 3 reps, no multiplexing; median)

| leg | rps | instructions | L1I miss | iTLB miss | L1D miss | dTLB miss | L2D refill |
|---|---:|---:|---:|---:|---:|---:|---:|
| B | 19,662 | 68,215 | 5,969 | 518 | 971 | 565 | 5,242 |
| K@S1 | 16,304 | 78,766 | 6,796 | 587 | 1,097 | 676 | 5,963 |
| Odiag@S1 | 17,727 | 73,888 | 6,577 | 686 | 1,144 | 780 | 5,856 |
| D@S1 | 17,656 | 74,225 | 6,654 | 677 | 1,146 | 785 | 5,914 |
| K@S4 | 13,180 | 92,197 | 8,307 | 609 | 1,353 | 789 | 7,253 |
| Odiag@S4 | 11,887 | 118,656 | 8,836 | 1,024 | 1,514 | 1,092 | 7,923 |
| Odiag@S4+NO_PIN | 14,390 | 92,319 | 7,552 | 985 | 1,333 | 998 | 6,772 |
| D@S4 | 15,575 | 79,656 | 7,462 | 778 | 1,331 | 1,026 | 6,863 |

Spread across reps is ≤ 1.2 % for instructions and ≤ 3.3 % for L1I misses.

- Pinning (Odiag@S4 − NO_PIN, two pin sequences per request): +26,336 instructions, +1,284 L1I misses,
  +1,152 L2 refills, +181 L1D misses, +39 iTLB misses, +95 dTLB misses per request.
  Per pin sequence that is ~640 L1I misses and ~580 L2 refills. In the idle pinbench loop the same path stays cache-hot.
  This is consistent with the in-nginx pin costing ~2.3× the idle unit cost (cold instruction and data paths in the
  kernel). It is consistent only; we did not measure miss latency.
- Hook (S1 − B):
  - K: +10,551 instructions, +827 L1I misses. The kernel trap path executes more instructions than bpftime's
    (Odiag +5,673, D +6,009), but its TLB misses are lower (iTLB +69 vs +168/+159; dTLB +112 vs +216/+220).
  - bpftime's hook misses more in the TLB: the trampoline and agent code are extra pages in the process.
- Output stage (S4 − B):
  - D +11,440 instructions vs K +23,982 and Odiag +50,440.
  - L1I misses: D +1,493, K +2,338, Odiag +2,867.
