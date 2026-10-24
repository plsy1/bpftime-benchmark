# Ordinary-array JIT integration result

New workload: runtime-dependent keys, 1,000 elements per invocation, checksum/hit/miss counts stored in a result map. Raw elapsed time includes the probe, key generation, lookup/read and checksum work. It is not a pure isolated helper cost and is NOT comparable to the old unused-result ten-operation table.

All five engines use the same BPF object, loader and victim. `frozen` is the pending C++ offset-pointer candidate; `poc` is the unchanged prior restricted-lifetime prototype; `off` and `inline` use the same integrated library with specialization disabled/enabled. No changes to the formal ten-item results.

| Case | Kernel ns/element | Frozen C++ | Prior POC | Integrated off | Integrated inline | Inline vs off | Inline/kernel | Paired wins / 6 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| control_hit | 3.7624 | 1.5923 | 1.5849 | 1.5919 | 1.5930 | +0.07% | 0.423x | 2 |
| hit | 5.8869 | 7.9089 | 3.0838 | 7.9206 | 3.0919 | -60.96% | 0.525x | 6 |
| control_mixed | 9.3035 | 2.4959 | 2.4870 | 2.4961 | 2.4968 | +0.03% | 0.268x | 1 |
| mixed | 12.3193 | 17.4031 | 11.0168 | 17.3128 | 9.6239 | -44.41% | 0.781x | 6 |
| unused (unused-result simplification; not a gain) | 3.8057 | 6.7679 | 1.1590 | 6.7792 | 1.1735 | -82.69% | 0.308x | 6 |

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
