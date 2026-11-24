# Jetson paper re-measurement (2026-11-23 →)

Jetson Orin Nano part of the paper re-measurement (plan of record: remeasure-plan-20261123.md, kept with the
NUC session). The x86 counterpart is `uprobe/x86-nuc-paper-20261123/` on benchmark-results/x86-nuc.

- `harness/`: shared files, byte-identical on both platforms (`SHA256SUMS`): PREREGISTRATION.md, run_paper.py, pinbench.c, nginx.conf.
- `jetson-harness/`: Jetson-only session wrapper, guard (`check_env.sh`) and affinity config.
- `scripts/`: build scripts (LLVM 16 + clang-15) and the E0 activation audit.
- `builds/`: per frozen arm the sha256, cmake options, source state and toolchain. Binaries stay in
  `~/src/paper-runs-20261124/builds/` (read-only) with a copy on the NUC.
- `sslsniff-src/`: counting loader and stage sources (+ the stock .orig, Makefile, sslsniff.h).
- `e0/`: E0 read-only checks and functional gate (all pass), see `e0/E0-summary.md`.

## Deviations and implementation notes
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

## 11-24 afternoon and evening sessions
- `runs-20261124/`: also E3pc (complete), E3pc2, E14, E15, E16, E18a/E18b (E18 split around apt-daily,
  pooled), the two smokes; `refused/` keeps the logs of guard-refused attempts. Per-leg captures stay on the Jetson
  except `J-E18a-20261124/excluded-leg-cap/` (the E18 leg excluded for a probe that was not running).
- `analysis/`: E14-E3pc.md, E15-E16.md (batched-wakeup kernel, exact identity), E18.md (new base: official
  70f7bc1f, dev c71a9987, dexact on c71a9987), E18-table-{n19,all}.txt, E18-filtered/ (rows without the excluded leg).
- `builds/`: Dx, Ol, Olpc, Dn, Dnpc, Dxn added. `scripts/pairgmr.py`: paired GMR with 95 % t-CI used for the paper numbers.
- Deviations for these sessions are in DEVIATIONS.md.
