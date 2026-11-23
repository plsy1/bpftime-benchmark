# x86 E7: counters (policy M, 16 B, wrk -d 20; perf stat on the nginx worker over t=2..18 s, normalized by the requests nginx logged in that window)

Session E7-counters-20261123-try1, all rows valid, 3 reps per leg and mode. Cycles converted at 3,500 MHz (nginx P cores).

## Syscalls per request (nginx worker, perf stat, median of reps)

| leg | n | sys_enter | sched_setaffinity | sched_getaffinity | getuid | getgid | getpid | gettid | process_vm_readv |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| Odiag@S2h | 3 | 20.014 | 0.000 | 0.000 | 5.996 | 5.996 | 0.005 | 0.000 | 0.000 |
| Odiag@S4+NO_PIN | 3 | 24.001 | 0.000 | 0.000 | 5.994 | 5.994 | 2.003 | 1.998 | 0.000 |
| B | 3 | 8.022 | 0.000 | 0.000 | 0.000 | 0.000 | 0.005 | 0.000 | 0.000 |
| D@S1 | 3 | 8.024 | 0.000 | 0.000 | 0.000 | 0.000 | 0.005 | 0.000 | 0.000 |
| K@S1 | 3 | 11.025 | 0.000 | 0.000 | 0.000 | 0.000 | 0.005 | 0.000 | 0.000 |
| Odiag@S1 | 3 | 8.024 | 0.000 | 0.000 | 0.000 | 0.000 | 0.005 | 0.000 | 0.000 |
| K@S4 | 3 | 11.023 | 0.000 | 0.000 | 0.000 | 0.000 | 0.005 | 0.000 | 0.000 |
| D@S4 | 3 | 8.025 | 0.000 | 0.000 | 0.000 | 0.000 | 0.005 | 0.000 | 0.000 |
| Odiag@S4+ALL3 | 3 | 8.022 | 0.000 | 0.000 | 0.000 | 0.000 | 0.005 | 0.000 | 0.000 |
| K@S3 | 3 | 11.024 | 0.000 | 0.000 | 0.000 | 0.000 | 0.005 | 0.000 | 0.000 |
| Odiag@S4 | 3 | 30.007 | 3.997 | 1.999 | 5.996 | 5.996 | 2.004 | 1.999 | 0.000 |
| Dpc@S4 | 3 | 10.023 | 0.000 | 0.000 | 0.000 | 0.000 | 0.005 | 0.000 | 1.999 |

Baseline (B) syscalls per request: 8.022; extra per request = sys_enter - baseline.

## Cycles per request (nginx worker; cpu_core user + kernel)

| leg | n | cycles user | cycles kernel | total | insn user | insn kernel | us/req at 3500 MHz |
|---|---:|---:|---:|---:|---:|---:|---:|
| Odiag@S4 | 3 | 24812 | 31853 | 56665 | 47887 | 57796 | 16.190 |
| Dpc@S4 | 3 | 22277 | 20402 | 42680 | 46170 | 29929 | 12.194 |
| D@S4 | 3 | 21517 | 17371 | 38888 | 46246 | 25145 | 11.111 |
| Odiag@S4+ALL3 | 3 | 22197 | 17492 | 39689 | 47778 | 25140 | 11.340 |
| Odiag@S2h | 3 | 20397 | 19768 | 40165 | 40934 | 28530 | 11.476 |
| D@S1 | 3 | 18769 | 17314 | 36083 | 40322 | 25094 | 10.309 |
| K@S1 | 3 | 19314 | 29417 | 48732 | 35197 | 34259 | 13.923 |
| B | 3 | 14472 | 17026 | 31499 | 35172 | 25031 | 9.000 |
| K@S4 | 3 | 19672 | 39989 | 59661 | 35157 | 44359 | 17.046 |
| Odiag@S1 | 3 | 18402 | 17168 | 35570 | 39706 | 25062 | 10.163 |
| Odiag@S4+NO_PIN | 3 | 23804 | 21027 | 44831 | 47764 | 29801 | 12.809 |
| K@S3 | 3 | 19413 | 32135 | 51548 | 35223 | 40815 | 14.728 |

## Cross-method closure: delta cycles/req / f vs E6 throughput delta x (us/req)

| step | delta cycles/req -> us | E6 delta x us/req | ratio |
|---|---:|---:|---:|
| K@S1 - B | 4.924 | 4.975 | 0.99 |
| K@S3 - K@S1 | 0.805 | 0.788 | 1.02 |
| K@S4 - K@S3 | 2.318 | 2.270 | 1.02 |
| Odiag@S1 - B | 1.163 | 1.151 | 1.01 |
| Odiag@S2h - Odiag@S1 | 1.313 | 1.350 | 0.97 |
| Odiag@S4 - Odiag@S2h | 4.714 | 4.787 | 0.98 |
| Odiag@S4 - Odiag@S4+NO_PIN | 3.381 | 3.542 | 0.95 |
| Odiag@S4 - Odiag@S4+ALL3 | 4.850 | 5.046 | 0.96 |
| D@S1 - B | 1.310 | 1.260 | 1.04 |
| D@S4 - D@S1 | 0.801 | 0.829 | 0.97 |
| Dpc@S4 - D@S4 | 1.083 | n/a (not in E6) |  |

## Kernel BPF stats (run_cnt and run_time per request)

K@S4+BPFSTATS rep 0: hits/req 6.000, program time 3084 ns/req; probe_SSL_rw_enter: 3.000 hits, 151 ns/hit, probe_SSL_read_exit: 2.000 hits, 656 ns/hit, probe_SSL_write_exit: 1.000 hits, 1319 ns/hit
K@S3+BPFSTATS rep 0: hits/req 6.000, program time 973 ns/req; probe_SSL_rw_enter: 3.000 hits, 151 ns/hit, probe_SSL_read_exit: 2.000 hits, 138 ns/hit, probe_SSL_write_exit: 1.000 hits, 244 ns/hit
K@S1+BPFSTATS rep 0: hits/req 6.000, program time 166 ns/req; probe_SSL_rw_enter: 3.000 hits, 19 ns/hit, probe_SSL_read_exit: 2.000 hits, 33 ns/hit, probe_SSL_write_exit: 1.000 hits, 43 ns/hit
K@S1+BPFSTATS rep 1: hits/req 6.000, program time 164 ns/req; probe_SSL_rw_enter: 3.000 hits, 18 ns/hit, probe_SSL_read_exit: 2.000 hits, 33 ns/hit, probe_SSL_write_exit: 1.000 hits, 43 ns/hit
K@S3+BPFSTATS rep 1: hits/req 6.000, program time 922 ns/req; probe_SSL_rw_enter: 3.000 hits, 137 ns/hit, probe_SSL_read_exit: 2.000 hits, 138 ns/hit, probe_SSL_write_exit: 1.000 hits, 235 ns/hit
K@S4+BPFSTATS rep 1: hits/req 6.000, program time 3071 ns/req; probe_SSL_rw_enter: 3.000 hits, 137 ns/hit, probe_SSL_read_exit: 2.000 hits, 671 ns/hit, probe_SSL_write_exit: 1.000 hits, 1318 ns/hit
K@S3+BPFSTATS rep 2: hits/req 6.000, program time 994 ns/req; probe_SSL_rw_enter: 3.000 hits, 152 ns/hit, probe_SSL_read_exit: 2.000 hits, 145 ns/hit, probe_SSL_write_exit: 1.000 hits, 247 ns/hit
K@S1+BPFSTATS rep 2: hits/req 6.000, program time 164 ns/req; probe_SSL_rw_enter: 3.000 hits, 19 ns/hit, probe_SSL_read_exit: 2.000 hits, 31 ns/hit, probe_SSL_write_exit: 1.000 hits, 44 ns/hit
K@S4+BPFSTATS rep 2: hits/req 6.000, program time 3079 ns/req; probe_SSL_rw_enter: 3.000 hits, 151 ns/hit, probe_SSL_read_exit: 2.000 hits, 657 ns/hit, probe_SSL_write_exit: 1.000 hits, 1312 ns/hit

## Reading
- Official makes 22 extra system calls per request (getuid 6, getgid 6, sched_setaffinity 4, sched_getaffinity 2, getpid 2, gettid 2); with ALL3 or in dev: 0. Dpc adds 2 (process_vm_readv, one per event). The plan's code-based estimate of ~18 was low: uid_gid runs on all 6 hits per request.
- Cross-method closure (plan 3.5 item 5): every throughput step in E6 is matched by the worker's cycle delta within 5% (ratios 0.95-1.04; threshold 25%).
- Kernel BPF stats: 6.000 hits per request at S1, S3 and S4 (invariance holds). Program body time per request: S1 0.17 us, S3 0.96 us, S4 3.08 us. The kernel's hook cost x(S1) - x(B) = 4.98 us/request is therefore about 4.8 us of trap and 0.17 us of program, about 0.80 us (2,800 cycles) per hit.
- The kernel legs show 3 more system calls per request than B (11 vs 8) at every stage, including S1; not investigated (the worker's own syscalls, e.g. fewer requests per epoll round when it is slower).
