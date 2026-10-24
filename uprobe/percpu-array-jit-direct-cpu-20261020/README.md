# Direct CPU query in the inlined per-CPU array lookup

The candidate optimizes the ALREADY INLINED version; it does not generalize
eligibility or optimize the bypassed C++ map lookup helper. It is uncommitted,
not pushed and still behind the default-off `BPFTIME_ARRAY_JIT_INLINE=1` option.

## Six-round consuming short test

Same BPF object, loader and victim, runtime-generated keys, actual value reads,
verified checksums/hit counts, live maps; CPU 5 at 1,728 MHz. Both previous and
current versions have JIT inlining ON. These are extra diagnostic workloads,
not the original unchanged uprobe benchmark. Lower is better, ns/element:

| Case | Previous INLINE | Current INLINE | Same-run kernel | Change vs previous |
|---|---:|---:|---:|---:|
| All hits | 6.6501 | 5.1515 | 6.8276 | -22.54% (6/6 wins) |
| Approximately half misses | 12.4826 | 11.5081 | 12.7191 | -7.81% (6/6 wins) |

Current/kernel is approximately 0.75x all-hit and 0.90x mixed. Control changes
are -0.35% and -0.20%. All 18 per-CPU runs verify four checksums/counts and CPU
5 result slots. Ordinary-array consuming results are separately retained in
`ordinary-ab3/`; no claim is based on the `unused` case. Do not mix these data
with historical unconsumed-return pure map-operation latency or treat the
added work as isolated helper latency.

## Implementation and safety

The original native helper 8 queries CPU, checks -1, sign-extends the result,
and reports failure/returns CPU 0. JIT now emits that small decision directly
and calls the existing public CPU API, avoiding an intermediate C++ wrapper
call/frame and five unused argument moves. The CPU API is not cached, marked
pure or replaced with private TLS offsets. Bounds, ownership, FD reuse/fork,
null-key/errno and signal/AOT limitations remain unchanged.

The LLVM overlay optionally resolves a cold runtime provider, which recognizes
the EXACT standard CPU helper pointer and supplies raw query/error callbacks.
There is no mutable global registry, extra BPF helper number or public helper
ABI replacement. Missing providers/custom helpers preserve the old path.
The shared cold error callback is used by both native helper 8 and the JIT:
raw -1 logs once and returns CPU 0; other negative values sign-extend and fail
CPU bounds. Production agent/server do NOT include CPU fault-injection wrappers.

Validation passed: 37 cases / 110,284 assertions and 18 host-contract cases /
201 assertions. Includes test-only CPU/failure injection, exact query counts,
log-once/CPU-0 failure behavior, negative/out-of-bounds CPU, real affinity changes,
null/invalid keys, unaligned/arbitrary widths, retained maps and fork isolation.
Actual JIT windows show raw i32 -1 checks and no five-register zero-argument
sequences. First-ret instruction counts are not whole-function/latency models.

The independent native VM executable is linked WITHOUT runtime. Its weak
provider is absent, yet per-CPU lookup/query/bounds/errno tests pass. This
validates that the new integration does not require the runtime in standalone
llvmbpf applications (`standalone.log`, `standalone-exit-status.txt`).

## Non-target screen and full suite

Array screen: 3 paired rounds; hash screen: 12 paired rounds, with known bimodal
ordinary hash update reviewed using its mean rather than a three-point median.
Candidate/previous ratios: ordinary array update 1.0002, per-CPU array update
0.9993, ordinary hash lookup 0.9964, hash update mean 0.9848, per-CPU hash lookup
1.0012, hash update 0.9990. No >3% regression. Non-target changes are not claimed
as intended optimizations. Exact binaries and CPU frequency checks pass.

The inherited regression driver has a stale convenience `base_source_commit`
label. Canonical baseline identity is the frozen per-CPU full10 artifact hashes
and `../percpu-array-jit-inline-20261020/source-sha256.json`, not that label.
Current source is the 33-file checked archive in `source/` and `source-sha256.json`.

Full 35-case x 10 suite is launched only after these gates. Completion status,
the ten-operation comparison and exact-artifact activation audit are under
`full10/`. Both array-lookup historical rows remain marked non-comparable
because their original BPF programs do not consume lookup results.

The earlier zero-argument-alias-only attempt failed its performance threshold
and was restored, not combined with this candidate; its sources, binaries,
valid six-round data and shell-runner error are retained separately under
`../percpu-array-jit-cpu-abi-20261020/`.

Source/build: `/home/jetson/src/bpftime-percpu-array-jit-20261020`.
Primary: `/home/jetson/src/bpftime-daily-state/worktrees/8da867d541f8/candidate`.
tmux: `uprobe-percpu-direct-cpu-20261020`.
