# Per-CPU array JIT CPU-call ABI experiment

2026-10-20. No commit/push requested or made for this experiment.

Status: incremental build and correctness tests running in tmux
`uprobe-percpu-cpu-abi-20261020`. Logs and statuses are in this directory.

Goal: optimize the ALREADY INLINED per-CPU array lookup, not its bypassed
C++ map helper and not eligibility generalization. Native baseline JIT code
has five `mov x0..x4,xzr` instructions before each CPU query. Introduce a
private zero-argument ORC alias to the exact same registered standard native
`uint64_t(void)` CPU helper. Keep public BPF helper 8 intact, retain its error
handling, preserve one dynamic CPU query per in-range lookup, all bounds,
null-key/errno, ownership, fork and custom-helper rules. No CPU caching,
private TLS access, map layout change, new BPF helper number or helper bypass.

Frozen baseline is previous completed full10:
`../percpu-array-jit-inline-20261020/artifacts`.
The baseline source for the three modified files is under `baseline-source/`.
Full prior source is `../percpu-array-jit-inline-20261020/source/`.
Primary source is `/home/jetson/src/bpftime-daily-state/worktrees/8da867d541f8/candidate`.
Independent warm build/source is `/home/jetson/src/bpftime-percpu-array-jit-20261020`;
its binaries can change, but prior full10 artifacts and evidence remain frozen.

Predeclared gates: correctness and actual JIT assembly must pass first;
six alternated rounds with unchanged consuming BPF/loader/victim and live maps.
All-hit target median >=2% improvement vs previous INLINE version, >=5/6
paired wins; mixed and controls must not show a repeatable >3% regression.
Then ordinary consuming A/B and non-target array/hash regression screens.
Only passing candidates proceed to the complete ten-round suite in tmux.
Unused-result timings are not gains and cannot replace historical lookup rows.

Unrelated dirty Makefile/hash diagnostics and pending per-CPU feature changes
from the previous completed task are preserved. Official llvmbpf submodule
must remain unmodified; its source changes stay in the parent-owned patch.
