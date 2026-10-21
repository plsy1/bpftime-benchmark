# Array JIT: libbpf global data in bound programs (2026-10-21)

Change: `bdd98355868b` on `codex/uprobe-official-177a1dc465ff` (local, not
pushed), on top of the `5466f0d` baseline. Direct map-value LDDWs (libbpf
`.rodata`/`.data`/`.bss`/`.kconfig`) no longer keep a program off the array
lookup JIT. On the kernel's direct-value terms (single-element ordinary array,
offset inside the value) each such map is bound/owned like an inlined lookup
target, and the loaded copy of the program replaces the LDDW with a 64-bit
constant of the retained element-0 address plus offset. The stored program
keeps its relocations; compilation (eager or lazy) never resolves global data
through `map_val(fd)`. Other targets/relocations keep the original path; global
data alone does not create a binding. No llvmbpf overlay change.

## Correctness

- `dev/`, `dev2/`: development builds of the working tree. `dev/` exposed a
  wrong test expectation (the uBPF interpreter's own bounds check rejects global
  data addresses; uBPF never binds), fixed in the test; `dev2/` passed.
- `formal/tests.log` (clean `bdd9835` worktree, LLVM 16, baseline options,
  `cmake-options.diff` empty): complete daily filter, **69 cases / 128422
  assertions passed**; the new `[array-jit-global]` cases cover close/reuse/
  reset of the global map, program writes and userspace updates on the shared
  value, a map used both as lookup target and global data, last-reference
  release (no leaked shared memory) and the rejected targets (multi-element,
  out-of-range offset, hash, per-CPU, BTF-ID relocation, uBPF, global data
  without an array lookup).

## Performance (`formal/`, CPU 5 at 1.728 GHz)

Consuming workloads, 6 alternating rounds, kernel / baseline `5466f0d` /
current `bdd9835`, ns per element (median):

| Workload | Case | Kernel | Baseline | Current | Current/kernel | Current vs baseline |
|---|---|---:|---:|---:|---:|---:|
| libbpf-style globals | ordinary hit | 5.89 | 21.99 | 3.09 | 0.53x | -85.9% (6/6) |
| libbpf-style globals | ordinary mixed | 12.28 | 30.83 | 8.53 | 0.69x | -72.3% (6/6) |
| libbpf-style globals | per-CPU hit | 6.91 | 24.79 | 5.17 | 0.75x | -79.1% (6/6) |
| libbpf-style globals | per-CPU mixed | 12.82 | 32.48 | 10.65 | 0.83x | -67.2% (6/6) |
| standard (no globals) | all cases | | | | unchanged | -0.2% .. +0.9% |

The globals variant (`-DBPFTIME_ARRAY_JIT_GLOBALS`) adds a `.rodata` setting
(0, checksums unchanged) and a `.bss` counter to the same consuming workload.
The baseline runs it at roughly unpatched-official speed: besides the JIT gate,
`uses_only_maps_of_type_for_helper` in `bpf_attach_ctx.cpp` also refuses the
typed C++ helper for any program with a direct map-value LDDW, so such programs
fall back to the generic helper. That remains true for non-array lookups.

Full ten rounds (`formal/full10`, 22:50-23:15, exit 0, sample/SHA validation
and default activation audit passed): all operations within +/-0.4% of the
baseline except ordinary hash update, +3.8% (34.11 -> 35.41 ns). Investigated:
its executed code (typed helper, `bpftime_hash_map::elem_update`,
`map_update_elem` dispatcher) is instruction-identical to the baseline apart
from address immediates (`formal/native-check`), the samples are bimodal
(~33/~36 ns) in every run and this run had one 43 ns outlier, and a 12-round
AB/BA screen of both builds on the same workloads (`formal/hash-update-screen`)
gives 33.43 vs 33.45 ns median, ratio of means 0.9997; every operation within
+/-0.6%. Not a regression.

`formal/failed-attempt-{1,2,3}`: two workload-build mistakes in my scripts
(skeleton `name` argument; BPF-side `u64` in skeleton types, fixed in the
source as `__u64` before the commit was finalized) and one wrong runner
assertion (libbpf creates separate `.rodata` and `.bss` maps; control programs
also look up `result_map`, so they are bound too). Runtime behaviour was
correct in all three; kept for provenance.

## Coverage (static survey, `../jit-eligibility-survey-20261021`)

| Corpus | Lookup programs passing the JIT gate | Inlinable array lookup sites |
|---|---|---|
| libbpf-tools (all) | 8 -> 179 of 183 | 0 -> 19 (all array sites) |
| libbpf-tools (userspace-runnable) | 1 -> 38 of 38 | 0 -> 9 |
| bpftime examples | 44 -> 54 of 55 | 18 -> 19 |

Remaining libbpf-tools rejections reference externs (ksym/kconfig; the static
analysis cannot tell them apart and counts them as blocked).
