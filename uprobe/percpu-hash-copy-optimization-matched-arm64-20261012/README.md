# Per-CPU hash copy-path optimization on Jetson ARM64

Status: **valid matched A/B; success**

## Configuration

- Jetson power mode: `MAXN_SUPER`
- pinned CPU: CPU5
- CPU frequency: min/max/current all 1.728 GHz for every sample
- compiler: GCC 13
- build type: `RelWithDebInfo`
- LTO: on
- probe read/write checks: off
- compile-time spdlog level: INFO
- VM: LLVM
- five paired rounds, 20,000 victim invocations per case and 1,000 warmups
- each timed victim invocation performs 1,000 map helper operations

Baseline is stage 1 (`build-uprobe-opt-20261011`); candidate is
`build-uprobe-hash-opt-20261012`. The harness, BPF object, source diff, CMake
caches, binary hashes, raw logs, frequency samples, and summary are archived in
this directory.

## Median net helper cost

| Case | Kernel | Baseline | Candidate | Candidate change |
|---|---:|---:|---:|---:|
| ordinary hash lookup | 25.77 ns | 58.28 ns | 58.29 ns | -0.02% |
| ordinary hash update | 91.40 ns | 62.06 ns | 62.45 ns | -0.63% |
| per-CPU hash lookup | 26.65 ns | 136.25 ns | 131.11 ns | **3.78% faster** |
| per-CPU hash update | 65.29 ns | 220.95 ns | 140.36 ns | **36.47% faster** |

The unchanged ordinary-hash cases serve as controls. Their sub-1% changes show
that the per-CPU gains are not a build-wide shift.

## Change

- Reuse the pre-sized per-CPU key scratch vector with `memcpy` instead of
  `vector::assign`.
- On update hits, copy the caller's value directly into the selected CPU slot;
  do not first populate a second shared-memory vector and then copy it again.
- On insertion, copy the zero template once and write the caller's value
  directly into the selected slot.
- Preserve the diagnostic `no_find` mode by copying into its existing scratch
  vector only in that mode.

`summary.json` is the compact result; `wall-raw.csv` contains every round.
