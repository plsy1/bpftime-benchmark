# Hash workload robustness on Jetson ARM64

Date: 2026-08-16

## Technical summary

This experiment tests whether the previously identified BPFtime hash-map costs remain valid when load factor, key size, and value size change. Kernel BPF and BPFtime use the same BPF object for each configuration, with operation-specific controls and 1000 helpers per BPF invocation.

The main findings are:

1. The existing high-load baseline reproduces within 3.34% of the prior strict matched experiment.
2. Increasing load from 64 to 1000 keys increases ordinary BPFtime lookup by 14.88 ns/helper and its PMU count by about 81 instructions/helper. This supports the existing probing/collision attribution.
3. Increasing key size from 4B to 64B adds about 116.64 ns to ordinary BPFtime lookup and about 412.08 ns to per-CPU BPFtime lookup. Lookup and update have almost identical BPFtime key-size slopes, confirming that their common key-copy/hash/equality/container path is responsible.
4. Increasing value size from 8B to 256B leaves lookup nearly unchanged, but adds 800.29 ns/helper to BPFtime per-CPU hash update, versus only 19.20 ns for kernel BPF. PMU records about 4487 additional BPFtime instructions/helper. This directly supports the shared-memory value-vector copy attribution.

These results make the earlier source-level diagnosis more robust. They also show an additional semantic boundary: ordinary hash update is faster in BPFtime at 4B and 16B keys, but becomes slower at 64B keys.

## Scope and metric

Each configuration contains two maps with identical dimensions:

- ordinary `BPF_MAP_TYPE_HASH`;
- `BPF_MAP_TYPE_PERCPU_HASH`.

The measured cases are stable lookup-hit and update-existing:

```text
hash_lookup
hash_update
percpu_hash_lookup
percpu_hash_update
```

State setup is outside the timed region. Each real case has a control with the same 1000-iteration loop and the same key/value initialization. Net cost is:

```text
net ns/helper = (real ns/invocation - control ns/invocation) / 1000
```

Tables below use five-run means. Raw values, medians, sample standard deviations, min/max, and paired gaps are in the CSV files.

## Environment and quality checks

- Jetson Orin Nano, ARM64 Cortex-A78AE;
- CPU5 pinned at 1,728,000 kHz for every wall-time victim;
- MAXN_SUPER and `jetson_clocks`;
- Linux `6.8.12-1021-tegra`;
- GCC 13.3.0, LLVM/Clang 15.0.7;
- source commit `1a1dc0d6c2026a5e4b0e4faa964e3eea8ccdd241`;
- five independent wall-time victims per engine/configuration;
- three paired PMU rounds per representative endpoint;
- 80 complete wall-time victims and 336 PMU event files;
- maximum wall-time CV: 3.52%; almost all cases are below 2%;
- final kernel link count: 0;
- final BPFtime shared-memory state: removed.

## Baseline reproduction

The `lf1000` configuration is the existing 1000-key, 1024-entry, 4B-key, 8B-value baseline.

| Case | Prior kernel | Current kernel | Difference | Prior BPFtime | Current BPFtime | Difference |
|---|---:|---:|---:|---:|---:|---:|
| ordinary hash lookup | 25.726 | 26.210 | +1.88% | 53.704 | 53.867 | +0.30% |
| ordinary hash update | 91.977 | 89.981 | -2.17% | 52.002 | 52.529 | +1.01% |
| per-CPU hash lookup | 26.956 | 26.057 | -3.34% | 153.994 | 157.045 | +1.98% |
| per-CPU hash update | 65.767 | 66.072 | +0.46% | 228.254 | 232.613 | +1.91% |

Units are ns/helper. All directions reproduce and all values remain within the 10% acceptance threshold.

## Load-factor sweep

Fixed dimensions: 1024 max entries, 4B key, 8B value.

| Active keys | Operation | Kernel | BPFtime | BPFtime − kernel |
|---:|---|---:|---:|---:|
| 64 | ordinary lookup | 16.88 | 38.99 | +22.11 |
| 256 | ordinary lookup | 18.94 | 40.21 | +21.27 |
| 512 | ordinary lookup | 20.27 | 43.89 | +23.62 |
| 1000 | ordinary lookup | 26.21 | 53.87 | +27.66 |
| 64 | ordinary update | 84.11 | 37.40 | -46.71 |
| 256 | ordinary update | 85.52 | 37.91 | -47.61 |
| 512 | ordinary update | 86.80 | 41.87 | -44.93 |
| 1000 | ordinary update | 89.98 | 52.53 | -37.45 |
| 64 | per-CPU lookup | 17.98 | 145.75 | +127.77 |
| 256 | per-CPU lookup | 19.26 | 148.36 | +129.10 |
| 512 | per-CPU lookup | 21.68 | 153.26 | +131.59 |
| 1000 | per-CPU lookup | 26.06 | 157.04 | +130.99 |
| 64 | per-CPU update | 62.34 | 220.68 | +158.34 |
| 256 | per-CPU update | 62.45 | 221.19 | +158.73 |
| 512 | per-CPU update | 62.76 | 227.27 | +164.51 |
| 1000 | per-CPU update | 66.07 | 232.61 | +166.54 |

From 64 to 1000 keys:

| Operation | Kernel increase | BPFtime increase | Gap increase |
|---|---:|---:|---:|
| ordinary lookup | +9.33 | +14.88 | +5.55 |
| ordinary update | +5.87 | +15.13 | +9.26 |
| per-CPU lookup | +8.07 | +11.29 | +3.22 |
| per-CPU update | +3.73 | +11.93 | +8.21 |

Ordinary BPFtime lookup instructions rise from 285.99 to 367.02 instructions/helper, while kernel rises only from 119.10 to 121.28. This is consistent with more work in BPFtime's open-addressed linear-probing path. Per-CPU BPFtime lookup instructions rise only from 920.41 to 930.31, so load factor is not the main source of its large fixed Boost container cost.

## Key-size sweep

Fixed dimensions: 512 active keys, 1024 max entries, 8B value.

| Key size | Operation | Kernel | BPFtime | BPFtime − kernel |
|---:|---|---:|---:|---:|
| 4B | ordinary lookup | 20.27 | 43.89 | +23.62 |
| 16B | ordinary lookup | 28.42 | 77.69 | +49.27 |
| 64B | ordinary lookup | 65.67 | 160.52 | +94.85 |
| 4B | ordinary update | 86.80 | 41.87 | -44.93 |
| 16B | ordinary update | 92.63 | 75.82 | -16.81 |
| 64B | ordinary update | 130.58 | 159.60 | +29.02 |
| 4B | per-CPU lookup | 21.68 | 153.26 | +131.59 |
| 16B | per-CPU lookup | 28.58 | 234.43 | +205.84 |
| 64B | per-CPU lookup | 64.96 | 565.34 | +500.38 |
| 4B | per-CPU update | 62.76 | 227.27 | +164.51 |
| 16B | per-CPU update | 69.08 | 310.31 | +241.23 |
| 64B | per-CPU update | 105.71 | 638.34 | +532.63 |

From 4B to 64B keys:

| Operation | Kernel increase | BPFtime increase | BPFtime slope |
|---|---:|---:|---:|
| ordinary lookup | +45.41 ns | +116.64 ns | 1.944 ns/additional byte |
| ordinary update | +43.79 ns | +117.73 ns | 1.962 ns/additional byte |
| per-CPU lookup | +43.29 ns | +412.08 ns | 6.868 ns/additional byte |
| per-CPU update | +42.95 ns | +411.07 ns | 6.851 ns/additional byte |

The near-identical lookup/update slopes within each BPFtime map type are the key result. They indicate a common key-processing path rather than an update-only value-copy effect.

PMU confirms the wall-time result:

| Operation | Kernel instructions, 4B → 64B | BPFtime instructions, 4B → 64B |
|---|---:|---:|
| ordinary lookup | 120.46 → 285.46 | 307.61 → 693.50 |
| ordinary update | 389.02 → 555.99 | 277.60 → 658.50 |
| per-CPU lookup | 125.29 → 290.38 | 931.17 → 3079.80 |
| per-CPU update | 272.36 → 437.45 | 1415.41 → 3559.31 |

For per-CPU hash, the additional 60 key bytes add about 2144–2149 instructions/helper in both lookup and update. This matches the shared `key_vec.assign()` plus byte-wise Boost hash/equality/container path identified by the leaf A/B experiments.

## Value-size sweep

Fixed dimensions: 512 active keys, 1024 max entries, 4B key.

| Value size | Operation | Kernel | BPFtime | BPFtime − kernel |
|---:|---|---:|---:|---:|
| 8B | ordinary lookup | 20.27 | 43.89 | +23.62 |
| 64B | ordinary lookup | 21.13 | 43.99 | +22.86 |
| 256B | ordinary lookup | 21.98 | 44.64 | +22.66 |
| 8B | ordinary update | 86.80 | 41.87 | -44.93 |
| 64B | ordinary update | 87.33 | 39.68 | -47.65 |
| 256B | ordinary update | 111.07 | 49.11 | -61.96 |
| 8B | per-CPU lookup | 21.68 | 153.26 | +131.59 |
| 64B | per-CPU lookup | 21.83 | 154.26 | +132.42 |
| 256B | per-CPU lookup | 21.31 | 153.54 | +132.23 |
| 8B | per-CPU update | 62.76 | 227.27 | +164.51 |
| 64B | per-CPU update | 61.96 | 403.30 | +341.34 |
| 256B | per-CPU update | 81.96 | 1027.56 | +945.60 |

Lookup is almost independent of value size in both engines. From 8B to 256B:

- ordinary BPFtime update increases only 7.24 ns and remains faster than kernel;
- per-CPU BPFtime update increases 800.29 ns, or about 3.227 ns per additional value byte;
- kernel per-CPU update increases only 19.20 ns;
- the BPFtime−kernel per-CPU update gap increases by 781.09 ns.

PMU gives the same direction:

| Per-CPU update | 8B value | 256B value | Increase |
|---|---:|---:|---:|
| Kernel cycles/helper | 109.87 | 141.21 | +31.34 |
| BPFtime cycles/helper | 392.60 | 1772.94 | +1380.33 |
| Kernel instructions/helper | 272.36 | 311.52 | +39.15 |
| BPFtime instructions/helper | 1415.41 | 5902.38 | +4486.97 |

This confirms that the expensive BPFtime per-CPU update path is tied to shared-memory value-vector/template access and copy, not generic helper dispatch alone.

## Relation to the existing source diagnosis

| Existing diagnosis | Robustness evidence | Status |
|---|---|---|
| Ordinary hash lookup is dominated by generic compare and linear probing at high load | Load raises BPFtime instructions and time; long keys raise lookup/update with a shared slope | Strengthened |
| Per-CPU hash find contains expensive Boost key/hash/equality/container work | 4B→64B adds about 412 ns and 2145 instructions to both lookup and update | Strengthened |
| Per-CPU hash update has an expensive value-vector copy | 8B→256B adds about 800 ns and 4487 instructions only to BPFtime update; lookup remains flat | Strongly confirmed |
| Ordinary hash update is faster in BPFtime | True for 4B/16B keys and all tested value sizes, but false at 64B keys | Scope restricted |
| Per-CPU hash's large base cost is mainly caused by high load | Load changes it only modestly | Rejected as primary explanation |

## Limitations

1. Key structures use an integer ID followed by zero padding. Increasing key size therefore changes both byte-processing length and hash/bucket distribution; the measured slope is a complete helper-path effect, not a pure isolated hash-loop cost.
2. PMU wraps the full victim process. Setup and warm-up are identical in each adjacent real/control pair and are subtracted, but PMU values remain process-level paired estimates.
3. The experiment covers lookup-hit and update-existing only. Lookup-miss, update-insert, and delete-miss remain separate semantic work.
4. Only Jetson ARM64 was tested. The goal is robustness of the existing Jetson source attribution, not a new cross-architecture comparison.
5. This is diagnostic code and does not modify the production runtime algorithm.

## Files

- `configs.csv`: complete wall-time configuration matrix;
- `pmu-endpoints.csv`: selected PMU endpoints and reasons;
- `run-wall.sh`: exact build and wall-time execution procedure;
- `run-pmu.sh`: exact paired PMU procedure;
- `analyze.py`: regenerates all derived CSV files;
- `environment.txt`: source, binaries, CPU/frequency and cleanup state;
- `wall-raw.csv`, `wall-summary.csv`, `gaps.csv`, `percpu-specific.csv`;
- `pmu-raw.csv`, `pmu-summary.csv`, `pmu-gaps.csv`;
- `raw/wall/` and `raw/pmu/`: complete victim, loader, time, frequency and perf output.
