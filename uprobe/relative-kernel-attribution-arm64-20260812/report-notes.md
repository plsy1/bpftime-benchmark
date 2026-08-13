# Report and chart contract

## Audience and decision

- Audience: BPFtime developers and researchers reviewing ARM64 map-helper overhead.
- Decision: provide a strict top-level comparison that complements the already completed source-path diagnosis.
- Required evidence: strict matched BPFtime-minus-kernel gaps and PMU confirmation.

## Chart contract

- Question: Which valid map operations have the largest BPFtime-versus-kernel net cost gaps on Jetson?
- Takeaway: per-CPU hash update and lookup dominate; ordinary hash update is faster in BPFtime.
- Chart family: signed horizontal comparison/ranking bar chart.
- Grain: one row per valid lookup/update operation and map type.
- Metric: `BPFtime net ns/helper - kernel net ns/helper` after operation-specific matched-control subtraction.
- Baseline: zero means equal cost; positive is slower in BPFtime and negative is faster.
- Labeling: direct numeric labels and operation names; no redundant category-color legend.
- Exact lookup: the adjacent table preserves kernel cost, BPFtime cost, gap, cycles, and instructions.

## Omission and qualification rationale

- Array delete is unsupported and therefore omitted.
- Corrected per-CPU hash delete is not mixed into this new five-run matrix because its current top-level evidence has a different run count.
- Internal L0-L3 values come from standalone diagnostics and are intentionally not subtracted from the new top-level results.
- No “production-context residual” is reported because such a cross-harness remainder is not an independently measured runtime stage.

## QA

- `validate.py` verifies run counts, PMU coverage, wall-time stability, and final cleanup.
- `artifact.json` is the canonical portable-report payload generated from the reviewed CSV files.
- Portable HTML packaging could not run on this Jetson host because no Node.js executable is installed. No system package was added for report rendering; the Markdown report and canonical JSON remain complete and reproducible.
