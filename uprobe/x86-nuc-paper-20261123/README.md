# Paper re-measurement campaign, x86 NUC (from 2026-11-23)

Plan: the re-measurement plan of 2026-11-23 (copied to the Jetson as ~/src/paper-handoff-20261123/remeasure-plan-20261123.md).
Rules fixed before the first formal measurement: `harness/PREREGISTRATION.md` (sha256 in `harness/SHA256SUMS`).

- Frozen code: O = official 4708f409, Odiag = ladder/odiag-4708f409 016a5151 (switches read at load time; 4bef75b7 was superseded because nginx workers get a cleared environment), L3port = ladder/l3port-4708f409 8ac1ec70, D = dev 9ccda7e8, Opc/Dpc = O/D with probe checks on. Built by `scripts/build-arm.sh` (LLVM 19, clang-19, gcc-13, LTO); binaries and sha256 in /home/y/src/bpftime-x86-runs/paper-20261124/builds (not committed).
- sslsniff: `sslsniff-src/` (counting loader; stage defines). S4 program sections are byte-identical to stock.
- Runner: `harness/run_paper.py` (one JSONL row per leg), session guard `harness/session.sh`, `plans/`.
- Raw rows: /home/y/src/bpftime-x86-runs/paper-20261124/raw/<session>/rows.jsonl (summaries will be committed here per experiment).

E0 smoke (2026-11-23, one rep each, policy M, 16 B unless noted) — functional checks, not results:
- Every build traces; K and D deliver 2 events per request; D reports every drop it makes (missing = reported).
- Official drops silently: about 0.01-0.04% at 16 B; at 256 KB under full load (-c 100) it delivered 11.7% of the written bytes and reported no loss.
- Kernel run_cnt: 6 probe hits per request (SSL_read runs twice per request; the second returns no data), not 4.
- Odiag switches work after 016a5151 (path counters match event counts); ALL3 ≈ L3port; NOOP_OUTPUT gives 0 events; uid/pid/tid fields agree with the kernel's.

## Deviations and notes (kept apart from the preregistration)
- 2026-11-23: plan leg name `B#M-t3` (Jetson, wrk {0,1,2} -t3) is written `B#Mt3`, because run_paper.py policy names are \w+. Naming only; criterion (e) unchanged.
- 2026-11-23 E1: the E1/E2 chain was launched detached, so session.sh did not find this Claude session's process tree; it was moved to CPUs 12-14 by hand during the first E1 round (about 13:55). Rows before that are flagged for a sensitivity check.
- 2026-11-23 E1: the Jetson session copied its frozen artifacts to the NUC (tar over ssh, then sha256 on the NUC) about 14:29:30-14:39:30, during E1. Rows overlapping that window are flagged "external load" and analyzed with and without.
- 2026-11-23: PREREGISTRATION.md line 6 still names Odiag as 4bef75b7; the frozen Odiag is 016a5151 (4bef75b7 superseded, see above). Reported by the Jetson session.
- 2026-11-23: the kernel leg always misses exactly one request: start_nginx's readiness curl is served before the kernel sslsniff attaches to the worker (bpftime's preloaded agent sees it). Analysis and the τ calibration subtract 1 request (1 read event, its bytes) from K's expected counts. Found by the Jetson session; x86 E1 K missing-minus-reported max is 2, consistent.
- 2026-11-23: criterion (e) nginx busy is computed as the sum over the nginx cores of policy M ({2,6} on x86): the single worker migrates between them.
