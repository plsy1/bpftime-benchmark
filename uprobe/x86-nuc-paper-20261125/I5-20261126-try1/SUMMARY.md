# I5-20261126-try1: x86 18-tool correctness audit (E12 protocol on the x86 NUC), summary

Eval plan I5. Session 21:04:38–21:53:51 (49.2 min; the plan estimated about 61 min), exit 0. The e6 sha256 check passed before and after; the manifest (e6/sha256.txt, rewritten at 21:02) covers builds-i5 and harness/audit.py 9d64c4a6. That audit.py differs from audit.py.pre-i5 only in the E6_BUILDS override.

285 rows = 19 tool entries × 5 backends × 3 runs, none missing (the plan said 270). Backends:
- K: kernel 6.12.113
- O: official 4708f409
- Ol: upstream head 70f7bc1f
- Dn: Tern c71a9987
- Dnpc: Tern with probe-read checks

builds-i5 = the frozen builds plus the syscall-trace transformer and the bpftime CLI for Ol, Dn and Dnpc (O is a symlink to builds/official). Placement: 4 s per run (audit.py default, as in E12); tools pinned to CPU 4, workloads to CPU 2.

Criterion (E12, PLAN:446-449): a cell is exact when reported + lost == expected in that run. lost exists only for gethostlatency, sslsniff, bpftrace-gethostlatency and opensnoop, and it is 0 in all 60 rows that have it (absent in the other 225). memleak and get_stack_id have no integer expected value.

Counts are over the Orin's 18 tools. The Jetson audit.py also runs syscount and excludes it from the 18; the 19-entry count is given in brackets.

Analysis: 2 independent analyzers (scratchpad/i5/a1.py, a1t.py, an2.py), reconciled, plus a third recomputation (scratchpad/i5/rec.py). Every count, delta and range below reproduces from the jsonl.

## Exact runs per tool (of 3; Δ = reported + lost − expected, run1/run2/run3)

Exact 3/3 on all 5 backends (10 tools): funclatency example, gethostlatency, bashreadline, sslsniff example, bashreadline example, and the bpftrace scripts uprobe-count, uretprobe-hist, ustack, bashreadline and gethostlatency.

The other 9:

| tool | K | O | Ol | Dn | Dnpc |
|---|---|---|---|---|---|
| funclatency-libbpf | 3/3 | n/a: workload rc −11 ×3 | 0/3: rep 0 of 728.3M/726.6M/724.5M (untraced, rc 0) | 3/3 | 3/3 |
| memleak (no exact value) | 1027/1796/1027 | 0 ×3 (2.24–2.25M stack errors) | 1032 ×3 (one empty stack) | 1026 ×3 | 1026 ×3 |
| malloc example | +1851/+1846/+1800 | +1016/+1022/+1018 | +995/+1009/+1004 | +1017/+1017/+1010 | +1017/+1012/+1013 |
| usdt_minimal | 3/3 (8/8) | 0/3 (7/8 ×3) | 0/3 (7/8 ×3) | 3/3 | 3/3 |
| get_stack_id (no exact value) | 451, rc 124 | 1272/1220/1394, rc −11 | 463, rc 124 | 457, rc 124 | 457, rc 124 |
| bpftrace-usdt-jvm-gc | 3/3 (50) | 0 of 50 ×3 | 0 of 50 ×3 | 0 of 50 ×3 | 0 of 50 ×3 |
| javagc | 3/3 (50) | n/a: JVM rc −6 ×3 | same | same | same |
| opensnoop | −808/−558/−951 | 0 of 10,300 ×3 | 0 of 38,800 ×3 | −201/−614/−613 | −269/−32/−244 |
| syscount (not in the 18) | +643/+446/+455 (counts all processes, by design) | n/a: rc −11 ×3 | n/a: tool does not load | 3/3 | 3/3 |

Notes on the table:
- rc 124 is the normal end of `timeout`.
- O syscount's expected value is 6,300, because the workload slows down before it dies.
- Opensnoop loss in percent:
  - K 2.08/1.44/2.45 %
  - Dn 0.52/1.59/1.58 %
  - Dnpc 0.70/0.08/0.64 %
- Totals over 3 runs: K 2,317, Dn 1,428, Dnpc 545.

## Per-backend summary (over 18 tools; [19])

| backend | exact 3/3 | 2/3 | never exact | no exact value / no count |
|---|---|---|---|---|
| K | 14 [14] | 0 | malloc, opensnoop [+syscount] | memleak, get_stack_id |
| Dn | 12 [13] | 0 | malloc, bt-usdt-jvm-gc, opensnoop | memleak, get_stack_id, javagc |
| Dnpc | 12 [13] (same set as Dn) | 0 | same | same |
| O | 10 [10] | 0 | malloc, usdt_minimal, bt-usdt-jvm-gc, opensnoop | funclatency-libbpf, memleak, get_stack_id, javagc [+syscount] |
| Ol | 10 [10] | 0 | funclatency-libbpf, malloc, usdt_minimal, bt-usdt-jvm-gc, opensnoop | memleak, get_stack_id, javagc [+syscount] |

The 3/3 sets:
- Dn/Dnpc: both funclatency builds, gethostlatency, bashreadline, usdt_minimal, sslsniff example, bashreadline example, and the 5 bpftrace scripts other than usdt-jvm-gc.
- O/Ol: funclatency example, gethostlatency, bashreadline, sslsniff, bashreadline example, and the same 5 bpftrace scripts.
- K: the Dn set plus bpftrace-usdt-jvm-gc and javagc.

Every cell has the same verdict in all 3 runs. There is no 2/3 case on x86.

## Why each inexact cell is inexact

Raw directory R = raw/I5-20261126-try1; run1 files unless noted. The same pattern holds in all 3 runs.

**funclatency-libbpf**
- O (verified): the tool logs `[map_init] Unsupported map type: 8` (tool.txt:11). The workload then fails the JIT with `Ext func not found: _bpf_helper_ext_0037` (bpf_current_task_under_cgroup), falls back to the interpreter, and callloop dies with rc −11 (workload.txt:9-10). This is loud.
- Ol (verified): the tool fails with `map 'cgroup_map': failed to create: -EOPNOTSUPP … failed to load BPF object` (tool.txt:11-15). The workload runs untraced at 182M ops/s with rc 0 (workload.txt:1-3).
- The failure is loud at the tool, but the harness raises no flag.
- Same as in E6; on the Orin the same for O (Ol has no Orin run).

**memleak** (verified)
- Dn/Dnpc report the true set every time: 1024 × 64 B at main+0xf4, the 8 KiB calloc at main+0x2b4, and the 4 KiB stdio buffer at _IO_file_doallocate. Total 1026 (memleak.Dn.workload.txt, last report).
- K r1 and r3 report 1027: the true set plus one 4 KiB `<null sym>` block. This is the stray block also seen in E6, probably the stdio buffer of memleak's forked child before exec (inferred).
- K r2 reports 1796 (1792 at main+0xf4, 2 at main+0x2b4, plus the two 4 KiB blocks; run2/memleak.kernel.workload.txt:26-34).
  - All three K final reports come just before "reaped child process", so "report taken at exit" does not single out r2.
  - It is a transient like E6's r0 (1444). The cause is inferred as in E6: a hash-map walk under concurrent insert/delete.
- O: "Top 0 stacks" and 2.24–2.25M "Unable to get stack trace" lines. Loud.
- Ol: one stack with no frame line, "16863936 bytes in 1032 allocations" (workload.txt:13-14). Every allocation sits under one empty stack, and about 16 MiB of agent allocations are counted as leaks. Silently wrong; mechanism verified in E6 SUMMARY.

**malloc example**
- The tool sums the busiest pid's per-second counts.
- bpftime backends: the excess of +995 to +1,022 over total_ops is mallocloop's own mallocs outside the timed loop (Orin R2 explanation). Example: Dn r1 sums to 4,698,017 against total_ops 4,697,000 (malloc-example.Dn.workload.txt:1528).
- K: +1,800 to +1,851. Measured with gdb outside sudo under the box's LANG=en_US.UTF-8: `taskset -c 2 true` makes 419 libc mallocs and `env true` 416 (setlocale; 9 with LANG=C), so 1,025 setup mallocs + ~835 ≈ +1,860, i.e. the harness's taskset and env pre-exec mallocs in the same pid, which only the kernel traces (sudo's own looked negligible; whether sudo keeps LANG not checked). The Orin ran env but no taskset, and its env apparently made few mallocs (locale; unknown).
  - Inferred, not tested: the extra ~800 are the pre-exec mallocs of the sudo child, taskset and env in the same pid, which only the kernel traces. x86 adds a taskset exec; the Orin ran without one (K +1,021–1,041).
- O shows no doubled interval here; on the Orin it over-counted 1.30–1.75× in 2 of 3 runs.

**usdt_minimal, O and Ol: 7/8 in every run**
- Verified from the raw output: in the last iteration the victim's own line is present, but the second probe's printk line is missing (O: workload.txt:10-11, "bpf: 971569 + 964336" then the victim line, then nothing). Dn has the third line (Dn: :10-12).
- Verified from source:
  - With the harness's BPFTIME_TRACE_PRINTK_OUTPUT=stdout, Tern sends each record with a write(2) loop. The code comment reads "nothing … to lose when the host is killed" (dnext-c71a9987 runtime/src/bpf_helper.cpp ~L218-229).
  - Official and upstream head use a bare vprintf into stdio (official-4708f409 bpf_helper.cpp:86; olatest-70f7bc1f :157).
- Inferred: the last printk sits in the stdio buffer after the victim's final endl, and `timeout`'s SIGTERM discards it. This is a lost output line, not a missed probe. Same count as Orin R8; the cause is not recorded there.

**get_stack_id** (no exact value)
- rc 124 is the normal `timeout` end. O crashes with rc −11, as on the Orin.
- Stack failures: K 168/120/37 (stack map full); Ol 4–5; Dn 2/26/32; Dnpc 1–2.
- O reports 1220–1394 victim malloc events, all with stacks, against 451–463 reported (283–459 with stacks) elsewhere, 2.7–3.1×. Not explained. (`reported` = all of the victim pid's malloc events, audit.py:287; with_stack is a separate field.)

**bpftrace-usdt-jvm-gc, every bpftime build: 0 of 50, in all 3 runs** (verified)
- The traced JVM logs `gum_interceptor_attach failed … GUM_ATTACH_WRONG_SIGNATURE, module=…/libjvm.so` and `Unable to instantiate handlers` (2 lines in each of the 12 bpftime workload files; Dn workload.txt:13-14; O/Ol :10-11).
- The JVM finishes normally (gc_collections 50).
- The tool prints "Attaching 1 probe..." and "@c: 0" (tool.txt:11,15) with no error. To the tool's user this is a silent zero.
- The gc__begin probe (0x890414) sits 2 bytes before the end of the 6-byte function VM_GC_Operation::notify_gc_begin (endbr64; nop; ret) in Debian OpenJDK 21's libjvm.so (objdump); Frida's inline hook refuses it (GUM_ATTACH_WRONG_SIGNATURE, verified; that the length is the reason is inferred).
- This is a problem common to all four bpftime builds, not specific to Tern.

**javagc, every bpftime build** (verified)
- After `bpftime attach`, the same Frida error appears, then "Destructor: bpf_attach_ctx" (javagc.Dn.workload.txt:18-20).
- The JVM then dies with a fatal SIGSEGV in the injected thread (:24). In hs_err_pid2944176.log:26-39, si_addr == pc == RIP 0x7fbb7e7a6401 with SEGV_MAPERR: the thread jumped into unmapped code.
- Inferred: the agent was unloaded after the failed attach while its thread was still running.
- JVM rc −6. `bpftime attach` returns rc 0 every time (:40). There are 12 hs_err files (4 builds × 3 runs).
- The crash is loud, but the attach CLI reports success.

**opensnoop**
- K/Dn/Dnpc (verified): the tool output has only correct openloop lines (/etc/hostname; K also shows openloop's own loader opens of ld.so.cache and libc.so.6). There are 0 error/warn/lost/fail matches in all 18 tool and workload files. The kernel's output also has 466–893 lines from other processes, because it traces system-wide; the harness filters them.
- Mechanism (bcc v0.37.0 source; causation inferred):
  - RINGBUF_SIZE is 256 KiB (compat.bpf.h:11).
  - Each event carries a 255×32-byte path (path_helpers.h:15), so a record is about 8.2 KB and the ring holds about 31 events. That is about 3 ms of consumer stall at 10k opens/s.
  - When reservation fails, the event is dropped with no counter (opensnoop.bpf.c:130-132, `if (!eventp) goto cleanup;`).
  - "Lost N events" (opensnoop.c:297-299) exists only on the perf-buffer path.
  - The BPF side picks the ring buffer through bpf_core_type_exists(struct bpf_ringbuf) (compat.bpf.h:29, 37; the user side also needs probe_ringbuf() to succeed, compat.c:38, trace_helpers.c:1253-1263, else events becomes a perf array). libbpf resolves that in the tool against the host's vmlinux BTF, so bpftime runs also reserve from the ring. This is inferred from source, not traced; consistent with it, no lost line appears.
- Consistent with a tool-side ring-buffer drop shared by the kernel and Tern (inferred; eval plan J1 gates this wording on perf counts and on the gap disappearing with a pinned consumer, neither recorded here: the consumer already had its own CPU 4 and K still lost 1.4–2.5 %). A second silent drop path exists (opensnoop.bpf.c:123-125, missed entry), so a runtime fault is not excluded.
- Which backend loses more varies by run: Dn r2 loses 614 against K's 558. Over 3 runs, K loses 2,317, Dn 1,428 and Dnpc 545. Why is not determined: there are no timestamps, because the tool runs without -T.
- O (verified):
  - The JIT cannot resolve `_bpf_helper_ext_2001000001` and falls back to the interpreter.
  - The workload log then repeats "Unable to compile eBPF program / ebpf_exec returned error: -1".
  - openloop slows to about 2.5k ops/s (median 2,514–2,536; expected 10,300).
  - The tool prints only its header: 0 delivered, silent at the tool.
- Ol (verified): "Program tracepoint__sys calls helper 2001000001 which is not registered … Unable to instantiate bpf prog handler 11/12/13: -22" (workload.txt:9-17). The workload runs untraced; the tool prints its header only.

**syscount**
- K counts every process's openat (39443 at tool.txt:5); over-count by design.
- O: map type 8 is unsupported and helper 37 is missing; the workload dies with rc −11 and the tool prints no table.
- Ol: the tool fails with "map 'cgroup_map': failed to create: -EOPNOTSUPP … failed to load BPF object" (tool.txt:23-26).
- Dn and Dnpc: openat 38800, exact.

## Silent loss (inexact, nothing reported by the tool)

| backend | silent at the tool | loud |
|---|---|---|
| K | opensnoop −558 to −951 (1.4–2.5 %), every run | none (malloc and syscount are over-counts) |
| Dn | opensnoop −201/−614/−613; bt-usdt-jvm-gc 0/50 (error only in the traced JVM's log) | javagc (JVM crash; attach rc 0) |
| Dnpc | opensnoop −269/−32/−244; bt-usdt-jvm-gc 0/50 | javagc |
| O | opensnoop 0 of 10,300; bt-usdt-jvm-gc 0/50; usdt_minimal 7/8 (output-sink artifact, inferred) | funclatency-libbpf, get_stack_id, syscount (rc −11); memleak (2.2M stack errors); javagc |
| Ol | opensnoop 0 of 38,800; bt-usdt-jvm-gc 0/50; usdt_minimal 7/8; memleak (one empty stack, agent memory counted) | funclatency-libbpf and syscount (tool fails to load; workload runs untraced with rc 0); javagc |

No cell reports any loss: lost = 0 in every row.

## Comparison with Orin E12 (orin-facts §13 R1-R10)

| tool | Orin | x86 | |
|---|---|---|---|
| funclatency-libbpf | D 3/3; O SIGSEGV | Dn/Dnpc 3/3; O rc −11 | same |
| gethostlatency | D 2/3 and K 2/3 (−1 once each); O 1/3 (other runs −0.15 to −0.20 %, silent) | 3/3 on every backend | x86 better |
| memleak | D 1026; K 1026–1027; O 0 | Dn 1026; K 1027/1796/1027; O 0 | same (K r2 transient) |
| malloc example | D +1,017–1,019 ≈ K +1,021–1,041; O 1.30–1.75× in 2/3 runs | Dn +1,010–1,017 ≈ O/Ol +995–1,022; K +1,800–1,851; O steady | "equal to the kernel's" does not hold on x86 |
| usdt_minimal | D 8/8; O 7/8 | Dn 8/8; O and Ol 7/8 | same |
| get_stack_id | no exact value; O SIGSEGV | same; O rc −11 | same |
| bashreadline ×2, sslsniff, funclatency example, 5 bpftrace scripts | 3/3 | 3/3 | same |
| bpftrace-usdt-jvm-gc, javagc | D and O exact | 0/50 and JVM crash on every bpftime build (Frida) | x86 worse (all bpftime builds) |
| opensnoop | D 2/3 (−54 in r3); K −19/−47/−58; O unsupported (rc 127) | Dn 0/3, Dnpc 0/3 (−32 to −614); K −558 to −951; O runs but delivers 0 | reproduced in kind, larger on x86 |

Totals over the 18:
- Tern: Orin 13 → x86 12. Gains gethostlatency, loses the 2 JVM tools.
- O: Orin 11 → x86 10. Same gain and losses.
- Common 3/3 sets on both machines: Tern 11 (both funclatency builds, usdt_minimal, bashreadline ×2, sslsniff, 5 bpftrace scripts); O 9. The Orin K total of 13 is derived (§13 gives no K count); x86 K is 14. (The Orin K total of 13 assumes K exact on every tool §13 does not mention; §13 states nothing about K there.)

## Paper-ready wording

x86 analogue of R9:

"On x86, \sys is exact in every run for 12 of the 18 tools (and for syscount). On the malloc example it is off by the same 1,025 setup mallocs (less 3–30 counts lost in the tool's per-second read-and-delete, inferred) as official and upstream. opensnoop is never exact: it loses 201–614 events per run unreported (32–269 with probe-read checks), and the kernel loses 558–951 per run, also unreported. Both JVM USDT tools fail under every bpftime build, because Frida refuses the gc__begin probe, which sits 2 bytes before the end of a 6-byte function in OpenJDK 21's libjvm.so. Official is exact for 10, upstream head for 10, the kernel for 14."

Two-platform form (replaces the Orin-only sentence):

"On both machines, \sys is exact in every run on the same 11 of 18 tools, plus gethostlatency on x86 and the two JVM tools on Arm (13 of 18 on Arm, 12 on x86). Official is exact for 11 and 10, and cannot run libbpf-tools funclatency on either. opensnoop silently drops events under both \sys and the kernel on both machines (Arm: \sys 54 in one of 3 runs, kernel 19–58 per run; x86: \sys 201–614 (checked 32–269), kernel 558–951 per run). This is consistent with the tool reserving an 8 KB record from a 256 KiB ring buffer and dropping it uncounted when the ring is full (inferred; J1 pending)."

**Can "Orin-only" be dropped?** Yes, with the two-platform sentence; not with R9's numbers verbatim. Four parts of R9 change on x86:
1. 13 becomes 12.
2. "Equal to the kernel's for one more" (malloc) fails: K's offset is about 800 larger, probably the harness's taskset/sudo pre-exec mallocs (inferred). On x86, \sys instead equals official and upstream.
3. "opensnoop and gethostlatency exact in 2 of 3 runs" becomes gethostlatency 3/3 and opensnoop 0/3.
4. The kernel's 19–58 becomes 558–951.

The x86 JVM failure has to be stated as a Frida attach limit common to all bpftime builds.

**opensnoop, "复现" (reproduce):** reproduced in kind. On both machines, Tern and the kernel lose events silently, through the same tool-side ring-buffer drop (inferred). The loss is larger on x86. "Tern loses less than the kernel" holds only in aggregate (Dn r2 614 > K 558), so it should not be stated per run.

## Disagreements between the analyzers (resolved)

1. A1 counted over 19 entries (13/19), A2 over 18 (12/18). The numbers agree; both are given.
2. K memleak:
   - A1's account ("extra = stdio buffer plus `<null sym>`") is corrected: the true 1026 already includes the stdio buffer, and K's extra block is the `<null sym>` one.
   - A1 and A2 both called K r2 a "report at exit". All three K final reports come just before reaping, so that does not explain r2; it is a transient, cause inferred as in E6.
3. O opensnoop rate: about 2.5k ops/s (A2), not 2.6k (A1).
4. "Dn/Dnpc lose fewer events than the kernel" (A2; also A2's "kernel shares at a larger magnitude"): false for Dn r2.
5. Ring buffer on bpftime: A2 called it assumed. The source shows the BPF side selects the ring buffer from host BTF. Still inferred, but no longer an assumption.
6. usdt_minimal sink: both analyzers inferred it. The source is now verified (Tern write(2) per record; official vprintf). That it causes the loss is inferred.
7. hs_err line range: 26-39 (siginfo at 32, RIP at 39).

## Deviations from the eval plan and from Orin E12

- 19 entries instead of 18: syscount was run (it is also in the Jetson audit.py) and is reported in brackets. The plan's 270 rows became 285. Both funclatency variants count among the 18, as on the Orin.
- Builds: x86 e6/builds-i5 = the frozen builds plus the transformer and bpftime CLI built from the same build directories (Ol, Dn, Dnpc), needed for opensnoop, syscount and javagc.
- Extra backends: Ol and Dnpc have no Orin counterpart.
- Pinning: tools on CPU 4, workloads on CPU 2 (the Orin was unpinned). This adds a taskset exec; with env, ≈ 835 setlocale mallocs explain K's larger malloc offset (gdb count above).
- 4 s per run (audit.py default, = E12).
- Tool and runtime versions: libbpf-tools v0.37.0 CO-RE (clang-19, libbpf 1.7.0); bpftrace 0.20.2 (0.23.2 does not work under bpftime, x86-facts §19.16); Debian OpenJDK 21 amd64.
- The plan said "有 Java，所以 JVM 工具也能做" (we have Java, so the JVM tools can run too): Java is present, but every bpftime build fails to attach to it.

## Flags

- Frequency: CPU 2 was busy (≥ 0.9) in 427 of 2,929 1 s samples, at 3,517–3,528 MHz (median 3,520), with none more than 60 MHz off 3,500. check-before/after PASS (CPUs 2, 4, 6 at 3,518–3,530 MHz).
- Load: 0.09 before, 0.23 after the sha check. The only active timer was tailscale-watchdog.
- Silent invalid rows the harness does not flag: Ol funclatency (reported 0, rc 0); O and Ol opensnoop (reported 0, rc 0, tool silent); Ol syscount (reported None, rc 0); Ol memleak (empty stack); all bpftime bt-usdt-jvm-gc (0, rc 0); javagc attach rc 0.
- bpftrace-ustack counts are exact, but on every backend, the kernel included, the stack is a single unsymbolized frame (main_in_stack false): K shows the probed IP (target_func+0), bpftime builds the return address in main (main+0x8c). It verifies counts only.
- get_stack_id O: 1220–1394 victim mallocs with stacks, against about 457 elsewhere. Not explained.
- The bashreadline `driver` field for Ol/Dn/Dnpc captured an agent log line instead of "sent 200 lines". Cosmetic.
- n = 3 runs in one session. Ol syscount r1's expected value is 39,000 (n/a anyway).
- Source paths cited:
  - /home/y/src/bpftime-paper/dnew-c71a9987/runtime/src/bpf_helper.cpp
  - /home/y/src/bpftime-paper/official-4708f409/runtime/src/bpf_helper.cpp
  - /home/y/src/bpftime-x86-runs/paper-20261124/e6/bcc-v0.37.0/libbpf-tools/{compat.bpf.h,path_helpers.h,opensnoop.bpf.c,opensnoop.c}
- Recompute script: /tmp/claude-1000/-home-y-src/8ed65bbb-2405-45b6-a329-69008d14bf3d/scratchpad/i5/rec.py
- Independent check: 2 analyzers (own scripts) reconciled, plus 2 verifiers (numbers; claims, incl. a gdb malloc count and objdump of libjvm.so); corrections applied above.
