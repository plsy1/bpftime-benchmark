# ARM64 BPFtime map-helper matched top-level comparison

## Technical summary

This experiment adds a unified matched-control comparison of ordinary and per-CPU array/hash lookup and update on Jetson ARM64. The real program and its operation-specific control use the same BPF program shape, 1000-iteration loop, key/value preparation, victim, CPU, frequency, and map state. Five interleaved wall-time runs and three paired PMU rounds confirm the BPFtime-minus-kernel direction and magnitude.

The largest measured gaps are per-CPU hash update (`+162.49 ns/helper`) and per-CPU hash lookup (`+127.04 ns/helper`). Ordinary hash update remains faster in BPFtime (`−39.98 ns/helper`).

This experiment does **not** create a new path-level decomposition. Existing L0–L3 and leaf A/B experiments already located the major BPFtime-internal costs. Values from those standalone diagnostics must not be subtracted from this top-level dataset to manufacture a “production-context residual”; such a remainder is not an independently measured runtime stage.

## Strict matched results

For each engine and operation:

```text
net engine cost = (real case - operation-specific matched control) / 1000
top-level gap   = BPFtime net cost - kernel net cost
```

| Operation | Kernel ns/helper | BPFtime ns/helper | Gap | Extra cycles | Extra instructions |
|---|---:|---:|---:|---:|---:|
| Ordinary array lookup | 1.396 | 12.847 | **+11.452** | +19.76 | +98.04 |
| Ordinary array update | 10.696 | 16.584 | **+5.888** | +9.81 | +66.02 |
| Per-CPU array lookup | 2.008 | 21.739 | **+19.732** | +33.83 | +164.05 |
| Per-CPU array update | 14.178 | 64.353 | **+50.175** | +86.31 | +358.06 |
| Ordinary hash lookup | 25.726 | 53.704 | **+27.978** | +46.95 | +242.46 |
| Ordinary hash update | 91.977 | 52.002 | **−39.975** | −69.74 | −71.02 |
| Per-CPU hash lookup | 26.956 | 153.994 | **+127.039** | +218.07 | +800.83 |
| Per-CPU hash update | 65.767 | 228.254 | **+162.487** | +279.74 | +1134.20 |

The wall-time gap sample standard deviations range from `0.048` to `0.485 ns/helper`. PMU differences have the same direction as wall time. This verifies that the gaps correspond to different executed work rather than timing noise.

## Relationship to the completed path investigation

The earlier investigations already established the following BPFtime-internal cost locations:

- **Ordinary array lookup/update:** the concrete array operation is small; generic map-handler and shared-memory fd/variant dispatch dominate the BPFtime path.
- **Ordinary hash lookup:** production-level and leaf A/B experiments identified the userspace spin lock, TRACE path, generic `memcmp`, and repeated modulo in linear probing. The 4-byte comparison and conditional-wrap variants were also verified in the complete official benchmark.
- **Per-CPU array lookup/update:** CPU selection, `std::function`, value access/copy, and common handler/SHM dispatch account for the principal internal work.
- **Per-CPU hash lookup/update:** Boost.Interprocess vector-key hash/find and per-CPU value-vector handling dominate inside the concrete implementation; common outer runtime layers add further cost.
- **Per-CPU hash delete-hit:** synchronous key/value/node destruction and Boost shared-memory allocator reclamation dominate the measured synthetic delete path.

Those results identify where BPFtime spends time internally. The present matched experiment independently measures how much the complete BPFtime path differs from the complete kernel path. They are complementary evidence, but they are not a single additive timing experiment.

## What this experiment adds

1. All valid ordinary/per-CPU lookup and update cases use the same operation-specific control design.
2. BPFtime and kernel values are paired under the same victim ABI, CPU5, locked frequency, loop, and map state.
3. Cycles and instructions independently confirm every slowdown and the ordinary hash-update speedup.
4. The results provide a stricter top-level comparison table for the existing system reports.

## What it does not establish

1. It does not assign an exact percentage of the top-level BPFtime-minus-kernel gap to each L0–L3 stage.
2. It does not define or measure a separate “production-context residual”.
3. It does not replace the existing production-level hash lookup A/B or per-CPU leaf diagnostics.
4. It does not include array delete, which is unsupported, or corrected hash delete-hit, whose destructive-state protocol is different from lookup/update.

## Current research boundary

The major map-helper cost paths have been located. No additional path splitting is required for the current diagnosis objective. Further work should be treated as a new objective:

- **Benchmark coverage:** add stable lookup-miss, update-insert, and delete-miss cases; or
- **Optimization:** redesign and validate the identified hot paths, followed by full top-level correctness, concurrency, and performance tests.

## Reproduction and sources

- `../helper-map-closure-arm64-20260812/`: strict array wall/PMU raw data and scripts.
- `../helper-hash-closure-arm64-20260812/`: strict hash wall/PMU raw data and scripts.
- `../helper-map-ladder-arm64-20260804/`: matched ordinary helper ladder.
- `../hash-lookup-ablation-arm64-20260804/`: ordinary hash lock/TRACE and L0–L3 diagnostics.
- `../hash-lookup-leaf-ab-arm64-20260804/`: ordinary hash leaf A/B.
- `../hash-lookup-top-optimization-arm64-20260804/`: complete official benchmark validation of safe leaf variants.
- `../percpu-array-path-arm64-20260804/`: per-CPU array L0–L3 diagnostics.
- `../percpu-hash-path-arm64-20260805/`: per-CPU hash L0–L3 and leaf diagnostics.
- `../percpu-hash-delete-layers-arm64-20260805/`: per-CPU hash delete destruction/reclamation diagnosis.
- `analyze.py`: regenerates only the strict `top-gap.csv` table from the new matched raw results.

Environment: Jetson Orin Nano, CPU5, MAXN_SUPER, CPU/GPU/EMC locked, host execution, root loader/victim, LLVM/Clang 15, GCC 13, Boost 1.83 runtime build, BPFtime LLVM JIT and LTO enabled.
