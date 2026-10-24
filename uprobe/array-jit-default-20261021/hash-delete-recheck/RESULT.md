# Ordinary hash delete isolated recheck

12 alternating AB/BA pairs, 24 fresh processes/maps; 10,000 timed invocations
and 1,000 untimed warmup invocations per arm. Each invocation first repopulates
1,000 keys OUTSIDE the timed interval, then times the 1,000 deletes. Only this
case executes. Original full-suite BPF object and loader remain unchanged.
A private single-case main links a separately compiled original test.c object
(main renamed), preserving its exported probe stubs and setup/timer functions.
This is NOT the identical historical victim ELF or a full-suite rerun.

Previous = immutable percpu-array-jit-direct-cpu-20261020/artifacts, old inline
switch 1; current = immutable array-jit-default-20261021/artifacts, no switch.
CPU 5 fixed at 1,728 MHz, 48 frequency samples pass, hashes before/after pass.
No runtime source or runtime binary changes, no kernel rerun.

| Ordinary hash delete, ns/op | Previous | Current |
|---|---:|---:|
| Mean | 37.0534 | 43.7309 |
| Median | 36.9963 | 43.6931 |
| Minimum | 36.8817 | 43.4589 |
| Maximum | 37.3106 | 44.0203 |

Current mean is +18.02%; median paired change +18.20%. Current is slower in
12/12 pairs; no samples removed. All current values exceed all previous values.
This reproduces the full-suite approximately +17.9% version-associated slowdown
and is not consistent with dismissing the difference as ordinary small noise.
Root cause has NOT been isolated; these results do not establish whether helper
registration, compiler/JIT layout or another implementation detail caused it.
Do not accept/push the default-on candidate as regression-free yet.

Raw samples: raw.csv; summary.json; environment.json; per-process logs. The
existing historical full-ten table is retained unchanged. No new kernel ratio
is reported because kernel was not measured in this recheck.
