# Formal repair: short verification and full ten rounds passed

2026-10-21. Fix applied to primary and independently built warm source;
all 35 locally archived source files match. Optimization source is committed
and pushed as c0939bd on codex/uprobe-official-8da867d541f8. Selected result text
and handoff documentation are published separately; see source-commit.txt.

## Original full-workload short A/B

12 alternating AB/BA pairs, original complete victim/loader/BPF object,
10000 invocations per case, fresh process/maps per arm. Baseline is the
last GOOD pre-default direct-CPU inline version, not the regressed candidate.
No kernel was run for this screen; do not derive a new kernel ratio.
Raw means in ns per map operation, lower is better:

| Operation | Last good | Fixed default-on | Change |
|---|---:|---:|---:|
| Ordinary array update | 11.551 | 11.568 | +0.14% |
| Ordinary hash lookup | 26.983 | 26.973 | -0.04% |
| Ordinary hash update | 34.108 | 34.493 | +1.13% |
| Ordinary hash delete | 37.066 | 36.309 | -2.04% |
| Per-CPU array update | 14.539 | 14.463 | -0.52% |
| Per-CPU hash lookup | 27.787 | 27.769 | -0.06% |
| Per-CPU hash update | 44.545 | 44.539 | -0.01% |
| Per-CPU hash delete | 99.875 | 98.849 | -1.03% |

The prior bad default-on version measured ~43.8 ns for ordinary hash delete
in the separate balanced diagnostic experiment. The formal build recovers
the regression. Do not claim the additional ~2% versus the last good build
as an established standalone optimization.

Both historical array lookup rows have unused results and cannot serve as
JIT performance gain metrics. They are replaced here by separate consuming
diagnostics, NOT directly comparable to historical pure lookup timings.

## Consuming lookup diagnostics

Six balanced rounds for each kind with same-run kernel, runtime keys,
actual value reads, checksum and hit/miss count verification. Medians in
ns per element include probe, key generation and result-accounting work.
The roughly half-miss workload is an added diagnostic, not an original case.

| Case | Last good inline | Fixed default-on | Fixed/kernel |
|---|---:|---:|---:|
| Ordinary all-hit | 3.0954 | 3.0942 | 0.526x |
| Ordinary mixed | 9.6749 | 8.4503 | 0.686x |
| Per-CPU all-hit | 5.1875 | 5.1189 | 0.750x |
| Per-CPU mixed | 11.5377 | 10.6386 | 0.835x |

All consuming checks pass. Mixed-case improvements are primarily the
already tested default/generalization change, not gains attributable to
this deletion repair. Whole-function machine-code inspection confirms
the generic delete dispatcher is once more inlined in the standard helper.

## Completed full verification

The original 35-case x 10 run FINISHED 2026-10-21 14:59:07 JST, status 0.
Timed duration: 1480.87 seconds (24 min 41 sec). Every kernel/userspace/embed
case has ten valid positive finite samples. Binary SHA and default activation
audit passed, with 17 explicitly audited cases and no enable variable.

Full ordinary hash delete: bad previous 43.69 -> fixed 36.64 ns (-16.1%),
same-run kernel 108.06 ns, ratio 0.34x. Versus last good pre-default full run
37.06 ns, this restores the prior performance level. No evident non-target
regressions; the largest slowdown versus the bad previous full run is ~0.2%.

The eight historically comparable map operations remain below same-run kernel.
Both array lookups must still use the separate consuming results above, not
unused-return historical timings. Full table:
[ten-operation-comparison.md](full10/ten-operation-comparison.md).

Validation is complete; no need to rerun merely because older notes said
pending. Implementation is committed and pushed as c0939bd, but is not yet
included in daily automation's patch stack. Only selected result text is
published; full source archives and binaries remain local. Model handoff:
[research and engineering handoff](https://github.com/plsy1/bpftime-benchmark/blob/summry/jetson/uprobe/bpftime-uprobe-handoff-20261021.md).
