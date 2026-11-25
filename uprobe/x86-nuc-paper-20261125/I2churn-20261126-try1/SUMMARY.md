# I2churn-20261126-try1 — map churn thread scaling (x86 NUC), summary

Eval plan I2 (W3, W6), churn part. Session ended 05:07:37, exit 0; checks before/after PASS (CPUs 0–7 at 3.5 GHz).
180 runs, every traced run counted exactly (uretprobe hits = workload calls, 0 misses). Workload churnloop (thread cap
raised 6 → 16 for this run; target_func code identical to E11's, churn/src/CHANGES-I2.txt): T threads, thread i pinned
to the i-th CPU of 0,2,4,6,1,3,5,7, so T ≤ 4 uses one thread per P core and T = 8 uses both SMT threads of the 4 P cores
(no oversubscription, SMT at T = 8). Objects: churn (insert at entry, lookup + delete at exit; 4-byte key, 8-byte value,
hash map) and control (same probes, no maps). Backends none, kernel, official 4708f409, L3port, upstream head 70f7bc1f
(Ol, with the #730 lock), Ol730x (lock removed), Tern c71a9987 (dnew), Dn730f (Tern + lock, PREREG-2b). 8 s per run,
3 reps, backend order rotated. Table: summary-i2.md (churn/summarize_i2.py). Per-call time per thread = T / calls/s.

## Per-call time per thread, churn object (us, mean of 3)

| T | kernel | official | L3port | Ol | Ol730x | Tern | Dn730f |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | 1.790 | 0.370 | 0.364 | 0.422 | 0.372 | 0.366 | 0.408 |
| 2 | 2.079 | 0.548 | 0.542 | 0.649 | 0.576 | 0.417 | 0.459 |
| 4 | 2.137 | 1.638 | 1.630 | 1.545 | 1.644 | 0.449 | 0.522 |
| 8 | 3.100 | 3.646 | 3.544 | 3.830 | 3.948 | 0.671 | 0.883 |

## Map cost = churn − control (us per call, ± combined t95 half-width, n = 3 each)

| T | kernel | official | L3port | Ol | Ol730x | Tern | Dn730f |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | 0.097 ± 0.007 | 0.073 ± 0.002 | 0.068 ± 0.001 | 0.070 ± 0.000 | 0.069 ± 0.001 | 0.049 ± 0.003 | 0.052 ± 0.008 |
| 2 | 0.127 ± 0.047 | 0.186 ± 0.039 | 0.189 ± 0.086 | 0.231 ± 0.061 | 0.215 ± 0.034 | 0.044 ± 0.012 | 0.053 ± 0.034 |
| 4 | 0.085 ± 0.104 | 1.254 ± 0.061 | 1.249 ± 0.036 | 1.072 ± 0.064 | 1.263 ± 0.055 | 0.054 ± 0.092 | 0.047 ± 0.012 |
| 8 | 0.022 ± 0.064 | 3.058 ± 0.468 | 2.949 ± 0.693 | 3.044 ± 0.078 | 3.354 ± 0.155 | 0.141 ± 0.105 | 0.142 ± 0.133 |

Control (probes only): kernel 1.69 → 3.08 us; official/L3port/Ol730x 0.30 → 0.59; Ol 0.35 → 0.79; Tern 0.32 → 0.53;
Dn730f 0.36 → 0.74.

## Reading (corrected after the independent check)

1. Every upstream variant's hash map cost grows with threads: ~0.07 us at 1 thread, ~1.1–1.3 us at 4, ~3.0–3.4 us at 8.
   At 8 threads their per-call time is above the kernel's in every rep (upstream 3.37–3.99 us vs kernel ≤ 3.12), with
   Welch t95: Ol +0.73 ± 0.06 and Ol730x +0.85 ± 0.14 us (clear), official +0.55 ± 0.46 (marginal), L3port +0.45 ± 0.69
   (CI includes 0). Ol/Ol730x (3.80–3.99 in every rep) sit above official/L3port (3.37–3.55 in reps 1–2). The 4 → 8
   step also adds SMT sharing (untraced workload 0.006 → 0.014 us, kernel control 2.05 → 3.08), so it is not threads alone.
2. Removing the #730 lock does not help: Ol730x's churn per-call is not lower than Ol's (Ol − Ol730x −0.10 ± 0.05 at
   T = 4, −0.12 ± 0.13 at T = 8). With the map present the lock even lowers the measured map cost on the upstream base
   (Ol 1.07 vs Ol730x 1.26 at T = 4), presumably by serializing probes; churn − control is therefore not a
   lock-independent map cost on that base.
3. Tern's map cost is 0.04–0.05 us up to 4 threads and 0.14 at 8 (upper t95 0.24; Dn730f the same); at 8 threads Tern is
   4.6× faster per call than the kernel and 5.4× than official (means; medians 4.5× and 5.1×). The mechanism (striped
   hash map, lock-free lookups) is from the code, not tested here.
4. The #730 lock on the probe path (control object): Ol − Ol730x +0.049, +0.092, +0.192 us at 1, 4, 8 threads; Dn730f −
   Tern +0.038, +0.080, +0.211 — similar (25 % apart at T = 1). With the map (churn object) the bases differ in sign
   (upstream −0.10/−0.12, Tern +0.07/+0.21 at T = 4/8); "same on both bases" holds for the control only (exploratory).
5. Reproduces E11 for official (1.638 mean / 1.640 median vs 1.653) and the kernel (2.137 vs 2.137) at 4 threads. E11's
   0.421 was the older Tern build (D, 9ccda7e8); this run's Tern c71a9987 gives 0.449 mean / 0.430 median.

## Flags

- n = 3 per cell; CIs for map cost combine two unpaired sets of reps. Backend order rotates by one position per rep,
  so relative order is almost fixed (official → L3port → Ol → Ol730x); rep 0 at T = 8 is high for all four upstream
  legs (3.84–3.88), which inflates the official/L3port CIs.
- Deviation: the pin restore at the end of this session failed (run-i2.sh listed SMT siblings, so pin-state.txt had
  duplicate C-state entries); CPUs 0–7 stayed pinned until ~07:15 (fixed then; see N7-20261126-try1/SUMMARY.md). This
  session's own data are unaffected.
- T = 8 shares each P core between two SMT threads (stated); E cores were not used.
- The pinning script printed "Invalid argument" for cpu0 cpuidle state0 to the terminal (not in console.log); busy
  checks before and after PASS.
- All 168 traced runs: hits = total_ops, misses 0, workload rc 0 (the 12 'none' runs have no loader).
- churnloop rebuild: target_func identical and at the same address; worker differs only in cpu_of's address; counters
  64-byte aligned (no false sharing); the E11 binary had debug info, the new one not.
- Independent check (subagent, own scripts): every table number reproduces; readings corrected as above.
