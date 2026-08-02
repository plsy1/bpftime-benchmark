# Official uprobe benchmark on x64

## Result

This is the same-host top-level anchor for `benchmark/uprobe` at BPFtime commit
`8ed291e130fe3f15f99955b0d259eb119efdaa7d`. The official victim was run in
full, with its default 100000 iterations, in five independent processes for
kernel BPF and five for BPFtime. Each ordinary map BPF program executes 1000
helpers per invocation.

The table reports the median of the five paired per-process values:

`net ns/helper = (map-case ns/invocation - empty-uprobe ns/invocation) / 1000`

| Operation | kernel net | BPFtime net | BPFtime - kernel | Lower cost |
|---|---:|---:|---:|---|
| array lookup | 2.765 ns/helper | 13.417 ns/helper | +10.652 ns | kernel |
| array update-existing | 34.251 ns/helper | 18.551 ns/helper | -15.700 ns | BPFtime |
| hash lookup-hit | 51.097 ns/helper | 80.090 ns/helper | +28.993 ns | kernel |
| hash update-existing | 130.270 ns/helper | 86.631 ns/helper | -43.639 ns | BPFtime |

So the remembered result was partly right: on this i7, BPFtime wins both update
cases, while kernel BPF wins both lookup cases. It is not a blanket BPFtime win.
The empty attached probe is also much cheaper in BPFtime: median 422 ns per
invocation versus 3362 ns for kernel BPF.

## Relation to the map-path diagnostics

The official top-level BPFtime values are close to the earlier LLVM JIT
real-minus-noop helper measurements:

| Operation | official uprobe net | JIT helper A/B | Difference |
|---|---:|---:|---:|
| array lookup | 13.417 ns | 9.984 ns | +3.433 ns |
| array update-existing | 18.551 ns | 19.435 ns | -0.884 ns |
| hash lookup-hit | 80.090 ns | 75.680 ns | +4.410 ns |
| hash update-existing | 86.631 ns | 84.321 ns | +2.310 ns |

This validates the userspace microbenchmark's direction on the same host. In
particular, the expensive x64 BPFtime hash implementation is visible at the
top level, but kernel hash update is still more expensive on this machine.
That distinction is why the next internal hash investigation should continue
to target BPFtime L0 for the cross-architecture question, while using this
official run only as the same-host end-to-end anchor.

BPFtime hash lookup/update varied more than the other cases: lookup ranged
75.359-80.841 ns/helper and update 79.781-89.191 ns/helper. All kernel rows and
both BPFtime array rows were tightly grouped. The raw values and standard
deviations are retained in `net-helper.csv`.

## Files

- `summary.csv`: raw ns/invocation for empty and four ordinary map cases.
- `net-helper.csv`: paired empty-subtracted ns/helper for all ten runs.
- `comparison.csv`: kernel versus BPFtime median comparison.
- `environment.txt`: source, toolchain, build, CPU, and power settings.
- `raw/*-run-*.txt`: complete official victim output.
- `raw/*-run-*.time.txt`: `/usr/bin/time -v` output for each victim process.
- `raw/*-loader.txt`: complete kernel and BPFtime loader output.
- `run-official-uprobe.sh`: exact orchestration, validation, and restoration.

Per-case hardware counters are not reported here because the unmodified
official victim runs every case sequentially in one process; whole-process
perf counters cannot be attributed to the four selected cases. Cycle and
instruction attribution remains in the dedicated JIT/direct diagnostics.
