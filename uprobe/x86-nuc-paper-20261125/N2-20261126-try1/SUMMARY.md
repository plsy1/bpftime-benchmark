# N2-20261126-try1 — E6/N2: real tools on multithreaded workloads (x86 NUC), summary

Eval plan E6/N2. Session 17:50:28–18:57:00 (66.5 min; plan 76), exit 0; check-before/after PASS (P cores 3518–3530 MHz,
E core 8 2538–2558 MHz); e6 inputs sha256 OK before and after (harness, workloads-mt, mt sources, bpftrace 0.20.2,
libbpf-tools; the bpftime builds are not in that list — their own sha256.txt check OK post hoc). 252 rows = 3 T × 4 tools ×
7 backends × 3 rounds. [V] = checked against jsonl/raw/source; [I] = inferred.

Workloads e6/workloads-mt callloop/mallocloop: one worker per CPU of the taskset mask, pinned. T=1: CPU 2; T=4: {0,2,4,6}
(one SMT thread per P core); T=8: 0–7 (adds the SMT siblings). Tools (consumer) on E core 8. ops/s = aggregate over threads
(median of per-second rates, 10 s); truth = exact total_ops. Tools: memleak (libbpf-tools; runs the workload as its child
with -c and stops on SIGCHLD), bpftrace-uprobe-count (`@c = count()`), bpftrace-uretprobe-hist (`uprobe { @s[tid]=nsecs }
uretprobe /@s[tid]/ { @lat=hist(nsecs-@s[tid]); delete(@s[tid]); } END { clear(@s) }`), funclatency-libbpf. Backends none,
K, Ol (upstream head 70f7bc1f, with #730), OlL3 (+ uid/gid cache), OlL3x730 (OlL3 without #730), Dn (Tern c71a9987), Dn730f
(builds/dn730f = ladder/dn730-c71a9987 @ 1a2d42ec: c71a9987 + revert of "no per-CPU execution lock" + the inlined
per-CPU lookup reading the lock's CPU, clean tree [V]). Per round the T order is a seeded permutation (r0 T4→T8→T1, r1
T8→T1→T4, r2 T1→T4→T8); backend order seeded by (session-T, round, tool). bpftrace runs: tool start, 3 s, 10 s workload,
2 s, SIGINT; if the tool has not exited after 10 s, a second SIGINT, 5 s, then SIGKILL (a ~26 s run = exit right after the
second SIGINT).

Validity. strict-valid = run rc 0 (memleak: tool_rc; bpftrace/funclatency rows: workload_rc — the jsonl has no tool rc, so
a bpftrace hang at END counts as rc 0), ops not null, reported > 0. 213/252 strict-valid: excluded are funclatency on all 3
upstream backends (27 rows, tool never loaded: `[map_init] Unsupported map type: 8`, as in E6) and 12 upstream hist rows at
T ≥ 4 (crash or hang). throughput-valid additionally keeps the 4 hang-at-END rows with rc 0 (217 valid; sensitivity only).
memleak on upstream (27 rows) is excluded from comparisons: every report is one empty stack (no stack walk, E6 finding).

Units: x = (1/ops_b − 1/ops_none)·1e6/E µs of wall time per event, paired with the same block's none run, mean and 95 % t-CI
(n = 3, df = 2); E = probe hits per op (count 1, hist 2, funclatency 2, memleak 3). x·T = µs of worker-thread CPU per event
(all T workers busy: util > 0.9 [V]; at T=8 SMT-thread time). GMR = paired geometric-mean ratio of ops/s, log-t CI (n = 2:
t = 12.71). Delivery: expected − reported per round (count, hist, funclatency); memleak: harness sum of outstanding
allocations (Dn/Dn730f: 1024 at mallocloop's 64-B stack + 4 / 13 / 17 others at T = 1/4/8, e.g. at T8 4 at pthread_create, 8
per-worker slot arrays, 1 stdio buffer, 4 in ld-linux; K: 1028, 1035–1037, and 2326/1037/1036 with extra 128/256 MB
two-allocation stacks).

## Per-cell results

Columns: n / median ops/s / CV % / x µs [95 % CI] / x·T / delivery. At T=1, x·T = x.

### T=1 (CPU 2)

| tool | backend | n | med ops/s | CV | x µs [CI] | x·T | delivery |
|---|---|---|---|---|---|---|---|
| memleak | none | 3 | 141.0M | 0.59 | – | – | – |
| | K | 3 | 354k | 0.73 | 0.942 [0.925, 0.959] | 0.94 | 1028 ×3 |
| | Ol / OlL3 / OlL3x730 | 3/3/3 | 1.004M / 1.010M / 1.060M | ≤0.72 | 0.330 / 0.328 / 0.312 | | empty stack, 1033–1034 |
| | Dn | 3 | 346k | 1.19 | 0.964 [0.936, 0.993] | 0.96 | 1028 ×3 |
| | Dn730f | 3 | 330k | 1.31 | 1.001 [0.969, 1.033] | 1.00 | 1028 ×3 |
| count | none | 3 | 180.8M | 0.08 | – | – | – |
| | K | 3 | 714k | 0.37 | 1.393 [1.380, 1.405] | 1.39 | exact |
| | Ol / OlL3 | 3/3 | 5.275M / 5.271M | 0.04 | 0.184 / 0.184 | | exact |
| | OlL3x730 | 3 | 5.753M | 0.20 | 0.168 [0.167, 0.169] | | exact |
| | Dn | 3 | 5.715M | 0.09 | 0.1695 [0.169, 0.170] | | exact |
| | Dn730f | 3 | 5.317M | 0.09 | 0.183 [0.182, 0.183] | | exact |
| hist | none | 3 | 180.8M | 0.03 | – | – | – |
| | K | 3 | 553k | 0.28 | 0.901 [0.894, 0.907] | | exact |
| | Ol / OlL3 / OlL3x730 | 3/3/3 | 1.877M / 1.915M / 2.095M | ≤0.36 | 0.263 / 0.259 / 0.235 | | exact |
| | Dn | 3 | 2.407M | 0.16 | 0.205 [0.204, 0.206] | | exact |
| | Dn730f | 3 | 2.202M | 0.30 | 0.224 [0.223, 0.226] | | exact |
| funclat | none | 3 | 180.7M | 0.15 | – | – | – |
| | K | 3 | 556k | 0.28 | 0.896 [0.890, 0.902] | | exact |
| | upstream ×3 | 0 | – | – | – | – | not loaded (9/9) |
| | Dn | 3 | 2.576M | 0.37 | 0.192 [0.190, 0.193] | | exact |
| | Dn730f | 3 | 2.329M | 0.02 | 0.212 [0.212, 0.212] | | exact |


### T=4 (CPUs 0, 2, 4, 6)

| tool | backend | n | med ops/s | CV | x µs [CI] | x·T | delivery |
|---|---|---|---|---|---|---|---|
| memleak | none | 3 | 561.1M | 0.44 | – | – | – |
| | K | 3 | 1.220M | 0.40 | 0.273 [0.270, 0.276] | 1.09 | 1035–1037 |
| | Ol / OlL3 / OlL3x730 | 3/3/3 | 2.199M / 2.221M / 2.413M | 1.7 / 0.4 / 2.1 | 0.150 / 0.150 / 0.137 | 0.60 / 0.60 / 0.55 | empty stack, 1042–1043 |
| | Dn | 3 | 557k | 0.74 | 0.597 [0.586, 0.608] | 2.39 | 1037 ×3 |
| | Dn730f | 3 | 542k | 0.92 | 0.615 [0.601, 0.629] | 2.46 | 1037 ×3 |
| count | none | 3 | 724.6M | 0.02 | – | – | – |
| | K | 3 | 2.536M | 0.67 | 0.392 [0.386, 0.399] | 1.57 | exact |
| | Ol | 3 | 10.76M | 0.69 | 0.0919 [0.0903, 0.0935] | 0.37 | exact |
| | OlL3 | 3 | 10.87M | 1.10 | 0.0906 [0.0881, 0.0932] | 0.36 | exact |
| | OlL3x730 | 3 | 12.23M | 0.45 | 0.0802 [0.0793, 0.0811] | 0.32 | exact |
| | Dn | 3 | 12.71M | 7.01 | 0.0745 [0.0618, 0.0872] | 0.30 | exact |
| | Dn730f | 3 | 12.51M | 2.12 | 0.0796 [0.0752, 0.0839] | 0.32 | exact |
| hist | none | 3 | 724.5M | 0.28 | – | – | – |
| | K | 3 | 1.840M | 0.74 | 0.270 [0.265, 0.275] | 1.08 | 3 / 1 / 0 |
| | Ol | strict 1, thru 2 | 1.683M (strict); 1.731M (thru) | – | 0.296 (n1); thru 0.289 [0.188, 0.389] | 1.19 (thru 1.15) | r0 exact; r1 SEGV; r2 hang at END |
| | OlL3 | 2 | 1.760M | 1.01 | 0.283 [0.258, 0.309] | 1.13 | r0 SEGV; 1 / 0 |
| | OlL3x730 | 2 | 1.693M | 0.66 | 0.295 [0.277, 0.312] | 1.18 | r0 SEGV; 3 / 2 |
| | Dn | 3 | 8.094M | 0.47 | 0.0612 [0.0605, 0.0620] | 0.25 | 1 / 1 / 1 |
| | Dn730f | 3 | 6.901M | 1.28 | 0.0721 [0.0698, 0.0745] | 0.29 | exact ×3 |
| funclat | none | 3 | 721.6M | 0.31 | – | – | – |
| | K | 3 | 1.841M | 0.73 | 0.271 [0.266, 0.276] | 1.08 | exact |
| | upstream ×3 | 0 | – | – | – | – | not loaded (9/9) |
| | Dn | 3 | 8.327M | 7.80 | 0.0622 [0.0494, 0.0750] | 0.25 | exact |
| | Dn730f | 3 | 7.059M | 0.86 | 0.0700 [0.0685, 0.0715] | 0.28 | exact |

Each Dn cell with CV ≈ 7 % comes from one outlier round [V]:
- count r0 is high: 14.30M against 12.71M and 12.67M.
- funclatency r1 is low: 7.26M against 8.33M and 8.36M.


### T=8 (CPUs 0–7, with SMT siblings)

| tool | backend | n | med ops/s | CV | x µs [CI] | x·T | delivery |
|---|---|---|---|---|---|---|---|
| memleak | none | 3 | 614.2M | 0.27 | – | – | – |
| | K | 3 | 1.760M | 0.69 | 0.188 [0.185, 0.192] | 1.51 | r0 2326 (2288 at the 64-B stack), r1 1037, r2 1036 |
| | Ol / OlL3 / OlL3x730 | 3/3/3 | 2.384M / 2.281M / 2.158M | 5.7 / 3.2 / 2.2 | 0.142 / 0.146 / 0.154 | 1.14 / 1.17 / 1.23 | empty stack, 1045–1046 |
| | Dn | 3 | 526k | 2.86 | 0.642 [0.596, 0.688] | 5.14 | 1041 ×3 |
| | Dn730f | 3 | 501k | 1.50 | 0.668 [0.643, 0.693] | 5.35 | 1041 ×3 |
| count | none | 3 | 555.8M | 3.38 | – | – | – |
| | K | 3 | 3.176M | 0.73 | 0.313 [0.307, 0.318] | 2.50 | exact |
| | Ol | 3 | 13.68M | 0.27 | 0.0714 [0.0707, 0.0720] | 0.57 | exact |
| | OlL3 | 3 | 13.77M | 2.47 | 0.0716 [0.0672, 0.0760] | 0.57 | exact |
| | OlL3x730 | 3 | 16.41M | 1.04 | 0.0591 [0.0575, 0.0608] | 0.47 | exact |
| | Dn | 3 | 21.58M | 2.45 | 0.0444 [0.0417, 0.0472] | 0.36 | exact |
| | Dn730f | 3 | 16.28M | 1.31 | 0.0594 [0.0576, 0.0612] | 0.48 | exact |
| hist | none | 3 | 554.4M | 3.50 | – | – | – |
| | K | 3 | 2.538M | 0.21 | 0.196 [0.195, 0.197] | 1.57 | 2 / 5 / 6 |
| | Ol | strict 0, thru 1 | (1.593M, hang row) | – | (0.313) | (2.50) | r0, r1 SEGV + lock_exception; r2 hang |
| | OlL3 | 0 | – | – | – | – | SEGV ×3 |
| | OlL3x730 | strict 1, thru 3 | 1.537M | thru 3.62 | strict 0.324 (n1); thru 0.322 [0.293, 0.351] | 2.58 | r0, r1 hang; r2 short by 1 |
| | Dn | 3 | 10.16M | 0.64 | 0.0482 [0.0475, 0.0489] | 0.39 | 0 / 0 / 3 |
| | Dn730f | 3 | 7.699M | 4.88 | 0.0631 [0.0554, 0.0708] | 0.50 | exact ×3 |
| funclat | none | 3 | 584.6M | 5.28 | – | – | – |
| | K | 3 | 2.541M | 0.48 | 0.196 [0.194, 0.199] | 1.57 | exact |
| | upstream ×3 | 0 | – | – | – | – | not loaded (9/9) |
| | Dn | 3 | 10.58M | 5.46 | 0.0452 [0.0392, 0.0512] | 0.36 | exact |
| | Dn730f | 3 | 8.713M | 1.77 | 0.0570 [0.0543, 0.0596] | 0.46 | exact |


The none baselines at T=8 are noisy (callloop CV 3.4–5.3 %); callloop none at T=8 runs at 0.77–0.81× its T=4 rate,
mallocloop at 1.09× [V]; cause not determined.

## Paired GMR of ops/s (strict-valid; n = 3 unless marked)

| T | tool | Dn/OlL3x730 | Dn730f/OlL3 | Dn/Ol | Dn/K | Dn730f/Dn | OlL3x730/OlL3 |
|---|---|---|---|---|---|---|---|
| 1 | memleak* | 0.325 [0.317, 0.334] | 0.330 [0.318, 0.341] | 0.344 | 0.977 [0.937, 1.018] | 0.964 [0.931, 0.997] | 1.052 |
| 1 | count | 0.992 [0.987, 0.998] | 1.009 [1.007, 1.010] | 1.083 | 7.99 | 0.930 [0.928, 0.932] | 1.093 |
| 1 | hist | 1.146 [1.136, 1.156] | 1.152 [1.135, 1.169] | 1.280 | 4.35 | 0.916 | 1.097 |
| 1 | funclat | NA | NA | NA | 4.62 | 0.905 | NA |
| 4 | memleak* | 0.230 [0.218, 0.243] | 0.244 [0.240, 0.248] | 0.252 | 0.458 [0.446, 0.471] | 0.970 [0.933, 1.008] | 1.093 |
| 4 | count | 1.078 [0.918, 1.265] | 1.137 [1.052, 1.229] | 1.232 [1.021, 1.487] | 5.20 [4.36, 6.20] | 0.936 [0.805, 1.087] | 1.128 [1.087, 1.170] |
| 4 | hist | 4.761 [4.725, 4.798] n2 | 3.929 [3.699, 4.174] n2 | 4.81 n1 (thru 4.66 [3.13, 6.95] n2) | 4.37 [4.24, 4.51] | 0.851 [0.817, 0.886] | 0.962 n2 |
| 4 | funclat | NA | NA | NA | 4.33 [3.54, 5.29] | 0.887 [0.729, 1.080] | NA |
| 8 | memleak* | 0.241 [0.214, 0.272] | 0.219 [0.207, 0.232] | 0.222 | 0.294 [0.276, 0.313] | 0.961 [0.928, 0.994] | 0.947 [0.827, 1.086] |
| 8 | count | 1.318 [1.209, 1.437] | 1.199 [1.152, 1.248] | 1.583 [1.486, 1.686] | 6.80 [6.41, 7.22] | 0.755 [0.709, 0.805] | 1.204 [1.108, 1.308] |
| 8 | hist | 6.61 n1 (thru 6.57 [5.92, 7.29] n3) | NA (OlL3 0/3 in both modes) | NA (thru 6.38 n1) | 4.01 [3.93, 4.09] | 0.768 [0.672, 0.878] | NA |
| 8 | funclat | NA | NA | NA | 4.28 [3.70, 4.95] | 0.796 [0.701, 0.905] | NA |

\* memleak against upstream is excluded from comparison (upstream stores every allocation under one empty stack: no stack walk); shown for completeness only.


## Pre-registered N2 criterion (eval plan L441, from reviews RV:173): supporting evidence only

Requirement: Dn/OlL3x730 and Dn730f/OlL3 both with 95 % lower bound > 1.05 and point ≥ 1.10, on ≥ 2 tools at T ≥ 4.
Evaluable tools: count and hist only (funclatency: upstream cannot load it; memleak: upstream does no stack walk).

| T | count | hist |
|---|---|---|
| 4 | fail: Dn/OlL3x730 1.078 [0.918, 1.265] (per round 1.161, 1.040, 1.036; without r0 ~1.04); Dn730f/OlL3 1.137 [1.052, 1.229] passes alone | pass (n = 2 each: 4.76 [4.73, 4.80]; 3.93 [3.70, 4.17]) |
| 8 | pass: 1.318 [1.209, 1.437]; 1.199 [1.152, 1.248] | not evaluable: OlL3 crashed 3/3 (throughput-valid Dn/OlL3x730 6.57 [5.92, 7.29], n = 3, passes) |

- **Literal pooled reading (≥ 2 tools across T ≥ 4): met** (T4 hist + T8 count, both pairs).
- **Per-T reading (≥ 2 tools at one T): not met** — at T4 count fails; at T8 hist is missing because upstream crashed, not
  because a ratio failed.
- The same verdicts hold with x ratios instead of ops GMR (T4 count 1.079 [0.917, 1.270] / 1.140 [1.053, 1.233]; T8 count
  1.331 [1.217, 1.456] / 1.205 [1.156, 1.255]; T4 hist 4.80 / 3.96).
- Caveats: the T4 hist pass rests on n = 2 upstream rounds that survived a tool crashing or hanging in 4 of 9 upstream runs
  at T = 4 (biased toward surviving runs). Which reading applies is the authors' call; either way it is supporting evidence
  only, not Q1's answer (Q1 = E7b, not met).
- Count at T=8 is the clean result: Dn/OlL3x730 grows with threads, 0.992 (T1) → 1.078 (T4) → 1.318 (T8).

## Scaling: median ops(T) / ops(1), T4 / T8 (upstream hist: throughput-valid)

| tool | none | K | Ol | OlL3 | OlL3x730 | Dn | Dn730f |
|---|---|---|---|---|---|---|---|
| memleak | 3.98 / 4.36 | 3.45 / 4.97 | 2.19 / 2.38 | 2.20 / 2.26 | 2.28 / 2.04 | **1.61 / 1.52** | **1.64 / 1.52** |
| count | 4.01 / 3.07 | 3.55 / 4.45 | 2.04 / 2.59 | 2.06 / 2.61 | 2.13 / 2.85 | 2.22 / 3.78 | 2.35 / 3.06 |
| hist | 4.01 / 3.07 | 3.33 / 4.59 | **0.92 / 0.85 (n1)** | **0.92 / NA** | **0.81 / 0.73** | 3.36 / 4.22 | 3.13 / 3.50 |
| funclat | 3.99 / 3.24 | 3.31 / 4.57 | NA | NA | NA | 3.23 / 4.11 | 3.03 / 3.74 |


1. **Upstream hist gets slower with threads, with and without #730 (0.73–0.92× its T1 rate) [V]; Tern's scales 3.4–4.2×.**
   [I, from source] @s is a BPF_MAP_TYPE_HASH; upstream maps it to fix_size_hash_map_impl with `should_lock = true`
   (olatest fix_hash_map.hpp:30), so every @s access (update at entry; lookup and delete at return: ≥ 3 per call) takes one
   map-wide pthread spinlock shared by all threads (map_handler.hpp:283, map_handler.cpp do_lookup/do_update). Tern's HASH
   has `should_lock = false` (dnew fix_hash_map.hpp:33), lock-free lookups and per-stripe writer locks
   (bpftime_hash_map.hpp:130-146; Tern 48b6c166). count has no @s (its @c is a PERCPU_ARRAY [V, Tern helper log]) and
   upstream count scales 2.0–2.85×. So the 4–5× hist gap at T ≥ 4 reflects Tern's map design under threads (on n = 2
   surviving upstream runs at T4).
2. **Tern's memleak scales worst of all backends [V]**: per-thread CPU per event 0.96 → 2.39 → 5.14 µs at T = 1/4/8 (K 0.94 →
   1.09 → 1.51); Dn/K 0.977 → 0.458 → 0.294. [I] Most likely the x86 stack walk: generate_stack creates and unrefs a
   gum_backtracer_make_accurate() object on every malloc (dnew frida_uprobe_attach_impl.cpp:490-503), with libunwind's
   dl_iterate_phdr / GObject global state as cross-thread serialization points; the stack-map hit path takes no lock and the
   hash map uses per-stripe locks. Upstream's per-thread memleak cost also grows (0.33 → 0.60 → 1.14 µs) without a stack
   walk. Not profiled. Recorded as a Tern finding (not fixed here).
3. **#730 lock cost with T**: grows for Tern on hist (Dn730f/Dn 0.916 / 0.851 / 0.768) and funclatency (0.905 / 0.887 /
   0.796), and on count at T8 on both bases (Dn730f/Dn 0.930 / 0.936 [0.805, 1.087] / 0.755; OlL3x730/OlL3 1.093 / 1.128 /
   1.204). Not on memleak (Dn730f/Dn 0.964 / 0.970 / 0.961; OlL3x730/OlL3 1.052 / 1.093 / 0.947). Dn730f also changes the
   inlined per-CPU lookup's CPU source (serves count's @c), so Dn730f/Dn on count is lock + that change. Both trees declare
   `interprocess_mutex locks[1024]` unpadded (40 B each) [V]; which CPUs share cache lines depends on the array's alignment
   in the segment (not checked) [I]; at T1 the lock already costs 7–9.5 % with no sharing.
4. Dn count, hist and funclatency scale 3.8–4.2× at T8, close to K's 4.45–4.59×; Dn/K is 4.0–8.0× on these tools at every
   T. Upstream memleak runs 2.9× (T1), 3.95× (T4), 4.5× (T8) Dn's ops on less work.

## Delivery and hist total vs truth (E4's real-tool version)

- count and funclatency: exact on every valid run of every backend at every T [V].
- hist, valid runs: T=1 exact on all 6 traced backends; at T ≥ 4 every non-zero shortfall is ≤ T−1 per run (13 runs) [V].
  Loss rates at T ≥ 4 (valid runs; 95 % Clopper–Pearson):

| backend | lost / events | rate | 95 % CP |
|---|---|---|---|
| K | 17 / 132.35M | 1.28e-7 | [7.5e-8, 2.1e-7] |
| Dn | 6 / 550.36M | 1.09e-8 | [4.0e-9, 2.4e-8] |
| Dn730f | 0 / 443.12M | 0 | [0, 8.3e-9] |
| OlL3x730 | 6 / 49.52M | 1.21e-7 | [4.5e-8, 2.6e-7] |
| OlL3 | 1 / 35.40M | 2.8e-8 | – |
| Ol | 0 / 16.93M | 0 | – |

  The kernel's rate is ~12× Dn's (CP intervals disjoint; a binomial test assuming independent events gives p ≈ 2e-8, but
  the ≤ T−1-per-run pattern argues against independence). Builds without #730 lose (Dn 6, OlL3x730 6), builds with it lose
  0–1 (Dn730f 0, OlL3 1, Ol 0) [I; small counts]. Mechanism open: candidates are a lookup miss on @s under concurrent
  insert/delete churn from T CPUs (kernel prealloc htab element reuse; Tern lock-free lookups next to per-stripe writers)
  and, for the kernel, the per-CPU recursion guard skipping a program [I]. Every call's entry and return fall inside the
  traced window (attach 3 s before, stop 2 s after the workload) [V harness].
- Upstream hist at T ≥ 4: of 18 runs, 6 are valid (losing 1–3 each, as above) and 12 failed (3 of these printed a partial
  histogram from a crashed workload: 30, 9,745,759 and 1,229,640 events, no truth).
- memleak: K, Dn, Dn730f hold the expected 1024 outstanding 64-B blocks at mallocloop's stack in every run except K T8 r0
  (2288 blocks): [I] a snapshot taken during churn (memleak prints every 1 s and stops on SIGCHLD; this report is stamped
  at the workload's end; earlier K snapshots in the same run swing between 1019 and 5414) or missed frees. Upstream:
  1033–1034 / 1042–1043 / 1045–1046 in one empty stack.

## Upstream failures (bpftrace-uretprobe-hist; upstream count 18/18 exact, K/Dn/Dn730f 0 failures on any tool)

Failures: 0/9 at T1, 4/9 at T4, 8/9 at T8; at T ≥ 4: Ol 5/6, OlL3 4/6, OlL3x730 3/6 [V]. Crash time ≈ END-probe log time − 2 s
(the harness sends SIGINT 2 s after the workload exits) [V]: 3 of 8 crashes at 6–8.5 s, 4 of 8 at ≥ 3 s.

| class | what happened | runs |
|---|---|---|
| S1 | bpftrace aborts with `terminate … boost::interprocess::lock_exception` at the point where its SIGINT-time END-probe "Created … /proc/self/exe, offset d4de0" line should appear; workload SIGSEGV (rc −11); nothing printed | T4 OlL3x730 r0 (6.0 s); T8 Ol r0 (14.1 s), Ol r1 (12.9 s) |
| S2 | workload SIGSEGV; partial @lat printed plus stale @s[tid] entries; no hang | T8 OlL3 r1 (12.6 s, 9.75M events, crash about 6 s in [I], 5 stale @s); T8 OlL3 r2 (6.9 s, 1.23M events, about 1 s in, 7 stale @s) |
| S3 | workload SIGSEGV within about 2 s; partial @lat (30 events); then bpftrace hangs | T4 OlL3 r0 (16.0 s) |
| S4 | workload SIGSEGV; END registered; nothing printed; hang | T4 Ol r1 (19.1 s); T8 OlL3 r0 (17.2 s) |
| S5 | workload finishes normally (rc 0, total_ops printed); END registered; nothing printed; hang; reported 0 | T4 Ol r2; T8 Ol r2, OlL3x730 r0, OlL3x730 r1 (all about 26.1 s) |


Totals: SIGSEGV T4 3, T8 5; hang with rc 0 T4 1, T8 3; lock_exception 1 + 2.

Mechanism (paths under olatest-70f7bc1f/runtime/src/):
1. [V code] Upstream's PERCPU_HASH takes no lock: a boost::unordered_map in shared memory with `should_lock = false`
   (per_cpu_hash_map.hpp:25-27); elem_update does an unlocked find and, on a miss, allocates from the segment and inserts
   (per_cpu_hash_map.cpp:108, :112, may rehash); every event does an unlocked find (:59); get_next_key/lookup_userspace
   (:136-187) take no lock and use the scratch key_templates[0] shared with CPU 0's worker. @lat is the PERCPU_HASH [I,
   strong indirect evidence: Tern's typed-helper selection stays silent for the uretprobe program, which happens only when a
   program uses both HASH and PERCPU_HASH].
2. [I] Concurrent insert/rehash/find corrupts that map: a find during a rehash reads a freed bucket array (SIGSEGV);
   corrupted chains make get_next_key loop while bpftrace prints @lat at END (hang). Inserts happen whenever a new histogram
   bucket appears, so late crashes fit new tail buckets.
3. [V code / I effect] S3 hang: @s uses a non-robust pthread spinlock in shared memory (fix_hash_map.hpp:30;
   map_handler.hpp:45-58, 283); a worker killed while holding it leaves bpftrace spinning.
4. [V code / I effect] S1: Boost 1.83 process-shared mutexes are robust and throw lock_exception on EOWNERDEAD; [I] the dead
   owner held the segment-manager mutex taken by per-CPU inserts; bpftrace's next allocation (END-probe handler at SIGINT)
   throws, uncaught.
5. Ruled out: Frida/uretprobe return handling (Tern uses the same Frida 16.1.2 listener pattern), upstream's identity caches.
6. Why Tern does not fail (Dn and Dn730f 12/12 at T ≥ 4) [V code]: Tern's per-CPU hash (48b6c166) is a fixed-capacity
   striped table: no rehash, lock-free lookups, per-stripe writer locks; slots come from chunks allocated from the segment
   once per 256 slots under the pool spinlock (dnew per_cpu_hash_map.hpp:23-26, .cpp:88-100), so the event path allocates
   rarely. [I] Tern's stripe/pool spinlocks are not robust either, and a workload dying inside a chunk allocation could leave
   the segment-manager mutex owner-dead (S1-like); not observed (Tern never crashed).
7. Not run; would confirm: upstream uprobe-only `@h = hist(arg0 & 0xffff)` at T4/T8 (expected to fail), uretprobe-only
   `@n = count()` (expected to pass), core dumps of the workload, `bpftrace -d` for the map types.

## Deviations from the eval plan

- Dn730f in place of the plan's Dn730 (as H7b; PREREG-2b's swap is defined for PREREG-2, applied here by analogy).
- No Kb and no checked leg (plan L171 asks every throughput experiment to carry them; the E6 section says Kb is not done
  for the real tools and must be stated). Kb needs per-tool patches; no Dnpc leg was planned for N2.
- Worker i runs on the i-th CPU of the mask in ascending order ("threads 4 cpus 0 2 4 6"); the sets match the plan; busy
  CPUs in the freq log match exactly (T1 {2}, T4 {0,2,4,6}, T8 {0–7}); CPU 8 (tool) above util 0.9 in only 9–18 samples per
  T ≥ 4 block, none at T1.
- Tool names: bpftrace count / hist / funclatency = bpftrace-uprobe-count / bpftrace-uretprobe-hist / funclatency-libbpf;
  bpftrace 0.20.2 (not 0.23.2: under bpftime its BEGIN/END probes never fire and printf never appears).
- Criterion cells funclatency and memleak are not evaluable; T8 hist not evaluable (upstream crashes).
- Raw outputs are in T<n>/ (not T<n>/measure/ as plan.txt says); memleak's report is in *.workload.txt.

## Flags

- Frequency: during the measure blocks 9303 of 9309 busy-worker-CPU samples at 3504–3520 MHz, 6 at 3499–3502 (all CPU 2,
  T4 r2); CPU 8 under load 2335–2570 MHz (median 2514); load before 0.19; Claude processes on CPUs 12–14.
- Ratios and x are paired within one (T, round, tool) block (minutes apart).
- Small n: n = 2 CIs use t = 12.71; every upstream hist number at T ≥ 4 is conditional on surviving a failing tool.
- T=8 numbers include SMT sharing; do not read T4 → T8 x·T growth as per-core cost growth.
- Tern findings recorded, not fixed (the Jetson session finished 11-26): memleak scaling (item 2 above); #730 cost growth.
- Upstream memleak and K memleak snapshots swing widely mid-run; final reports are stable except K T8 r0.
- Independent check: 2 analyzers + 1 failure diagnosis (own scripts), reconciled; then 3 verifiers (numbers, claims,
  completeness). All table numbers reproduce; the verifiers' corrections (hist program, upstream slowdown mechanism,
  criterion readings, loss rates, memleak hypothesis, #730 counterexamples, run structure, crash timing, deviations,
  roundings) are applied above.
