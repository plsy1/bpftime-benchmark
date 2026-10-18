# Per-CPU array update regression investigation

All rechecks use the same frozen baseline and inline-leaf binaries, CPU 5
locked at 1728000 kHz, and unchanged BPF test objects. No runtime source
change was made during this investigation. The two new tests alternate
AB/BA three times each; the second also exchanges the harness labels.
Times below are control-subtracted ns/helper, not raw full-suite times.

| Test | Old update | Inline update | Median change | Paired mean delta |
|---|---:|---:|---:|---:|
| Original short A/B | 14.09839 | 14.19182 | +0.663% | +0.06149 ns |
| Balanced six-round A/B | 14.14575 | 14.11889 | -0.190% | -0.00710 ns |
| Reversed-label six-round A/B | 14.09259 | 14.15564 | +0.447% | +0.05797 ns |

## Lookup benefit check

| Test | Old lookup | Inline lookup | Median change |
|---|---:|---:|---:|
| Original short A/B | 10.62401 | 9.85484 | -7.240% |
| Balanced six-round A/B | 10.62566 | 9.88518 | -6.969% |
| Reversed-label six-round A/B | 10.64229 | 9.87212 | -7.237% |

## Assembly evidence

The 264-byte update leaf and 168-byte specialized update helper have
identical normalized instruction sequences between builds, including
the same calls, checks, stack frames and data movement. The leaf moves
from 0x4f2234 to 0x4f2230; the helper from 0x4e8ec0 to 0x4e8f40.
This excludes added update instructions as the explanation; it does
not prove which microarchitectural or environmental effect caused the
original timing difference. See [assembly comparison](update-assembly/summary.json).

## Interpretation limits

Retain the original full-run observation: 14.3241 to 14.4416 ns
(+0.82%). Matched rechecks determine whether that magnitude is stable;
they do not erase the observation or prove the absence of a smaller
effect. No padding/alignment workaround is justified without a
reproducible layout experiment. These are process-level samples, not
independent samples for every helper invocation.
