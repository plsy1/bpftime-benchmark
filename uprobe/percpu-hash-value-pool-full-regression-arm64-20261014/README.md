# Four-byte per-CPU hash value-pool optimization on Jetson ARM64

Status: **success**

## Configuration

- source commit: `69cb4dc reuse four-byte per-cpu hash value storage`
- Jetson mode: `MAXN_SUPER`
- CPU5 min/max/current: 1.728 GHz for all 31 recorded samples
- GCC 13, `RelWithDebInfo`, LTO on
- probe read/write checks off; compile-time log level INFO
- five-round matched A/B followed by the complete ten-round uprobe benchmark
- complete official benchmark duration: 2,176.31 seconds

For four-byte per-CPU hash keys, each Boost hash node now contains a compact
shared-memory pointer and slot number instead of owning a separately allocated
`bytes_vec` value. Values live in incrementally allocated chunks. Delete
returns the slot to an allocation-free free list, and a later insertion clears
and reuses it. Declaring a large `max_entries` does not allocate the whole pool
up front.

## Matched helper-closure A/B

| Operation | Kernel | Fast4 baseline | Value-pool candidate | Change |
|---|---:|---:|---:|---:|
| ordinary hash lookup | 25.94 ns | 56.91 ns | 54.30 ns | -4.58% |
| ordinary hash update | 90.84 ns | 63.30 ns | 61.58 ns | -2.71% |
| per-CPU hash lookup | 26.76 ns | 67.30 ns | 60.94 ns | **-9.45%** |
| per-CPU hash update | 65.83 ns | 79.49 ns | 70.75 ns | **-10.99%** |

These are median net nanoseconds per helper after subtracting the
operation-specific control. The archived A/B baseline predates the inline-key
commit, so its per-CPU lookup/update changes are cumulative across the inline
key and value-pool commits. The official table below includes both the fixed
pre-optimization baseline and the immediately preceding inline-key run, so it
shows cumulative progress and this commit's regression check separately.

## Complete official benchmark

Values are mean nanoseconds per victim invocation; each map victim invocation
executes 1,000 helpers. `Initial` is the last complete, strict, unoptimized
environment-recovery run at commit `1a1dc0d` on 2026-10-07. `Previous` is the
immediately preceding inline-key run at commit `77468b3`; it is retained to
detect regressions introduced by this commit. Array delete error-return paths
are excluded.

| Valid map operation | State | Kernel | Initial | Previous | Current | Initial to current | Previous to current | Current / kernel |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| ordinary array lookup | not optimized | 3,810.31 | 15,937.20 | 16,155.72 | 15,928.92 | -0.05% | -1.40% | 4.18x |
| ordinary array update | not optimized | 13,091.42 | 23,509.55 | 23,660.76 | 23,475.62 | -0.14% | -0.78% | 1.79x |
| ordinary hash lookup | not optimized | 28,666.74 | 58,472.54 | 58,012.43 | 55,845.20 | -4.49% | -3.74% | 1.95x |
| ordinary hash update | faster than kernel | 93,798.09 | 63,182.38 | 61,957.70 | 61,894.21 | -2.04% | -0.10% | 0.66x |
| ordinary hash delete | faster than kernel | 107,969.58 | 42,638.57 | 42,650.60 | 42,327.04 | -0.73% | -0.76% | 0.39x |
| per-CPU array lookup | optimized previously | 4,402.29 | 27,727.95 | 21,694.29 | 21,481.07 | **-22.53%** | -0.98% | 4.88x |
| per-CPU array update | optimized previously | 16,629.04 | 75,928.93 | 27,650.75 | 27,369.25 | **-63.95%** | -1.02% | 1.65x |
| per-CPU hash lookup | optimized | 29,740.74 | 141,245.11 | 62,870.94 | 61,758.02 | **-56.28%** | -1.77% | 2.08x |
| per-CPU hash update | nearly at kernel | 67,948.49 | 220,961.13 | 71,505.56 | 70,281.26 | **-68.19%** | -1.71% | 1.03x |
| per-CPU hash delete | value reclamation optimized | 152,351.54 | 986,684.05 | 756,633.29 | 363,364.38 | **-63.17%** | **-51.98%** | **2.39x** |

This commit's intended delete path accounts for about 393 ns/helper of the
previous-to-current reduction, matching the independently measured direct-path
result. Across the complete optimization series, delete is down by about 623
ns/helper. Lookup and update did not regress against the immediately preceding
run. Delete remains about 211 ns/helper slower than the kernel; the remaining
large costs are the Boost node unlink/deallocation and the common BPFtime
helper/handler path, not per-entry value-vector reclamation.

## Correctness

- per-CPU hash specialized key, arbitrary size, value-pool reuse, stale-data,
  memory high-water, and chunk-growth tests: 4,974 assertions
- existing cross-CPU per-CPU hash suite: 1,232 assertions
- generic kernel-map semantics: 7,954,602 assertions
- cross-process shared-memory lookup/iteration/delete: 304 assertions
- complete benchmark: 17 kernel, 17 userspace, and one embedded-VM case, each
  with ten finite positive samples

Raw data are in `summary.json`, `wall-raw.csv`, and
`full/benchmark-output.json`.
