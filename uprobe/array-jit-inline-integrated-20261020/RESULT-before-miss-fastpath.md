# Matched array JIT POC result

New workload: runtime-dependent keys, 1,000 elements per invocation, checksum/hit/miss counts stored in a result map. Raw elapsed time includes the probe, key generation, lookup/read and checksum work. It is not a pure isolated helper cost and is NOT comparable to the old unused-result ten-operation table.

All five engines use the same BPF object, loader and victim. `frozen` is the pending C++ offset-pointer candidate; `poc` is the unchanged prior restricted-lifetime prototype; `off` and `inline` use the same integrated library with specialization disabled/enabled. No changes to the formal ten-item results.

| Case | Kernel ns/element | Frozen C++ | Prior POC | Integrated off | Integrated inline | Inline vs off | Inline/kernel | Paired wins / 6 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| control_hit | 3.7605 | 1.5916 | 1.5836 | 1.5937 | 1.5943 | +0.03% | 0.424x | 3 |
| hit | 5.8868 | 7.9076 | 3.0842 | 7.9174 | 3.0917 | -60.95% | 0.525x | 6 |
| control_mixed | 9.2883 | 2.4953 | 2.4867 | 2.4965 | 2.4973 | +0.03% | 0.269x | 3 |
| mixed | 12.2952 | 17.4051 | 10.9994 | 17.3865 | 13.7101 | -21.15% | 1.115x | 6 |
| unused (unused-result simplification; not a gain) | 3.8043 | 6.7700 | 1.1570 | 6.8776 | 1.1774 | -82.88% | 0.309x | 6 |

30 process-run checksum/count validations and 60 frequency checks passed; artifact hashes remained unchanged. The unused case is specifically excluded from optimization-gain claims.

## Actual JIT code evidence

Code dumps start at the actual JIT entry and are bounded by its readable executable mapping. They are windows, not exact symbol sizes: padding or other functions may follow. Counts below stop at the first return and are descriptive, not full CFG cost models.

| Engine | Program | Instructions through first return | Call sites in that prefix |
| --- | --- | ---: | ---: |
| off | control_hit | 36 | 1 |
| off | hit | 66 | 2 |
| off | control_mixed | 52 | 1 |
| off | mixed | 66 | 2 |
| off | unused | 23 | 1 |
| inline | control_hit | 45 | 1 |
| inline | hit | 82 | 2 |
| inline | control_mixed | 61 | 1 |
| inline | mixed | 82 | 2 |
| inline | unused | 27 | 1 |

Scope: LLVM JIT, ordinary-array lookup-only programs, opt-in integration. Program-owned references retain original backing and mapping across FD close/reuse/reset/unlink/recreate. Misses resolve the same pinned identity. Signal callbacks and AOT replacement are explicitly rejected for bound programs. See lifecycle tests and README for remaining restrictions.
