# J0 (ATC week 1): Jetson status and read-only checks, 2026-11-25

No measurement. Data from existing sessions only.

## (a) E14 and E3pc2 after chain h (11-24 09:24:30)

Both finished with exit 0:
- J-E14-20261124: 09:24:31 -> 10:13:52, 160 measure rows.
- J-E3pc2-20261124: 10:15:52 -> 12:42:32, bookend 2, run3 Dpc, run4 Opc, bookend 4.

Analysed in `analysis/E14-E3pc.md` and pushed in results 1d7afb66.

## (c) All-core CPU per event (E16, E18; stock legs; paired GMR, 95 % t-CI)

Script: `scripts/cpuevent.py`. Full output: `c-cpuevent-E16.txt`, `c-cpuevent-E18.txt`, `c-percore-E18.txt`.

Method:
- CPU-s = sum of per-core busy (user + system + irq + softirq from /proc/stat) x wall_s.
- events = 2 x req_nginx.
- Two core sets:
  - "all" = cores 0, 1, 3, 4, 5. Core 2 (harness) is not recorded.
  - "srv" = cores 3, 4, 5 (sslsniff + nginx, no wrk).
- The unhooked E18 row is excluded, as in E18.md.

### E18, all cores

| ratio | GMR [95 % CI] | diff us/event [95 % CI] |
|---|---|---|
| K_wb32 / K | 0.8608 [0.8504, 0.8712] | -8.70 [-9.46, -7.95] |
| O-stock / K | 1.0227 [1.0091, 1.0365] | +1.41 |
| Ol-stock / K | 0.8912 [0.8807, 0.9018] | -6.80 |
| D-stock / K | 0.8015 [0.7937, 0.8093] | -12.40 |
| Dn-stock / K | 0.8759 [0.8639, 0.8881] | -7.74 |
| Dpc-stock / K | 0.8848 [0.8652, 0.9049] | -7.16 |
| Dnpc-stock / K | 0.8963 [0.8846, 0.9081] | -6.45 |
| Dxn-stock+EXACT / K (n = 19) | 1.0083 [0.9915, 1.0254] | +0.56 |
| Ol-stock / K_wb32 | 1.0353 [1.0269, 1.0439] | +1.90 |
| Olpc-stock / K_wb32 | 1.1187 [1.1087, 1.1287] | +6.38 |
| D-stock / K_wb32 | 0.9311 [0.9236, 0.9387] | -3.70 |
| Dn-stock / K_wb32 | 1.0176 [1.0045, 1.0308] | +0.96 |
| Dpc-stock / K_wb32 | 1.0279 [1.0098, 1.0464] | +1.54 |
| Dnpc-stock / K_wb32 | 1.0413 [1.0211, 1.0618] | +2.25 |
| Dxn-stock+EXACT / K_wb32 | 1.1729 [1.1459, 1.2006] | +9.34 |
| Dn-stock / D-stock | 1.0929 [1.0800, 1.1059] | +4.66 [+4.03, +5.29] |
| Dnpc-stock / Dpc-stock | 1.0130 [0.9853, 1.0414] | +0.72 |
| Ol-stock / O-stock | 0.8714 [0.8609, 0.8821] | -8.21 |
| Dxn-stock+EXACT / Dxn-stock | 1.1269 [1.0937, 1.1612] | +7.11 [+5.25, +8.96] |

### E16, all cores

| ratio | GMR [95 % CI] |
|---|---|
| Dpc-stock / K_wb32 | 1.0181 [1.0085, 1.0278] |
| Dx-stock / K_wb32 | 1.0338 [1.0205, 1.0473] |
| D-stock / K_wb32 | 0.9338 [0.9238, 0.9439] |
| Dx-stock+EXACT / K_wb32 | 1.1960 [1.1840, 1.2081] |
| Dx-stock+EXACT / Dx-stock | 1.1569 [1.1402, 1.1738]; +8.75 us/event |

### Unexplained: Dn uses more CPU per event than D

Dn uses 9 % more all-core CPU per event than D (+4.7 us/event) on both stock and counting legs, while its throughput is only 0-2 % lower.

- Almost all of the extra CPU is on core 3, where sslsniff runs: median 6.0 us/event for Dn-stock against 1.4 for D-stock, consistent over all 20 reps.
- The sslsniff process's own CPU is about the same (2.73 vs 2.54 us/event). So the extra core-3 time is not the consumer process's user/system time.
- Dnpc (the same new base with checks) does not show it (1.5 us/event).

The cause is not investigated. Candidates are the new base's consumer wakeup or nap path, or irq/softirq on core 3.

## (d) E16 exactness cost 6.27 us and the 8.9-10.8 %

- Dx-stock+EXACT - Dx-stock = 6.27 us/event [5.71, 6.84] (n = 20, paired, 2 events per request). Counting legs: 6.10 [5.47, 6.74].
- 8.9-10.8 % is the CI of the deficit of Dx-stock+EXACT against K, so it checks:
  - Dx-stock+EXACT / K = 0.9016 [0.8925, 0.9109], a deficit of 9.8 % [8.9, 10.75].
  - Counting legs: 0.9059 [0.8952, 0.9167].
- EXACT / switch off (throughput) = 0.8516 [0.8393, 0.8640].

## (f) Build time

A full bpftime build (cmake build of bpftime-agent, syscall-server and tools, -j4, LTO, including the LTO links) takes 4-5 min on Orin. Log start to log end:

| build | time | date |
|---|---|---|
| Ol | 4 m 20 s | 11-24 |
| Olpc | 4 m 15 s | 11-24 |
| Dn | 4 m 43 s | 11-24 |
| Dxn | 3 m 58 s | 11-24 |
| Dnpc | 4 m 43 s | 11-24 |
| Dx | 4 m 03 s | 11-24 |

Builds ran one at a time with about 5 GB free.

## (g) Kernel facilities (read-only)

| check | result |
|---|---|
| /sys/kernel/btf/vmlinux | present (8.2 MB), CONFIG_DEBUG_INFO_BTF=y |
| commit_creds in available_filter_functions | yes |
| fexit | attaches: bpftrace 0.20.2 `kretfunc:vmlinux:commit_creds` loads and runs (args `struct cred *new`, retval) |
| config | CONFIG_DYNAMIC_FTRACE_WITH_DIRECT_CALLS=y, WITH_ARGS=y, BPF_JIT=y, FRAME_POINTER=y |

Running kernel: 6.8.12-1021-tegra-btf, our own BTF build, not from a deb. No linux-image-*tegra* package is installed. apt candidates:
- meta `linux-image-nvidia-tegra` 6.8.0-1039.40;
- versioned images up to 6.8.0-1023 (-rt up to 1032).

## (h) Why the kernel get_stack_id example has only stack failures

This is not missing frame pointers. The failures are -EEXIST (-17) from the stack map.

- The kernel uprobe on libc malloc/free is system-wide. In E12 run 3 the tool saw 150,197 events from many processes. The top one is udev-worker 1120353 with 61,128; the victim had 75.
- The stack map has max_entries 1000, and the program uses no BPF_F_REUSE_STACKID. Other processes' stacks fill the buckets within the first ~2,000 events. From then on, a new stack that hashes to a taken bucket gets -EEXIST.
- About 46,000 events of other processes did get valid user stacks ("got err=0" + call stack), so user unwinding works on this kernel.
- All 75 victim events got stack_id=-17.

bpftime traces only processes that load the agent, so its map holds only the victim's stacks.

For a fair comparison, the example would need a pid filter, a larger map or BPF_F_REUSE_STACKID. That is a change to the tool, not to bpftime; it was not done.

Side note: udev-worker 1120353 has been alive for 2 days and uses about 0-1 % CPU (5 min CPU in total). It is background noise during measurement.

## (i) Java and CPU layout

- Java: OpenJDK 17.0.20.1 (Ubuntu 24.04 package), /usr/bin/java.
- CPU: 6 x Cortex-A78AE, max 1728 MHz, one frequency domain. lscpu shows 1 cluster of 6 cores.
- topology/cluster_id: cpu0-3 = 0, cpu4-5 = 1. These are the two L2/CPU complexes of Orin Nano.
- Policy M uses cores as follows:
  - wrk on 0-1;
  - harness on 2;
  - sslsniff on 3;
  - nginx on 4-5, which is the other cluster.

## Dn and BPFTIME_CONSUMER_NAP_US

- Dn (c71a9987) reads BPFTIME_CONSUMER_NAP_US, default 100 us (runtime/src/bpftime_shm.cpp:477).
- It also reads BPFTIME_CONSUMER_NAP_WINDOW_US (default 2000) and BPFTIME_CONSUMER_POLL_US (default 0).
- The string is in builds/Dn, Dnpc, Dxn and D libbpftime-syscall-server.so, and not in Ol.

## (e) Perf buffer, shards and drain (sslsniff path; code survey, nothing run)

O = official-4708f409, Ol = official-70f7bc1f, D = dev-9ccda7e8, Dn = dev-c71a9987. Arm64 has 4 KiB pages and 6 CPUs.

| | layout | ring size | overflow | consumer drain (epoll_wait) | producer syscalls per sample | knobs |
|---|---|---|---|---|---|---|
| O | 1 consumer ring per CPU slot (6), each fed by per-(pid, tid) producer shards; the shard list is unbounded | 64 KiB data per ring and per shard | silent drop, no PERF_RECORD_LOST | 100 ms timeout passed through; polls every 1 ms (sigtimedwait); wakeup_events/watermark ignored | ~5 (affinity x3, getpid, gettid) | none |
| Ol | as O | as O | as O | 1 ms wrapper loop: kernel epoll_wait(0), then a 1 ms userspace poll, until 100 ms; wakeup settings ignored | 1 (getpid) | none |
| D | as O, plus one overflow shard per event | as O | -ENOSPC, loss counted, PERF_RECORD_LOST sent | 100 ms passed through; 100 us futex nap for 2 ms after records; idle: futex sleep up to 100 ms; honours watermark (half shard) and wakeup_events per shard | 0 in a steady stream; FUTEX_WAKE only if a consumer is sleeping, or the sample is urgent and a consumer is napping | NAP_US 100, NAP_WINDOW_US 2000, POLL_US 0 |
| Dn | as D | as D | as D | as D, **but behind Ol's 1 ms wrapper**: the idle 100 ms sleep becomes ~1 ms per inner call | as D | as D |
| bpf (sslsniff.c) | 1 ring per CPU (6) | 16 pages = 64 KiB per CPU | drop + PERF_RECORD_LOST | perf_buffer__poll(100 ms) | 0 (irq_work in the kernel) | SSLSNIFF_PB_WAKEUP=N -> wakeup_events N |

Checked by hand:
- Dn `runtime/syscall-server/syscall_context.cpp` handle_epoll_wait (around line 1595): loop of `orig_epoll_wait_fn(epfd, ..., 0)` then `bpftime_epoll_wait(..., min(remaining, 1))`.
- D (line ~1152) calls `bpftime_epoll_wait(epfd, evt, maxevents, timeout)` directly.
- The wrapper came into Dn with the rebase onto 70f7bc1f.

The wrapper is a candidate for Dn's extra CPU on core 3 (section c). It does not explain why Dnpc, which has the same wrapper, shows no excess. Not verified.

Other notes:
- On bpftime, the shard and the consumer ring together buffer 128 KiB per (thread, CPU) path, against 64 KiB per CPU on bpf.
- O and Ol: when shm is full, emplace_back of a new shard has no try/catch, so bad_alloc could reach the traced process. Code only.
- O and Ol drop SSLSNIFF_PB_WAKEUP, so a "Ol + wb32" arm would be identical to Ol.
