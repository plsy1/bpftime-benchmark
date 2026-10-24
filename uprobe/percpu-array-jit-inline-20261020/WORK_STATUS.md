# Per-CPU array lookup JIT — 2026-10-20

Base ordinary-array feature committed and pushed as `9ffbffb` on
`codex/uprobe-official-8da867d541f8`; its results were pushed as `6886c0c` on
`benchmark-results/jetson`.

Current experiment is uncommitted and default-off (`BPFTIME_ARRAY_JIT_INLINE=1`).
Primary source: `/home/jetson/src/bpftime-daily-state/worktrees/8da867d541f8/candidate`.
Independent source/build: `/home/jetson/src/bpftime-percpu-array-jit-20261020`.
tmux: `uprobe-percpu-array-jit-20261020`.

Status: final build, correctness/consuming short tests and non-target regression
checks all passed. Complete 35-case x 10 suite launched in tmux at 16:49 JST. Final
stage is `null-guard`: check `null-guard-exit-status.txt`, then
`regression-exit-status.txt` and `full10/exit-status.txt`.
36 cases / 110,242 assertions and 18 host cases / 201 assertions passed.
Six-round medians: hit 12.289 -> 6.648 ns (kernel 6.828), mixed 20.278 ->
12.340 ns (kernel 12.807). These consume values; not comparable to historical
pure/unused lookup latency. Kernel margins are small; avoid broad superiority claims.
Non-target short ratios vs previous build: ordinary array update 0.9999,
per-CPU array update 1.0045, ordinary hash lookup 0.9911, hash update 0.9311,
per-CPU hash lookup 1.0044, hash update 0.9986. No >3% regression. Do not infer
an intended hash-update optimization from a non-target short measurement.

Implementation:
- Reuse owned map/mapping binding; accept homogeneous per-CPU-array lookup programs.
- Use canonical implementation dimensions and checked 32-bit total allocation size.
- Reuse native array descriptor with key stride = value_size * cpu_count.
- Mark private standard helper as `bpftime_bound_percpu_array_lookup_v1`.
- JIT rejects null/out-of-range keys first; calls standard CPU helper 8 for EACH
  in-range lookup, then bounds-checks CPU
  and index and forms base + index*stride + cpu*value_size.
- Do not hoist/cache CPU selection or assume pinned execution; preserve userspace
  selection-at-lookup semantics (not kernel preemption protection).
- Missing/custom CPU helper, mixed maps, other helpers, interpreter, AOT and signal
  callbacks retain the prior conservative eligibility/fallback rules.
- Fork children reject inherited bound programs and must reload.

New tests cover CPU counts/widths, unaligned keys, misses, invalid CPU, changing CPU
within one invocation, real CPU-affinity changes, FD replacement, fork, reclamation,
and custom CPU helper fallback. Consuming benchmark uses per-CPU data AND result maps,
runtime keys and checksum verification. The loader verifies results came from CPU 5.
Compare inline on/off/kernel on the exact same artifacts; unused lookup is NOT a
speedup metric and must not replace the historical pure map-operation result.

Unrelated pre-existing dirty Makefile/hash diagnostics must remain excluded.
Daily automation and the new per-CPU experiment have NOT been pushed/deployed.
