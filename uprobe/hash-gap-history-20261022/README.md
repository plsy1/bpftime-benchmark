# Hash helper gap across the optimization history (2026-10-22)

Question: how did the three hash consuming variants of
`../hash-lookup-helper-gap-20261021` compare with kernel BPF at each step of
the optimization history? These mixed-map workloads did not exist before
2026-10-21, so earlier builds had never been measured on them.

`run-history.py` (derived from that directory's `run-gap.py`): kernel plus six
frozen bpftime builds, all 21 (engine, variant) arms alternating within each
round (order reversed on odd rounds), 6 rounds, 50000 invocations, CPU 5 at
1.728 GHz (252 frequency samples, all 1.728 GHz), checksums and hit/miss
counts verified for every arm, artifact hashes unchanged. Run 01:12-02:11 JST,
exit 0. `trial/` is a one-round 5000-iteration smoke run.

| Engine | Build | Notes |
|---|---|---|
| official-177a1dc | `consuming-official-vs-optimized-20261021/official-artifacts` | unpatched official |
| opt-in-9ffbffb | `array-jit-inline-integrated-20261020/artifacts` | all 27 archived sources equal `9ffbffb`; 8da base; array JIT opt-in, off here |
| default-5466f0d | `baseline-177a1dc-jit-llvm16-20261021/artifacts` | `c0939bd` default/generalized array JIT on 177a1dc |
| globals-bdd9835 | `array-jit-global-data-20261021/formal/artifacts` | global data in array JIT |
| dispatch-baf0cc85 | `hash-helper-fixes-20261022/dev1-dispatch/artifacts` | fix 2, dev build |
| selection-c3090b59 | `hash-helper-fixes-20261022/dev2-selection/artifacts` | fix 1 + 2, dev build |

Helper selection / binding per build (`helper-evidence.json`) matched the code:
typed hash helper for v1 from 9ffbffb on, for v2/v3 only in c3090b59; v3 bound
from 5466f0d on, never v1/v2.

Median raw ns per element, ratio to same-run kernel:

| Variant | Case | Kernel | official | 9ffbffb | 5466f0d | bdd9835 | baf0cc85 | c3090b59 |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| v1 hash only | hit | 31.68 | 65.26 (2.06x) | 37.14 (1.17x) | 38.00 (1.20x) | 37.67 (1.19x) | 38.34 (1.21x) | 37.97 (1.20x) |
| v1 hash only | mixed | 41.75 | 946.48 (22.67x) | 359.13 (8.60x) | 360.00 (8.62x) | 359.80 (8.62x) | 359.99 (8.62x) | 359.05 (8.60x) |
| v2 hash + globals | hit | 31.76 | 65.31 (2.06x) | 56.46 (1.78x) | 56.74 (1.79x) | 57.11 (1.80x) | 57.13 (1.80x) | 37.97 (1.20x) |
| v2 hash + globals | mixed | 41.90 | 946.62 (22.59x) | 392.71 (9.37x) | 390.60 (9.32x) | 393.18 (9.38x) | 390.56 (9.32x) | 358.98 (8.57x) |
| v3 hash + array | hit | 31.70 | 65.24 (2.06x) | 56.55 (1.78x) | 76.48 (2.41x) | 76.53 (2.41x) | 61.73 (1.95x) | 44.02 (1.39x) |
| v3 hash + array | mixed | 41.98 | 946.45 (22.54x) | 392.07 (9.34x) | 411.86 (9.81x) | 412.52 (9.83x) | 397.45 (9.47x) | 365.38 (8.70x) |

Control cases (no map access) are equal across builds (bpftime 1.59-1.63 /
2.50-2.54, kernel 3.76-3.79 / 9.31-9.33). No arm's six samples spread more
than 5%.

Findings:

1. The generalized default array JIT (`c0939bd`, measured as `5466f0d`) made
   hash + array programs slower: v3 hit 56.55 -> 76.48 ns (1.78x -> 2.41x
   kernel), mixed 9.34x -> 9.81x. v1/v2 were not affected.
2. Fix 2 (`baf0cc85`) removed most of it (v3 hit 61.73 ns, 1.95x); fix 1
   (`c3090b59`) then gave v2/v3 the typed hash helper: v2 equals v1
   (1.20x), v3 hit 44.02 ns (1.39x), below the pre-generalization 56.55 ns.
   v3 keeps about 6 ns over v1 from the bound-program scope.
3. Misses on the full table stay 8.6-8.7x kernel in every optimized build;
   official 177a1dc is 22.6x (its whole hash path is slower: hit 2.06x).
