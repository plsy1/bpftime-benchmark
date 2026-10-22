# Direct original map helpers in bound programs (f26cc335, 2026-10-22)

Commit `f26cc335` (local, not pushed). In a program bound for the array JIT,
helper slots 1-3 hold the bound dispatcher; hash lookup/update/delete with a
constant handle paid a thread-local scope read and an indirect call only to
reach the original helper. Now `bind_array_maps` registers the program's
exact original lookup/update/delete helpers at private llvmbpf slots
(`vm/compat/include/bpftime_direct_map_helpers.hpp`) and marks constant
non-array handles as direct (new `array_map_descriptor::direct_helpers` via
the overlay patches); the JIT pass redirects those calls. Array, dynamic and
canonicalized handles keep the dispatcher. Regression filter: 74 cases /
239322 assertions (`tests.log`); `[array-jit-dispatch]` asserts 3 redirected
calls and still covers dynamic use and FD replacement.

## A/B against 1cb20807 (same run, with kernel)

`gap6/` (three program shapes of `../hash-lookup-helper-gap-20261021`, 6
rounds, 50000 calls) and `mutate6/` (`../hash-mutate-20261022` workloads, 6
rounds, 20000 calls); raw ns per element, ratio to same-run kernel:

| Program | Case | Kernel | 1cb20807 | f26cc335 | Change |
|---|---|---:|---:|---:|---:|
| hash only | lookup, all hit | 31.78 | 26.06 (0.82x) | 25.33 (0.80x) | -2.8% |
| hash only | lookup, about half missing | 41.71 | 34.26 (0.82x) | 33.87 (0.81x) | -1.1% |
| hash + global data | lookup, all hit | 31.91 | 25.35 (0.79x) | 25.90 (0.81x) | +2.2% |
| hash + global data | lookup, about half missing | 41.76 | 34.02 (0.81x) | 34.43 (0.82x) | +1.2% |
| hash + array (bound) | lookup, all hit | 31.57 | 30.86 (0.98x) | 26.02 (0.82x) | **-15.7%** |
| hash + array (bound) | lookup, about half missing | 41.68 | 41.65 (1.00x) | 34.26 (0.82x) | **-17.7%** |
| hash only | update | 90.68 | 30.87 (0.34x) | 29.83 (0.33x) | -3.4% |
| hash only | delete + insert | 212.05 | 74.54 (0.35x) | 74.91 (0.35x) | +0.5% |
| hash + global data | update | 91.75 | 27.95 (0.30x) | 28.25 (0.31x) | +1.0% |
| hash + global data | delete + insert | 212.18 | 75.23 (0.35x) | 74.88 (0.35x) | -0.5% |
| hash + array (bound) | update | 91.02 | 35.57 (0.39x) | 29.77 (0.33x) | **-16.3%** |
| hash + array (bound) | delete + insert | 212.54 | 89.07 (0.42x) | 75.28 (0.35x) | **-15.5%** |

The bound program is now as fast as the hash-only program for lookup,
update and delete. Changes of up to +-3% in unbound programs, whose code
paths did not change, are within run-to-run variation.

## Full ten rounds, userspace only (`full10-f26cc335/`)

Exit 0, activation audit passed. Against 1cb20807 (`../hash-probe-bound-20261022/full10-1cb20807`,
different run) every map operation is within +-1%. Two non-map rows moved:
uprobe + uretprobe 322.83 -> 358.08 ns (+10.9%, all ten samples 355-364) and
probe write mean +6.4% (bimodal: some samples ~4850 instead of ~4273 ns).

AB/BA screen (`full10-f26cc335/abba-screen/`, 20 rounds, 1cb20807 vs
f26cc335, full 17-case victim): uprobe + uretprobe 326.07 -> 358.89 ns,
median paired ratio 1.106, slower in 20/20 pairs (real). Probe write median
paired ratio 1.001; slow-mode samples in 3/20 f26cc335 runs, 0/20 1cb20807.
Neither path executes code changed by this commit (unbound programs, no map
helpers); suspected code placement. Being checked with an all-functions
64-byte-aligned build.
