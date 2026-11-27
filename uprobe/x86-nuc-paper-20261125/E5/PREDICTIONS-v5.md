# E5 predictions, addendum v5: real applications (E5-real; written before any E5-real build or run)

Written 2026-11-27 ~13:15 JST, before writing or building the E5-real probes, loader, runner or Java program, and before any
run. Does not replace v1–v4. Covers the eval plan's "real applications" part of E5 (eval-plan §E5, W4d: "how common").

## Method (deviation from the plan's pairing, stated up front)

The plan pairs K and Tern records "by tid + time window" on two different functions. That trusts the tid Tern reports,
which is one of the audited fields (a wrong tid could pair a record with the wrong thread and hide the error). E5-real
instead pairs on data that both sides see and that does not come from an identity helper:

- Tern side (each bpftime leg): a uprobe on libc `write` (all processes that load the agent) records
  {ktime, pid_tgid, uid_gid, comm, fd, buf, count}.
- Truth side (every leg, same run): a kernel `tracepoint/syscalls/sys_enter_write` program records the same fields,
  filtered to a dedicated cgroup v2 that holds the workload and all its descendants (the load generator runs outside it).
  At syscall entry the kernel's helpers are the truth; nothing between libc `write` entry and the syscall changes
  identity.
- Pairing: a Tern record (fd, buf, count, t) pairs with the earliest unused truth record with the same (fd, buf, count)
  and t_truth in [t, t + 20 ms] (both clocks CLOCK_MONOTONIC: kernel bpf_ktime_get_ns; bpftime steady_clock). Records
  with ≥ 2 candidates from different threads in the window are counted as ambiguous. Fields compared on pairs: tgid,
  tid, uid, gid, comm. Reported per workload, leg and process name (truth comm): pairs, field mismatches / pairs,
  unpaired Tern records, truth records without a Tern record (coverage), ambiguous.
- Control leg Kc: a kernel uprobe on libc `write` (cgroup-filtered) instead of bpftime: checks the pairing itself.

## Workloads (each in its own cgroup, the agent via LD_PRELOAD on bpftime legs, inherited by children)

- N (nginx de-privileged worker): Debian nginx 1.26.3 (binary extracted from the .deb, no service), master as root,
  `user nobody nogroup;`, 1 worker, TLS on a local port with the harness test.cer/key; 2000 HTTPS requests from curl
  outside the cgroup. Writes: worker SSL_write → write, access log, error log; master start-up writes.
- M (`make -j4`): a clean build of libbpf's src/ (the official tree's third_party/libbpf, copied to a scratch dir,
  static library only). Processes: make, sh (→ dash), cc (→ x86_64-linux-gnu-gcc-14), cc1, as
  (→ x86_64-linux-gnu-as), ar (→ x86_64-linux-gnu-ar).
- J (JVM named threads): OpenJDK 21, a Java program with 4 threads named e5-worker-1..4, each writing 2000 lines with
  unbuffered FileOutputStream; worker 1 renames itself to e5-renamed after 1000 lines (Thread.setName on the current
  thread); the main thread writes 100 lines before and after.

## Legs

Kc, O, Ol, Dn, Dnpc, Dxn+BPFTIME_DIAG_EXACT=1, Dnextpc, Dnextpc+BPFTIME_CAREFUL=1; 3 runs each.

## Predictions (fields wrong on paired records; "-" = none)

Basis: E5 v1–v4 results (glibc fork and glibc set*id are right on every build; O/Ol/Dn/Dnpc/Dnextpc take comm from the
basename of readlink(/proc/self/exe), cached process-wide; careful mode reads PR_GET_NAME per thread; exact asks the
kernel).

| Workload | Kc | O, Ol, Dn, Dnpc, Dnextpc (off) | Dxn+EXACT | Dnextpc+CAREFUL |
|---|---|---|---|---|
| N | - | - (fork + glibc setuid/setgid = W1/W7g; comm "nginx" = exe basename) | - | - |
| M | - | comm on records from sh ("dash"), cc ("x86_64-linux-gn"), as ("x86_64-linux-gn"), ar ("x86_64-linux-gn"); not from make or cc1 | - | - |
| J | - | comm on every record from a named Java thread (e5-worker-*, e5-renamed: reported "java"); not on the main thread ("java") | - | - |

- pid/tid and uid/gid: right on every paired record in every workload on every leg (no raw clone/vfork child writes
  before exec, no raw credential syscalls). Uncertain: a posix_spawn/vfork child that writes before exec (e.g. an exec
  error) would be W3-class; none expected in a successful build.
- Kc: 0 mismatches, coverage ≈ 100 %, ambiguous ≈ 0 (pairing works).
- bpftime legs: coverage < 100 % only where a process does not run the agent or writes before the agent attaches
  (start-up); the comparison is on pairs only. Mismatch rate for M and J reported as the share of records (and of
  processes / threads) affected.
- Uncertain: the JVM under the agent (E12/I5: the JVM crashed on a USDT attach in libjvm; a libc uprobe is different).
  A crash or a non-zero exit is a finding for that leg, not a prediction failure.
