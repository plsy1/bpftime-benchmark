# Per-CPU hash lookup follow-up

Baseline: commit `09e1576` plus the validated ordinary hash key-hoist patch
from `ordinary-hash-loop-20261018/full10`. Do not drop that patch when
restoring this experiment's baseline.

Baseline binary snapshots:
`/tmp/bpftime-percpu-lookup-20261018.iyqaA3/baseline`.
Baseline source copies are in `baseline/` in this result directory.

## Current-version profile

The 200,000-invocation lookup-only profile completed successfully. Of 7,492
CPU sampling events, 6,190 have per-CPU `elem_lookup` as the top frame, 394
have its specialized helper, and 293 have libc `sched_getcpu`. The capture
includes initialization and warmup; these are sample counts, not isolated
component timings. Backtraces across the JIT boundary are incomplete, so
the assessment relies on resolved top-frame samples and the actual assembly.

The production lookup function is 1,812 bytes and reserves a 192-byte stack
frame. Its uint32 lookup path materializes several shared-memory offset
pointer and global-iterator temporaries. Merely having more code does not
prove that splitting the function will improve performance.

## First candidate: bucket-local iterators

For four-byte keys, use the map's public `bucket(key)`, `begin(bucket)` and
`end(bucket)` interface to visit the same chain without constructing a
global iterator and its bucket-group state. Retain the existing hash,
equality predicate, shared-memory map representation and locking policy.
The generic-key lookup remains unchanged. Empty maps are checked before
accessing bucket-local iterators.

The first attempt exposed a Boost local-iterator `operator->` compilation
error: its implementation returns a node pointer where a value pointer is
required. Candidate `local-iterator-v2` uses the working public `operator*`
and an entry reference. No installed Boost headers were modified.

The existing per-CPU tests plus the new lookup regression case cover empty
maps, null keys, missing keys, unaligned input, colliding uint32 keys,
generic keys sharing the same uint32 prefix, deletion and per-CPU values.
`run-phase.sh` tests correctness, builds and snapshots the production
libraries, then runs five alternating A/B rounds with same-round kernel
controls. Short A/B numbers subtract operation-specific control overhead.

## Five-round A/B result

Candidate `local-iterator-v2` passed all seven per-CPU hash test cases
(6,347 assertions). Production lookup code size fell from 1,812 to 1,520 bytes.

| Operation | Baseline median ns/helper | Candidate median ns/helper | Same-run kernel median ns/helper | Candidate change |
|---|---:|---:|---:|---:|
| Ordinary hash lookup | 25.8480 | 26.0763 | 26.3439 | +0.88% |
| Ordinary hash update | 33.1775 | 35.6380 | 92.0466 | +7.42% |
| Per-CPU hash lookup | 34.1545 | 26.7730 | 26.8536 | -21.61% |
| Per-CPU hash update | 44.2443 | 44.2998 | 65.5596 | +0.13% |

Per-CPU lookup improves in every paired round. Ordinary hash update values
show two clusters near 33 and 35–36 ns/helper in both builds; the five-round
median difference is not enough to dismiss or establish a regression.
Verify it in the complete ten-round run before accepting the new candidate.
The small ordinary lookup shift also remains part of the full-suite check.

Decision: proceed to full ten-round validation with immutable candidate
libraries. Compare the full result with both the original unoptimized
2026-10-07 run and the immediately previous ordinary-key-hoist full run.
Do not directly compare control-subtracted short A/B times with the raw
full-suite per-helper numbers.

## Complete ten-round validation

Completed successfully at 2026-10-18 21:10:44 JST, exit status 0, in
1500.07 seconds. All 17 kernel cases, 17 userspace cases and the embedded
case have ten finite positive samples. Both frozen library checksums passed.

Per-CPU hash lookup, raw full-suite average divided by 1000 helpers:

- Original unoptimized BPFtime: 141.25 ns/helper.
- Immediately previous accepted result: 34.9530 ns/helper.
- Current candidate: 27.8153 ns/helper, 20.42% lower than previous.
- Same-run kernel: 29.9691 ns/helper; current/kernel is approximately 0.93.
- All ten current lookup samples are below the previous version's samples.

The complete run confirms the improvement independently established by
the five-round paired A/B. Eight of the ten map-operation averages are
now below their same-run kernel counterparts. The remaining slower cases
are ordinary array lookup (2.08x) and per-CPU array lookup (2.64x).

Ordinary hash update remains an observation item: its full-run mean rose
from 34.3201 to 34.9159 ns/helper (+1.74%). Both runs contain modes near
33 and 36 ns/helper; the previous run has three high-mode samples and
the current run has six. Its median consequently shifts much more than
its mean. This does not establish a sustained regression, but these
measurements alone cannot completely rule out a small regression either.
Do not report an unconditional all-operations-no-regression claim.

Ordinary hash lookup is +0.41% in latency and remains below same-run kernel.
Its ratio moving from 0.92x to 0.96x also reflects the kernel average
changing from 29.1245 to 28.1937 ns/helper across the two complete runs.

See [the ten-operation comparison](full10/ten-operation-comparison.md) and
[the full benchmark report](full10/results.md). Accepted runtime commits are
`a1ef4a7e69b3cf17b120a3148a3a4ba1d580a8b9` (ordinary key hoist) and
`df934cd13027a2d14acec3e7fbfa87b144adab1e` (per-CPU bucket-local lookup).
Native profiler databases and reports remain host-only; portable profile
logs, source snapshots and the measurements are archived in this branch.
