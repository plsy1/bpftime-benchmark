# DX1-20261125-try1 summary (exploratory; 16:14:56–16:52, exit 0, all rows valid, noisy_core 0)

Question: does the 1 ms epoll wrapper loop Tern inherited in the rebase (upstream 82e7dc60, syscall_context.cpp
handle_epoll_wait) cost anything on x86? DnE/DnEpc = c71a9987 + ddd61834 (pass the timeout through, as D 9ccda7e8).

Throughput, round 0, n = 10, paired:
- DnE/Dn 1.0020 [0.9968, 1.0073], dx −0.011 µs/event (90% [−0.035, 0.012]) → equivalent at ±0.084
- DnEpc/Dnpc 0.9996 [0.9935, 1.0058], dx +0.002 → equivalent
- Dn/D 0.9993 [0.9950, 1.0037] → equivalent; Dnpc/Dpc 1.0006
- Dn/OlL3x730 1.0315 [1.0213, 1.0418] (C1: 1.0361); Dnpc/Kb 1.2396 [1.2214, 1.2581]

Consumer CPU (schedstat, ns/event, medians): DnE 856, Dn 866, D 851, Dnpc 881, DnEpc 864, K 2,156, Kb 771.
Round 1 (perf on the nginx worker, per request): futex 0 and sched_wakeup ≈ 0.001 for D, Dn, DnE, Dnpc, DnEpc —
under load the consumer stays in its nap window, so the wrapper loop never decides anything on x86.
Tick-based /proc/stat busy of the sniff core disagrees with schedstat (Dpc 152 vs 862 ns/event): do not use tick busy
for consumer CPU.

Round 2 (perf cyc on the worker, per request, 3 reps): OlL3x730 user cycles 22,793 / instr 47,940, kernel cycles 17,946;
Dn user 21,400 / 46,108, kernel 17,393. User-space difference 1,393 cycles/request (≈0.2 µs/event at 3.5 GHz),
kernel 554 (≈ the 2 getpid). The C1 residual is user-space runtime code, not system calls. → DX2 stage ladder.

Flag: the C1 verification workflow overlapped the first rows (NOTES-preliminary.md).
