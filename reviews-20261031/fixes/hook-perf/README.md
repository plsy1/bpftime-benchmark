# Hook cost after the review fixes (2026-10-31)

`transformer-lazy.so` (d807ef93, before) vs `transformer-fixes.so` (dev
8c108309: 784-byte frame, FPCR saved and compared, CFI, execve/execveat
compares); `transformer-fixes832.so` is the latter with an 832-byte frame (a
check, not a commit). The agent is the current build in every leg.
`../../../syscall/coverage-20261030/perf/run_layers*.py`; cpu5 at 1728000.

- `go.sh` (x2), 10 alternating rounds (`summary.txt`): the hook alone
  (`rewrite`, dummy agent) 397.6 -> 397.4 ns. With the real agent the medians
  were +3.5 ns (agent-other) and +8.6 ns (agent-exit), from a bimodal spread:
  some runs at the old level, some about 6 ns higher.
- `go3.sh` (`summary3.txt`): the 832-byte frame (same 64-byte phase as before)
  did not change that, so it is not the frame's cache-line phase.
- perf stat (`perf-*.jsonl`, 20M writes, 3 runs): 12 more user instructions
  per write in every leg (FPCR mrs/str/ldr/mrs/cmp/b, the execve compares);
  user cycles per write: rewrite 114.6 -> 114.1, agent-other 129.0 -> 131.1
  (+2 cycles, about 1.2 ns), agent-exit noisy (244-260). Wall time in these
  runs: agent-other 425.6 -> 426.3 ns.

So the fixes cost about 2 cycles per call in user time; the larger medians
of the first runs are the leg's known run-to-run bimodality.
