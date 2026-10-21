# Default-on array JIT: formal delete regression repair

User approved applying the diagnosed fix on 2026-10-21. Primary and warm
sources now match; 35 files are archived in source/ with SHA-256 manifest.
Optimization source remains uncommitted/unpushed. Selected result text and
handoff documents are published separately; no automation update was made.

Change: array_map_binding::erase calls the concrete ordinary/per-CPU array
implementation, not the generic all-map delete dispatcher. These bindings
only admit these built-in implementations; both are lock-free for this
operation and retain the original -1 / EINVAL behavior. Owned mapping, map
identity, fork guard, default inlining and mixed-map support are unchanged.

The added generic caller had changed GCC 13 + LTO inlining decisions for
ordinary hash deletion even though that operation does not use array JIT.
Formal native-code inspection confirms restoration: the standard delete
helper is again 9460 bytes / 2365 instructions, with no separate generic
delete dispatcher branch. Function sizes cover the all-map helper, not the
instructions executed for an individual operation. See native-comparison.json.

Build status 0; correctness 42 cases / 110422 assertions, host contract
18 cases / 201 assertions, default behavior 5 cases / 138 assertions passed.
Artifacts are full newly linked copies (agent, server, CLI), not diagnostic
symlinks to the earlier server. Exact hashes are in artifact-sha256.txt.

Short performance validation PASSED: 12 alternating AB/BA rounds
using the original full victim, loader and BPF object, 10000 invocations per
case. Baseline is the last good pre-default version in
../percpu-array-jit-direct-cpu-20261020/artifacts (old enable flag 1).
Current uses no enable flag. Every process gets fresh maps, affinity CPU 5,
checked 1728 MHz frequency and immutable measured binaries. There is no
same-run kernel in this screen. Original array lookup does not consume its
return value, so those two rows are not used to claim performance gains.

Separate consuming ordinary/per-CPU array checks: six balanced rounds with
same-run kernel and the same pre-default baseline, runtime keys, actual
value reads, checked checksum/hit/miss counts. All-hit and half-miss cases
are additional diagnostic workloads, not original uprobe benchmark cases.

After short non-regression checks passed, run-full.sh completed 35 cases x 10,
same-run kernel, sample/SHA validation, and a separate default activation
audit. It excludes dead-result array timings from historical gain claims.
All tests run serially in tmux session uprobe-array-jit-default-20261021.

Short status 0: all 24 full-workload processes and 36 consuming workload
processes passed sample, frequency, hash, activation and checksum checks.
See RESULT.md for numbers. Complete ten-round verification FINISHED at
2026-10-21 14:59:07 JST, exit status 0, timed duration 1480.87 seconds.
All 35 cases have ten valid samples; artifact SHA checks and the separate
default activation audit passed. Ordinary hash delete recovered from 43.69
to 36.64 ns (0.34x same-run kernel), with no evident non-target regression.
See full10/ten-operation-comparison.md. New optimization code remains
uncommitted/unpushed and is not yet in the daily replay stack. Current handoff:
[handoff document](https://github.com/plsy1/bpftime-benchmark/blob/summry/jetson/uprobe/bpftime-uprobe-handoff-20261021.md).

Publication scope: selected result descriptions, complete ten-round JSON/table,
consuming raw/summary/environment data, validation logs and hashes. The full
scripts, source archive, per-process logs and binary artifacts described above
remain available at the documented local path, not in this selected publication.
