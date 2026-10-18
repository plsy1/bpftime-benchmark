# Ordinary hash lookup C++ implementation experiments

Baseline: `09e1576` (latest accepted per-CPU hash flatten optimization).
The immutable baseline libraries are in
`/tmp/bpftime-hash-loop-20261018.C7fgIJ/baseline`.

The runtime uses GCC 13, RelWithDebInfo (`-O2`), LTO enabled,
probe read/write checks disabled and production A/B switches compiled out.
The CPU is pinned to CPU 5 at 1728000 kHz.

Candidates are tested individually against the same baseline:

1. `key-hoist`: select a uint32 or generic comparison loop before probing;
   load the uint32 query key once outside the loop.
2. Pointer stepping: replace per-probe `index * bucket_size` addressing
   with incrementing a bucket pointer and wrapping at the table end.

The hash calculation, prime bucket count, stored data layout, tombstone
rules and locking are preserved.

`run-phase.sh` checks fixed-hash correctness, builds and snapshots the
libraries, then runs five paired rounds of the existing production-mode
helper-closure benchmark. Engine order alternates between rounds.
The short benchmark reports control-subtracted nanoseconds per helper;
these numbers must not be directly mixed with the raw full-suite table.

The regression test covers 1, 3, 4, 8 and 16-byte keys, unaligned query
keys, 1 and 8-byte values, collisions starting in the final bucket,
wraparound, full tables, tombstones and reinsertion. Long keys share
their first four bytes to verify that generic comparisons inspect the
full key.

Only a candidate with convincing short A/B evidence should proceed to
the full ten-round uprobe suite, including same-run kernel measurements.

## Short A/B decision

| Candidate | Baseline median ns/helper | Candidate median ns/helper | Change |
|---|---:|---:|---:|
| Key-path selection outside the loop | 26.9872 | 25.8627 | 4.17% lower |
| Pointer stepping alone | 27.1628 | 27.5364 | 1.38% higher |

The key-hoist candidate was faster in all five paired lookup rounds.
Other short-run hash results are retained in each phase's `ab5/summary.json`;
ordinary hash update shows greater run-to-run variability and still needs
the full-suite regression check. Short-run kernel lookup median for key-hoist
was 25.8600 ns/helper, effectively tied with the candidate.

Selection: retain key-hoist and discard pointer stepping. The final
production header is byte-for-byte identical to the measured key-hoist
snapshot. `run-selected.sh` reruns the expanded correctness tests, restores
the working build, checks that both library binaries match the measured
snapshot, and starts the complete ten-round uprobe suite.

The full-suite summary automatically compares the current result with the
original unoptimized 2026-10-07 run and the immediately previous accepted
per-CPU-flatten run. It includes same-run kernel values for all ten operations.

## Complete ten-round result

Completed successfully at 2026-10-18 15:25:46 JST (exit status 0).
Benchmark duration: 1508.83 seconds, approximately 25 minutes.
All 17 kernel cases, 17 userspace cases and the embedded case have ten
positive, finite samples. Both measured library checksums remained unchanged.

Ordinary hash lookup, using the raw full-suite average divided by 1000:

- Original unoptimized BPFtime: 58.4725 ns/helper.
- Previous accepted version: 28.1163 ns/helper.
- Current candidate: 26.8642 ns/helper (4.45% less than previous).
- Same-run kernel: 29.1245 ns/helper; current/kernel is 0.9224.
- Current lookup samples span 26.7740–27.0387 ns/helper, while all kernel
  lookup samples span 29.0268–29.2050 ns/helper.

The full-suite ordinary hash result confirms the short paired A/B gain.
The previous full-run kernel average was 28.2084 ns/helper, so some of the
change in the cross-run BPFtime/kernel ratio also reflects kernel variation;
the separate paired A/B establishes the implementation's own benefit.

Other operations have no clear regression in this run. The small array and
ordinary-hash-delete mean increases are accompanied by outliers; their
medians remain within approximately 0.5% of the previous run. Do not attribute
the ordinary hash update mean reduction to this lookup-only change.

Seven of the ten map-operation averages are now below same-run kernel BPF.
The remaining slower operations are ordinary array lookup (2.10×), per-CPU
array lookup (2.67×) and per-CPU hash lookup (1.19×).

See [the ten-operation comparison](full10/ten-operation-comparison.md) and
[the full benchmark report](full10/results.md).

Accepted runtime commit: `a1ef4a7e69b3cf17b120a3148a3a4ba1d580a8b9`.
