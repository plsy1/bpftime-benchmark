# Ordinary array lookup JIT inline POC

Independent source snapshot: `/home/jetson/src/bpftime-array-jit-poc-20261019`.
Base source is `fefe2f6` plus the separately validated, uncommitted offset-pointer
branch C++ candidate. Original worktree, libraries, pending changes, and replay
configuration are not modified. POC is not a production-ready optimization.

Reuse llvmbpf's existing array inline pass, bridge array descriptor registration
through VM compatibility/C APIs, and opt in in the runtime loader only via
`BPFTIME_ARRAY_JIT_POC=1`. LLVM only; other backends return ENOTSUP. Maps and
shared-memory mappings must remain alive and unchanged until the victim exits.
Dynamic close/reuse/reset/remap is explicitly unsupported in this first POC.
No global raw-pointer cache, automatic enablement, or change to shared layouts.

Unlike the original pass, invalid-key branches retain the originally registered
helper call, preserving its failure/errno semantics. Key load alignment is 1
rather than assuming all input key pointers are aligned to 4 bytes. Runtime
registration uses metadata from the implementation, not duplicate handler
attributes, and rejects >32-bit aggregate-size arrays from specialization.

The original benchmark's unused lookup result can be simplified after inlining.
New matched tests consume lookup values into an observable result, use
runtime-dependent keys, check hit/miss counts and checksums, and inspect real
JIT behavior. Old formal ten-item results must not be overwritten or compared
as if the workload were unchanged. Same candidate library with POC off/on
isolates registration/inlining; the old frozen C++ baseline is an extra control.

Validation covers VM bridge correctness (hit bypasses helper, miss calls helper once,
errno, key widths, unaligned key, live backing, dynamic/unregistered fallback,
unsupported backend), existing array/hash regression tests, then matched
uprobe samples under tmux with fixed CPU/frequency. No commit/push or production
acceptance is authorized by this POC result alone.

## Completed validation

Build, regression tests, four-engine smoke tests and six paired benchmark rounds
completed successfully. The 20 selected test cases passed 106,733 assertions.
All 24 benchmark process runs passed checksum/hit/miss checks; all 48 frequency
checks recorded CPU 5 at 1,728,000 kHz. Artifact hashes stayed unchanged.

| New consuming workload | POC off | POC inline | Same-run kernel | Inline vs off | Inline/kernel |
| --- | ---: | ---: | ---: | ---: | ---: |
| All hits | 7.8933 ns | 3.0816 ns | 5.8842 ns | -60.96% | 0.524x |
| Mixed hits/misses | 17.2517 ns | 11.0473 ns | 12.3074 ns | -35.96% | 0.898x |

Values are medians of raw ns/element; both cases improved in all six paired
rounds. The independent frozen C++ control agreed closely with POC off. These
measurements include key generation, value reads and checksum work, not just
the lookup helper; they do not replace the old ten-operation table.

Actual JIT entry dumps show direct backing-array address calculation and value
loads on the valid-key fast path. Helper call sites remain in cold invalid-key
fallback blocks. The unused-result case can simplify and is excluded from all
optimization-gain claims; its loop was not shown to disappear entirely.

Detailed results and code-window analysis: [RESULT.md](RESULT.md). Source files,
patch, digests, logs, raw samples and analysis scripts are preserved here.
Initial failures (missing kernel BTF in the diagnostic and static test factory
retention) were corrected; their logs are retained separately. The diagnostic
uses a fixed ARM64 register layout without CO-RE, tested on this Jetson kernel.

This validates the performance potential, not production readiness. Before
integration, map ownership and invalidation across close/reuse/reset/remap must
be addressed. The experiment remains opt-in, ordinary-array/LLVM-only, in an
independent source copy. Nothing has been committed or pushed.
