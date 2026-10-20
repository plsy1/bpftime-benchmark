# Ordinary-array JIT integration and lifetime validation

Source: `/home/jetson/src/bpftime-array-jit-integrated-20261020`.
Independent copy of the 2026-10-19 POC, preserving the prior POC's code,
binaries and evidence. The validated integration has also been applied to
`/home/jetson/src/bpftime-daily-state/worktrees/8da867d541f8/candidate`.
All 27 archived source files match that primary worktree byte-for-byte. Its
earlier C++ offset-pointer candidate and unrelated diagnostic changes remain.
The original primary files are backed up in `primary-before-integration/`.

Integration uses the existing shared map reference count, a program-owned map
binding, and shared ownership of the exact managed mapping addressed by the JIT.
FD close/reuse, server reset and unlink/recreate must not redirect the binding.
Both inlined hits and the fallback helper use the same retained identity.
Handler mutation and reference acquisition are serialized on a named recursive
interprocess mutex; no map/handler shared layout changes and no per-hit lock.

Opt-in: `BPFTIME_ARRAY_JIT_INLINE=1`; default remains off. First integration is
LLVM JIT, ordinary-array-only, lookup-only programs using the known standard
array helper. Other map operations, custom helpers, diagnostic modes, direct
map-value relocations, interpreter, uBPF, CUDA and MPK keep their existing path.
Bound programs explicitly reject signal execution and AOT replacement. The
ordinary-array scope is not expanded to per-CPU or hash JIT specialization.

The internal standard-helper name `bpftime_bound_array_lookup_v1` permits the
JIT to canonicalize a proven out-of-range access to invalid FD -1. The fallback
then returns the same ENOENT/null without consulting the binding or FD tables.
Custom helpers retain both their arguments and their original error behavior.
Deferred one-time helper registration also handles interpreter-to-JIT reloads
without changing an already registered LLVM helper slot.

An atfork child generation makes inherited bound programs return ENOTSUP.
They must be reloaded in the child; inherited snapshots do not decrement the
parent's reference count or free its shared name storage. This adds no getpid
syscall to a normal invocation. Concurrent attach/detach quiescence, raw clone,
fork from a signal interrupting active JIT code, abrupt-process cleanup and
mixing incompatible runtime versions are not claimed. Management APIs still
require their existing external serialization. A crashed owner can retain
shared references until segment removal. Default enablement is intentionally
not changed.

Tests cover map identity after FD close/reuse/reset, miss errno against a
larger replacement map, retained mappings across unlink/recreate, dup updates,
last-reference reclamation, concurrent close/replacement, another process
closing the FD, fork-child isolation, failed-load safety, interpreter reloads,
metadata consistency, and custom/dynamic-helper fallback. Builds and tests run in
tmux `uprobe-array-jit-integrated-20261020`.

Benchmark methodology retains the observable checksum workload from the POC.
It is not directly comparable to the old unused-result ten-operation table.
No commit/push or daily-workflow deployment has been performed.

## Final locked-version checks

- 31 selected correctness/regression cases: 108,768 assertions passed.
- 18 host-contract/configuration/allocation cases: 201 assertions passed.
- Five-engine smoke test and six paired rounds: all 30 checksum/count checks,
  60 CPU-frequency checks and immutable-artifact checks passed.
- Additional three-round non-target array/hash comparisons passed the 3%
  median regression gate for historically comparable operations.

Final consuming-workload medians (raw ns/element, including probe, runtime key
generation, value read and checksum):

| Case | Inline off | Inline on | Same-run kernel | vs. off | Inline/kernel |
| --- | ---: | ---: | ---: | ---: | ---: |
| All hits | 7.8950 | 3.0941 | 5.8830 | -60.81% | 0.526x |
| Mixed hits/misses | 17.5412 | 9.6320 | 12.2370 | -45.09% | 0.787x |

Both cases won all six pairs. These are workload-level elapsed times, not
isolated helper latencies, and must not replace the historical unused-result
array-lookup row. The unused-result demonstration is excluded from gain claims.

Detailed evidence: `RESULT.md`, `ab6-accepted/`, `smoke-accepted/`,
`array-regression3/`, `hash-regression3/`, `source-sha256.json` and
`integration-vs-cpp-candidate.diff`. The generated LLVM translation unit uses
the parent-owned checked patch; the official submodule remains unmodified.

The full ten-round suite completed in 1,495.79 seconds (24m 55.79s): 17 kernel,
17 userspace and one embedded-VM case each have ten finite positive samples.
`full10/exit-status.txt`, `accepted-exit-status.txt` and
`full10/post-validation-exit-status.txt` all contain 0. The final comparison is
`full10/ten-operation-comparison.md`; all seven frozen artifact digests passed.
The source tree, user diagnostic files, and clean official LLVM submodule were
rechecked. No owned benchmark process or default BPFtime SHM remains.

The original timing runner did not pass INFO to its victims, so the original
INFO-count-only postcondition exited 2 after all timing samples completed.
That original exit and script are preserved as `full10/*before-activation-audit*`.
`finalize-full.py` resumed all remaining validation, using the exact same loader,
BPF object, agent/server and victim in an explicitly logged auxiliary run
(17 cases, 1,000 iterations; excluded from timing data). It confirmed the pinned
`array_map_looku` program and actual JIT inline log. The validation runner is
fixed to use this separate audit; the ten timing samples were neither rerun nor
modified. Details: `full10/final-validation.json` and `full10/activation-audit/`.

The other nine map-operation means changed by -0.5% to +1.4% versus the prior
full run; no material map regression was found. For completeness, write-helper
mean increased 4.08% because three samples were around 4,834–4,851 ns/invocation;
its median changed only 0.084% (4,271.59 to 4,275.20). This is not counted as an
optimization gain or proof of a persistent regression. Standalone uprobe,
uretprobe and their combination changed +0.89%, +0.61% and +1.83%; read helper
-0.24%, embedded VM mean -3.00%.

Earlier stages are retained separately: `ab6/` exposed the first integration's
13.71 ns mixed-case fallback overhead; `ab6-final/` fixed that; `ab6-release/`
added the reload guard; `ab6-accepted/` additionally contains fork isolation.
Prior sources and relocated binary hashes are preserved in the corresponding
`source-before-*` and `artifacts-before-*` archives. Initial helper-registration
and backend-test-assumption failures are logged separately, not hidden.
