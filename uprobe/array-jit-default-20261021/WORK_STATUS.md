# Default per-call-site array lookup JIT

LATEST FOLLOW-UP: user approved the formal repair on 2026-10-21. Primary
and warm erase paths now call the concrete array implementations. New
source/artifact archive and results are in
../array-jit-default-delete-fix-20261021/. Build/correctness, twelve paired
full-workload short rounds and both six-round consuming suites passed.
Ordinary hash delete 37.066 -> 36.309 ns versus last GOOD pre-default build;
other comparable map operations have no >3% regression. Full ten-round
verification FINISHED at 2026-10-21 14:59:07 JST, status 0, duration 1480.87 s;
all 35 x 10 samples, binary SHA checks and default activation audit passed.
Full ordinary hash delete is 36.64 ns (0.34x same-run kernel), recovering
the regression; no evident non-target regression. Source is committed/pushed
as c0939bd on codex/uprobe-official-8da867d541f8; no daily patch-stack update.
Latest handoff:
/home/jetson/src/summry/uprobe/bpftime-uprobe-handoff-20261021.md.
Earlier statements below that primary was unchanged describe the diagnostic
stage, not now.

2026-10-21. User requests removal of the opt-in switch and automatic array
lookup inlining, including mixed ordinary/per-CPU arrays, hash maps, updates
and unrelated helpers. Implementation frozen; consistent rebuild/correctness
PASSED, as did six balanced rounds for BOTH consuming array workloads and the
standalone native VM without runtime. No commit or push. Exact 35-file source
archive matches primary and warm build; native submodule remains clean.

Default cases: 5 cases / 138 assertions passed without enable variable and
again with obsolete BPFTIME_ARRAY_JIT_INLINE=0. Main correctness: 42 cases /
110422 assertions; host contract: 18 cases / 201 assertions. Build/short status
both 0 with actual success logs. No new bpftime_prog ODR warnings in final build.
Consuming all-hit medians: ordinary 3.0950 -> 3.0926 ns (kernel 5.8852,
0.53x); per-CPU 5.1479 -> 5.1471 ns (kernel 6.8556, 0.75x). Essentially stable.
Additional half-miss diagnostics: ordinary 9.6531 -> 8.4546 ns (0.69x kernel),
per-CPU 11.5074 -> 10.6503 ns (0.84x kernel). These are added consuming
workloads, not historical pure lookup latency. Default-on current arm has no
enable variable; previous frozen INLINE arm uses the old flag=1.

Full 35-case x 10 FINISHED 13:58:44 JST, pipeline status 0; sample/SHA and
default activation audit passed. This is NOT a performance acceptance: ordinary
hash delete regressed from 37.0590 to 43.6896 ns/op (+17.9%).

User requested an isolated recheck. hash-delete-recheck/ contains 12 AB/BA pairs
on frozen previous/current binaries, 10000 timed + 1000 untimed warmup invocations,
original BPF and loader, private single-case main + original timer/stubs.
PASSED measurement validation (status 0, 48 frequency checks, hashes stable),
but REPRODUCED performance regression: 37.0534 -> 43.7309 ns/op (+18.02%),
12/12 pairs slower. Root cause NOT yet diagnosed. No source/binary changes
or commits/pushes in this recheck. Do not call the default-on candidate stable
or regression-free. See hash-delete-recheck/RESULT.md.

Subsequent user-authorized investigation localized the cause: adding the
bound-array erase caller changes GCC/LTO inlining of the generic delete
dispatcher. Diagnostic-only direct array erase restores inlining and latency:
12 balanced ABC blocks: previous 37.0997, regression 43.7989, diagnostic
36.2556 ns/op. Diagnostic wins all 12 blocks; correctness/host tests pass.
Primary remains unchanged; warm build differs only at array_map_binding.hpp.
No production fix/promote/commit/push authorized yet. See
hash-delete-investigation/RESULT.md. Non-target performance still untested
for the diagnostic artifact; accepting default-on requires that follow-up.

Primary: /home/jetson/src/bpftime-daily-state/worktrees/8da867d541f8/candidate
Warm independent build: /home/jetson/src/bpftime-percpu-array-jit-20261020/build-jit
Previous immutable artifacts: ../percpu-array-jit-direct-cpu-20261020/artifacts
Prior source saved in baseline-source; unrelated dirty diagnostics preserved.

Design: explicit per-map kind in parent-owned generated llvmbpf descriptor;
only known standard lookup implementations may opt into automatic binding.
LLVM chooses constant-map call sites, unknown/hash/custom calls retain their
original helper. Bound array misses and standard mutations use the same owned
map identity; custom helpers are never bypassed. Retain CPU checks/query error
contract, ownership/FD reuse/mapping/fork guards, interpreter/AOT/signal limits.

Gates: mixed-map+helper default-on tests, previous lifetime/CPU/host tests,
standalone compatibility, consuming matched short A/B vs previous inline
version (not historical unused-result benchmark), then full suite in tmux.
Do not edit active runner scripts or alter frozen previous binaries.

Build history: attempt 1 fixed an int/unsigned helper-map interface mismatch.
Attempt 2 stopped before tests because separate top-level targets repeatedly
reinstalled libbpf/recompiled headers; its EXIT-only trap's 0 after SIGINT is
NOT success (see its NOTE). Attempt 3 stopped before tests on stale bpftime_prog
objects after source mtimes were preserved during an earlier build. Current
runner refreshes compiler dependencies and affected interface/test timestamps;
new class-layout warnings are not acceptable. Existing ebpf_inst native/compat
ODR warning predates this change. Check actual test logs, not status alone.
