# x64 kernel map runtime A/B results (2026-08-02)

## Result

This run does not support a broad claim that ordinary x64 kernel BPF map
paths are heavier than the ARM64 baseline. Only array update-existing was
heavier on x64. Hash lookup-hit was approximately equal, while array
lookup-hit and hash update-existing were lighter.

| Operation | x64 mean ns/helper | Sample stddev | CV | ARM64 ns/helper | x64 / ARM64 |
| --- | ---: | ---: | ---: | ---: | ---: |
| array lookup-hit | 0.515357 | 0.028233 | 5.478% | 1.384079 | 0.372346 |
| array update-existing | 18.791241 | 0.160475 | 0.854% | 10.951741 | 1.715822 |
| hash lookup-hit | 25.919897 | 0.387134 | 1.494% | 26.768945 | 0.968282 |
| hash update-existing | 75.992031 | 5.136293 | 6.759% | 90.822847 | 0.836706 |

The x64 array update path was 71.6% heavier than ARM64. The other x64
paths were 3.2% to 62.8% lighter. These ratios compare two controlled hosts
with different CPUs, kernels, and configurations; they must not be treated
as an isolated ISA effect.

## Method

- Repository commit: `8ed291e130fe3f15f99955b0d259eb119efdaa7d`
- Branch: `codex/official-no-btf`
- Host: Intel Core i7-8750H, Ubuntu 26.04, kernel `7.0.0-27-generic`
- Selected logical CPU: 5; SMT sibling: 11
- SMT: enabled
- CPU governor: `powersave`
- Turbo: enabled (`intel_pstate/no_turbo=0`)
- BPF compiler: upstream Clang 15.0.7, commit `8dfdcc7b7bf6`
- Host compiler: GCC 12.5.0
- Parameters: 20,000 invocations per round, 5 rounds, 1,000 helpers per
  invocation, and 1,000 warm-up invocations

The metric is calculated from kernel BPF runtime counters:

```text
net ns/helper =
  (real run_time_ns/run_cnt - control run_time_ns/run_cnt) / 1000
```

Control and real execution order alternates on odd and even rounds. Maps are
primed before every measurement. Keys 0 through 999 are present before the
BPF programs use the same keys, so lookups are hits and updates operate on
existing entries.

## Validation

- The raw CSV contains 60 data rows plus one header.
- Each of the four operations has five `real-minus-control` samples.
- All 40 control/real program samples have `run_cnt=20000`.
- The summary script independently recomputes every reported delta from its
  control and real rows before calculating statistics.
- `kernel.bpf_stats_enabled` was restored from 1 to its original value of 0.
- No Docker or process wall-time metric was used.
- No diagnostic program semantics were modified.

The standard deviation in `summary.csv` is the sample standard deviation
(`n-1`). The validation assessment is ready to share with the caveat that
this is one host run and cross-platform differences cannot be assigned to
the ISA alone.

## Files

- `raw.csv`: loader output and per-round counters
- `summary.csv`: statistics and ARM64 ratios
- `environment.txt`: complete host, source, toolchain, CPU, and run settings
- `summarize.awk`: reproducible validation and aggregation

## Next Step

Because the three non-array-update paths are not heavier on x64, the next
cross-architecture diagnostic should measure x64 BPFtime JIT helper/no-op
A/B and direct L0-L3 paths to check for differences in the BPFtime numerator.
