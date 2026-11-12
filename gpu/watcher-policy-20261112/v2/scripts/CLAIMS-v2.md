# Claims to verify (v2 rounds, 2026-11-12 13:28-14:05 JST)

Raw logs: logs/v2/r1..r4 (rounds4.sh; 19 cells x sets P M100 AC AC2, shuffled per round and cell);
summary logs/v2/summary.md (SETS="P M100 AC AC2" ROUNDS_FILE=rounds-v2-cells.txt summarize.py logs/v2).
AC = the AC of the earlier rounds (same build, BPFTIME_CUDA_WATCHER_V2 unset). AC2 = AC with
BPFTIME_CUDA_WATCHER_V2=1: C's window is twice the last request gap, at most 50 us, and closes on any gap
of 50 us or more (no episode cap); A's unseen latch needs two hits within 1 s (launch notes do not reset
the count) and comes off after 1 s with no request served while no noted kernel was in flight.
Patch: watcher-policy-v2.patch (against bpf_attach_ctx_cuda.cpp.ctl). Victim: spin3.cu with GRAPH modes
2 (graph and plain launches alternate) and 3 (first half graphs), built as before (-O3
-D__NV_LEGACY_LAUNCH, shared cudart).
"CPU" = victim CPU s per wall s minus P's of the same round (median over 4 rounds; 1.0 = one core).

1. All 304 runs valid; max temperature 54.1 C.
2. c23 (request gaps about 55-115 us, no load): AC busy-polls nearly all the time: CPU +1.026 (M100
   +1.032), win300 for 3996 of 4001 requests, cap_forced 435 (the 1 ms cap does not stop it). AC2:
   CPU +0.072, win0 for every request. Price: host-call median 14.7 -> 25.9 us, per-launch mean
   111.7 -> 121.5 us.
3. c24 (same gaps, full load): > 1 ms AC2 0.27% vs AC 2.72% vs M100 2.63%; load vs P 0.977 vs 0.962
   vs 0.949; CPU +0.028 vs +0.240 vs +0.323.
4. c21 / c22 (gaps about 155-295 us): AC's window opened for only part of the requests (c21: win0
   2040, win20 1856, win80 68, win300 38 of 4001), CPU +0.091 vs AC2 +0.048 (no load); latency equal
   (median 66.8 vs 69.2 us); c22 (full load) AC and AC2 equal (CPU +0.036 / +0.041, > 1 ms 0.54 /
   0.40%). The predicted continuous polling at 170-300 us gaps did not happen; it happened at 55-115 us.
5. Small bursts c25-c28 (2, 4, 8 calls per launch; 4 under full load): AC2's window sits at 21-50 us
   (win80) where AC's is at most 20 (win20); AC2 costs 0.012-0.014 cores more; cost per call is
   equal within 1.7 us (c25 51.4 vs 52.7, c26 30.6 vs 31.3, c27 20.1 vs 20.5, c28 48.4 vs 46.7).
   Sizing the window from the gap does not help small bursts.
6. c05 (32 calls per launch, full load): AC2 is worse in every round: per-launch mean 3,282-3,296 us
   vs AC 3,115-3,135 (cost per call 22.1 vs 16.9 us); guard_skips 4,754 vs 616. c06 (no load) equal
   (12.3 us per call).
7. Latch: c18 (graphs), c29 (alternating) and c30 (graphs, then plain): every AC and AC2 run latched
   once after 2 hits; AC2 unlatched once in each c30 run (unseen_unlatch=1), AC never. The predicted
   hole (alternating launches never latch under AC) did not occur with this pattern. No run of a
   non-graph cell latched; 4 runs had one isolated hit (r2 c05 AC2, r2 c10 AC2, r4 c11 AC2, r2 c17
   AC), each with h0_note=0: before the watcher had seen any launch note, i.e. at the process's first
   probed launch, whose note arrives after its kernel posted.
8. c02 r2 AC2 is disturbed: 14 calls > 1 ms (other rounds 0; M100 / AC 0-2), guard_skips 72 (others
   2-3); tegrastats shows two cores at 90-100% during that run, which the victim (about 3% of a core)
   does not explain. The c02 summary line for AC2 (> 1 ms 0.12%, max 5,299 us) rests on that run.
9. c01, c10, c11, c15, c17: AC2 equals AC within noise (CPU, load, > 1 ms, latency).
