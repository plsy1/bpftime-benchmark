# Fixed-frequency ARM64 versus x64 official uprobe benchmark

## Scope

This is the definitive locked-frequency comparison of the unmodified official
`benchmark/uprobe` top-level victim at BPFtime commit
`8ed291e130fe3f15f99955b0d259eb119efdaa7d`:

- ARM64 source: `../uprobe-top-arm64-20260803` on Jetson Orin Nano, CPU5 at 1.728 GHz.
- x64 source: `../uprobe-top-x64-20260803-fixedfreq` on Intel i7-8750H, CPU5 at 2.2 GHz.

Both runs used one pinned victim thread, 100000 iterations, five independent
processes per environment, LTO ON, LLVM JIT ON, and probe read/write checks
OFF. The x64 SMT sibling was offline; CPU5 on the Jetson has no SMT sibling.

The Jetson result's original `comparison-x64.csv` used the earlier
`uprobe-top-x64-20260802` archive. This combined report instead uses the newer
hard-fixed x64 archive. Neither source archive is modified.

## Absolute result

Values are medians of five paired, same-process measurements:

`net ns/helper = (map case ns/invocation - __bench_uprobe ns/invocation) / 1000`

| Operation | ARM64 kernel | x64 kernel | ARM−x64 | ARM64 BPFtime | x64 BPFtime | ARM−x64 |
|---|---:|---:|---:|---:|---:|---:|
| array lookup | 2.563 | 2.757 | -0.194 | 12.901 | 13.393 | -0.492 |
| array update-existing | 11.858 | 34.132 | -22.274 | 16.426 | 18.547 | -2.121 |
| hash lookup-hit | 27.808 | 49.948 | -22.141 | 54.608 | 77.501 | -22.892 |
| hash update-existing | 93.342 | 129.108 | -35.766 | 51.647 | 83.218 | -31.571 |

Every measured BPFtime net path is lower in absolute ns/helper on ARM64 than
on x64. The top-level data therefore do not support the claim that BPFtime's
userspace map path is generally heavier on ARM64.

## Winner and driver

| Operation | ARM64 BPFtime/kernel | x64 BPFtime/kernel | ARM64 winner | x64 winner | Main ratio-shift driver |
|---|---:|---:|---|---|---|
| array lookup | 5.033x | 4.857x | kernel | kernel | small; kernel denominator |
| array update-existing | 1.385x | 0.543x | kernel | BPFtime | kernel denominator |
| hash lookup-hit | 1.964x | 1.552x | kernel | kernel | kernel denominator by relative reduction |
| hash update-existing | 0.553x | 0.645x | BPFtime | BPFtime | BPFtime numerator by relative reduction |

Array update is the only winner reversal. ARM64 kernel is 22.274 ns/helper
lower than x64, a 65.3% reduction, while ARM64 BPFtime is only 2.121 ns/helper
lower, an 11.4% reduction. The reversal is therefore overwhelmingly caused
by the kernel denominator, specifically the unusually cheap Jetson kernel
array-update path.

For hash lookup, both ARM64 paths are about 22 ns/helper lower in absolute
time. The kernel reduction is larger proportionally (44.3% versus 29.5%), so
the BPFtime/kernel ratio rises on ARM64 even though BPFtime itself is
absolutely cheaper there.

For hash update, both architectures favor BPFtime. ARM64 BPFtime falls 37.9%
relative to x64, compared with a 27.7% reduction for kernel, strengthening the
BPFtime advantage on ARM64. Array lookup is nearly architecture-neutral in
absolute time and keeps the same winner.

## Interpretation limits

These are direct elapsed-time results from different CPUs, fixed at their own
platform frequencies. The official victim deliberately remains unmodified
and runs all 17 cases sequentially, so it does not provide attributable
per-case hardware counters. Actual cycles/helper and instructions/helper must
come from the dedicated JIT helper A/B and direct-layer diagnostics; they
should not be inferred by multiplying these ns values by nominal frequency.

The top-level result establishes which end-to-end path wins on each host and
which numerator or denominator drives the cross-platform ratio. It does not,
by itself, identify the microarchitectural cause of the x64 hash-map body
cost; that remains the role of the hash L0 perf and H0-H5 investigation.

## Files

- `comparison.csv`: exact absolute medians, differences, ratios, winners, and drivers.
- `../uprobe-top-arm64-20260803/`: Jetson raw data, environment, and validation.
- `../uprobe-top-x64-20260803-fixedfreq/`: x64 raw data, turbostat evidence, and validation.
