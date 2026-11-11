# Claims to verify (attribution controls, 2026-11-12 02:42-03:11 JST)

Raw logs: logs/ctl/r1..r4 (cells c01 c02 c10 c15 c17 from rounds2.sh, c05 c06 from rounds3.sh);
summary logs/ctl/summary.md (SETS="P M100 A A0 AC AC0" ROUNDS_FILE=rounds-ctl-cells.txt).
Sets: A0 = A with BPFTIME_CUDA_WATCHER_HOT_US=0 (no busy poll after a request); AC0 = AC with
BPFTIME_CUDA_WATCHER_K_DEFER=0 (no deferral sleeps). The build also changed A's unseen latch to
need two consecutive hits (reset when a noted launch ends a park). Set order shuffled per round and
cell with a seeded Python shuffle.

1. All 168 runs valid; max temperature 53.6 C.
2. One host call per launch (c01, c02, c10, c15, c17): A0, AC0 and AC are the same within noise
   in CPU, latency and load, and all three differ from A / M100 the same way. So AC's gain there
   comes entirely from dropping the 200 us busy poll; the deferral sleeps add nothing (c01 > 1 ms
   A0 0.84%, AC0 0.90%, AC 1.00%, A 3.48%, M100 3.55%; c15 0.21 / 0.22 / 0.46 vs 6.91 / 6.91%;
   CPU c02 0.019 / 0.019 / 0.021 vs A 0.093).
3. With 6 load threads the busy poll itself hurts: A (with it) vs A0 (without), c01 load vs P
   0.979 vs 0.991, c15 0.948 vs 0.989.
4. Bursts (32 calls per launch, c05 / c06): without any busy poll (A0) every call waits for a
   100 us sleep: cost per call 109 / 105 us vs M100 17.0 / 10.7, per-launch time 6,073 / 5,876 us
   vs P 2,580 / 2,529. C's adaptive window opens there (win20 = 74,400 of 76,832 requests) and
   keeps AC at M100's level (17.1 / 12.4 us). So C's window is what lets AC drop the busy poll
   in sparse calls without losing bursts.
5. The deferral sleeps help only in bursts: AC vs AC0 cost per call 17.1 vs 19.3 (c05), 12.4 vs
   14.9 us (c06).
6. c10 (400 calls per run) tail counts are noisy across the two measurement rounds: M100 > 1 ms
   0.50% in the main round, 1.31% here; A 0.19% / 0.56%; differences in c10 are not reliable.
7. Unseen latch after the fix: 0 latches in the 80 A-family runs of c01/c02/c10/c15/c17, 2 runs
   with one isolated hit.
8. c17 (duty-cycled load) stays bad for every sleeping set (> 1 ms 25.7-30.7%), so dropping the
   busy poll does not address it.
