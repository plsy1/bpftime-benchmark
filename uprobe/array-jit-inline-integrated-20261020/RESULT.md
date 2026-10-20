# Ordinary-array JIT integration result

New workload: runtime-dependent keys, 1,000 elements per invocation, checksum/hit/miss counts stored in a result map. Raw elapsed time includes the probe, key generation, lookup/read and checksum work. It is not a pure isolated helper cost and is NOT comparable to the old unused-result ten-operation table.

All five engines use the same BPF object, loader and victim. `frozen` is the pending C++ offset-pointer candidate; `poc` is the unchanged prior restricted-lifetime prototype; `off` and `inline` use the same integrated library with specialization disabled/enabled. No changes to the formal ten-item results.

| Case | Kernel ns/element | Frozen C++ | Prior POC | Integrated off | Integrated inline | Inline vs off | Inline/kernel | Paired wins / 6 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| control_hit | 3.7663 | 1.5908 | 1.5838 | 1.5953 | 1.5960 | +0.04% | 0.424x | 2 |
| hit | 5.8830 | 7.8940 | 3.0826 | 7.8950 | 3.0941 | -60.81% | 0.526x | 6 |
| control_mixed | 9.2933 | 2.4952 | 2.4869 | 2.4972 | 2.4994 | +0.09% | 0.269x | 2 |
| mixed | 12.2370 | 17.4216 | 11.0113 | 17.5412 | 9.6320 | -45.09% | 0.787x | 6 |
| unused (unused-result simplification; not a gain) | 3.8039 | 6.7702 | 1.1563 | 6.7824 | 1.1723 | -82.72% | 0.308x | 6 |

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
