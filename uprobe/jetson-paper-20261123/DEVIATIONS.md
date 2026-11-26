# Jetson deviations and implementation notes (paper campaign)

- 2026-11-23 PREREGISTRATION.md line 6 names Odiag = 4bef75b7; the frozen Odiag is 016a5151 (4bef75b7 read the switches with getenv at first use, which nginx workers cannot see because nginx clears the environment when forking workers). Same switches, read at library load.
- 2026-11-23 Plan E1 leg `B#M-t3` is written `B#Mt3` (run_paper.py policy names are \w+). Policy Mt3 = nginx {4,5}, sslsniff {3}, wrk {0,1,2} -t3. Agreed with the NUC session.
- 2026-11-23 Policy U ("no pinning") is written as all six CPUs {0-5} for nginx, sslsniff and wrk. An empty list would make run_paper.py skip sched_setaffinity, and the children would inherit the harness mask (docker exec taskset -c 2).
- 2026-11-23 AOT cache (plan 1.5 "clear the cache directory per session"): dev 9ccda7e8 keeps compiled AOT objects in the bpftime shared memory next to the program (syscall server compiles at BPF_PROG_LOAD), not on disk. run_paper.py removes the shm before and after every leg, so every leg starts with an empty cache; nothing to clear per session.
- 2026-11-23 K legs are always one request short: start_nginx's readiness curl is served before the kernel sslsniff attaches to the worker (bpftime agents are preloaded and see it). Audit for K subtracts 1 request (1 read, 261 bytes at 16 B).
- 2026-11-23 Backup of frozen artifacts to the NUC uses tar over ssh (no rsync on the NUC). Backup is never run while an x86 session is measuring.
- 2026-11-23 nginx busy for criterion (e) under M: one worker migrating between cpu4 and cpu5, so busy = sum over {4,5}.
- 2026-11-23 Gate (b) failed on the Jetson (Odiag/O = 0.9864, 90 % CI [0.9731, 1.0000], n = 20; 24 of 30 paired reps negative). Per G0(b) the official "re-entering the kernel" ladder on the Jetson uses real patch builds: L1 = f14b7c0d (no per-event pinning), L2 = be93dd87 (+ uid/gid cache), L3port = 8ac1ec70 (+ pid/tid cache), same options as L3port. Odiag legs stay as supplementary within-binary rows; Odiag − O is reported separately. Additivity check 3 (single knock-outs vs all) is not possible with real patches; on the Jetson only the cumulative order is compared. Agreed with the NUC session (x86 adds the same L1/L2 session for symmetry).
- 2026-11-23 Gate (a) fallback applied per platform and payload (PREREG): on the Jetson 16 B O and D and 256 KB D were not shown equivalent at n = 20, so E4 adds O-stock, D-stock, Opc-stock, Dpc-stock at 16 B and D-stock at 256 KB (throughput from the stock legs, audit from the counting legs); E6 adds O-stock and D-stock at S4 as a check of the S4 total. x86 uses the fallback only at 256 KB.
- 2026-11-23 E2 supplement E2s (B#S, K#S, O#S, Odiag#S, 10 reps): O#S was not in the E2 leg set and gate (b) failed on the Jetson, so G2 cannot be read from Odiag#S.
- 2026-11-23 E3 uses the shared kernel_bookend.py (sha256 63d7542c…) and the x86 runner structure (bookend 0, A, B, bookend 2, B, A, bookend 4) on cpu5.
- 2026-11-23 Freezing made builds/ itself read-only, so the first L1 build could not copy its artifacts; the L1 copy step was completed by hand from the finished build directory (same commands as build-arm.sh), L2 rebuilt with the script.
- 2026-11-24 E6 on the Jetson has 28 legs: the plan's 24 plus L1, L2 (real-patch ladder, main official ladder per gate b) and O-stock, D-stock at S4 (gate a fallback check of the S4 total). Guard minutes 130.
- 2026-11-24 E8 on the Jetson adds O-stock and D-stock at every payload (gate a fallback; throughput from the stock legs, audit from the counting legs).
- 2026-11-24 E3 Opc/Dpc not run in the night chain: each checked read/write case is 100k calls x ~0.9 ms x 10 iterations, so an Opc run exceeds the 3600 s run timeout and the session cannot end before the 06:15 apt-daily window. Run after the night chain once the apt timers were stopped, with a larger run timeout and an honest guard window (run-e3.sh hardcodes 130 min; the O/D session took 162 min).
- 2026-11-24 dpkg-db-backup.timer fires at 00:00, inside E5/E6 of the night chain; it is not in the guard's timer list. Recorded, rows not excluded (objective rule only).
- 2026-11-23 22:59 apt-daily.timer and apt-daily-upgrade.timer stopped (`systemctl stop`, user decision) for the rest of the Jetson campaign so experiments run back to back; restored when the measurements end. E3 Opc/Dpc follows the night chain with run timeout 7200 s and guard window 300 min.
- 2026-11-24 Shared harness v2 from the NUC session (run_paper.py 274419bb, adds --perf-mode/--perf-start/--perf-dur; identical behaviour without --perf-mode; analyze_paper.py 6dcb26dd, analyze_e7.py 8f2f9b0e). Installed after E8; old run_paper.py kept as harness/run_paper.py.v1-478e0ed7. Sessions record the harness sha.
- 2026-11-24 E7 on the Jetson: the host `perf` wrapper fails on the BTF kernel, so session.sh passes PATH with /usr/lib/linux-tools/6.8.0-1020-nvidia-tegra first to the docker exec (PAPER_EXEC_PATH, E7 only); `nsenter -t 1 -m -- perf` resolves it in the host mount namespace. cyc events cycles:u,cycles:k,instructions:u,instructions:k,task-clock (config perf_cyc_events). All 13 events of both modes counted unmultiplexed in a host test. Legs as x86 plus O, L1, L2, L3port at S4.
- 2026-11-24 E3 Opc/Dpc was cut at 05:59 by the outer `timeout 25200` of its chain wrapper (set before the chain waited 4 h for the night chain; my error). bookend 0, run1 Opc (66 min), run2 Dpc (51 min) and bookend 2 completed and validated; run3 Dpc was interrupted (session exit 143, cleanup and end guard ok, no rows from run3). The second half (bookend 2 -> run3 Dpc -> run4 Opc -> bookend 4) runs as session J-E3pc2-20261124 (run-e3.sh PART2=1) after the E10-E14 chain, so the ABBA pairs span two sessions.
- 2026-11-24 Supplementary E7c (Jetson): nginx worker cache/TLB misses per request (L1I, L1D, iTLB, dTLB, L2 refill, instructions; 6 counters, no multiplexing) for B, K/Odiag/D at S1 and S4, Odiag@S4+NO_PIN, 3 reps; follows the NUC observation that on the Orin in-application hook and pin cycles are several times the microbenchmark unit costs. Queued after the cpuset control.
- 2026-11-24 E14 (07:50) and E3pc2 (08:27) refused at start by the guard: man-db.timer due 09:03:44 inside their windows. Failed attempts kept as raw/J-E14-20261124.refused-guard-mandb and raw/J-E3pc2-20261124.refused-guard-mandb (no measurement started). Both rerun by chain g after man-db fired (same session ids). E10 and E11 ran between E13 and E14 as planned.
- 2026-11-24 E12 O opensnoop rc 127: official 4708f409's libbpftime-agent-transformer.so imports `syscall_addr`, which official defines only in x86-64 inline asm (text_segment_transformer.cpp), so the workload fails to load on aarch64 ("undefined symbol: syscall_addr"). Recorded as "not supported on aarch64" (functional absence), same cause as E11 layers-O rc 127. Not a build error of the frozen O.
- 2026-11-24 E13 n4-many kernel M=1000 marked ok=false in both reps: outattach did not exit within the runner's 60 s after detaching 1,000 uprobes (TimeoutExpired); the start-time samples were collected before the teardown and are reported, flagged.
- 2026-11-24 Chain g (09:05) was refused again for E14 and E3pc2: the guard also rejects a timer that fired within the last 20 min (man-db LastTrigger 09:03:53). Attempts kept as *.refused-guard-mandb2 (no measurement started). Chain h restarts both at 09:24:30.
- 2026-11-24 E15 (NUC request: K with batched consumer wakeups). sslsniff-src/sslsniff.c replaced by the NUC patch (sha 702ab070; the pre-patch file kept as sslsniff.c.pre-wb, identical to the NUC's pre-wb). Only sslsniff-s4 rebuilt (scripts/build-sslsniff-s4-wb.sh, new sha 3cceb9c8); old binary in sslsniff-archive/. Without SSLSNIFF_PB_WAKEUP the loader passes zeroed perf_buffer_opts, i.e. libbpf's default (sample_period 1), so sessions before and after use the same behaviour. harness/run_paper.py replaced by the NUC's edeec7df (K legs now get leg env; old file kept as run_paper.py.v2-274419bb). Jetson plan adds O-stock and D-stock (16 B gate (a) fallback). K_wb legs are counting legs (the stock loader has no switch), as on x86.
- 2026-11-24 E16 (NUC request: exact checked runtime). Dx = dev 9ccda7e8 + the NUC's two format-patch commits applied with git am on branch ladder/dexact-9ccda7e8 (local SHAs 845c77c9, 1d8eda4d, tree 55471b35; x86 SHAs 87e0117a, 66ed229e), built with checks ON as Dpc (build-arm.sh Dx … checks). builds/ was made writable for this build and frozen again afterwards. config-jetson.json gains "Dx". Jetson plan adds D-stock, Dpc-stock, Dx-stock, Dx-stock+BPFTIME_DIAG_EXACT=1.

## 2026-11-24 14:45 JST: apt timers restored, Jetson paper queue finished

E15 (J-E15-20261124) and E16 (J-E16-20261124) ended 14:36, all rows valid; analysis/E15-E16.md.
apt-daily.timer and apt-daily-upgrade.timer, stopped by the user on 11-23 22:59, were started again
(`systemctl start`). No further Jetson paper measurements are queued; any Orin O-latest / new-D round
will start with a fresh guard check.

## 2026-11-24 16:10 JST: E18, the new base (added after the plan of record)

User (via the NUC session): run the daily task, move dev to the latest official, then measure again.
- Official upstream master 70f7bc1f (137 commits past 4708f409) is the new base; the daily pushed official-latest
  and the migrated dev `dev/uprobe-official-70f7bc1f3363` = c71a9987 (240 replayed commits + a reconcile commit
  + removal of upstream's #730 per-CPU execution lock and #785/#805 epoll signal taking + test updates).
- New arms, built with scripts/build-arm.sh (same options as O/D/Dx):
  - Ol = 70f7bc1f, checks off;
  - Olpc = 70f7bc1f, checks on (upstream's default);
  - Dn = c71a9987, checks off;
  - Dxn = ladder/dexact-c71a9987 (845c77c9 replayed, 1d8eda4d ported onto the new comm cache), checks on.
- Worktrees: ~/src/bpftime-paper/{official-70f7bc1f,dev-c71a9987,dexact-c71a9987}.
- Build chain: scripts/build-newbase-20261124.sh. builds/ was made writable for it, then refrozen.
- config-jetson.json: Ol, Olpc, Dn and Dxn added.
- Added after the NUC's x86 E18 set: Dnpc = c71a9987 with checks on (scripts/build-dnpc-20261124.sh), and the
  old D and Dpc repeated in the round.
- plans/E18.txt: 2 rounds × 10 reps, 21 legs, 16 B, policy M, with stock legs.
- The daily task ran jetson_clocks again (14:46). Clocks are re-checked before E18.
- The frozen paper arms (O, D, Dx, ...) and their numbers are unchanged; E18 is an addition.
- 16:23 the guard refused E18 (apt-daily.timer due 18:03:59 inside the 120 min window; refused dirs
  `J-E18-20261124.refused-load`, `.refused-guard-apt`). E18 is split by round: J-E18a (round 0, plans/E18a.txt)
  now, J-E18b (round 1, plans/E18b.txt) after the apt run (guard: no timer fired within 20 min). Analysis pools both.

## 2026-11-24 19:20 JST: E18a refused at 16:25, re-run after E18b

E18a (round 0) was refused by the start guard at 16:25 (load 0.42 right after waitload passed);
kept as `raw/J-E18a-20261124.refused-load-1625`. E18b (round 1) ran 18:26-19:15, 210/210 valid.
E18a re-run by `scripts/chain-20261124k.sh` (same plan, retries on guard refusal), so round 1
precedes round 0 in wall time; rounds are pooled as before.

## 2026-11-24 20:20 JST: E18 analysis exclusions and notes

- E18a round 0 rep 3 `Dxn-stock+BPFTIME_DIAG_EXACT=1` excluded from the headline: 19,289 rps (= B),
  sslsniff CPU 0.03 s (other 680 stock legs >= 0.40 s), wall 37.9 s (median 14.2 s). The probe
  was not running; stock legs have no event count, so the harness marked it valid. Rule applied
  after the fact: stock leg with sslsniff CPU < 0.3 s. Scan of all Jetson sessions found no other
  such row. Both n = 19 and n = 20 numbers are in analysis/E18.md.
- A tool call at 19:55:04 JST (progress check requested by the user) ran during E18a round 0
  rep 6. Excluding rep 6-7 changes no E18 ratio by more than 0.6 % and no verdict.
- 2026-11-25 ATC week 1 on Orin (user OK ~14:55; NUC plan PREREG-2 + Orin addendum plans/PREREG-2-orin-addendum.md written before any run).
  - New builds Dpcr (9ccda7e8 checks ON, fresh build dir, options/source/toolchain identical to Dpc; .text same size, ~90 k
    address-operand bytes differ) and DnR (Dn + -falign-functions=64) via scripts/build-arm2.sh (build-arm.sh + BDIR_TAG and
    EXTRA_CMAKE). harness/config-jetson.json gained Dpcr and DnR.
  - J1 (prep/J1-run.sh) was refused 3 times (16:03, 16:08, 16:12; kept as raw/J-J1-20261125.refused-*): load 0.37-0.75 at the
    guard right after waitload passed (< 0.15). Cause: check_env.sh hashed every build (sha256 --check) before reading the
    load, so the guard measured its own hashing (J11 start: 0.04 before, 0.85 after). Fixed 16:16: the load is read before the
    sha256 pass and both values are logged. Likely the same cause as the E18a refusal of 11-24 16:25. J1 then ran 16:17-16:23.
  - One cross-session message was sent while chain a was measuring (about 15:30-15:40, during J4); the user asked for a status.
    Not mapped to a leg; J4 is exploratory.
  - Timers paused 16:15 for the rest of the campaign (user: "两边都把影响测量的定时任务给停掉，全力搞任务", confirmed "停" here):
    disable --now anacron, apt-daily, apt-daily-upgrade, dpkg-db-backup, e2scrub_all, fstrim, fwupd-refresh, man-db,
    motd-news, update-notifier-download, update-notifier-motd; stop systemd-tmpfiles-clean (static). No crontabs exist.
    State before and RESTORE.sh: timers-paused-20261125/. Network/remote-access units untouched.
  - J13 will run its two rounds back to back, not on two nights (user, 11-25): the leg x night interaction test of PREREG-2 §5
    is not possible on Orin; round 0 vs round 1 is still compared.
  - J1 perf stat -a syscalls:sys_exit_openat reads 0 in every kernel run (the bpf program is attached to the same
    tracepoint) and ~41.8 k in the D runs; the kernel's true count comes from run_cnt and openloop's total_ops instead.
- 2026-11-26 chain c (SMOKE-J7, J8, J6 v1, J2, J13; 17:5x 11-25 -> 00:26:38 11-26): every session exit 0 on the first try,
  no guard refusals, no tool calls from this session during the chain.
  - J13 has no OlL3 leg (NUC's leg list), so M6 (Dn730 vs OlL3) exists only in J8 (exploratory). The J13 Holm family is the
    5 tests M1-M5.
  - Dn730 is not lock-equivalent to upstream: its inlined per-CPU array lookup reads sched_getcpu instead of the CPU pinned
    by the #730 guard (NUC E4, 11-25: 1-54 lost updates per run). Every M6 / P4 statement carries this caveat. Not fixed.
  - J2 funclatency-libbpf on Ol, Olpc, OlL3x730: the tool fails to load (map type 8 unsupported), workload runs untraced; rows
    kept as is, not a throughput number.
  - 00:27:05-00:27:12 11-26: this session ran one `ssh nuc` (ls + pgrep) while NUC was measuring I3 round 2 (NUC's message
    had said E3/E4/E5 were done, not that I3/I9 had started). The scp before it failed on a local zsh glob and never
    connected. NUC mapped it to one row (I3 r2 rep 9 K, -0.04 % vs the next lowest), flagged, not rerun. No further NUC
    access until NUC's idle message.
- 2026-11-26 PREREG-2b (x86 harness/PREREG-2b.md, sha256 01cb8b5d..., frozen 2026-11-26T01:50:36 JST, before E4f and before any
  Dn730f sslsniff run). If NUC's E4f gate passes (Dn730f loses 0 updates in all 20 counting runs), Dn730f replaces Dn730:
  M6 = Dn730f vs OlL3, P4 = Dn730f - Dn vs Ol - Ol730x. Otherwise M6 leaves the confirmatory family (Holm over 5 per platform)
  and is reported as exploratory.
  - Dn730f = Dn730 (d7adca41) + 19efeb16 (inlined per-CPU array lookup reads my_sched_getcpu); worktree
    ~/src/bpftime-paper/dn730f-c71a9987 (logs/worktrees-Dn730f-20261126.log), built 02:24-02:29 as builds/Dn730f
    (scripts/chain-20261126d.sh after J6 v2), config-jetson.json gained Dn730f.
  - Orin J13s (M6 supplement): plans/J13s.txt, sha256 c6bd6e31a95c0e1342152ae7e535c8300ffb99c060c9e79a686cf7e46723b00c
    (plans/J13s.sha256), written 02:4x 11-26 before any J13s run. Legs B, Kb, OlL3, Dn730f, OlL3x730; 16 B, policy M, S4 counting
    loader; n = 40 (2 rounds x 20, 1 warm-up rep), seeds sha256(session:exp:round:rep). M6 paired only within J13s; M1-M5 from
    J13; Orin family 6 tests, margin ±0.634 us/event. Runner scripts/chain-20261126e.sh, started only after the gate result.
  - J6 v2 (J-J6v2-20261126, 01:49-02:24) ran pinbench-ws v2 from NUC's handoff-J6v2-20261125.tar.gz (sha256 f4e04b88..., fetched
    01:45 while NUC was idle), protocol prep/J6v2/J6v2-protocol.md; ws list and masks per NUC (64 KB and cross dropped, 4096 KB added).
  - SMOKE-Dn730f (J-SMOKE-Dn730f-20261126, not analysed; plans/SMOKE-Dn730f.txt, 1 rep x 5 s, Dn730f + Dn730): first try refused
    by the start guard (load 0.39, my own analysis/build just before; kept as raw/J-SMOKE-Dn730f-20261126.refused-load-*); rerun
    after waitload passed: both legs valid, 0 lost, write_ev = read_ev, sniff CPU 0.44 s each.
  - NUC E4f (02:15-02:33): Dn730f 0 lost in all 20 counting runs, Dn730 lost in all 10 b1 runs, so the PREREG-2b gate passed on
    the data; NUC asked to hold J13s until its independent review of the data and the code fix is done.
  - NUC go for J13s (02:5x): both independent x86 reviews passed (data: Fisher 10/10 vs 0/10 p = 1.1e-5, only b1 discriminates;
    code: the guard matches upstream 70f7bc1f line by line, the only bare CPU read left is the guard itself; Orin's own git grep
    agrees). Build check here: Dn730f cmake options = Dn730's, checks OFF like OlL3 (OlL3 lacks only the dev-tree options
    MAP_PRODUCTION_AB / MAP_USAGE_STATS / PATCH_EXECUTABLE). Notes kept for the paper, no effect on J13s (Frida uprobes):
    on the trap/signal path Dn730f returns -ENOTSUP for programs with a bound array where upstream runs them unlocked;
    the ladder unit test test_array_jit_lifetime.cpp:240-258 (raw CPU -1 expects exec 0) should fail on lock-restored
    ladder trees (guard returns -EAGAIN). Ladder tests only.
  - J13s ran 02:42-03:32 (exit 0 first try, 200/200 valid, no tool calls during the run). Holm now over 6 tests per PREREG-2b:
    M5 Holm-adjusted p 0.055 (0.028 with 5), M6 0.055; M6 TOST equivalent. P4 has no same-session estimate (exploratory only).
  - NUC's optional Orin BPFTIME_AOT_CACHE=0 probe-read run was skipped: E3 (11-23) already ran D with the AOT cache on by
    default and gave D/O 1.002, the same harness that shows the x86 penalty.
