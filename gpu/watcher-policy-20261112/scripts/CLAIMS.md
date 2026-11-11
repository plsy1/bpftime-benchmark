# Claims to verify (watcher-policy rounds, 4 rounds x 18 cells x 7 sets, 504 runs)

Sets: P = no bpftime; W = dev 46f5a7cb (WFE while a noted launch runs, else N's sleep up to 1 ms);
M100 = old loop (200 us busy poll after a request, then sleeps capped at 100 us, slack 1 us);
A = park on the launch note when no probed kernel is in flight (1 ms backstop), 100 us sleeps while
one is, 200 us busy poll after a request; AC = A with C (adaptive hot window from request gaps
instead of the fixed 200 us, then 8 x 20 us deferral sleeps); AB = A with B (WFE instead of
sleeping while /proc/loadavg shows a spare core, latched off by run_delay or a late WFE return);
ABC = all three.

"CPU" = victim CPU s per wall s minus P's of the same round (median of 4 rounds).

1. All 504 runs valid (c18 needed launches + 2 records: graph mode warms up once more through
   the graph; fixed in summarize.py). Max temperature 54.4 C.
2. AC has the lowest CPU of the sleeping sets in every single-lane cell: 0.017-0.024 vs M100
   0.088-0.092 in c01/c02/c07/c18; in short kernels c14 0.060 vs 0.514.
3. With no load AC's host-call latency equals M100's (median 67-70 us, p90 about 108 us in
   c02/c07/c09/c14/c18).
4. Full load c01: AC load vs P 0.989 vs M100 0.980 vs W 0.865; > 1 ms share AC 1.04% vs M100
   3.50% vs W 11.29%. Short kernels full load c15: AC load 0.980 vs M100 0.942; > 1 ms 0.45% vs
   6.61%; p99 120 us vs 3,937 us.
5. Where AC is worse than M100: low GPU duty full load c10 (> 1 ms 1.69% vs 0.50%), duty-cycled
   load c17 (30.66% vs 25.14%), burst no load c06 cost per call 12.4 vs 11.2 us.
6. Idle agent c11: A-family CPU 0.002 vs M100 0.011 (W 0.002); VDD_CPU_GPU_CV AC 1,470 mW vs
   M100 1,509 vs P 1,458 vs W 1,476.
7. A alone is about M100 (CPU 0.089 vs 0.088 in c01) because it keeps the 200 us busy poll after
   each request; the park only matters when no kernel is in flight (c11, c09/c10 gaps).
8. B's gate: open with idle cores (c02, c07 on3, c09, c14) -> latency 14.8-16.2 us median (W:
   13.8-15.4) and W's CPU (about 1.0); closed under on6 (c01, c03, c05, c10, c15: gate_opens 0,
   latch_trips 1) -> like A/AC. Duty-cycled c17: AB/ABC in between (> 1 ms 11.70/13.01% vs W
   0.15% vs M100 25.14%; load 0.907/0.927 vs W 0.879 vs M100 0.944).
9. Two processes partial load c16: AB/ABC 1.5% > 1 ms at 1.70 CPU (W 0.92% at 1.99, M100 3.84%
   at 0.11); the gate opened about 1,000-1,250 times per run.
10. CUDA graph c18 (launches not noted): A-family latches "unseen" after 2 backstop hits
    (unseen_latch=1) and then serves like M100 (median 69-70 us); W is 533 us median because it
    only WFEs on noted launches and otherwise sleeps up to 1 ms; AB 14.9 us.
11. Two processes with no load (c13): every sleeping set has p99 about 2.5 ms and 3.5-3.9% > 1 ms;
    W/AB/ABC p99 about 373 us. Hypothesis: GPU time-slicing between the two contexts; a late
    answer lets the other context's kernel run. Not tested.
12. Duty-cycled load c17: sleeping sets' max host-call latency is about 5.0-5.1 ms, the load's
    on-phase length, and P's own per-launch mean rises to 3,645 us (vs 2,527 no load). Hypothesis:
    a thread waking from a short sleep waits out the load's on-phase under EEVDF. Not tested.
13. Power holds only under jetson_clocks (cpuidle WFI/c7 off): WFE sets W/AB/ABC draw about the
    same as P (c02: 1,713/1,706/1,727 vs 1,699 mW), sleeping sets more (M100 1,766, A 1,760,
    AC 1,747).
14. c14 power is thin: P's window (1.3 s) holds no whole second, so P has no power value and the
    others rest on one whole second.
