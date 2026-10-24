# Hash lookup helper gap (2026-10-21)

Consuming hash workload (the array consuming diagnostic built with a hash data
map, `f24dc9b` sources in `source/`, `build-workloads.sh`): 1024-entry u32->u64
hash filled with keys 0..1023, 1000 runtime LCG keys per invocation, value
checksum and hit/miss counts verified. `hit` keys 0..1023, `mixed` 0..2047
(about half absent). Runtime = frozen `bdd9835` artifacts. `run-gap.py`: six
(engine, variant) arms alternating per round, 6 rounds, 50000 invocations,
CPU 5 at 1.728 GHz; helper selection asserted from the agent log.

| Variant | Maps | Lookup helper | hit bpftime / kernel | mixed bpftime / kernel |
|---|---|---|---:|---:|
| v1-hash-only | data hash, result hash | typed hash | 37.65 / 31.96 = 1.18x | 359.9 / 42.0 = 8.56x |
| v2-hash-globals | v1 + `.rodata`/`.bss` | generic | 56.64 / 31.84 = 1.78x | 392.7 / 42.1 = 9.33x |
| v3-hash-mixed | data hash, result array | generic, program bound for array JIT | 76.75 / 31.62 = 2.43x | 412.7 / 41.6 = 9.91x |

Raw ns per element (median); control cases (no map access) are equal across
variants (bpftime 1.60/2.50, kernel 3.8/9.3).

Findings:

1. Typed vs generic helper, about +19 ns per lookup (v1 -> v2). The attach
   layer picks the typed helper only if every map LDDW of a program has one
   type and there is no direct map-value LDDW (`uses_only_maps_of_type_for_helper`,
   `bpf_attach_ctx.cpp`). The typed helpers re-check the map type at runtime
   and fall back to the generic dispatcher, so the restriction is only a
   performance heuristic.
2. Bound-program overhead on non-array lookups, about +20 ns per lookup
   (v2 -> v3). In a program bound for the array JIT, every non-inlined lookup
   goes through `array_jit_execution_scope::lookup` (thread-local scope,
   `bound_arrays`/`unbound_map_handles` tree lookups, `raw_helpers.at(1)`)
   before the original helper. Introduced with the array JIT (`9ffbffb`/`c0939bd`);
   it affects common mixed programs such as hash + per-CPU array (sslsniff).
3. Misses on a full table are 8.6-9.9x kernel, hits 1.18x even with the typed
   helper. `bpftime_hash_map` uses open addressing with linear probing and
   `next_prime(max_entries)` buckets (1031 for 1024), so a full map has ~99%
   load and a miss probes until one of the few empty buckets. The original
   benchmark (keys 0..999 present, sequential) never exercises this. Same
   structure in official `177a1dc`; changing it is a map data-structure change.
