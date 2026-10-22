# Hash helper fixes (started 2026-10-22; fixes done, formal validation pending)

Follow-up to `../hash-lookup-helper-gap-20261021` (findings 1-3 there).

## Fix 2: constant-time dispatch of non-inlined calls in bound programs

Commit `baf0cc85` on `codex/uprobe-official-177a1dc465ff` (**local only, not
pushed, no formal validation yet**). `array_jit_execution_scope::resolve`
uses FD-indexed tables (`bound_by_fd`, `unbound_by_fd`) built in
`bind_array_maps`, and the original lookup/update/delete helpers are cached
(`raw_lookup`/`raw_update`/`raw_erase`); handles outside the tables keep the
dynamic `find_array` path. New test `[array-jit-dispatch]`: hash update/lookup/
delete in a bound program, a dynamic FD inside the table range, dynamic use of
the bound and of the unbound FD, and hash FD replacement (non-array maps stay
unpinned).

`dev1-dispatch/` (dev build of exactly the committed tree, `working-tree.diff`):
complete daily filter 70 cases / 128455 assertions passed; hash gap runner,
6 rounds (before = `../hash-lookup-helper-gap-20261021/gap6`, bdd9835, a
different run):

| Variant | Case | Kernel | Before | After | Change |
|---|---|---:|---:|---:|---:|
| v1-hash-only | hit | 31.91 | 37.65 | 37.65 | 0.0% |
| v2-hash-globals | hit | 31.85 | 56.64 | 55.77 | -1.5% |
| v3-hash-mixed | hit | 31.86 | 76.75 | 61.05 | -20.5% |
| v3-hash-mixed | mixed | 41.76 | 412.68 | 397.15 | -3.8% |

The remaining ~5 ns of v3 over v2 is the thread-local scope read and the
indirect call; removing it needs the JIT to call the original helper directly
for constant non-array sites (llvmbpf overlay change, optional).

## Fix 1: typed helper selection

Commit `c3090b59` on top of `baf0cc85` (**local only, not pushed, no formal
validation yet**). `uses_only_maps_of_type_for_helper` is replaced by
`typed_helper_map_type` (`runtime/src/attach/bpf_attach_ctx.cpp`), used for
lookup and update alike. Verified first: every typed helper re-checks the map
type and otherwise calls `handler->map_lookup_elem/map_update_elem(...,
false)`, exactly the generic `bpftime_shm` path in base mode (including
ENOENT on a bad FD), so selection is only a performance choice. Rule:

- direct map-value LDDWs (global data) are ignored;
- maps the kernel never passes to lookup/update (prog array, perf event
  array, stack trace, ring buffer) are ignored;
- one of ARRAY / HASH / PERCPU_ARRAY / PERCPU_HASH -> its typed helper;
- arrays (ordinary and/or per-CPU) plus exactly one hash kind -> that hash
  helper (eligible array sites are inlined by the JIT, the rest take the
  typed helper's fallback);
- any other map kind (LRU hash, LPM, map-in-map, ...), both hash kinds, or
  both array kinds without a hash -> generic, as before.

This deviates from the plan above in one point: unknown lookup-capable map
kinds keep the generic helper instead of being ignored. Log strings are
unchanged. New tests `[typed-helper-selection]` (in
`test_array_jit_lifetime.cpp`, covered by the daily `[array-jit-lifetime]`
tag): selection and execution through `bpf_attach_ctx` for hash only, hash +
global data, hash + inlined array + global data + prog array, two hash kinds
and hash + LRU hash; typed vs generic helper equivalence for array, missing
and out-of-range handles.

`dev2-selection/` (dev build of exactly `c3090b59`, `run-dev.sh` with
`GAP_RUNNER=run-gap.py`): complete daily filter 72 cases / 128538
assertions passed. `run-gap.py` here is the gap runner with only the expected
selection changed (all three variants typed for hit/mixed, v3 still bound);
workloads are read from `../hash-lookup-helper-gap-20261021`. Six rounds,
`dev1` = `baf0cc85` (different run), raw ns per element (median):

| Variant | Case | Kernel | dev1 | dev2 | dev2/kernel | Change |
|---|---|---:|---:|---:|---:|---:|
| v1-hash-only | hit | 31.87 | 37.65 | 38.01 | 1.19x | +1.0% |
| v1-hash-only | mixed | 41.95 | 360.06 | 359.07 | 8.56x | -0.3% |
| v2-hash-globals | hit | 31.87 | 55.77 | 37.88 | 1.19x | -32.1% |
| v2-hash-globals | mixed | 42.10 | 390.56 | 359.11 | 8.53x | -8.1% |
| v3-hash-mixed | hit | 32.53 | 61.05 | 43.97 | 1.35x | -28.0% |
| v3-hash-mixed | mixed | 42.14 | 397.15 | 365.48 | 8.67x | -8.0% |

v2 now equals v1. v3 keeps about 6 ns over v1: the thread-local scope and
indirect call of bound programs (see fix 2; removing it needs the llvmbpf
overlay change). v1 code is unchanged; its +1.0% is within the spread of its
samples (37.1-38.1 ns). Against the original `bdd9835` gap run, v2 hit
56.64 -> 37.88 ns and v3 hit 76.75 -> 43.97 ns (-42.7%).

## After both fixes

Formal validation as for `../array-jit-global-data-20261021/formal`: clean
worktree build (LLVM 16, baseline options), complete daily filter, consuming
standard + globals runs and the hash gap runner against `bdd9835`, full ten
rounds plus an AB/BA screen if anything moves; then the LLVM 15 daily stack
replay (round 4) and adding `f24dc9b` (diagnostic), `baf0cc85` and `c3090b59`
to `config.json`. Use this directory's `run-gap.py` (new expectations).
