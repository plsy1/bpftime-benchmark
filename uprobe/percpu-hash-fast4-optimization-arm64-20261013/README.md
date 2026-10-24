# Four-byte per-CPU hash optimization on Jetson ARM64

Status: **valid matched A/B; success**

## Configuration

- resulting source commit: `f9cf67f optimize four-byte shared-memory key hashing`
- Jetson mode: `MAXN_SUPER`; CPU5 min/max/current fixed at 1.728 GHz
- GCC 13, `RelWithDebInfo`, LTO on
- probe read/write checks off; compile-time spdlog level INFO
- LLVM VM; five paired rounds
- 20,000 victim invocations and 1,000 warmups per sample
- 1,000 helpers per timed victim invocation

The baseline is the equality-only build. The candidate reads a four-byte key
with `memcpy` into `uint32_t` and returns the integer as its hash, matching the
repository's existing `uint32_hasher`. Other key sizes retain the byte-wise
Boost hash.

## Median net helper cost

| Case | Kernel | Baseline | Candidate | Candidate change |
|---|---:|---:|---:|---:|
| ordinary hash lookup | 25.68 ns | 56.92 ns | 56.93 ns | 0.03% slower |
| ordinary hash update | 91.38 ns | 63.11 ns | 63.12 ns | 0.02% slower |
| per-CPU hash lookup | 26.98 ns | 99.38 ns | 67.14 ns | **32.44% faster** |
| per-CPU hash update | 66.12 ns | 114.47 ns | 79.31 ns | **30.71% faster** |

Ordinary hash is unchanged and serves as the control. The candidate brings
per-CPU lookup to 40.16 ns above kernel and update to 13.19 ns above kernel in
this helper-closure measurement.

## Mechanism and correctness

A stats smoke test used 11,000 successful finds in each build:

| Build | Hash calls | Equality calls | Equality calls/find |
|---|---:|---:|---:|
| equality-only baseline | 11,000 | 14,696 | 1.336 |
| direct four-byte hash | 11,000 | 11,000 | 1.000 |

The gain therefore combines two effects: five serial `boost::hash_combine`
operations are removed, and the benchmark's sequential integer keys no longer
require extra collision traversal. The short stats logs are under `stats/`.

Tests passed for 1/4/16/17/64-byte keys and 1/8/24/257-byte values (305
assertions including the direct-hash checks), the existing ordinary hash suite
(1,232 assertions), LRU maps (4,135 assertions), and subprocess shared-memory
maps (204 assertions).

Changing the hash changes bucket placement for four-byte `bytes_vec` keys.
Processes sharing one map must therefore use the same runtime build; mixed old
and new binaries are not compatible for an already populated shared-memory
table. Map API semantics and stored key/value bytes are unchanged.

`summary.json` is the compact result; `wall-raw.csv` contains all five rounds.

