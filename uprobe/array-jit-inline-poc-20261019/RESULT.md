# Matched array JIT POC result

New workload: runtime-dependent keys, 1,000 elements per invocation, checksum/hit/miss counts stored in a result map. Raw elapsed time includes the probe, key generation, lookup/read and checksum work. It is not a pure isolated helper cost and is NOT comparable to the old unused-result ten-operation table.

All four engines use the same BPF object, loader and victim. `frozen` is the pending C++ offset-pointer candidate; `off` and `inline` use the same independently built library with registration disabled/enabled. No changes to the formal ten-item results.

| Case | Kernel ns/element | Frozen C++ ns/element | POC off | POC inline | Inline vs off | Inline/kernel | Paired wins / 6 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| control_hit | 3.7580 | 1.5912 | 1.5912 | 1.5835 | -0.49% | 0.421x | 6 |
| hit | 5.8842 | 7.8958 | 7.8933 | 3.0816 | -60.96% | 0.524x | 6 |
| control_mixed | 9.2914 | 2.4945 | 2.4943 | 2.4858 | -0.34% | 0.268x | 6 |
| mixed | 12.3074 | 17.4315 | 17.2517 | 11.0473 | -35.96% | 0.898x | 6 |
| unused (unused-result simplification; not a gain) | 3.7839 | 6.7676 | 6.7774 | 1.1565 | -82.94% | 0.306x | 6 |

24 process-run checksum/count validations and 48 frequency checks passed; artifact hashes remained unchanged. The unused case is specifically excluded from optimization-gain claims.

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

Scope: controlled map lifetime and mapping, LLVM JIT, ordinary array only, opt-in POC. Invalid-key paths preserve the registered helper fallback. No dynamic close/reuse/reset/remap or production acceptance is claimed.
