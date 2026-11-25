# C1-20261125-try1 — deviations and flags (written 2026-11-25 after the independent check, workflow wf_c8e28d7f-f73)

Session valid under PR 2.0.5 (267 rows, 0 invalid). Flags only; no row is dropped.

1. **PR 2.0.1 timer start-guard not enforced.** session.sh records timers.txt but does not refuse to start; apt-daily
   was due 15:01:21 (3.5 min in). It ran 15:01:22 and finished within the second. Overlapping row: r0 rep1 Kb (−1.06%).
   fwupd-refresh 15:05:31 (instant): r0 rep2 DnR, Dn/NAP0. anacron 15:32:37 (0 jobs): r1 rep0 Kb.
2. **This session (the x86 Claude session) paused the timers during C1**, at the user's request, 15:43:39–15:44:31:
   `sudo systemctl disable --now` on 11 timers, `crontab` replace, `journalctl`. It should have waited for C1 to end.
   Overlapping rows: r1 rep4 K, Dn, OlL3x730R; r1 rep5 Dn/NAP0-WB32 (all within ±1% of their leg medians). The session
   is therefore two regimes (≈46 min with timers enabled, the rest paused); Dn−OlL3x730 by round: −0.185 and −0.221 µs/event.
3. **PREREG-2 §7 citation error.** The "uid/gid step 0.562" and "pid/tid step 0.190 µs/event" figures come from E6x/summary.md:23
   (4708f409 base), not E18. Values unchanged.
4. **missing_ev undercounts write-side loss at 16 B** (write term ⌈Δbytes/16384⌉, SSL_write ≈ 261 B here). Use the event-count
   test 2·req − delivered − lost instead. Upstream legs (Ol*, Ol730x, OlL3x730*) lose up to 1,598 events per run silently
   (no lost count); Dn family 0 unreported; K/Kb exactly 2 (the readiness curl). Delivered per request equal within 1e-4.
5. **Layout controls are partial.** -falign-functions=64 re-aligns bpftime and spdlog code only (0.16–0.21 → 0.73–0.79 of
   functions on 64 B); libbpf, frida-gum and LLVM are prebuilt/unchanged. P5 is a weak layout control.
6. **nginx worker core not recorded.** The single worker lands on cpu2 or cpu6 per row (policy M nginx {2,6}); rps differs
   by up to 1.3% between them (Dn). M5 same-core pairs: −0.208 µs/event, unchanged.
7. **Leg → build mapping** is not stored per row; config-x86.json was edited at 16:08:30 (DnE/DnEpc added after C1).
   Mitigated by the sha guard at start and end and by the round-2 syscall fingerprints.
8. SMT siblings cpu3/5/7 count as "non-measured" in PREREG-2 §6; cpu3 > 3% busy in 111/267 rows (max 4.2%, under 5%).

Harness fixes before the next session: record worker_cpu and per-build agent sha256 in every row; add the event-count
loss test; make session.sh refuse to start when a maintenance timer is due within the session + 20 min.
