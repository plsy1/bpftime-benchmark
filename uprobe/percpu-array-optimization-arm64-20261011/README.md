# Per-CPU array production optimization — Jetson ARM64, 2026-10-11

## Scope

First implementation step after the August attribution experiments and September/October environment recovery.
Only `ensure_on_current_cpu` changes: a forwarding-reference callable replaces `std::function`.
The per-CPU array lookup/update bodies, current-CPU selection, error handling, data layout, map dispatch and BPF programs are unchanged.
No diagnostic shortcut is enabled. This is not the earlier `percpu_array_direct` ablation.

Source: `/home/jetson/src/bpftime`, branch `codex/uprobe-map-optimization`, based on `1a1dc0d6c2026a5e4b0e4faa964e3eea8ccdd241`.
The source patch and validation script will be captured with each A/B run.

## Controls

- Preserved baseline: `build-uprobe-strict-20260910` (the September 10 / October 7 baseline).
- Candidate: `build-uprobe-opt-20261011` (separate build directory).
- GCC/G++ 13, LLVM 15, RelWithDebInfo, LTO ON, read/write checks OFF, compiled log level INFO.
- Candidate additionally builds unit tests; runtime compiler flags/definitions match the baseline (only build-directory include paths differ).
- CPU5 pinned to 1.728 GHz under MAXN_SUPER; frequency checked before and after each matched victim.
- Matched harness: existing `helper-map-closure`, key 4 B, value 8 B, 1,000 helper calls/invocation, operation-specific controls, setup/warmup outside timing.
- Formal A/B: 5 independent victims per engine, 20,000 invocations/case, 1,000 warmup invocations/case. Engine order alternates and case order rotates.
- Net helper time: `(operation ns/invocation - corresponding control ns/invocation) / 1000`; compare medians across independent victims.
- Ordinary array lookup/update are unchanged-code controls, not targets of this optimization.

## Evidence / status

- `build.log`: independent candidate build, including tests.
- `baseline-update-disassembly.txt`, `candidate-update-disassembly.txt`: original update path calls `operator new(32)` and a `std::function` invoker/destructor; candidate has no such wrapper allocation or indirect call in this function.
- `unit-tests.log`: all 7 selected map test cases passed (3,514 assertions), including 2 new per-CPU array tests for errors/value preservation and concurrent CPU-slot writes at 8/24/256 B; existing tests cover helper/userspace exchange and per-CPU hash/ordinary hash behavior.
- `smoke/`: 1 short victim per engine (2,000 invocations, 100 warmup), pipeline validation only; not used for the formal effect estimate.
- `validation/`: formal 5-round matched A/B, followed by the candidate's 10-round official uprobe regression. `environment.json` records running/success/failed status; `full/benchmark.log` contains live full-suite progress.
- Background session: `uprobe-opt-20261011`. `run.log` contains matched progress and the summary.

## Reproduction

From `/home/jetson/src/bpftime`, with no other BPFtime experiment running and the known-unused prior shared memory removed:

```sh
python3 -u benchmark/uprobe/diagnostics/compare_runtime_builds.py \
  --baseline build-uprobe-strict-20260910 \
  --candidate build-uprobe-opt-20261011 \
  --output /absolute/path/to/new-output-directory \
  --runs 5 --iterations 20000 --warmup 1000 --full
```

The output directory must be new. The runner records binary hashes, runtime source diff, both CMake caches and the benchmark scripts; it rejects missing/nonfinite samples and checks 17 kernel + 17 userspace + 1 embedded-VM series with 10 values each before marking the full suite successful.

## Formal matched results

5 rounds completed. Median net ns/helper (not full-suite raw ns/invocation):

| Operation | Kernel | Baseline | Candidate | Candidate time change |
|---|---:|---:|---:|---:|
| Ordinary array lookup | 1.364 | 14.828 | 15.014 | +1.25% |
| Ordinary array update | 10.889 | 23.076 | 23.121 | +0.20% |
| Per-CPU array lookup | 1.975 | 26.686 | 20.634 | -22.68% |
| Per-CPU array update | 14.415 | 75.690 | 27.142 | -64.14% |

Source: `validation/summary.json`; individual timings: `validation/wall-raw.csv` and per-engine logs.
Both target operations improved in every measured round. Ordinary array lookup is slightly slower despite unchanged source; do not dismiss it as noise without further checking. Binary-layout effects are a possibility, not an established cause.

Full official regression finished successfully at 2026-10-11 23:15 JST. The runner validated and archived all 35 series: 17 kernel, 17 BPFtime and 1 embedded-VM series, each with 10 samples. Binary hashes and the CPU5 1.728 GHz lock remained unchanged throughout the experiment.

## Boundaries

Matched-harness improvement is not an end-to-end application speedup. The full official uprobe benchmark is a separate regression check.
Do not count array deletion rejection as successful deletion throughput, or interpret earlier diagnostic deletion deferral as a production optimization.
Per-CPU hash copy/traversal, deletion reclamation and ordinary-map dispatch are subsequent work, not changes in this candidate.
