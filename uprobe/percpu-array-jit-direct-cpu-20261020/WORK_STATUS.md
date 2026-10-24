# Direct CPU query in the ALREADY INLINED per-CPU array lookup

Date 2026-10-20. No commit/push yet. tmux:
`uprobe-percpu-direct-cpu-20261020`. Status as of 20:35 JST: full 35-case x 10
suite RUNNING (launched about 20:33); kernel run 1 completed, run 2 started.
Build, correctness, six-round consuming A/B, non-target regression screen and
standalone-VM compatibility all passed (each exit status 0). Full status is
`full10/exit-status.txt` when finished; progress is in `full10/runner.log` and
the selected tmux `full-progress` window. No commit or push yet.

Consuming all-hit: previous INLINE 6.6501 ns -> candidate INLINE 5.1515 ns
(-22.54%, 6/6 paired wins), same-run kernel 6.8276 ns (~0.75x kernel).
Added half-miss diagnostic: 12.4826 -> 11.5081 ns (-7.81%, 6/6 wins), kernel
12.7191 ns (~0.90x). These are additional consuming workloads, NOT the original
unchanged uprobe test. Do not mix with historical unused-return lookup results.
Correctness: 37 cases / 110284 assertions; host: 18 cases / 201 assertions.
Frozen 33-file source archive checked equal to primary and measured source.

Baseline is the frozen completed per-CPU JIT feature under
`../percpu-array-jit-inline-20261020/artifacts`, NOT the un-inlined C++ helper.
Same consuming BPF object/loader/victim, runtime keys/value checksums; invalid
keys are only an additional custom diagnostic workload, not an original uprobe
benchmark case. Unused-result data cannot be claimed as lookup optimization.

The preceding zero-argument-alias-only experiment was rejected (<0.3% gains)
and its own code restored; see `../percpu-array-jit-cpu-abi-20261020/RESULT.md`.

This candidate weakly resolves an OPTIONAL cold runtime provider in LLVM.
Provider returns raw CPU getter and the shared error callback only for the
EXACT registered standard CPU helper function. Linux uses public sched_getcpu,
not private TLS/rseq offsets. A standalone VM/custom helper or absent provider
keeps its original helper call. No mutable registry, extra BPF helper number,
shared map layout change or helper-8 replacement.

JIT calls raw int(void) once per in-range lookup, sign-extends non--1 results;
raw -1 calls the SAME cold callback as standard helper 8, preserving log-once
and CPU 0 return. Then existing CPU bounds and address computation apply.
Preserve null/key checks, errno behavior, bindings/FD identity, fork fail-closed,
signal/AOT limitations. Tests use a sched_getcpu wrapper ONLY in the test
executable for CPU/failure/query-count injection; production libraries never do.

Source primary:
`/home/jetson/src/bpftime-daily-state/worktrees/8da867d541f8/candidate`.
Warm independent source/build: `/home/jetson/src/bpftime-percpu-array-jit-20261020`.
It can be rebuilt; prior performance artifacts remain frozen. New files before
this candidate are saved in `baseline-source/`; remaining prior source in the
previous experiment's source archive. Preserve all unrelated dirty diagnostics.

Gates declared before results: correctness + actual JIT assembly; six balanced
per-CPU consuming rounds, >=2% all-hit gain and >=5/6 wins vs previous INLINE
version; mixed/controls no repeatable >3% loss. Then ordinary consuming and
non-target map A/B checks before full 35-case x 10 regression in tmux.
