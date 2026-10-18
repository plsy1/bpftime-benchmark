# Per-CPU array lookup follow-up

Baseline source commit: `df934cd13027a2d14acec3e7fbfa87b144adab1e`.
It includes both accepted hash lookup optimizations. Production binaries
are frozen in `/tmp/bpftime-percpu-array-lookup-20261018.w2eqFb/baseline`.

Accepted on 2026-10-19 as source commit
`eb09b972d59ca7845e3e1251a2ad96845b93feaf` following the update regression
investigation below. The accepted change is lookup-leaf inlining and its
correctness tests only; the update implementation is unchanged. The commit is
also included in the daily workflow's explicit replay list. Complete ten-round
results, both additional matched rechecks and assembly evidence are retained.

## Current-version profile

The lookup-only capture completed successfully with 4,849 sampling events.
Top-frame samples: specialized helper 2,065 (42.6%), production lookup
1,087 (22.4%), libc `sched_getcpu` 1,071 (22.1%). The capture includes
initialization and warmup, so these are sample shares rather than isolated
component timings. Cross-JIT backtraces may be incomplete.

Assembly confirms that the specialized helper tail-branches to the 132-byte
`elem_lookup_base` function. The helper and the leaf each set up and restore
a stack frame. The lambda/callable wrapper has already been optimized away.
The previous 2026-10-14 inline experiment moved the map-handler wrapper into
a header; it did not force the underlying lookup body into this helper.

## First candidate: inline the lookup leaf

Move the unchanged `elem_lookup_base` body into its class definition and
mark it `always_inline`. Preserve bounds checks, null-key errors, shared
memory layout, data addressing and one current-CPU query per lookup.

The correctness gate runs all per-CPU array tests, including a new test
which changes the executing thread's affinity between calls on the same
map. It checks distinct CPU values, first and last valid keys, invalid
keys, null keys and 1/8/24/257-byte values. Existing concurrent write and
CPU-slot tests are retained.

The production A/B uses GCC 13, `-O2`, LTO on, probe checks off, CPU 5 pinned
at 1728000 kHz. Five alternating baseline/candidate/kernel rounds use the
existing array helper-closure suite. Short A/B values subtract control
overhead and must not be directly mixed with raw full-suite numbers.

If the array candidate improves, screen hash operations before running the
complete ten-round suite: expanding shared lookup code can affect code
layout even when the hash source files are unchanged. The hash source
checksums are explicitly verified before building and before full testing.

## Array A/B result

All four per-CPU array test cases passed (5,612 assertions). Assembly confirms
the leaf is fully inlined: its standalone production symbol is gone and the
specialized helper has one stack frame and one call to `sched_getcpu`.

| Operation | Baseline median ns/helper | Candidate median ns/helper | Change |
|---|---:|---:|---:|
| Ordinary array lookup | 6.9247 | 6.9308 | +0.09% |
| Ordinary array update | 11.3537 | 11.3756 | +0.19% |
| Per-CPU array lookup | 10.6240 | 9.8548 | -7.24% |
| Per-CPU array update | 14.0984 | 14.1918 | +0.66% |

The target improves in all five paired rounds. `run-hash-and-full.sh`
uses the same frozen libraries for the hash regression screen, then starts
the complete ten-round benchmark if the stable hash operations do not show
a median regression above 3%. Ordinary hash update is reviewed separately
because of its previously observed bimodality. Passing this screen is not
a claim that smaller regressions are absent; the full table remains the
final regression check.

## Complete ten-round validation

The hash A/B screen passed, with median changes of +0.09% for ordinary
hash lookup, +0.05% for ordinary hash update, -0.18% for per-CPU hash lookup
and +0.19% for per-CPU hash update.

The full suite completed successfully at 2026-10-18 22:42:50 JST in
1497.14 seconds. Both pipeline and full-run exit statuses are 0, all cases
have ten finite positive samples, and both frozen library checksums passed.

Per-CPU array lookup falls from 11.6398 to 10.9237 ns/helper (-6.15%).
Same-run kernel is approximately 4.41 ns/helper, leaving a 2.48x ratio.
All ten candidate lookup samples (10.8987–10.9715 ns) are below all ten
previous-version samples (11.5897–11.7022 ns). This corroborates the
five-round matched A/B benefit.

There is a small per-CPU array update regression to retain explicitly:
14.3241 to 14.4416 ns/helper (+0.82%, approximately 0.12 ns).
Its previous range is 14.2738–14.3706 ns; the new range is
14.3777–14.4971 ns. The short paired A/B also increased by 0.66%.
Do not dismiss this repeatable direction as measurement noise. The update
implementation does not call the changed lookup method, so the cause
is not yet established; code generation/layout is a hypothesis, not a
confirmed explanation.

Both hash lookup gains are preserved in this complete run. Eight of ten
map operations remain below same-run kernel BPF; the two array lookups
remain slower. At this initial full-run review the candidate was not yet
committed, pending investigation of the small update side effect.

See [the complete ten-operation table](full10/ten-operation-comparison.md)
and [the full benchmark report](full10/results.md).

## Update regression investigation

Two additional six-round matched tests used the unchanged frozen libraries,
100,000 iterations, 5,000 warmup iterations, CPU 5 and the same locked frequency.
Each test balances AB/BA order; the second also swaps the harness's baseline
and candidate labels. Both completed successfully with frozen binary checksums
and frequency checks passing. No runtime implementation was changed.

After mapping the labels back to the actual versions, per-CPU array update
median changes were -0.19% and +0.45%, rather than a consistent +0.82%.
The respective paired mean deltas were -0.0071 and +0.0580 ns/helper.
The original full-run increase remains a valid observation, but its magnitude
does not reproduce reliably in the new matched tests. A smaller layout effect
is not ruled out; neither a source-level update regression nor a pure-noise
explanation has been established.

The update leaf and specialized update helper have identical normalized
instruction sequences in the two frozen builds. Only addresses/relocations
differ: the 264-byte leaf shifts from 0x4f2234 to 0x4f2230, and the 168-byte
helper from 0x4e8ec0 to 0x4e8f40. This excludes extra update instructions,
allocations or copies as the cause, but does not establish a particular
instruction-cache/alignment mechanism.

The lookup benefit remains consistent: -6.97% and -7.24% in the two new
short tests, corroborating the full suite's -6.15%. Do not add an alignment
or padding workaround based on these small, inconsistent update results.
No production fix was introduced during this diagnostic task. The lookup
candidate was subsequently accepted as recorded above.
See [the detailed investigation](update-investigation.md).
