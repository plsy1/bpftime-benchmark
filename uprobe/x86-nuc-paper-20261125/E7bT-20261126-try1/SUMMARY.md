# E7bT-20261126-try1 — E7b real tools on 4-thread HAProxy (x86 NUC), summary

Eval plan E7b "服务器上的真实工具": memleak and funclatency attached to HAProxy (nbthread 4 on CPUs 2,3,6,7; policy H4T =
H4 with the access log on fd 3, haproxy-t4-fd3.cfg), 16 B responses, wrk as H7b; legs B, K, OlL3x730, Dn, Dn730f (Tern +
#730 lock, PREREG-2b, for the plan's Dn730); 10 reps (+1 warm-up) per tool, legs shuffled per rep. Session exit 0, checks
PASS, 110 rows. Harness: run_paper.py --tool (new mode, .pre-e7b backup; the sslsniff path is unchanged, checked in the
smoke with B#H4 and Dn#H4 rows). The tool replaces the sslsniff loader: libbpf-tools v0.37.0 (as E6) with
BPFTIME_SHM_MEMORY_MB=512 for bpftime (the container's /dev/shm is 1 GiB; 1024 SIGBUSed HAProxy in the first smoke).
funclatency: `funclatency -u <libssl>:SSL_write` started before HAProxy on bpftime legs, `-p <pid>` after it on K;
memleak: `memleak -c /rp/sw 3600` (static wrapper starts HAProxy with the agent; one report at SIGINT). Paired statistics
in summary-e7bt.txt (GMR of rps and per-request wall time x = (1/rps − 1/rps_B)·1e6, 95 % t-CI, n = 10).

## funclatency on SSL_write (1 call per request; uprobe + uretprobe)

| leg | median rps | valid | count vs requests |
|---|---:|---:|---|
| B | 323.1k | 10/10 | – |
| K | 264.0k | 10/10 | −1 in every run |
| Dn | 299.6k | 10/10 | +1 in every run |
| Dn730f | 297.0k | 10/10 | +1 in every run |
| OlL3x730 | – | 0/10 | tool cannot load (cgroup array, -EOPNOTSUPP), as E6 |

Dn/K 1.1324 [1.1245, 1.1402], Dn730f/K 1.1247 [1.1211, 1.1284], Dn730f/Dn 0.9933 [0.9862, 1.0003]. Extra wall time per
request: K 0.693 µs [0.684, 0.701], Dn 0.250 [0.226, 0.274], Dn730f 0.273 [0.262, 0.283]; Dn − K −0.443 [−0.467,
−0.419] (4 server threads serve in parallel: thread CPU ≈ 4 × these). Counts exact up to a constant offset (Tern +1, K −1 in every run, relative to req = CumReq − 1: Tern's total equals CumReq exactly in all 20 runs, K's CumReq − 2, its tool attaching after the readiness curl; Tern's +1 is unexplained and also seen on upstream legs' sslsniff write counts in H7b).

## memleak (22 handlers: malloc family, free, mmap/munmap/mremap; user stack per allocation)

| leg | median rps | valid | top-10 stacks of the final report: allocations; total frame lines |
|---|---:|---:|---|
| B | 323.7k | 10/10 | – |
| K | 49.9k | 10/10 | 13–121; 76–89 frames |
| Dn | 12.8k | 10/10 | 51–1401; 73–89 frames |
| Dn730f | 13.2k | 10/10 | 75–1626; 76–93 frames |
| OlL3x730 | 126.7k | 10/10 (harness) | 28,025–28,055 allocations under one empty stack (0 frames): no stack walk, and the agent's own allocations counted (E6 finding) — not comparable |

Dn/K 0.2576 [0.2551, 0.2601]; Dn730f/K 0.2625; K/B 0.155; Dn/B 0.040. Extra wall time per request: K 16.9 µs, Dn 74.5 µs
[73.7, 75.4], Dn730f 73.1 µs; Dn − K +57.6 µs/request [56.8, 58.4]. The report columns cover only the top-10 stacks, and the
stack map overflowed in every K and Tern run ("N stack traces could not be displayed … hash collisions": K 1,998–5,949,
Dn 3,933–7,854, Dn730f 4,920–7,793; OlL3x730 0), so they show which startup stacks survived, not a comparable count; the
report is taken ≥ 1 s after the load stopped. OlL3x730's 28k is the whole outstanding set merged under one empty stack.
HAProxy's threads were on CPU only 0.89–0.91 of the time under Tern memleak (34.6–35.4 s of 40 s) vs ≥ 0.99 for B, K and
OlL3x730: Tern blocks, consistent with lock contention in the unwind path (inferred), so x(Dn) is not all unwinding CPU.
The #730 lock is significantly *faster* here: Dn730f/Dn 1.0193 [1.0106, 1.0280] (x −1.47 µs [−2.13, −0.81]; unexplained).

## Reading

- On a probe-bound real tool on a multithreaded server, Tern beats the kernel: funclatency on SSL_write 1.13× K
  throughput, 0.25 vs 0.69 µs extra wall time per request, exact counts; the #730 lock costs 0.7 % [−0.03, 1.4] (not significant). Upstream cannot run this tool (OlL3x730 here; Ol and Olpc failed the same way in E6).
- On memleak Tern loses badly: HAProxy runs at 4.0 % of B under Tern vs 15.5 % under the kernel (Dn/K 0.258), i.e.
  ~57 µs more per request. This is the N2 finding on a real server: each malloc takes a user stack through Gum's x86
  accurate (libunwind) backtracer, which also scales poorly across threads (inferred; not profiled; the 0.9 on-CPU fraction shows blocking; aarch64 has an FP walk). Upstream is faster mainly because it walks no stack (E6 verified it does no unwinding).
- For the paper: E7b's tool part shows both sides — a probe-bound tool (funclatency here, sslsniff in H7b) is cheaper than the kernel under threads,
  stack-capturing tools are the x86 weak point (a Tern finding, recorded, not fixed).

## Flags

- HAProxy log on fd 3 (H4T): request count = CumReq − 1 as in H7b (LOG_SKIPPED = 1 in all 99 completed rows); access-log drops ≤ 2.1k/run (as H7b, ≤ 2,096).
- K's tool attaches after HAProxy (funclatency -p) or spawns it (memleak -c); the bpftime tools start first on both.
- n = 10, one session; no Kb leg (not applicable: no sslsniff); no perf round.
- First smoke (E7bT-smoke-20261126-try1) failed on bpftime legs: HAProxy exited after the agent attached; the session
  record has no signal; a manual reproduction in the container showed SIGBUS (rc 135) with a 1 GiB segment and none with
  512 MB. Second smoke (…-try2): all rows valid except the expected OlL3x730 funclatency load failure.
- Harness: the 11 tool_failed rows carry the previous leg's access-log request count (no HAProxy ran; rows invalid,
  results unaffected); fixed afterwards (run_leg removes access.log in tool mode; run_paper.py.pre-e7bfix). tool_derive
  accepts a memleak row with 0 frames and does not record the stack-map overflow.
- Independent check: 2 verifiers (numbers; claims and validity): all numbers reproduce; corrections applied above.
