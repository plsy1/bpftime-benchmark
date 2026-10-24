# Exact-output four-byte hash-loop unrolling on Jetson ARM64

Status: **valid matched A/B; diagnostic result, not selected as the final hash optimization**

## Configuration

- `MAXN_SUPER` with CPU5 min/max/current fixed at 1.728 GHz
- GCC 13, `RelWithDebInfo`, LTO on
- probe read/write checks off; compile-time spdlog level INFO
- five paired rounds; 20,000 victim invocations and 1,000 warmups
- 1,000 map helpers per timed victim invocation

Baseline is the equality-only build (`build-uprobe-hash-equal-opt-20261013`).
Candidate is `build-uprobe-hash-key-opt-20261013`, which preserves the legacy
Boost hash value exactly but spells out the four byte-combine calls instead of
using a counted loop.

## Median net helper cost

| Case | Baseline | Candidate | Candidate change |
|---|---:|---:|---:|
| ordinary hash lookup | 58.706 ns | 56.906 ns | 3.07% faster |
| ordinary hash update | 62.017 ns | 63.165 ns | 1.85% slower |
| per-CPU hash lookup | 99.089 ns | 98.905 ns | 0.19% faster |
| per-CPU hash update | 113.965 ns | 112.357 ns | 1.41% faster |

The per-CPU effect is only 0.18--1.61 ns/helper by median. Ordinary controls
also move in opposite directions. The safe conclusion is that loop control is
not the previously attributed 13--15 ns hash cost; the serial
`boost::hash_combine` operations themselves dominate it. This exact-output
variant is retained as an auditable intermediate result but is too small and
inconsistent to count as the completed hash optimization.

`summary.json` contains the compact statistics and `wall-raw.csv` contains all
rounds. `source.diff` records the exact candidate used for this run.
