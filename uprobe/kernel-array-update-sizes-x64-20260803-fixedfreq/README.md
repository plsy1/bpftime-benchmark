# x64 kernel array-update value-size sweep

## Conclusion

This experiment repeats the Jetson ordinary kernel-BPF array
`update-existing` value-size sweep on the fixed-frequency x64 host using the
same BPF source and run parameters.

The original 8B cross-architecture gap is dominated by a fixed x64 path cost,
not by copy work that grows with value length:

- At 8B, x64 is 22.996 ns/helper and 54.647 cycles/helper above ARM64.
- From 8B to 256B, x64 grows by only 11.982 ns and 27.602 cycles, versus
  16.485 ns and 28.262 cycles on ARM64.
- From 64B to 256B, x64 grows by 9.067 ns and 17.923 cycles, while ARM64 grows
  by 15.051 ns and 25.588 cycles. The x64 disadvantage therefore does not grow
  with the long-copy region; it shrinks to 18.494 ns at 256B.
- LLC misses remain effectively zero on x64. This is not a cache-capacity or
  cold-memory effect.

The result rules out payload-length-dependent memcpy cost as the main cause of
the original 8B gap. It does not, by itself, separate the fixed cost of the
out-of-line `bpf_obj_memcpy` call from the rest of `array_map_update_elem` and
its helper wrapper. The earlier x64 disassembly showed that live path calling
`bpf_obj_memcpy` and `bpf_obj_free_fields`; a narrower helper-internal profile
or matched kernel instrumentation is still needed to attribute the fixed
roughly 55-cycle excess between those components.

## Formal Results

Net wall time is:

```text
(real ns/program - control ns/program) / 1000 helpers/program
```

Net hardware counters use the same subtraction per profiled BPF run:

```text
(real counter/run_cnt - control counter/run_cnt) / 1000 helpers/program
```

| Value | x64 ns/helper | x64 cycles/helper | x64 instructions/helper | L1D loads/helper | LLC misses/helper |
|---:|---:|---:|---:|---:|---:|
| 8B | 31.974 | 70.168 | 104.153 | 25.037 | 0.000005 |
| 16B | 32.863 | 69.231 | 106.160 | 27.039 | 0.000012 |
| 32B | 37.627 | 76.767 | 121.686 | 27.043 | 0.000031 |
| 64B | 34.890 | 79.847 | 139.181 | 32.043 | 0.000002 |
| 128B | 38.744 | 83.078 | 159.192 | 40.046 | 0.000007 |
| 256B | 43.957 | 97.769 | 211.223 | 56.055 | 0.000007 |

The 32B wall-time step is repeatable within its five wall rounds, but wall time
falls again at 64B. Static JIT size also changes by threshold, and PMU profiles
were separate executions, so this non-monotonic point should not be treated as
a linear memcpy slope. The 64B-to-256B interval is the more useful long-copy
comparison.

## ARM64 Comparison

| Value | ARM ns | x64 ns | x64-ARM ns | ARM cycles | x64 cycles | x64-ARM cycles | ARM instr. | x64 instr. | x64-ARM instr. |
|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| 8B | 8.978 | 31.974 | 22.996 | 15.521 | 70.168 | 54.647 | 69.016 | 104.153 | 35.136 |
| 16B | 9.212 | 32.863 | 23.651 | 16.157 | 69.231 | 53.074 | 67.014 | 106.160 | 39.147 |
| 32B | 9.411 | 37.627 | 28.216 | 16.247 | 76.767 | 60.520 | 68.051 | 121.686 | 53.635 |
| 64B | 10.412 | 34.890 | 24.477 | 18.195 | 79.847 | 61.652 | 72.021 | 139.181 | 67.160 |
| 128B | 13.831 | 38.744 | 24.913 | 23.767 | 83.078 | 59.311 | 82.017 | 159.192 | 77.175 |
| 256B | 25.463 | 43.957 | 18.494 | 43.783 | 97.769 | 53.986 | 108.013 | 211.223 | 103.210 |

The 8B-to-32B x64-minus-ARM gap is approximately fixed at 23.0-28.2 ns and
53.1-60.5 cycles, with the 32B threshold as the outlier. x64 executes
increasingly more instructions as values grow, but its incremental cycles are
not worse than ARM64. This points to different copy implementation and
instruction throughput, while the absolute short-value disadvantage remains a
fixed helper/map-path issue.

## Run Conditions

- Source: `codex/official-no-btf` at
  `176eb291ead95eb5f8a56280deae626fac46eaa9`, clean before and after the run.
- Intel Core i7-8750H, physical CPU5; SMT sibling CPU11 offline during capture.
- `intel_pstate` active, all governors `performance`, turbo disabled, and
  `min_perf_pct=max_perf_pct=100` during capture.
- Every wall/PMU turbostat record reports CPU5 `Bzy_MHz=2200`.
- GCC 13.4 host compiler; LLVM/Clang 15.0.7 BPF compiler from LLVM commit
  `8dfdcc7b7bf66834a761bd8de445840ef68e4d1a`.
- The Jetson host compiler was GCC 13.3; the BPF compiler and BPF source commit
  match exactly.
- Loader and victim ran as root and were pinned to CPU5.
- 20,000 invocations x 5 rounds, plus 1,000 warm-up invocations.
- Each real BPF invocation performs 1,000
  `bpf_map_update_elem(..., BPF_ANY)` calls.
- Odd rounds are control-first; even rounds are real-first.
- PMU metrics were collected separately, one value-size control/real pair per
  loader run.

## Validation

- All 48 profile JSON files have `run_cnt=101000`.
- Every JSON has `enabled == running`; there was no PMU multiplexing.
- All 25 turbostat files report CPU5 `Bzy_MHz=2200`.
- All stderr files are empty.
- After capture, CPU11, governor, turbo, and pstate limits were restored.
- Final `kernel.bpf_stats_enabled=0` and BPF link count is zero.

## Files

- `raw-wall/`: five alternating wall-time rounds for all six sizes.
- `raw-profile-<metric>-pairwise/`: JSON, xlated/JIT dumps, raw CSV, and
  turbostat output for each isolated pair.
- `wall-summary.csv`: wall-time distribution statistics.
- `profile-summary.csv`: per-program and net per-helper PMU values.
- `combined-summary.csv`: x64 wall time, PMU values, and frequency closure.
- `comparison-arm64.csv`: absolute x64/ARM64 values and differences.
- `jit-static-summary.csv`: static control/real xlated and JIT instruction
  counts.
- `environment.txt`: commits, toolchain, CPU, frequency, hashes, and cleanup
  state.
- `run-*.sh`, `profile-x64.sh`, and `summarize.py`: exact reproduction and
  summarization scripts.
