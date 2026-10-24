# Per-CPU array lookup JIT specialization

The previous ordinary-array optimization was committed and pushed as `9ffbffb`
on `codex/uprobe-official-8da867d541f8`. Its validation results are on the results
branch in commit `6886c0c`. This new per-CPU extension is not committed or pushed.
Default remains OFF; opt in with `BPFTIME_ARRAY_JIT_INLINE=1`.

## Final short-test results

Six alternated rounds, same BPF object/loader/victim and same runtime binary for
inline off/on, CPU 5 at 1,728 MHz. Every run verifies four value checksums and
hit/miss counts; result-map CPU slots must show execution on CPU 5.

| Consuming workload | Inline off | Inline on | Same-run kernel | On/off | On/kernel |
|---|---:|---:|---:|---:|---:|
| All hits | 12.289 ns | 6.648 ns | 6.828 ns | 0.541x (-45.9%) | 0.974x |
| Approximately half misses | 20.278 ns | 12.340 ns | 12.807 ns | 0.609x (-39.1%) | 0.964x |

Medians, raw ns/element including probe/key generation/value reads/checksum work;
NOT isolated helper latency and NOT directly comparable to the historical pure
map-operation benchmark. The reductions versus inline off are substantial;
the 2.6–3.6% margin versus kernel is small and should be described cautiously.
Do not count the separate unused-result case as an optimization gain.

Evidence: `ab6-null-guard/`, `smoke-null-guard/`, `ordinary-ab3-null-guard/`.
Correctness: 36 cases / 110,242 assertions, plus 18 host/config/allocation cases /
201 assertions. Includes real CPU-affinity changes, two different CPU selections
within one JIT invocation, odd widths, unaligned keys, CPU/index bounds, null key,
FD replacement, retained-map identity, fork rejection and final reclamation.

## Implementation

Reuse the ordinary-array feature's owned implementation/mapping references.
Only homogeneous per-CPU-array lookup-only programs with known standard lookup
and CPU helper implementations specialize. The existing descriptor carries
key stride = value_size * canonical implementation CPU count. No shared map
layout change, arbitrary helper slot reservation, fixed-CPU binding or new hash
algorithm is introduced. The upstream llvmbpf submodule remains unchanged.

JIT code rejects null/out-of-range keys before querying CPU. For each in-range
lookup it calls the existing standard CPU helper, checks the result against
the bound allocation, and computes base + key*stride + cpu*value_size. The CPU
call must not be pure or hoisted/cached. Misses preserve ENOENT/null via the
private standard fallback. Missing/custom CPU helpers retain the original path.

This preserves userspace selection-at-lookup behavior, NOT kernel preemption
protection while the caller subsequently uses a returned pointer. CPU hotplug
support, inherited-program execution after fork, AOT and signal callbacks are
not added. The feature remains experimental and default-off.

## Experiment stages

- `ab6/`: first correct version queries CPU even for a proven index miss;
  6.737 ns all-hit, 15.843 ns mixed. Its binaries are preserved under
  `artifacts-before-key-first/`, source changes under `source-before-key-first/`.
- `ab6-key-first/`: bounds before CPU lookup reduces mixed to 12.405 ns.
  Binaries and changed sources preserved in `artifacts-before-null-guard/`
  and `source-before-null-guard/`.
- `ab6-null-guard/`: final null-key compatibility guard; all-hit 6.648 ns,
  mixed 12.340 ns. All 31 archived source files match the primary worktree.

`array-regression3/` and `hash-regression3/` compare unchanged operations with
the previous ordinary-array integrated build. Both inlined array-lookup cases
are explicitly excluded from historical unused-result comparisons.

Full 35-case x 10 regression suite: see `full10/` and its completion status;
the full timing run is distinct from the consuming short tests above. The
complete ten-operation table is generated at `full10/ten-operation-comparison.md`
only after the full suite and exact-artifact activation audit pass.

Source/build: `/home/jetson/src/bpftime-percpu-array-jit-20261020`.
Primary branch: `/home/jetson/src/bpftime-daily-state/worktrees/8da867d541f8/candidate`.
tmux: `uprobe-percpu-array-jit-20261020`.
