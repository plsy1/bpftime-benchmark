# I6-20261126 — memory (x86 NUC), summary (analysis of existing rows, no new machine time)

Source: /proc/<pid>/smaps_rollup recorded by run_paper.py at the end of every measured row (row fields nginx_smaps_kb =
the server: the nginx worker, or the haproxy process; sniff_smaps_kb = the consumer: kernel sslsniff, or the bpftime
tool process = sslsniff under the syscall server). Medians per leg, 16 B; full tables in tables.md (C1, H7a, H7b, DN1,
M4). MB = MiB.

## Per process

| | server Rss | server Pss | server Private_Dirty | consumer Rss | consumer Pss | consumer Private_Dirty |
|---|---:|---:|---:|---:|---:|---:|
| nginx worker, B / K (C1, DN1) | 10.5 | 7.1 | 4.8 | – / 2.6 | – / 1.3 | – / 0.3 |
| nginx worker, Tern (Dn, Dnpc, Dnext±CAREFUL) | 20.1–20.6 | 11.8–12.0 | 4.9 | 45.3–45.8 | 42.1–42.5 | 7.6 |
| nginx worker, upstream (Ol family) | 22.4–22.5 | 13.0 | 5.0 | 27.1–27.5 | 24.1–24.5 | 3.9 |
| nginx worker, Tern + view (M4 Dviewpc+CAREFUL+VIEW) | 36.5 | 19.9 | 4.9 | 45.7 | 42.4 | 7.6 |
| haproxy 1 thread (H7a): B / K / Tern / upstream | 88.2 / 88.2–89.8 / 126.4–127.0 / 139.6–140.4 | 85.4–87.0 / 121.9–122.4 / 135.2–135.9 | 77.4–79.0 / 84.4–84.5 / 86.9–87.0 | | | |
| haproxy 4 threads (H7b): B / K / Tern / upstream | 88.2 / 88.2 / 127.2–127.5 / 140.1–140.2 | 85.3 / 122.3–122.6 / 135.4–135.5 | 77.4 / 84.5–84.6 / 87.1 | | | |

## Reading

- The in-process agent adds ~10 MB Rss to an nginx worker (Tern +9.7, upstream +12.0), almost all shared: Private_Dirty
  grows by 0.1–0.2 MB; Pss +4.8 (Tern) / +5.9 (upstream) MB because the worker shares the agent's pages and the bpftime
  shm with the nginx master. In haproxy (one process, nothing to share with) the agent adds 38–39 MB Rss and ~7 MB
  Private_Dirty (Tern) vs 51–52 MB and ~9.6 MB (upstream).
- Per thread: haproxy at 4 threads vs 1 thread uses +0.3–0.8 MB more under Tern (126.4–127.0 → 127.2–127.5) and
  +0.4 MB under upstream; B and K are unchanged at 88.2 MB (haproxy preallocates). So the agent's per-thread cost is
  ≲ 0.3 MB/thread here (cross-session: H7a and H7b ran hours apart, with different connection counts).
- The consumer: Tern's tool process (sslsniff under the syscall server) is 45.5 MB Rss / 7.6 MB Private_Dirty, upstream's
  27 MB / 3.9 MB, kernel sslsniff 2.6 MB / 0.3 MB. The difference is the syscall server, the JIT/LLVM libraries and the
  shm mapping (Pss ≈ Rss: not shared with another process).
- Careful mode (DN1) changes nothing measurable (±0.2 MB). The P5a view adds 16 MB Rss / 8 MB Pss to the worker: the 2^18
  × 64 B view array (16 MiB) is mapped whole at mmap time (shared with the master); Private_Dirty unchanged. The kernel
  keeps one 16 MiB copy (vmalloc) for the whole machine.
- Not measured: churn 1/8-thread runs (the eval plan's MB/thread source; I2churn rows carry no smaps), kernel-side
  memory of the bpftime shm segment (BPFTIME_SHM_MEMORY_MB=1024 is a reservation; only touched pages count in Rss).
