# ATC 2027 measurement campaign, x86 NUC (2026-11-25 → 26)

Plan: paper-drafts/atc-2027/notes/eval-plan.md (the "eval plan"). Preregistration: `harness/PREREG-2.md` (frozen
2026-11-25 before C1), amendments `PREREG-2a.md` (margin base, 11-25) and `PREREG-2b.md` (Dn730 → Dn730f for M6 and
the Orin J13s supplement; frozen 2026-11-26 01:50:36, before the E4f gate), each with its `.sha256`.

Builds (binaries and sha256 in /home/y/src/bpftime-x86-runs/paper-20261124/builds, not committed; `scripts/build-arm.sh`,
LLVM 19 / clang-19 / gcc-13, LTO): K = kernel sslsniff; O = official 4708f409; Ol = upstream head 70f7bc1f (with the
#730 per-CPU exec lock); Ol730x / OlL3 / OlL3x730 = upstream ladders; L3port = official + ported patches; Dn / Dnpc /
Dxn = Tern c71a9987 unchecked / checked / exact; Dn730f = Tern + #730 lock with the inlined lookup fixed (ladder
1a2d42ec); Dnngp / dnngpctl = P13 ablation and its same-path control (ladder fa8d07d0 / c71a9987).
Harness: `harness/run_paper.py` (rows), `harness/session.sh` (guards), `plans/`, analysis `harness/analyze_paper.py`,
`harness/summarize_*.py`. sslsniff loaders: `sslsniff-src/` (s4 counting loader, s4lat2 latency loader, s4pb
SSLSNIFF_PB_PAGES, s4mp one -p per worker; BPF objects of s4pb/s4mp instruction-identical to s4).

Each session directory has rows.jsonl (one row per leg run), SUMMARY.md (results, readings and flags, each checked by
independent subagents with their own scripts; corrections are recorded in the summary), the analysis tables and the
session's check/exit files. Capture directories (cap/) stay on the NUC.

| session | what | summary |
|---|---|---|
| C1-20261125-try1 | exploratory 2×2 ladder (PREREG-2 §9) | C1 notes in paper-drafts x86-facts §19 |
| DX1 / DX2-20261125-try1 | Tern exact / per-stage checks | x86-facts §19 |
| J6x2-20261125-try1 | pin cost under a working set (x86) | x86-facts §19.3 |
| E3-20261125-try1 | latency / notification policy factorial | SUMMARY.md |
| E4/E4-20261125-try1 | per-CPU lost updates, #730 lock price | E4/E4-20261125-try1/SUMMARY.md |
| E4/E4f-20261126-try1 | Dn730f gate (PREREG-2b): 0/20 lost → passed | SUMMARY.md |
| E5/results | field-level identity audit, 2,970 cells all as predicted | E5-20261125-try1.SUMMARY.md |
| I3-20261125-try1 | payload sweep 1–256 KiB | SUMMARY.md |
| I9-20261125-try1 | co-location policy C | SUMMARY.md |
| RPR-20261126 / RPRA-20261126-try1 | probe-read gap = AOT cache's indirect helper calls | RPR-20261126/SUMMARY.md |
| P13-20261126-try1 | grace-period counter ablation | SUMMARY.md |
| I4-20261126-try1 | perf buffer size sweep at 256 KiB | SUMMARY.md |
| I2churn-20261126-try1 | map churn on 1/2/4/8 threads | SUMMARY.md |
| I2N-20261126-try1 | nginx with 2 workers (policy W2) | SUMMARY.md |
| N7-20261126-try1 | Fig. 1 stage ladder with real builds | SUMMARY.md |
| H7a-20261126-try1 | E7a: HAProxy 1 thread, 16 B and 4 KiB | SUMMARY.md |
| H7b-20261126-try1 | E7b: HAProxy 4 threads; Q1 Dn730f/OlL3 1.053 → not met | SUMMARY.md |

Deviations and notes:
- 2026-11-26 00:27:05–12: an ssh from the Jetson session overlapped I3 round 2 rep 9 leg K (I3 DEVIATIONS.md; no
  effect on the readings). Since then every status message to the Jetson states the NUC's state.
- Harness changes during the campaign, each with a `.pre-*` backup on the NUC: policy C (I9), stage S4pb (I4), stage
  S4mp and multi-worker nginx (I2, `nginx-2w.conf`, policy W2); the single-worker path is unchanged.
- E7 (H7a/H7b): HAProxy image bpftime-ssl-haproxy:x86-20261126 (R/docker-haproxy); harness server interface (run_paper.py vs .pre-e7); policy H4 uses CPUs of PREREG-2 §6's non-measured set, so its noisy_core check uses {5, 15} (post hoc). session.sh now logs every pinned CPU (.pre-pinlog).
- churnloop's thread cap was raised 6 → 16 for I2 (`churn/src/CHANGES-I2.txt`; target_func code identical to E11's).
- 2026-11-26 05:14–07:15: the CPU pin restore after I2churn failed (run-i2.sh listed SMT siblings; duplicate entries in
  pin-state.txt), leaving CPUs 0, 1, 3, 5, 7 at a fixed 3.5 GHz with C-states ≥ 2 off during I2N and N7 (measured CPUs
  were set by session.sh as usual). Likely trigger of the core-0 activity (2–16 % busy) seen only in those two sessions
  (rows flagged noisy_core, kept per PREREG-2 §6). Restored by hand; `harness/pin-cpus.sh` restore now takes the first
  entry of a duplicated key.
- Timers that affect measurement were paused for the campaign (only tailscale-watchdog active); restore with
  /home/y/src/bpftime-x86-runs/paper-20261124/timers-paused-20261125/RESTORE.sh.
