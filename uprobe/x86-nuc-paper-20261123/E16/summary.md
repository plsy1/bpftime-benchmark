# x86 E16: exact, checked runtime measured directly (policy M, 16 B, 2 x 10 reps, 7 legs rotated)

Dx = dev 9ccda7e8 + paper diag (ladder/dexact-9ccda7e8: 87e0117a, 66ed229e; patches in patches/), probe-read/write checks ON, same cmake options as dpc, same vm/llvm-jit 87bac39a. BPFTIME_DIAG_EXACT=1 (read at load) makes bpf_get_current_pid_tgid, the perf shard lookup and ringbuf identity call getpid/gettid, bpf_get_current_uid_gid call getuid/getgid, and bpf_get_current_comm call prctl(PR_GET_NAME), on every call. Session E16-exact-20261124-try1: 140 rows, all valid, exit 0.

rows=140 valid=140 reps=20 payload=None exp=E16
| leg | n | median rps | CV% | GMR vs K [95% CI] | x vs B us/req [95% CI] | ev/req | del/exp | lost/req | missing-reported |
|---|---:|---:|---:|---|---|---:|---:|---:|---:|
| Dx | 20 | 81242 | 1.03 | 1.3964 [1.3894, 1.4035] | 3.128 [3.020, 3.235] | 2.0000 | 1.00000 | 0.000000 | -174..0 |
| Dx+BPFTIME_DIAG_EXACT=1 | 20 | 65356 | 0.87 | 1.1220 [1.1156, 1.1285] | 6.142 [6.022, 6.262] | 2.0000 | 1.00000 | 0.000000 | -65..0 |
| Dpc | 20 | 81249 | 0.87 | 1.3972 [1.3906, 1.4038] | 3.121 [3.046, 3.196] | 2.0000 | 1.00000 | 0.000000 | -247..0 |
| K | 20 | 58093 | 0.66 |  | 8.014 [7.912, 8.116] | 2.0000 | 1.00000 | 0.000000 | -179..2 |
| K+SSLSNIFF_PB_WAKEUP=32 | 20 | 65706 | 0.37 | 1.1323 [1.1286, 1.1360] | 6.002 [5.907, 6.097] | 2.0000 | 1.00000 | 0.000000 | -338..2 |
| B | 20 | 109542 | 1.98 | 1.8716 [1.8531, 1.8902] |  |  |  |  |  |
| D | 20 | 89031 | 0.75 | 1.5315 [1.5233, 1.5398] | 2.040 [1.937, 2.144] | 2.0000 | 1.00000 | 0.000000 | -473..0 |
TOST Dx vs Dpc margin ±0.015: ratio 0.9994 90% CI [0.9943, 1.0046] n=20 -> EQUIVALENT
DIFF x(Dx+BPFTIME_DIAG_EXACT=1) - x(Dx) = 3.014 us/request [2.929, 3.100] n=20; per event (/2) 1.507 us
DIFF x(K) - x(Dx+BPFTIME_DIAG_EXACT=1) = 1.871 us/request [1.778, 1.965] n=20; per event (/2) 0.936 us
DIFF x(K+SSLSNIFF_PB_WAKEUP=32) - x(Dx+BPFTIME_DIAG_EXACT=1) = -0.140 us/request [-0.216, -0.064] n=20; per event (/2) -0.070 us
DIFF x(Dpc) - x(D) = 1.080 us/request [1.016, 1.144] n=20; per event (/2) 0.540 us

Paired (95% t-CI, n=20):
- Dx+EXACT / K = 1.1220 [1.1156, 1.1285]: +12.2% over the stock kernel.
- Dx+EXACT / K_wb32 = 0.9909 [0.9860, 0.9958]: 0.9% slower than the kernel with batched wakeups.
- D / K_wb32 = 1.3526 [1.3467, 1.3585]; Dpc / K_wb32 = 1.2340 [1.2286, 1.2394]; K_wb32 / K = 1.1323 [1.1286, 1.1360] (E15 replicated: 1.1318).

## Reading
- Control: Dx without the switch is equivalent to the frozen Dpc (TOST ±1.5%).
- Exactness costs 1.507 µs/event (5,270 cycles at 3.5 GHz) on top of checks (Dpc - D 0.540 µs/event). That is more than the additive estimate from official ladder steps (uid/gid 0.56 + pid/tid 0.19), because exact mode also stops caching pid_tgid (3 hits/event) and reads comm with prctl.
- Against a kernel with the same notification policy, an exact, checked userspace runtime spends the whole budget on x86: 0.9% slower. The earlier additive estimate (+25% vs stock K) was too optimistic; measured is +12.2% vs stock K.
