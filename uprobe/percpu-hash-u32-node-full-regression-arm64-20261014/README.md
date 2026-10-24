# Four-byte per-CPU hash inline-key optimization on Jetson ARM64

Status: **success**

## Configuration

- source commit: `77468b3 optimize four-byte per-cpu hash node keys`
- Jetson mode: `MAXN_SUPER`
- CPU5 min/max/current: 1.728 GHz for every recorded sample
- GCC 13, `RelWithDebInfo`, LTO on
- probe read/write checks off; compile-time log level INFO
- five-round matched A/B followed by the complete ten-round uprobe benchmark

Four-byte per-CPU hash keys are stored directly as `uint32_t` in the Boost
shared-memory hash node instead of as a separately allocated `bytes_vec`.
Other key sizes keep the generic representation.

## Matched helper-closure A/B

| Operation | Kernel | Fast4 baseline | Inline-key candidate | Change |
|---|---:|---:|---:|---:|
| ordinary hash lookup | 25.93 ns | 58.72 ns | 57.95 ns | -1.31% |
| ordinary hash update | 91.70 ns | 61.85 ns | 61.53 ns | -0.51% |
| per-CPU hash lookup | 26.69 ns | 67.29 ns | 61.65 ns | **-8.37%** |
| per-CPU hash update | 65.65 ns | 79.57 ns | 71.35 ns | **-10.33%** |

These values are net nanoseconds per helper from the operation-specific
control subtraction. The ordinary hash cases are unchanged controls.

## Complete official benchmark

Values below are mean nanoseconds per victim invocation; each map victim
invocation executes 1,000 helper operations. `Before` is the complete fast4
regression from 2026-10-13. Array delete error-return paths are excluded.

| Valid map operation | State | Kernel | Before | After | This change | After / kernel |
|---|---|---:|---:|---:|---:|---:|
| ordinary array lookup | not optimized | 3,783.14 | 15,953.39 | 16,155.72 | +1.27% | 4.27x |
| ordinary array update | not optimized | 13,088.55 | 23,771.27 | 23,660.76 | -0.46% | 1.81x |
| ordinary hash lookup | not optimized | 28,064.92 | 58,833.58 | 58,012.43 | -1.40% | 2.07x |
| ordinary hash update | faster than kernel | 93,370.94 | 62,992.11 | 61,957.70 | -1.64% | 0.66x |
| ordinary hash delete | faster than kernel | 107,227.09 | 42,267.31 | 42,650.60 | +0.91% | 0.40x |
| per-CPU array lookup | optimized previously | 4,402.76 | 21,380.63 | 21,694.29 | +1.47% | 4.93x |
| per-CPU array update | optimized previously | 16,581.75 | 27,376.60 | 27,650.75 | +1.00% | 1.67x |
| per-CPU hash lookup | optimized again | 28,978.95 | 68,011.14 | 62,870.94 | **-7.56%** | 2.17x |
| per-CPU hash update | optimized again | 67,716.81 | 79,502.18 | 71,505.56 | **-10.06%** | 1.06x |
| per-CPU hash delete | partially improved by representation | 150,663.12 | 923,316.63 | 756,633.29 | **-18.05%** | 5.02x |

The unchanged operations moved between -1.64% and +1.47%. The per-CPU hash
lookup/update gains are larger than that control range and agree with the
matched helper-closure A/B. Delete also benefits because its node no longer
owns and destroys a separate four-byte shared-memory key vector, but the
remaining synchronous value-vector destruction and allocator reclamation are
still the dominant delete target.

## Correctness

- per-CPU hash key/value-size and specialized-key tests: 323 assertions
- existing cross-CPU per-CPU hash suite: 1,232 assertions
- generic kernel-map semantics: 7,907,163 assertions
- cross-process shared-memory lookup/iteration/delete: 304 assertions
- complete benchmark: 17 kernel, 17 userspace, and one embedded-VM case, each
  with ten finite positive samples

Raw data are in `summary.json`, `wall-raw.csv`, and `full/benchmark-output.json`.
