# Four-byte per-CPU hash node-pool optimization on Jetson ARM64

Status: **success**

## Configuration

- source base: `69cb4dc reuse four-byte per-cpu hash value storage`, plus
  the exact runtime patch archived in `source.diff`
- Jetson mode: `MAXN_SUPER`
- CPU5 min/max/current: 1.728 GHz for all 31 recorded samples
- GCC 13, `RelWithDebInfo`, LTO on
- probe read/write checks off; compile-time log level INFO
- five-round matched A/B followed by the complete ten-round uprobe benchmark
- experiment duration: 1,783.40 seconds

For four-byte per-CPU hash keys, delete now extracts the Boost hash node into
a bounded shared-memory reuse pool instead of unlinking and deallocating it.
A later insertion changes the extracted node's key and value reference and
relinks the same allocation. The existing value-slot pool is still responsible
for clearing and recycling the per-CPU value storage. This gives the timed
delete/setup cycle the same broad preallocation/free-list shape as the default
kernel BPF hash implementation while preserving map size and iteration
semantics.

The direct production delete path fell from approximately 329 ns/helper to
97 ns/helper before the complete benchmark was started.

## Matched helper-closure A/B

| Operation | Kernel | Previous build | Node-pool candidate | Change |
|---|---:|---:|---:|---:|
| ordinary hash lookup | 25.81 ns | 54.00 ns | 56.01 ns | +3.72% |
| ordinary hash update | 92.04 ns | 61.67 ns | 61.56 ns | -0.18% |
| per-CPU hash lookup | 26.75 ns | 60.99 ns | 61.00 ns | +0.02% |
| per-CPU hash update | 65.85 ns | 70.76 ns | 72.38 ns | +2.28% |

These are median net nanoseconds per helper after subtracting the
operation-specific control. The matched closure does not contain a delete
case. Lookup is unchanged. Existing-key per-CPU update shows a small 2.28%
regression; the complete benchmark below also sees a 3.23% increase, so this
should be retained as a measured tradeoff rather than dismissed as noise.

## Complete official benchmark

Values are mean nanoseconds per victim invocation; each map victim invocation
executes 1,000 helpers. `Initial` is the fixed strict unoptimized recovery run
at commit `1a1dc0d` on 2026-10-07. `Previous` is the immediately preceding
value-pool run at commit `69cb4dc`. Array delete error-return paths are
excluded.

| Valid map operation | State | Kernel | Initial | Previous | Current | Initial to current | Previous to current | Current / kernel |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| ordinary array lookup | not optimized | 3,788.51 | 15,937.20 | 15,928.92 | 15,865.79 | -0.45% | -0.40% | 4.19x |
| ordinary array update | not optimized | 13,018.22 | 23,509.55 | 23,475.62 | 23,485.23 | -0.10% | +0.04% | 1.80x |
| ordinary hash lookup | not optimized | 28,081.77 | 58,472.54 | 55,845.20 | 57,291.78 | -2.02% | +2.59% | 2.04x |
| ordinary hash update | faster than kernel | 93,662.73 | 63,182.38 | 61,894.21 | 62,651.38 | -0.84% | +1.22% | 0.67x |
| ordinary hash delete | faster than kernel | 108,045.44 | 42,638.57 | 42,327.04 | 42,394.23 | -0.57% | +0.16% | 0.39x |
| per-CPU array lookup | optimized previously | 4,221.21 | 27,727.95 | 21,481.07 | 21,341.24 | **-23.03%** | -0.65% | 5.06x |
| per-CPU array update | optimized previously | 16,641.32 | 75,928.93 | 27,369.25 | 27,087.14 | **-64.33%** | -1.03% | 1.63x |
| per-CPU hash lookup | optimized | 28,721.77 | 141,245.11 | 61,758.02 | 62,094.96 | **-56.04%** | +0.55% | 2.16x |
| per-CPU hash update | near kernel | 67,999.35 | 220,961.13 | 70,281.26 | 72,552.41 | **-67.17%** | **+3.23%** | 1.07x |
| per-CPU hash delete | node reclamation optimized | 152,153.81 | 986,684.05 | 363,364.38 | 109,645.98 | **-88.89%** | **-69.82%** | **0.72x** |

The delete improvement is about 254 ns/helper relative to the previous build
and 877 ns/helper across the full optimization series. The current result is
about 42.5 ns/helper, or 27.9%, faster than the same-run kernel result. Its ten
samples are also tight: 109,539.84 to 109,861.56 ns per invocation with a
120.18 ns standard deviation.

The node pool therefore closes the remaining large delete gap. The main
follow-up from this change is the small per-CPU hash update regression, not
additional delete-path allocator work.

## Correctness

- per-CPU hash specialized key, arbitrary size, value-slot reuse, stale-data,
  memory high-water, and chunk-growth tests: 4,974 assertions
- generic kernel-map semantics: 7,896,944 assertions
- cross-process shared-memory delete, node reuse, reinsertion, lookup, and
  destruction: 304 assertions
- complete benchmark: 17 kernel, 17 userspace, and one embedded-VM case, each
  with ten finite positive samples

Raw data are in `summary.json`, `wall-raw.csv`, and
`full/benchmark-output.json`.
