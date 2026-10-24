# Per-CPU hash equality optimization on Jetson ARM64

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

Baseline is the validated copy-path build
(`build-uprobe-hash-opt-20261012`); candidate is the equality-only build
(`build-uprobe-hash-equal-opt-20261013`). The BPF object and harness are shared.
The source diff, CMake caches, binary hashes, raw rows, frequency samples, and
summary are archived in this directory.

## Median net helper cost

| Case | Kernel | Baseline | Candidate | Candidate change |
|---|---:|---:|---:|---:|
| ordinary hash lookup | 25.83 ns | 56.71 ns | 56.91 ns | 0.35% slower |
| ordinary hash update | 91.72 ns | 63.34 ns | 63.35 ns | 0.02% slower |
| per-CPU hash lookup | 27.04 ns | 130.91 ns | 99.23 ns | **24.20% faster** |
| per-CPU hash update | 66.09 ns | 140.04 ns | 114.32 ns | **18.36% faster** |

Ordinary hash uses `std::equal_to<bytes_vec>` rather than the optimized
`bytes_vec_equal`, so it is an unchanged control. Its sub-0.4% movement supports
attributing the per-CPU improvement to equality comparison rather than a
build-wide shift.

## Change and correctness

- Reject unequal lengths before examining key bytes.
- Compare the common four-byte BPF key as two fixed-width `uint32_t` values.
- Use one `memcmp` for other non-empty key sizes instead of the shared-memory
  vector's element-wise equality path.
- Preserve hash values, bucket selection, collision behavior, and map layout.

Correctness passed for 1/4/17-byte keys and 1/8/24/257-byte values (180
assertions), the existing per-CPU hash suite (1,232 assertions), LRU hash maps
(4,135 assertions), and subprocess shared-memory hash maps (204 assertions).

`summary.json` is the compact result; `wall-raw.csv` contains every round.
