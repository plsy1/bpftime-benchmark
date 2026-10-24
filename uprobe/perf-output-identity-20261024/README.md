# bca2ebce: cache the producer pid and tid in software perf output (2026-10-24)

Found with perf (`../ssl-nginx-ablation2-20261024/perf`): every `bpf_perf_event_output` looked up its
producer shard with `getpid()` and `syscall(SYS_gettid)`; glibc caches neither. In the nginx worker
traced by sslsniff (16 B) these two system calls took 2.8% of the worker's cycles. The identity is
now cached per thread and cleared in an atfork child handler; the shard match still compares pid
and tid.

| Check | Result |
|---|---|
| `[perf_event]` | 8 cases; new "fork child writes to its own producer shard" fails without the atfork reset |
| Daily filter (`tests.log`) | 92 cases / 254753 assertions |
| ssl-nginx audit 256 KB | 18.0 events/request (kernel 17.5), same as 7ed339ad |
| Full ten rounds (`full10-bca2ebce`) | all 15 items within +/-1.7% of 7ed339ad |
| Profile after the fix | `bpf_perf_event_output` inclusive 4.67% -> 1.58% of worker samples |

## ssl-nginx, fair scope, 2 rounds x 10 (`bench/summary.md`)

| Leg | 16 B req/s | vs kernel | us/req over baseline | 256 KB req/s | vs kernel | us/req over baseline |
|---|---:|---:|---:|---:|---:|---:|
| baseline | 17,167 | +45.2% | 0 | 1,746 | +43.6% | 0 |
| kernel | 11,820 | 0 | 26.3 | 1,216 | 0 | 250 |
| 7ed339ad | 13,570 | +14.8% | 15.4 | 1,326 | +9.1% | 181 |
| bca2ebce | 13,878 | **+17.4%** | 13.8 | 1,358 | **+11.7%** | 164 |

bca2ebce vs 7ed339ad, paired per rep: 16 B +2.0% (15/20 faster), 256 KB +3.1% (12/20; CV 3.6%).
