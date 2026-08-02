# Official uprobe benchmark on Jetson ARM64

## Result

This directory is the Jetson Orin Nano top-level counterpart to
`uprobe-top-x64-20260802`. Both experiments use BPFtime commit
`8ed291e130fe3f15f99955b0d259eb119efdaa7d`, the complete official
`benchmark/uprobe` victim, 100000 iterations, and five independent root
processes per environment. Loader and victim were pinned to CPU5. The Jetson
was in MAXN_SUPER mode with CPU5 fixed at 1.728 GHz for all ten runs.

Each ordinary map case invokes its helper 1000 times. The primary comparison
uses the median of five same-process, empty-probe-subtracted values:

`net ns/helper = (map case ns/invocation - __bench_uprobe ns/invocation) / 1000`

| Operation | ARM64 kernel | ARM64 BPFtime | BPFtime/kernel | Lower cost |
|---|---:|---:|---:|---|
| array lookup | 2.563 ns/helper | 12.901 ns/helper | 5.033x | kernel |
| array update-existing | 11.858 ns/helper | 16.426 ns/helper | 1.385x | kernel |
| hash lookup-hit | 27.808 ns/helper | 54.608 ns/helper | 1.964x | kernel |
| hash update-existing | 93.342 ns/helper | 51.647 ns/helper | 0.553x | BPFtime |

On this Jetson, kernel BPF has the lower net cost for both lookups and array
update, while BPFtime has the lower net cost for hash update. The empty
attached probe itself is substantially cheaper in BPFtime: the median is
376.530 ns/invocation versus 1229.214 ns/invocation for kernel BPF.

## Raw top-level medians

These values are the directly reported ns/invocation before empty-probe
subtraction. All five raw samples plus mean, median, standard deviation, and
min/max are in `summary.csv`.

| Case | ARM64 kernel | ARM64 BPFtime | x64 kernel | x64 BPFtime |
|---|---:|---:|---:|---:|
| `__bench_uprobe` | 1229.214 | 376.530 | 3361.932 | 422.372 |
| `__bench_array_map_lookup` | 3791.931 | 13280.756 | 6127.173 | 13839.945 |
| `__bench_array_map_update` | 13090.906 | 16804.950 | 37617.441 | 18988.125 |
| `__bench_hash_map_lookup` | 29035.312 | 54984.103 | 54459.050 | 80530.666 |
| `__bench_hash_map_update` | 94567.502 | 52023.748 | 133631.643 | 87071.068 |

## ARM64 versus x64

| Operation | ARM64 kernel net | x64 kernel net | ARM/x64 | ARM64 BPFtime net | x64 BPFtime net | ARM/x64 | Winner flip |
|---|---:|---:|---:|---:|---:|---:|---|
| array lookup | 2.563 | 2.765 | 0.927x | 12.901 | 13.417 | 0.962x | no |
| array update-existing | 11.858 | 34.251 | 0.346x | 16.426 | 18.551 | 0.885x | yes: BPFtime on x64, kernel on ARM64 |
| hash lookup-hit | 27.808 | 51.097 | 0.544x | 54.608 | 80.090 | 0.682x | no |
| hash update-existing | 93.342 | 130.270 | 0.717x | 51.647 | 86.631 | 0.596x | no |

The array-update reversal is primarily a kernel-denominator effect in this
same-benchmark comparison: ARM64 kernel array update is only 0.346x its x64
net value, whereas ARM64 BPFtime is 0.885x its x64 value. Consequently,
BPFtime changes from 0.542x kernel cost on x64 to 1.385x on ARM64.

Hash lookup shifts in the same direction more mildly: the ARM64/x64 ratios
are 0.544x for kernel and 0.682x for BPFtime. Array lookup is nearly unchanged
on both paths. For hash update, both architectures favor BPFtime; the ARM64
BPFtime path decreases more relative to x64 than the kernel path does.

These cross-host raw-time ratios describe where the platform-relative ratio
changes arise; they do not by themselves identify a microarchitectural root
cause, because the hosts use different CPUs, kernels, and system software.
They also do not support a blanket claim that the BPFtime map path executes
more slowly on ARM64: every selected BPFtime net helper value is numerically
lower on this Jetson than on the x64 host. The decisive change for array
update is the much cheaper Jetson kernel path.

## Variance and validation

The five ARM64 samples are tightly grouped. Net ns/helper standard deviations
range from 0.005 to 0.101 ns for kernel and from 0.059 to 0.259 ns for
BPFtime. The complete statistics and raw values are retained in
`net-helper.csv`.

Validation performed by the exact run script:

- source commit and MAXN_SUPER mode were hard-checked before running;
- kernel and BPFtime loader startup messages were checked;
- kernel links were present for kernel runs and absent for BPFtime runs;
- every BPFtime victim reported `shm_open_type 1`;
- all ten frequency records show CPU5 min/current/max at 1728000 kHz;
- every victim completed all 17 official cases;
- final kernel link count was zero and `/dev/shm/bpftime_maps_shm` was absent.

The source tree contained four pre-existing, unrelated untracked paths before
and after the experiment. They are recorded verbatim in `environment.txt` and
were not modified or included in this result.

## Files

- `summary.csv`: five raw ns/invocation values and descriptive statistics.
- `net-helper.csv`: paired empty-subtracted ns/helper values and statistics.
- `comparison.csv`: Jetson kernel versus BPFtime median comparison.
- `comparison-x64.csv`: cross-architecture values, ratios, and winner flips.
- `environment.txt`: source, toolchain, build, CPU topology, power, and cleanup state.
- `raw/*-run-*.txt`: complete stdout for all ten official victims.
- `raw/*-run-*.time.txt`: `/usr/bin/time -v` for every victim.
- `raw/*-run-*.frequency.txt`: CPU5 frequency at the start of every victim.
- `raw/*-loader.txt`: complete kernel and BPFtime loader output.
- `run-official-uprobe.sh`: exact orchestration and validation script.
- `parse.py`: exact parser and comparison generator.
