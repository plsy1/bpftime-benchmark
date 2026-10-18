# Accepted ARM64 hash lookup optimizations — 2026-10-18

Runtime branch: `codex/uprobe-official-8da867d541f8`.

| Accepted change | Runtime commit | Previous ns/helper | Current ns/helper | Same-run kernel ns/helper | Current/kernel |
|---|---|---:|---:|---:|---:|
| Ordinary hash: choose key comparison loop before probing | `a1ef4a7` | 28.12 | 26.86 | 29.12 | 0.92x |
| Per-CPU hash: use bucket-local iterators | `df934cd` | 34.95 | 27.82 | 29.97 | 0.93x |

Each row comes from its own complete ten-round run, with a preceding paired
five-round A/B experiment. The modifications preserve the existing hashes,
map layouts and synchronization semantics. A tested pointer-stepping
ordinary-hash candidate was rejected because it did not improve performance.

- [Latest complete ten-operation table](percpu-hash-lookup-followup-20261018/full10/ten-operation-comparison.md)
- [Ordinary hash experiment and validation](ordinary-hash-loop-20261018/README.md)
- [Per-CPU hash profile, experiment and validation](percpu-hash-lookup-followup-20261018/README.md)
- [Original unoptimized ten-round baseline](uprobe-top-arm64-20261007-strict-repro-official-no-btf/benchmark-output.json)
- [Previous per-CPU-flatten ten-round baseline](percpu-hash-flatten-full10-arm64-20261018/benchmark-output.json)

The latest full run has eight of ten map operations below same-run kernel
BPF. Ordinary array lookup (2.08x) and per-CPU array lookup (2.64x) remain
slower. Ordinary hash update remains an observation item: its full-run mean
rose 1.7%, with both builds showing modes near 33 and 36 ns/helper; a small
regression has not been ruled out.
