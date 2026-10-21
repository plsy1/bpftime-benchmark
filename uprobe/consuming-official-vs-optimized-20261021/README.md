# Consuming array lookup: unpatched official vs optimized (2026-10-21)

Question: how does bpftime without our optimizations do on our own consuming
array workload (`benchmark/uprobe/diagnostics/array_jit_poc*`, added by us in
`9ffbffb`/`c0939bd`; not an upstream benchmark)?

Arms, alternating, 6 rounds x 50000 invocations x 1000 elements, CPU 5 at 1.728 GHz:
- `kernel`: same BPF object in kernel eBPF.
- `previous` (in raw/summary files) = **unpatched official `177a1dc`**, built in a clean
  detached worktree `/home/jetson/src/bpftime-official-177a1dc-llvm16-20261021`
  with the baseline's LLVM 16 options (`cmake-options-vs-baseline.diff` only lists
  options that exist solely in the patched tree). Asserted to emit no `[array-jit` logs.
- `inline` = optimized `5466f0d` frozen artifacts from `../baseline-177a1dc-jit-llvm16-20261021`.
All arms use the workload binaries built in that baseline. Runners are generated
from `../array-jit-default-20261021/run-{ordinary,matched}.py` (assertions only
adapted for an official arm). All checksums/hit/miss counts passed; exit 0.

Median raw ns per element (includes probe, key generation, value read, accounting):

| Case | Kernel | Official | Optimized | Official/kernel | Optimized/kernel | Optimized vs official |
|---|---:|---:|---:|---:|---:|---:|
| ordinary hit | 5.77 | 21.81 | 3.10 | 3.78x | 0.54x | -85.8% |
| ordinary mixed | 12.25 | 30.17 | 8.44 | 2.46x | 0.69x | -72.0% |
| per-CPU hit | 6.89 | 29.95 | 5.12 | 4.35x | 0.74x | -82.9% |
| per-CPU mixed | 12.78 | 39.25 | 10.64 | 3.07x | 0.83x | -72.9% |
| control_hit (no map access) | 3.76 | 1.61 | 1.60 | 0.43x | 0.42x | -1.1% |
| control_mixed (no map access) | 9.30 | 2.52 | 2.50 | 0.27x | 0.27x | -0.7% |

Control cases are equal for official and optimized: the gain is entirely in the
map lookup path. Control-subtracted values are indicative only (the control loop
has no helper call and a different branch structure); see the handoff document.
