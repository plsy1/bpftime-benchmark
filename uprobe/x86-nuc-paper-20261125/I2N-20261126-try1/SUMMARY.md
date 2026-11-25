# I2N-20261126-try1 — nginx with 2 workers (x86 NUC), summary

Eval plan I2 (W3, W6), multi-worker part. Session ended 05:59:19, exit 0; checks before/after PASS. 160 measured rows
(+ 8 warm-up), all valid. Policy W2 (new, harness/config-x86.json): `worker_processes 2` (harness/nginx-2w.conf), both
workers under the CPU mask {2,6} (not pinned per worker), consumer on 4, wrk on 8–11 with 4 threads; 16 B payload; 2 rounds
× 10 paired reps. Kernel legs use the multi-pid counting loader `sslsniff-s4mp` (one -p per worker; BPF object
instruction-identical to s4; built by scripts/build-sslsniff-mp.sh), bpftime legs the s4 loader (agent only in nginx, no
-p). Harness change (run_paper.py vs .pre-i2): start_nginx takes the worker count and config; per-process metrics are
summed over workers; the 1-worker path is unchanged (smoke B and K under policy M matched earlier sessions).
Tables: summary-i2n.md (analyze_paper.py), sensitivity.txt. Kb = K+SSLSNIFF_PB_WAKEUP=32 (with S4mp).

## Throughput (GMR of rps, both rounds, n = 20, 95 % CI; rounds differ by ≤ 0.008, O/K 0.024)

| ratio | value |
|---|---|
| Dn / Kb | 1.384 [1.380, 1.388] |
| Dnpc / Kb | 1.263 [1.259, 1.266] |
| Ol / Kb | 1.186 [1.181, 1.192] |
| Dn / Ol | 1.167 [1.161, 1.172] |
| Dnpc / Ol | 1.065 [1.059, 1.070] |
| L3port / Dn | 0.968 [0.962, 0.974] |
| Dn / K | 1.572 [1.566, 1.578] |
| Kb / K | 1.136 [1.132, 1.140] |
| O / K | 1.007 [0.954, 1.062] (4 low O runs, see flags; without them 1.055 [1.051, 1.060], n = 16) |

Median rps: B 215k (2 × the 1-worker B of ~109k), Dn 173k, L3port 168k, Dnpc 158k, Ol 149k, Kb 125k, O 116k, K 110k.
Delivery: median ≥ 99.99 % per leg, ≥ 99.92 % in every run. Reported loss occurs in some K (8/20), Kb (10/20), Dn (5/20,
max 0.083 % of events) and Dnpc (3/20) runs; O, Ol and L3port report none but miss up to ~1,800 events per run unreported
(2·req − delivered; median 190 for O, 72 for L3port).

## Reading (corrected after the independent check)

1. With two nginx workers Tern still leads the kernel: unchecked 1.38× Kb (1.57× K), checked 1.26× Kb. Like-for-like
   single-worker numbers from other sessions (policy M, 16 B, S4 cnt loader): C1 Dn/Kb 1.350, Dn/K 1.532, Dn/Ol 1.149;
   DX1 Dn/Kb 1.355, Dnpc/Kb 1.240; E18 Dn/Kb 1.353, Dn/K 1.533, Dnpc/Kb 1.238, Dn/Ol 1.143. The two-worker lead is about
   2–2.5 % larger (CIs do not overlap) while Kb/K matches (1.136 vs 1.135). Cross-session and with a different kernel
   loader (S4mp: two uprobe consumers per hit, each filtering by pid, a small extra kernel cost the 1-worker K does not
   pay), so read it as "the lead holds, slightly larger", not as a measured scaling effect.
2. Against upstream head Tern keeps 16.7 % (unchecked) and 6.5 % (checked); L3port is 3.2 % behind Tern unchecked.
3. All legs deliver ≥ 99.92 % per run (16 B), so the ratios compare like with like; upstream's small shortfall is
   unreported, the kernel's and Tern's are reported.
4. "Not slower than Ol when not oversubscribed" (eval plan success criterion): met (Dn and Dnpc both faster).

## Flags

- Deviation: CPUs 0, 1, 3, 5, 7 stayed at a fixed 3.5 GHz with C-states ≥ 2 off during this session (a failed restore
  after I2churn; details in N7-20261126-try1/SUMMARY.md); the likely trigger of the core-0 activity below.
- noisy_core: 103 of 160 rows have core 0 above 5 % busy (median 8 %, max 16 %); no other non-measured core. Core 0 was
  quiet in single-worker sessions (I9 median 0.3 %); cause not identified. Per leg: K 20/20, O 18, Dnpc 18, Kb 18, Dn 15,
  B 11, Ol 2, L3port 1. Within-leg rps of flagged and unflagged rows can be compared only for B (215.4k vs 214.7k) and
  Dn (173.3k vs 172.3k), where they agree; K has no unflagged row and no clean kernel ratio exists. Core-0 busy tracks the
  leg (K 0.112 vs L3port 0.016 median) and correlates weakly with consumer-core busy (Spearman 0.20), so a leg-dependent
  disturbance cannot be ruled out. Clean-subset ratios that exist agree (Dn / Ol 1.170, n = 4; L3port / Dn 0.963, n = 5).
  Main results include all rows (PREREG-2 §6 rule).
- O: 4 of 20 runs at 74–104k rps (others 114.6–117.0k); in each, one worker's core was underused (core 2 busy 0.26–0.77
  vs ~0.99), always core 2. Likely an uneven split of wrk's 100 keep-alive connections between the workers
  (nginx-2w.conf has no reuseport, no worker_cpu_affinity, accept_mutex off), but per-worker connection or request
  counts were not recorded, so the cause is unverified. Only O shows it here.
- Workers share the mask {2,6}; the scheduler kept them on separate cores (cores 2 and 6 at ~0.99 in nearly every row).
- The kernel loader differs from the bpftime legs' loader only in its attach loop (one -p per worker); both load the same
  BPF object.
- analyze_paper.py's "x vs B us/req" (and per event /2) assume one saturated core; under W2 two cores are saturated, so
  per-request CPU is twice that figure. Ratios (GMR) are unaffected; do not compare these x values with 1-worker ones.
- Harness limits found in review (no effect on this session's data): per-worker CPU/schedstat/connection counts are not
  recorded (only sums); nginx_smaps_kb sums Rss/Shared over workers (double-counts shared pages; only Pss and
  Private_Dirty sums are meaningful); nginx_core and nginx_last_cpu describe one worker only; the S4mp loader does not
  check attach results (all 42 kernel-leg stderr files show "attached for 2 pid(s)" and ev/req 2.0000), and it leaves
  env.pid at the last worker for the GnuTLS/NSS attaches (nginx uses neither).
- Independent check (2 subagents: numbers, harness review): every number reproduces; corrections applied above.
