# Official x64 uprobe benchmark at fixed frequency

## Result

This supplements `uprobe-top-x64-20260802` with a hard-frequency-controlled
run of the same unmodified official `benchmark/uprobe` victim. The source,
build, CPU affinity, iteration count, warm-up, and five-process repetition are
unchanged. CPU 5 was held at its 2.2 GHz base frequency by using the
performance governor, disabling turbo, and setting Intel pstate's minimum and
maximum performance percentages to 100. Its SMT sibling, CPU 11, was offline.

`turbostat` measured `Bzy_MHz=2200` and `Busy%=99.77` for every kernel and
BPFtime process. Core temperature remained between 40 and 45 C, with no sign
of thermal throttling.

The table reports the median of five paired per-process values:

`net ns/helper = (map-case ns/invocation - empty-uprobe ns/invocation) / 1000`

| Operation | kernel net | BPFtime net | BPFtime - kernel | Lower cost |
|---|---:|---:|---:|---|
| array lookup | 2.757 ns/helper | 13.393 ns/helper | +10.636 ns | kernel |
| array update-existing | 34.132 ns/helper | 18.547 ns/helper | -15.584 ns | BPFtime |
| hash lookup-hit | 49.948 ns/helper | 77.501 ns/helper | +27.552 ns | kernel |
| hash update-existing | 129.108 ns/helper | 83.218 ns/helper | -45.890 ns | BPFtime |

The same-host conclusion is unchanged: kernel BPF is cheaper for both lookup
cases, while BPFtime is cheaper for both update cases. The median empty probe
cost is 3366.888 ns/invocation for kernel BPF and 420.783 ns/invocation for
BPFtime.

## Previous-run comparison

Compared with `uprobe-top-x64-20260802`, which disabled turbo and selected the
performance governor but did not force Intel pstate's minimum performance to
100%, the fixed-frequency medians changed as follows:

| Path | array lookup | array update | hash lookup | hash update |
|---|---:|---:|---:|---:|
| kernel | -0.29% | -0.35% | -2.25% | -0.89% |
| BPFtime | -0.18% | -0.02% | -3.23% | -3.94% |

Array results are effectively identical. Hash results moved modestly lower,
especially the noisier BPFtime hash cases, but no winner or investigation
direction changed. This fixed-frequency archive is the better controlled
same-host top-level anchor.

## Validation

All ten victim processes exited with status 0 and emitted all 17 official
cases. Each BPFtime victim attached to shared memory with `shm_open_type 1`;
the BPFtime phase had zero kernel BPF links. After completion, CPU 11 and the
original pstate/governor settings were restored, with no residual BPF links or
BPFtime shared memory.

The loader logs contain libbpf's optional BPF-token warning (`-95`) on this
kernel. Both loaders nevertheless reached `Successfully started!`, and the
warning did not affect attachment or benchmark completion.

## Files

- `summary.csv`: raw ns/invocation for empty and four ordinary map cases.
- `net-helper.csv`: paired empty-subtracted ns/helper for all ten runs.
- `comparison.csv`: fixed-frequency kernel versus BPFtime medians.
- `comparison-previous.csv`: fixed-frequency versus 2026-08-02 medians.
- `frequency-summary.csv`: per-process turbostat measurements.
- `environment.txt`: source, toolchain, build, CPU, and power settings.
- `fixed-frequency-state.txt`: pstate and affinity state captured after setup.
- `raw/*-run-*.txt`: complete official victim output.
- `raw/*-run-*.time.txt`: `/usr/bin/time -v` output per victim process.
- `raw/*-run-*.turbostat.txt`: per-process frequency, load, temperature, and power.
- `raw/*-loader.txt`: complete kernel and BPFtime loader output.
- `run-official-uprobe-fixedfreq.sh`: exact setup, run, validation, and restoration.

Per-case hardware counters are not reported because the unmodified official
victim runs all cases sequentially in one process. Cycle and instruction
attribution remains in the dedicated JIT/direct diagnostics.
