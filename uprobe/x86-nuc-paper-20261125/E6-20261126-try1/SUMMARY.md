# E6-20261126-try1 — E6 P2′: real-tool throughput (x86 NUC), summary

Eval plan E6 P2′. Session 16:26:19–17:39:48, exit 0; checks before/after PASS; e6 inputs sha256 OK before and after (plan lines 1 and 7; manifest of this run = e6/sha256.txt.E6). 270 rows = 6 tools × 9 backends × 5 rounds, one per cell, none missing. Of the 240 traced rows, 135 have an exact count (gethostlatency, bpftrace-gethostlatency, bpftrace-uretprobe-hist on all 8 traced backends; funclatency on K, Dn, Dnpc); 35 give no valid per-event cost (funclatency O/L3port 10, rc −11; funclatency Ol family 15, untraced; memleak O/L3port 10, ops null); 15 (memleak Ol family) are not like-for-like; the 40 malloc-example rows are inexact on every backend. Harness e6/harness/measure.py + audit.py (x86 copy of the Jetson J2 harness; this run used the audit.py now kept as audit.py.pre-n2, sha 76929ab5). Backends: none (no tracer), K (kernel 6.12.113), O (official 4708f409), L3port, Ol (upstream head 70f7bc1f), Olpc, OlL3x730, Dn (Tern c71a9987), Dnpc. Tools: gethostlatency, funclatency, memleak (libbpf-tools v0.37.0, CO-RE), malloc-example (official example, "-core" copy), bpftrace-uretprobe-hist and bpftrace-gethostlatency (bpftrace 0.20.2). Workloads (gcc -O2): resolveloop (1 getaddrinfo per op), callloop (1 target_func call per op), mallocloop 64 B × 1024 slots (1 free + 1 malloc per op). 10 s per run; ops/s = the workload's median per-second rate (the upper median of 10). Tools on CPU 4, workloads on CPU 2 (P cores fixed at 3.5 GHz), measure.py on 12–14. Backend order: a seeded permutation per (round, tool), sha256(session:round:tool).

Units: x = (1/ops_b − 1/ops_none) · 1e6 / E, in us per event, paired with the none run of the same round and tool; mean and 95 % t-CI (n = 5, df = 4). E = probe-program invocations per op: gethostlatency 2 (uprobe + uretprobe on getaddrinfo), bpftrace-gethostlatency 2, bpftrace-uretprobe-hist 2, funclatency 2, memleak 3 (malloc entry, malloc return with bpf_get_stackid(BPF_F_USER_STACK), free entry), malloc-example 1 (malloc uprobe + one bpf_printk). Ratios /K, /Ol = paired GMR of ops/s, log-space t-CI. Ratios vs none (40–430× on the call/malloc loops) are not reported (eval plan, W3). x averages over E events; where only one probe per op reads user memory (gethostlatency: entry probe; bpftrace-gethostlatency: return probe), the cost per checked read is 2 × the per-event Δx. Table: e6-table.json.

## Per-event cost and ratios

### gethostlatency (E = 2; none 55.3k ops/s, CV 0.45 %; counts exact 40/40, lost 0)

| backend | median ops/s | x us/event | /K | /Ol |
|---|---:|---|---|---|
| K | 48.9k | 1.220 [1.196, 1.244] | – | 0.937 [0.931, 0.943] |
| O | 49.4k | 1.084 [1.004, 1.164] | 1.013 [1.006, 1.021] | 0.950 [0.948, 0.951] |
| L3port | 52.7k | 0.462 [0.390, 0.535] | 1.080 [1.073, 1.087] | 1.012 [1.009, 1.015] |
| Ol | 52.0k | 0.573 [0.502, 0.644] | 1.067 [1.061, 1.074] | – |
| Olpc | 50.7k | 0.830 [0.751, 0.908] | 1.040 [1.032, 1.047] | 0.974 [0.972, 0.976] |
| OlL3x730 | 51.9k | 0.590 [0.527, 0.653] | 1.065 [1.058, 1.073] | 0.998 [0.993, 1.003] |
| Dn | 52.5k | 0.488 [0.443, 0.533] | 1.077 [1.072, 1.082] | 1.009 [1.004, 1.013] |
| Dnpc | 51.2k | 0.736 [0.669, 0.804] | 1.050 [1.043, 1.056] | 0.983 [0.980, 0.987] |

Paired Δx: Dn − OlL3x730 −0.102 [−0.146, −0.058] (GMR 1.011 [1.006, 1.015]); Dn − Ol −0.085 [−0.128, −0.043]; Dn − K −0.732 [−0.782, −0.683]; Dnpc − K −0.484 [−0.546, −0.421]; Dnpc − Dn +0.249 [0.208, 0.289]; Olpc − Ol +0.257 [0.238, 0.275]; O − L3port +0.621 [0.594, 0.649]; L3port − Dn −0.025 [−0.069, +0.018].

### bpftrace-gethostlatency (E = 2; none 55.4k, CV 0.43 %; counts exact 40/40, lost 0)

| backend | median ops/s | x us/event | /K | /Ol |
|---|---:|---|---|---|
| K | 49.1k | 1.191 [1.124, 1.258] | – | 0.943 [0.935, 0.952] |
| O | 52.0k | 0.597 [0.538, 0.656] | 1.062 [1.060, 1.063] | 1.002 [0.993, 1.011] |
| L3port | 52.1k | 0.578 [0.528, 0.629] | 1.064 [1.060, 1.068] | 1.004 [0.997, 1.011] |
| Ol | 51.9k | 0.614 [0.552, 0.677] | 1.060 [1.051, 1.069] | – |
| Olpc | 49.5k | 1.087 [1.014, 1.159] | 1.010 [1.006, 1.014] | 0.953 [0.945, 0.961] |
| OlL3x730 | 52.1k | 0.585 [0.525, 0.646] | 1.063 [1.056, 1.070] | 1.003 [0.993, 1.013] |
| Dn | 50.6k | 0.881 [0.817, 0.945] | 1.031 [1.024, 1.038] | 0.973 [0.962, 0.984] |
| Dnpc | 49.1k | 1.183 [1.100, 1.265] | 1.001 [0.995, 1.006] | 0.944 [0.933, 0.955] |

Paired Δx: Dn − OlL3x730 +0.296 [0.276, 0.316] (GMR 0.970 [0.968, 0.972]); Dn − Ol +0.267 [0.159, 0.374]; Dn − K −0.310 [−0.378, −0.242]; Dnpc − K −0.008 [−0.064, +0.048]; Dnpc − Dn +0.302 [0.267, 0.337]; Olpc − Ol +0.473 [0.393, 0.553]; O − L3port +0.019 [−0.015, +0.053].

### bpftrace-uretprobe-hist (E = 2; none 182M, CV 0.87 %; histogram total exact 40/40; no loss counter)

| backend | median ops/s | x us/event | /K | /Ol |
|---|---:|---|---|---|
| K | 551k | 0.902 [0.898, 0.906] | – | 0.295 [0.292, 0.298] |
| O | 2.10M | 0.236 [0.234, 0.238] | 3.79 [3.75, 3.84] | 1.118 [1.112, 1.125] |
| L3port | 2.15M | 0.230 [0.228, 0.233] | 3.88 [3.83, 3.93] | 1.145 [1.140, 1.150] |
| Ol | 1.88M | 0.264 [0.262, 0.266] | 3.39 [3.36, 3.43] | – |
| Olpc | 1.94M | 0.255 [0.251, 0.259] | 3.51 [3.45, 3.57] | 1.036 [1.023, 1.048] |
| OlL3x730 | 2.10M | 0.237 [0.234, 0.239] | 3.78 [3.74, 3.82] | 1.115 [1.100, 1.131] |
| Dn | 2.40M | 0.207 [0.205, 0.210] | 4.31 [4.25, 4.38] | 1.271 [1.260, 1.283] |
| Dnpc | 2.43M | 0.204 [0.202, 0.206] | 4.38 [4.31, 4.45] | 1.291 [1.284, 1.298] |

Paired Δx: Dn − OlL3x730 −0.029 [−0.032, −0.026] (GMR 1.140 [1.124, 1.156]); Dnpc − Olpc −0.051 [−0.053, −0.049] (GMR 1.247 [1.236, 1.258]); Dn − K −0.695 [−0.701, −0.689]; L3port − Dn +0.023 [0.021, 0.026].

### funclatency-libbpf (E = 2; none 181M, CV 0.72 %; only K, Dn, Dnpc valid, all exact 15/15; no loss counter)

| backend | median ops/s | x us/event | /K |
|---|---:|---|---|
| K | 554k | 0.902 [0.898, 0.906] | – |
| Dn | 2.55M | 0.194 [0.193, 0.195] | 4.61 [4.57, 4.64] |
| Dnpc | 2.59M | 0.191 [0.189, 0.194] | 4.66 [4.60, 4.72] |

Dn − K −0.708 [−0.713, −0.704]; Dnpc − Dn −0.002 [−0.005, +0.001]. No Ol-family or O number (see failures).

### memleak (E = 3; none 146M, CV 1.33 %; no exact count: expected is "~1024 outstanding")

| backend | median ops/s | x us/event | /K | /Ol | last report (allocs) |
|---|---:|---|---|---|---|
| K | 353k | 0.944 [0.936, 0.953] | – | 0.353 [0.349, 0.357] | 1444, 1027, 1027, 1026, 1027 |
| Ol † | 998k | 0.332 [0.330, 0.334] | 2.83 [2.80, 2.87] | – | 1032 ×5, one empty stack |
| Olpc † | 1.02M | 0.325 [0.323, 0.327] | 2.89 [2.85, 2.93] | 1.021 [1.011, 1.031] | 1032 ×5, one empty stack |
| OlL3x730 † | 1.06M | 0.312 [0.308, 0.315] | 3.01 [2.98, 3.05] | 1.064 [1.052, 1.076] | 1032 ×5, one empty stack |
| Dn | 340k | 0.979 [0.964, 0.995] | 0.964 [0.952, 0.977] | 0.341 [0.336, 0.346] | 1026 ×5, the true set |
| Dnpc | 340k | 0.975 [0.962, 0.987] | 0.969 [0.951, 0.987] | 0.342 [0.339, 0.346] | 1026 ×5, the true set |

† Does not unwind (see the memleak section below); not like-for-like, and not reportable as upstream's per-event cost. Dn − K +0.035 [0.022, 0.048]; Dnpc − K +0.030 [0.012, 0.049]; (Dn − OlL3x730 +0.667 [0.652, 0.682], unequal work). O, L3port: no throughput (failures).

### malloc-example (E = 1; none 147M, CV 0.98 %; no run exact)

| backend | median ops/s | x us/event | /K | /Ol |
|---|---:|---|---|---|
| K | 1.37M | 0.726 [0.720, 0.732] | – | 0.412 [0.406, 0.418] |
| O | 3.62M | 0.268 [0.265, 0.271] | 2.67 [2.63, 2.71] | 1.099 [1.090, 1.108] |
| L3port | 3.67M | 0.266 [0.264, 0.269] | 2.68 [2.68, 2.69] | 1.104 [1.089, 1.120] |
| Ol | 3.32M | 0.295 [0.292, 0.298] | 2.43 [2.40, 2.46] | – |
| Olpc | 3.38M | 0.291 [0.287, 0.295] | 2.46 [2.43, 2.50] | 1.014 [0.999, 1.029] |
| OlL3x730 | 3.47M | 0.282 [0.281, 0.283] | 2.54 [2.52, 2.56] | 1.045 [1.031, 1.059] |
| Dn | 1.18M | 0.843 [0.832, 0.854] | 0.862 [0.853, 0.872] | 0.355 [0.352, 0.358] |
| Dnpc | 1.20M | 0.829 [0.823, 0.835] | 0.877 [0.869, 0.885] | 0.361 [0.358, 0.364] |

Dn − Ol +0.548 [0.540, 0.556]; Dn − K +0.117 [0.107, 0.126]. This tool measures the bpf_printk sink, not the probe (Reading 6), and is not used for per-event claims.

## Delivery

- Exact (reported = expected): gethostlatency, bpftrace-gethostlatency and bpftrace-uretprobe-hist on all 8 traced backends (120/120 runs); funclatency on K, Dn, Dnpc (15/15). lost = 0 in all 80 gethostlatency/bpftrace-gethostlatency runs; the other tools have no loss counter.
- memleak: Dn and Dnpc report the true outstanding set in every run (1026 = 1024 × 64 B at main+0xf4, the 8 KiB calloc at main+0x2b4, a 4 KiB stdio buffer at _IO_file_doallocate). K's reports always include an extra 4 KiB `<null sym>` block (its address has the page offset of `_IO_file_doallocate+0x83` in another libc mapping: probably the stdio buffer of memleak's forked child before exec, inferred); the stdio buffer itself is in K's last report only in r1, r2, r4 (1027); r3 is 1026 without it (equal to the truth by coincidence); r0's 1444 (1442 at main+0xf4) is a transient. The per-second reports over-count mid-run on K (up to 3075), on the Ol family (up to 4544) and occasionally on Dn/Dnpc (up to 1924). bpftime runs print 11 reports, and the last is always 1026 (Dn) or 1032 (Ol family); K prints 10, and its last can land on a transient. Cause inferred (hash-map walk under concurrent insert/delete); "report during exit" alone does not fit the mid-run transients.
- malloc-example, reported − expected, where expected = the loop's total_ops (excludes the 1024 setup mallocs):
  - K +1759 to +1846. The larger excess is presumably the pre-exec mallocs of sudo, taskset and env in the same pid (inferred).
  - Ol, Olpc, OlL3x730 +955 to +985; Dn, Dnpc +996 to +1008; O and L3port in the runs without a doubled interval (O r1, r4; L3port r0, r2–r4) +987 to +1007. Dn's/Dnpc's printk line count equals total_ops + 1026 in all 10 runs, so their map count is short by 18–30 (Dn 18–27, Dnpc 19–30); the Ol family's shortfall is presumably 41–71 (assuming the same 1026 baseline). The tool reads, prints and deletes each pid's count every second, which can lose increments; rate alone does not explain the Ol family's larger loss (O/L3port run faster and lose 19–39).
  - O r0, r2, r3 and L3port r1 over-count by +3.62M, +3.62M, +552k and +7.27M. Each excess is one or two intervals counted twice: 7,237,202 and 7,244,526 vs ~3.62M; in O r3 the first partial interval (550,961) is counted again in the next (4,221,390); L3port r1 has two (7,185,526 and 7,262,237). That is 5 doubled intervals in 4 of the 10 O/L3port runs and 0 in the 25 Ol-family/Dn/Dnpc runs. That a delete was lost is inferred (no delete failure is logged).

## Failures and silent invalid rows

- These failures are of libbpf-tools v0.37.0 funclatency, whose program declares a cgroup array (`cgroup_map`, map type 8) and calls bpf_current_task_under_cgroup (helper 37) even without -c. The official example funclatency (example/tracing/funclatency-core, no cgroup map) exists in the harness and was not run (the tool list follows J2).
- funclatency on Ol, Olpc, OlL3x730 (15/15):
  - `[map_init] Unsupported map type: 8` → `map 'cgroup_map': failed to create: -EOPNOTSUPP` → the tool exits.
  - The workload runs untraced (182M ops/s ≈ none), reported 0, workload_rc 0.
  - measure.jsonl has no failure flag for this. These rows are dropped here by reported = 0; a naive table would give x ≈ 0. Same on the Orin.
- funclatency on O, L3port (10/10): map type 8 is only logged. The agent's JIT then fails (`Ext func not found: _bpf_helper_ext_0037`, i.e. bpf_current_task_under_cgroup), falls back to the interpreter, and the traced callloop dies (workload_rc −11). Same as on the Orin (OF:234).
- memleak on O, L3port (10/10):
  - Every malloc return logs `There is no frida hook running` + `Unable to get stack trace`: 5.53–5.65M per run, 736–751 MB of output per run.
  - "Top 0 stacks", reported 0.
  - The workload's ops/total_ops lines are missing, so ops = null (cause not traced). O memleak reporting 0 was also seen on the Orin (OF:234; not in the J2 relay).

## memleak: why the Ol family is 3× faster

- **Verified (raw outputs):** all 15 Ol/Olpc/OlL3x730 runs print `Top 1 stacks` with 0 frame lines in every report, and the last report is 16,863,936 B in 1032 allocations in every run. K, Dn and Dnpc print 3–4 stacks with real frames (see Delivery). stack_errors, stack_lookup_failures and map_errors are 0 on these backends, so the harness cannot see the problem.
- **Verified (source, 70f7bc1f):**
  - `uprobe_listener_on_leave` does not set `current_thread_gum_cpu_context`; only on_enter does (frida_internal_attach_entry.cpp).
  - `generate_stack` then logs "There is no frida hook running" and returns nullptr (frida_uprobe_attach_impl.cpp:291–299).
  - bpf_helper.cpp (~L1305–1316) treats nullptr as an empty stack, which is stored and hashed to one constant id.
  - memleak calls get_stackid in the malloc uretprobe, so every allocation gets the same empty stack and no unwinding happens.
  - O (4708f409) fails the same call loudly (negative return → reported 0). Upstream later turned the error into an empty stack, so the failure became silent.
- **Verified (source, c71a9987):** Tern's on_leave still does not set the thread-local (to save ~5 ns per return), but its generate_stack finds the uretprobe CPU context through the current Gum invocation (`gum_interceptor_get_current_invocation()`, f5bfbd2b; frida_uprobe_attach_impl.cpp ~L446–455), and an empty stack returns −EFAULT as in the kernel. The frame-pointer fast path (37ea8e64) is under `#if defined(__aarch64__)`, so x86 uses `gum_backtracer_make_accurate()`.
- **Inferred:** Dn's +0.647 us/event over Ol (≈ 1.9 us per op) is the x86 accurate (libunwind) unwind per malloc. Not profiled (no perf run). Dn ≈ K (/K 0.964) is a coincidence of different cost structures: K's cost is mostly its uprobe trap (0.902 us/event on uretprobe-hist without a stack; stack capture adds ~0.04), while Tern's ~0.65 us/event unwind cancels its ~4.6× probe advantage.
- **Inferred:** Ol's extra 16,786,112 B (= 16 MiB + 8,896 B) in 6 allocations are consistent with two 8 MiB agent-thread stacks (memleak hooks mmap) plus ~8.9 KB of attach-time allocations; Tern hides these (agent.cpp agent_internal_section, whose comment names memleak and the agent threads' 8 MB stacks). Not itemized: all sit on one empty stack.

## Comparison with the Orin (J2, unpinned; relayed in notes/orin-campaign-20261126.md, CI only where given)

| tool | ratio | x86 E6 | Orin J2 |
|---|---|---|---|
| gethostlatency | Dn / OlL3x730 | 1.011 [1.006, 1.015] | 1.027 [1.016, 1.037] |
| bpftrace-uretprobe-hist | Dn / OlL3x730 | 1.140 [1.124, 1.156] | 1.148 |
| bpftrace-uretprobe-hist | Dnpc / K | 4.38 [4.31, 4.45] | 2.74 |
| funclatency-libbpf | Dn / K | 4.61 [4.57, 4.64] | 2.95 |
| malloc-example | Dn / K | 0.862 [0.853, 0.872] | 0.98 |
| bpftrace-gethostlatency | Dn / OlL3x730 | 0.970 [0.968, 0.972] | 0.856 |
| bpftrace-gethostlatency | Dnpc / K | 1.001 [0.995, 1.006] | 0.943 |
| memleak | Dn / OlL3x730 | 0.320 [0.315, 0.326] † | 0.941 |
| memleak | Dnpc / Olpc | 0.335 [0.331, 0.340] † | 1.076 |
| memleak | Dnpc / K | 0.969 [0.951, 0.987] | 1.88 |

- Same direction on both machines except memleak Dnpc/Olpc and Dnpc/K (unequal work on x86) and bpftrace-gethostlatency Dnpc/K (a tie on x86, 0.943 on the Orin). The funclatency failures (Ol family, O) are the same.
- memleak (both points inferred): the Ol bug is architecture-independent, so Ol's Orin stacks were probably empty too; and on the Orin, Dn has the aarch64 frame-pointer walk. Check Ol's Orin memleak output for frames before quoting either number.
- Among like-for-like tools (excluding memleak's unequal work and malloc-example's sink), bpftrace-gethostlatency is the only one where Dn loses to upstream on both machines.

## Reading

1. **Criterion (eval plan: ≥ 4 tools on x86 with a CI'd per-event cost and exact counts): met for K, Dn and Dnpc, with the J2 tool list.**
   - The four tools are gethostlatency, bpftrace-gethostlatency, bpftrace-uretprobe-hist and funclatency-libbpf: 5/5 exact each, CI half-widths 0.001–0.083 us/event.
   - It holds only with bpftrace-gethostlatency in place of the plan's usdt_minimal (fixed in plan.txt before the run, following J2). With the plan's own list, 3 tools qualify (memleak and malloc are not exact; usdt_minimal was not measured).
   - The 4 tools are 2 probe sites × 2 front-ends (getaddrinfo and callloop:target_func, each traced by a libbpf and a bpftrace program).
   - O, L3port and the Ol family qualify on only 3 (they cannot load libbpf-tools funclatency).
   - Per the plan (L440) the sentence goes: it is now sections/eval.tex:120 ("So far we have measured throughput end to end only for sslsniff in nginx."). Suggested replacement content (ours, not the plan's): exact on 4 tools for kernel and Tern; upstream head cannot load libbpf-tools funclatency and records memleak allocations under one empty stack; official loses memleak entirely.
2. Call-bound tools: Tern's uprobe + uretprobe costs 0.19–0.21 us/event vs the kernel's 0.90 (4.3–4.7× K's ops/s). Against OlL3x730 (upstream head + uid/gid cache, #730 lock removed), Dn is 14 % faster on uretprobe-hist (−0.029 us/event). Per probe pair that is 0.38–0.42 us for Tern vs 1.80 for the kernel. The probe-read checks cost ~0 here because there is no user read (Dnpc − Dn −0.003, Olpc − Ol −0.009).
3. getaddrinfo tools (op ~18 us): every tracer costs 5–12 % of throughput.
   - Dn beats the kernel by 0.73 (libbpf) and 0.31 (bpftrace) us/event.
   - Dnpc ties the kernel on bpftrace-gethostlatency (−0.008 [−0.064, +0.048]); on the Orin Dnpc loses (0.943, no CI).
   - Dn vs upstream is −0.10 us/event on libbpf gethostlatency but +0.30 on bpftrace-gethostlatency. The str() read is byte-identical in both trees. Both tools do tid-keyed hash map work and one output per op; the bpftrace program adds a second output (time()) and more hash operations, and if bpftrace 0.20.2 uses a ring buffer (it links ring_buffer__poll) Tern's ringbuf output path is a candidate. Unverified.
4. The probe-read checks cost +0.25 us/event on libbpf gethostlatency on both bases (Dnpc − Dn +0.249, Olpc − Ol +0.257), i.e. ~0.50 us per checked read, and +0.30 / +0.47 us/event (0.60 / 0.95 per read) on bpftrace-gethostlatency; ~0 on tools without a user read.
5. On libbpf gethostlatency O is 0.62 us/event above L3port (O + f14b7c0d/be93dd87/8ac1ec70), consistent with O's CPU pinning in bpf_perf_event_output (sched_getaffinity + 2 × sched_setaffinity per output, 4708f409 bpf_helper.cpp ~L488–549), which L3port's first patch removes. It is small on bpftrace-gethostlatency (+0.019 [−0.015, +0.053], possibly a ringbuf output) and on uretprobe-hist (+0.006 [0.004, 0.008], no output). Inferred from source, not profiled.
6. malloc-example measures the printk sink, not the probe:
   - Dn/Dnpc emit one spdlog record per call (`[ts][info][pid] bpf_trace_printk: …`, 86 B) to stderr, one write(2) per record, because BPFTIME_TRACE_PRINTK_OUTPUT is unset and the harness sets BPFTIME_LOG_OUTPUT=console (c71a9987 bpf_helper.cpp ~L164–240; bpftime_logger.hpp ~L92–96).
   - O/Ol do a bare vprintf into a 4 KiB stdio buffer (31 B per line; 70f7bc1f bpf_helper.cpp ~L150–160). K's trace_printk goes to the trace ring.
   - Output bytes/s are similar on the bpftime backends (Dn 102, Ol 103, O 112 MB/s); a per-record system call explains Dn's ~0.84 us/event as well (which one dominates is not determined).
   - So Dn's 0.355× Ol and 0.862× K reflect the sink, not the probe. A fair run needs the same sink on every backend.
7. memleak's only like-for-like pair is Dn vs K: +0.035 [0.022, 0.048] us/event, /K 0.964.

## Deviations from the eval plan

- usdt_minimal → bpftrace-gethostlatency. usdt_minimal has no throughput workload (audit.py t_usdt_minimal; it is not in measure.py's BASE), and the tool set is the Jetson's J2 list. "bpftrace hist" = bpftrace-uretprobe-hist, "funclatency" = funclatency-libbpf, "malloc" = malloc-example.
- bpftrace 0.20.2 instead of the plan's 0.23.2, because under bpftime 0.23.2's BEGIN/END probes never fire and printf output never appears (x86-facts §19.16).
- libbpf-tools: bcc v0.37.0 built CO-RE with clang-19 and bpftime's libbpf 1.7.0 (x86-facts §19.16), not the plan's nobtf build from realprog-audit-20261025/src.
- No USDT coverage: with usdt_minimal replaced, all six tools are uprobe/uretprobe (usdt_minimal's victim sleeps 1 s per iteration; a throughput workload would have been needed).
- memleak has no exact-count definition and malloc-example is inexact by construction. For these two the plan's "delivered/expected" is given as described above, not as a ratio.
- Kb not run (as planned). N2 is not part of this session. The session took 73.5 min vs the plan's 81.

## Flags

- Frequency: CPU 2 was busy (≥ 0.9) in 2405 of 4385 1 s samples, at 3499–3568 MHz (median 3520); 2 samples were more than 60 MHz off 3500. CPU 4 (tools) was ≥ 0.5 busy in only 4 samples, so the tool CPU never limited throughput. check-before/after: CPUs 2, 4, 6 at 3518–3530 MHz, PASS.
- Load: 1-min load 0.09 before and 0.17 after the sha check; top-before idle; the only active timer was tailscale-watchdog.
- Order randomization: all 270 `order` fields equal the sha256(session:round:tool) permutation. none's position covers 0–8 across tools (per tool: gethostlatency 2–8, malloc-example 0–7, funclatency 0–7, memleak 1–6, uretprobe-hist 0–7, bpftrace-gethostlatency 1–8).
- n = 5 per cell, one session. The getaddrinfo tools' x CIs are ±0.02–0.08 us/event (0.4 % baseline noise on an ~18 us op); paired-ratio half-widths ±0.1–1.2 %.
- Silent invalid rows that the harness does not flag: funclatency on the Ol family (reported 0, rc 0) and memleak on the Ol family (empty stacks). Suggested audit checks: reported > 0 for funclatency; frame count and bytes for memleak.
- memleak stack_lookup_failures for O/L3port (13,944–20,831) is a sum over all 11 reports: audit.py parses stdout + stderr, the "Top" lines are on stdout and the WARNING lines on stderr, so every WARNING lands after the last "Top ". Per report it is 1,255–3,673.
- PRINTK_FLOOD line counts on O/Ol (harness-side, not used here) undercount, presumably because lines split across read chunks.
- The raw dir is 7.44 GB (6.93 GiB), mostly the 10 O/L3port memleak workload outputs (736–751 MB each); measure.py trims only tool output.
- audit.py was edited after the session (17:41: E6_WL override for N2, default unchanged) and sha256.txt was rewritten. This run's manifest is sha256.txt.E6 (audit.py 76929ab5 = audit.py.pre-n2).
- The Orin numbers are relayed from the Jetson's analysis/J2/: unpinned, with CIs only for gethostlatency Dn/OlL3x730.
- Independent check: 3 analyzers + 1 memleak investigation (own scripts), reconciled; then 3 verifiers (numbers, claims, completeness). All table numbers reproduce; the verifiers' corrections (ranges, run counts, K's memleak set, mechanisms, Orin comparison, plan deviations, last-digit roundings) are applied above.
