# Code layout check (2026-10-22)

Question: are the non-map shifts seen after the latest commits (per-CPU array
update +1.1% since c3090b59, uprobe + uretprobe +10.6% and probe write slow
mode in f26cc335) caused by code placement rather than code logic?

Method: build the same tree as `f26cc335` in a separate directory
(`candidate/build-align`, untracked) with `-falign-functions=64` for C, C++
and the LTO link (`aligned-flags.txt`), everything else as the dev build.
`run-layout.py`: 24 rounds, all six orders of 1cb20807 / f26cc335 /
f26cc335-aligned, full 17-case victim, 10000 iterations, CPU 5 at 1.728 GHz,
no kernel. Exit 0.

| Case | 1cb20807 | f26cc335 (paired ratio) | f26cc335 aligned (paired ratio) |
|---|---:|---:|---:|
| uprobe + uretprobe (ns/inv) | 323.37 | 361.53 (1.115) | 324.27 (1.002) |
| uprobe (ns/inv) | 198.25 | 198.57 (1.001) | 210.94 (**1.065**) |
| uretprobe (ns/inv) | 274.28 | 274.51 (1.002) | 275.85 (1.005) |
| per-CPU array update (ns/op) | 14.572 | 14.562 (0.999) | 14.386 (0.986) |
| ordinary hash update (ns/op) | 29.32 | 30.96 (1.022) | 27.80 (1.004) |
| probe write slow-mode samples | 0/24 | 4/24 | 0/24 |

All other cases within +-1.5%.

Findings: with identical source, changing only function placement removes
the uprobe + uretprobe slowdown, the per-CPU array update shift and the probe
write slow mode, so they are layout effects, not effects of the commits'
logic. Aligning every function is not a fix: it makes uprobe 6.5% slower.
Frida (prebuilt, not affected by the flag) and our code move relative to each
other with every change; without a profiler on this kernel (`perf` is not
installed) the hot spot behind each effect is not identified. The flag is
not adopted; full-run rows of probe/uretprobe paths can move by up to ~10%
between builds for this reason.
