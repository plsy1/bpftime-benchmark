# Continuation checkpoint

COMPLETED 2026-10-20 ~01:26 JST. Full 35 cases x10 samples passed, 1495.79s.
Separate exact-artifact INFO activation audit passed (17 cases x1000 iterations,
not included in timings). The original log-count gate failed with exit 2 because
the historical victim environment omits INFO; its exit/script are preserved,
finalize-full.py reran all postchecks, and the runner is corrected for future
runs. Both final exit-status files and post-validation status are 0. Primary
source hashes/user files verified, no benchmark process/default SHM remains.
README and ten-operation-comparison.md contain final evidence. No more work
required beyond final self-contained handoff; no commit/push authorized.

User asked to finish ordinary-array lookup JIT integration, not just the POC.
Do not expand to per-CPU/hash JIT or push/commit without a new request.

Final source is LOCKED: no more runtime changes while the full suite runs.
Source snapshot/build: `/home/jetson/src/bpftime-array-jit-integrated-20261020/build-jit`.
All 27 source files have been applied with apply_patch to the primary worktree
`/home/jetson/src/bpftime-daily-state/worktrees/8da867d541f8/candidate`, branch
`codex/uprobe-official-8da867d541f8`, HEAD still fefe2f6. Source digests match.
User's five diagnostic files passed their prior preserved SHA manifest. Original
primary versions are backed up in `primary-before-integration/` and JSON.
The official llvmbpf submodule is clean; the parent-owned CMake patch generates
one patched translation unit in the build tree.

Implemented ownership/refcounts, exact mapping retention, FD close/reuse/reset
safety, consistent miss handling, unaligned keys, custom-helper fallbacks,
failed-load guards, interpreter/JIT reload handling, and fork-child isolation.
Fork guard is an atomic generation changed in pthread_atfork's child hook.
Inherited child bindings return ENOTSUP and do NOT decrement parent refcounts.
Snapshots use an empty name; class friend permits clearing only borrowed local
pointer fields before child destruction. No getpid syscall on the hot path.

Opt-in BPFTIME_ARRAY_JIT_INLINE=1; default off. Eligible LLVM ordinary-array,
lookup-only, known standard helper. Not signal/AOT/MPK/CUDA or generic programs.
No concurrent-detach/quiescence, raw clone, interrupted-signal-fork, crash owner
reclamation, or mixed-runtime-version claims. No daily workflow deployment.

Final validation completed:
- 31 selected tests, 108768 assertions, all pass (tests.log).
- 18 host-contract/config/allocation tests, 201 assertions, all pass.
- ab6-accepted: 5 engines x 6 rounds, all 30 checksum and 60 frequency checks pass.
- All hits: off 7.8950, on 3.0941, kernel 5.8830 ns/element; -60.81%, 0.526x.
- Mixed: off 17.5412, on 9.6320, kernel 12.2370; -45.09%, 0.787x.
- Both win 6/6; these are raw NEW consuming-workload elapsed times, not pure
  helper costs or directly comparable with the historical unused-result table.
- array-regression3/hash-regression3 passed 3% median non-target regression gates.

FULL TEN-ROUND SUITE IS RUNNING, not finished yet. Started ~01:01 JST October 20;
as of 01:10 JST it is kernel run 6/10, then userspace 10 and embedded 10 remain.
tmux: uprobe-array-jit-integrated-20261020, selected current-progress window.
Driver: run-accepted.sh, stdout accepted.log; full log full10/runner.log.
Frozen full artifacts are artifacts/. Do NOT rebuild or edit those artifacts.
run-full.sh refreshes CLI using generated make rules and builds embedded binary;
both completed and frozen SHA checks passed. Full runtime-sha256.txt covers 7
artifacts. Tool is current source too, not an old cleanup-only tool.

NEXT:
1. Keep watching full10/runner.log without busy polling (clock sleep <=60s;
   concise commentary <=60s per developer). Kernel rounds ~87s each.
2. Require BOTH full10/exit-status.txt and accepted-exit-status.txt ==0.
   Script validates 17 kernel +17 userspace +1 embed, 10 finite positive samples
   each, >=10 actual inline log sites, and immutable artifacts. Automatically
   writes full10/ten-operation-comparison.md via summarize-full.py.
3. Inspect results/regressions against previous C++ full10 and verify primary
   source digests, frozen hashes, preserved user diagnostic files, no owned
   benchmark processes and no default bpftime SHM left. No live source edits.
4. Update README running paragraph to completed counts/duration, optionally
   append final lifecycle notes to RESULT.md. Earlier attempts are kept in
   ab6/, ab6-final/, ab6-release/, source-before-*, artifacts-before-* with
   relocation digest JSON; only ab6-accepted is final fork-safe version.
5. Final Chinese answer: implementation integrated, full verification passed,
   target new-workload gains; clearly default off/not committed/pushed. User's
   standing preference is a ten-operation table with initial, initial/kernel,
   previous, current, same-run kernel, current/kernel and optimization status.
   Ordinary array lookup row must say separate test/not historically comparable,
   NOT use unused-result JIT speed as a gain; show hit/mixed in separate 2 rows.
   Link absolute RESULT.md / full10 table. Initial and previous numbers are in
   generated ten-operation-comparison.md. Do not claim all operations won or
   a production-ready/general concurrent fork/attach implementation.

All commands use exec_command require_escalated because default bwrap fails on
this Jetson. Edits via apply_patch. No subagents, no skills used. No git reset,
checkout, staging, commit, push or daily service changes performed.
