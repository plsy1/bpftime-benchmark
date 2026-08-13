# Matched array-helper closure on ARM64

This directory contains the strict array lookup/update comparison used by `../relative-kernel-attribution-arm64-20260812/`.

- `run-wall.sh`: five interleaved wall-time runs per engine, 50,000 victim iterations.
- `run-pmu.sh`: three paired PMU rounds per engine, metric, and case, 20,000 victim iterations.
- `summarize.py`: regenerates all CSV summaries from `raw/` and `raw-pmu/`.
- `closure.csv`: operation-specific control subtraction and BPFtime-minus-kernel gaps.
- `pmu-closure.csv`: the same closure for cycles and instructions.
- `environment.txt`: source/runtime identities, CPU topology, frequency, and cleanup state.

Each real case and control has the same 1000-iteration loop and key/value preparation. The only intended difference is the actual map-helper call.
