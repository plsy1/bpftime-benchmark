# DN1-20261126-try1 — Dnext careful mode on nginx (x86 NUC), summary (exploratory; C2 is confirmatory)

Session ended 19:49:10, exit 0; checks before/after PASS. Policy M, 16 B, S4 counting loader, 2 rounds × 10 paired reps
(+1 warm-up), legs shuffled per rep; round 2: perf stat (sc), 3 × 20 s. 160 measured rows, all valid; noisy_core rows 0.
Builds: dnext/dnextpc = Tern c71a9987 + careful-mode patch 6b6b247e (ladder/dnext-c71a9987), CMake options identical to
dnew/dnewpc. Table: summary-dn1.md (analyze_paper.py).

## Results (n = 20)

| comparison | throughput ratio [90 % CI] (TOST ±1.5 %) | Δx µs/event [95 % CI] (TOST ±0.084) |
|---|---|---|
| Dnext (off) vs Dn | 0.9997 [0.9941, 1.0053] equivalent | +0.001 [−0.037, +0.040] equivalent |
| Dnextpc (off) vs Dnpc | 1.0022 [0.9972, 1.0072] equivalent | −0.014 [−0.052, +0.024] equivalent |
| Dnext + CAREFUL vs Dnext (same binary) | 1.0007 [0.9975, 1.0040] equivalent | −0.004 [−0.026, +0.018] equivalent |
| Dnextpc + CAREFUL vs Dnextpc (same binary) | 0.9974 [0.9936, 1.0013] equivalent | +0.016 [−0.013, +0.045] equivalent |

vs Kb (K + SSLSNIFF_PB_WAKEUP=32), GMR of rps [95 % CI]: Dnpc 1.2328 [1.2248, 1.2409], Dnextpc 1.2355 [1.2308, 1.2402],
Dnextpc + CAREFUL 1.2323 [1.2272, 1.2375]; Dn 1.3562 [1.3482, 1.3644], Dnext + CAREFUL 1.3568 [1.3501, 1.3635]. Median rps:
B 109.4k, Dn 89.3k, Dnext 88.9k, Dnext+CAREFUL 89.2k, Dnpc 81.0k, Dnextpc 81.0k, Dnextpc+CAREFUL 80.8k, Kb 65.7k.
Delivery ≥ 0.99993 of events on every Tern leg.

perf sc (median per request): Dnextpc 10.02 system calls (2.00 process_vm_readv), Dnextpc + CAREFUL 10.02 (2.00): careful
mode adds no system call in nginx's steady state (no spawn or rename happens; comm and identity are cache hits).

## Reading

- The switch is inert when off (B6): Dnext and Dnextpc are equivalent to Dn and Dnpc within ±1.5 % and ±0.084 µs/event,
  despite being different binaries (B5's layout control had shown a 0.87 % systematic difference between same-semantics
  binaries; here the difference is within ±0.3 %).
- Careful mode is free on nginx: per-event cost change −0.004 / +0.016 µs/event, both CIs inside ±0.05; sslsniff calls
  bpf_get_current_comm and bpf_get_current_pid_tgid on every event, so the per-thread comm cache is on the hot path.
  N5a's cost criterion (≤ 0.084 µs/event vs the switch-off leg) is met here (exploratory; C2 repeats it). With I8
  (correctness), careful mode makes Tern's comm and identity match the kernel on every libc-mediated event at no
  measurable cost; the 1.530 µs/event "price of exactness" (naive per-call system calls, x86-facts §19.2) is an upper bound
  of a naive design, not the price of correctness.
- The careful mode was verified to be active in nginx: in the measurement image (glibc 2.39) nginx under the Dnextpc agent
  with BPFTIME_CAREFUL=1 logs "Careful mode: interposers bound, caches on" in both nginx processes it started (manual check after the
  session; the harness discards nginx's stderr); run_paper.py passes the leg env to nginx (run_paper.py:603).

## Flags

- Exploratory session (not pre-registered); C2 holds the confirmatory numbers.
- n = 20, one session; checks PASS; noisy_core 0 rows.
