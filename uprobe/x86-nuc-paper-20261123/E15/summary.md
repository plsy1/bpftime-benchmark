# x86 E15: kernel baseline with batched consumer wakeups (policy M, 16 B, 2 x 10 reps, 7 legs rotated)

Why: review round 2 of the HotOS draft. Kernel sslsniff uses libbpf's perf_buffer defaults (sample_period = wakeup_events = 1), so the kernel queues a consumer wakeup (irq_work) for every event. Official bpftime never wakes its consumer (1 ms polling), and Tern's consumer naps under a steady stream. So the "budget" K − L3port may partly be notification policy.
Change: the counting loader reads SSLSNIFF_PB_WAKEUP=N and passes perf_buffer_opts.sample_period = N, which libbpf turns into wakeup_events = N. Unset means unchanged (sslsniff-s4 rebuilt in place; the old binary is kept as sslsniff-s4.pre-wb; the BPF object is unchanged). run_paper.py now applies leg env to K legs. Session E15-kwakeup-20261124-try1: 140 rows, all valid, exit 0.

rows=140 valid=140 reps=20 payload=None exp=E15
| leg | n | median rps | CV% | GMR vs K [95% CI] | x vs B us/req [95% CI] | ev/req | del/exp | lost/req | missing-reported |
|---|---:|---:|---:|---|---|---:|---:|---:|---:|
| K | 20 | 58090 | 0.56 |  | 8.045 [7.993, 8.096] | 2.0000 | 1.00000 | 0.000000 | -329..2 |
| D | 20 | 89081 | 0.70 | 1.5309 [1.5250, 1.5370] | 2.076 [2.028, 2.125] | 1.9999 | 0.99996 | 0.000073 | -612..0 |
| B | 20 | 109287 | 0.61 | 1.8778 [1.8707, 1.8849] |  |  |  |  |  |
| K+SSLSNIFF_PB_WAKEUP=256 | 20 | 65744 | 0.65 | 1.1317 [1.1275, 1.1359] | 6.042 [5.990, 6.093] | 2.0000 | 1.00000 | 0.000000 | -248..2 |
| L3port | 20 | 87851 | 0.94 | 1.5087 [1.5007, 1.5167] | 2.243 [2.188, 2.298] | 2.0000 | 0.99998 | 0.000000 | 0..516 |
| O | 20 | 60427 | 0.63 | 1.0393 [1.0353, 1.0434] | 7.393 [7.338, 7.448] | 2.0000 | 1.00000 | 0.000000 | 0..259 |
| K+SSLSNIFF_PB_WAKEUP=32 | 20 | 65732 | 0.62 | 1.1318 [1.1268, 1.1368] | 6.041 [6.001, 6.081] | 2.0000 | 1.00000 | 0.000000 | -323..2 |
DIFF x(K) - x(L3port) = 5.802 us/request [5.732, 5.872] n=20; per event (/2) 2.901 us
DIFF x(K+SSLSNIFF_PB_WAKEUP=32) - x(L3port) = 3.799 us/request [3.736, 3.861] n=20; per event (/2) 1.899 us
DIFF x(K+SSLSNIFF_PB_WAKEUP=256) - x(L3port) = 3.799 us/request [3.746, 3.852] n=20; per event (/2) 1.900 us
DIFF x(O) - x(L3port) = 5.151 us/request [5.078, 5.224] n=20; per event (/2) 2.575 us
DIFF x(K) - x(K+SSLSNIFF_PB_WAKEUP=32) = 2.003 us/request [1.932, 2.075] n=20; per event (/2) 1.002 us
DIFF x(K) - x(K+SSLSNIFF_PB_WAKEUP=256) = 2.003 us/request [1.943, 2.063] n=20; per event (/2) 1.002 us

## Reading
- Batched wakeups make the kernel 13.2% faster: K+WAKEUP=32 is 1.1318 [1.1268, 1.1368] vs K, and 256 gives the same. The per-event wakeup costs 1.00 µs per event (2.003 µs/req [1.932, 2.075]), which is almost all of K's output stage in E6 (1.135 µs/event). Delivery is unchanged (2.0000 ev/req, del/exp 1.00000); the final consume flushes the tail.
- Budget per event against the tuned kernel: K_wb − L3port = 1.900 µs = 6,650 cycles at 3.5 GHz, against 2.901 µs = 10,150 against stock K. The tuned budget is about the hook saving alone (E6: 1.912 µs, 6,690 cycles), so the output saving came from notification policy.
- Official's entries (O − L3port 2.575 µs, 9,010 cycles) are 136% of the tuned budget, and the pin alone (6,380) is 96%. Official vs tuned K: 1.0393/1.1318 = 0.918, i.e. 8% slower. L3port vs tuned K 1.333, D 1.353.
- An exact checked runtime vs tuned K (additive estimate: D + checks (E4 Dpc − D 1.097) + uid/gid (E6x 1.124) + pid/tid (0.381) µs/req) would be about +10%; without checks about +19%.
- Tradeoff: batching delays delivery by up to N events (N=32 at ~130k events/s is about 0.25 ms). Tern's naps bound the delay at about 0.1 ms (dev CLAUDE.md).
