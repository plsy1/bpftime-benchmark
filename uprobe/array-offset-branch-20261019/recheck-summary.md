# Precommit paired recheck

Same frozen binaries as full ten-round test; no rebuild. Six rounds, 100,000 BPF invocations per measured case, 3,000 warmup, CPU 5 at 1728 MHz. Initial baseline/candidate order is reversed relative to the original screen, then alternated each round. Each invocation performs 1,000 helpers.

Positive change means candidate is slower. Net timings subtract the same-process control; raw timings do not. Neither replaces the formal full-suite table.

| Operation | Net baseline ns | Net candidate ns | Net median change | Raw median change | Paired net losses / 6 | Paired net change range |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| array_lookup | 6.469169 | 5.776494 | -10.707% | -9.230% | 0 | -11.238% to -10.562% |
| array_update | 11.331768 | 11.334420 | +0.023% | +0.057% | 2 | -0.210% to +0.542% |
| percpu_array_lookup | 9.696309 | 9.713408 | +0.176% | +0.199% | 3 | -0.583% to +0.513% |
| percpu_array_update | 14.172411 | 14.188028 | +0.110% | +0.125% | 3 | -1.352% to +0.383% |

This reports observed differences only. It does not establish zero regression or a confidence bound below the measurement resolution. The completed full-ten table remains unchanged.
