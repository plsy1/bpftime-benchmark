# ARM64 BPFtime versus kernel map-path task

## Scope decision

All remaining experiments run on the Jetson ARM64 host. Do not run additional
x64 experiments and do not modify, rebuild, or boot a diagnostic Linux kernel.
The x64 kernel-internal work is closed as comparator background.

The research target is BPFtime on ARM64:

1. Explain why BPFtime array update costs more than kernel BPF on Jetson.
2. Explain why kernel BPF wins array lookup on Jetson.
3. Explain why kernel BPF wins hash lookup on Jetson.

Do not investigate array delete: kernel array maps do not support meaningful
element deletion. Hash update and hash delete have the same winner on both
architectures and are outside this next phase.

## Repositories

Source:

```text
https://github.com/plsy1/bpftime-benchmark.git
branch: codex/official-no-btf
current diagnostic commit: 176eb291ead95eb5f8a56280deae626fac46eaa9
official top-level result runtime commit: 8ed291e130fe3f15f99955b0d259eb119efdaa7d
```

Results:

```text
branch: benchmark-results/jetson
handoff parent: df2610f120481c6c635f3fbbc0edda7d33784a71
```

Start by recording branch, commit, and dirty status. Do not clean or overwrite
pre-existing Jetson changes. Verify whether production runtime files differ
between the official top-level commit and the diagnostic commit. If they do,
do not silently combine those measurements; report the mismatch and rerun the
minimum exact-commit comparison needed.

## Definitive top-level ARM64 values

Source archive:

```text
uprobe/uprobe-top-arm64-20260803/
```

All values are five-process medians after subtracting the same-process empty
uprobe and dividing by 1000 helpers:

| Operation | kernel BPF | BPFtime | BPFtime minus kernel | Winner |
|---|---:|---:|---:|---|
| array lookup | 2.563 ns | 12.901 ns | +10.338 ns | kernel |
| array update-existing | 11.858 ns | 16.426 ns | +4.568 ns | kernel |
| hash lookup-hit | 27.808 ns | 54.608 ns | +26.800 ns | kernel |

These three within-Jetson gaps are the quantities to explain. Cross-platform
ratios are not the target of this phase.

## Existing evidence to audit first

Read these archives before running anything:

```text
uprobe/uprobe-top-arm64-20260803/
uprobe/array-path-arm-diagnosis-20260801/
uprobe/hash-path-arm-diagnosis-20260801/
uprobe/kernel-map-runtime-arm64-20260801/
uprobe/kernel-map-runtime-arm64-20260803-profile/
uprobe/kernel-array-update-sizes-arm64-20260803/
uprobe/map-path-x64-20260802/README.md
```

The last file contains a convenient combined layer table, but only its ARM64
columns are in scope. Do not use x64 values to drive the ARM64 attribution.

Important existing ARM64 measurements:

| Path | Wall ns/helper | cycles/helper | instructions/helper |
|---|---:|---:|---:|
| BPFtime array lookup JIT real-minus-noop | 10.063 | 17.374 | 85 |
| BPFtime array update JIT real-minus-noop | 14.507 | 25.047 | 134 |
| BPFtime array lookup direct L3 | 8.920 | 15.400 | 89 |
| BPFtime array update direct L3 | 13.178 | 22.753 | 131 |
| kernel array lookup runtime A/B | 1.384 | not collected in that archive | not collected |
| kernel array update runtime A/B | 10.952-11.037 | 19.034 | 82.029 |
| BPFtime hash lookup JIT real-minus-noop | 41.558 | 71.754 | 270.905 |
| kernel hash lookup runtime A/B | 26.769 | not collected in that archive | not collected |

The kernel 8B value-size result (`8.978 ns`, `15.521 cycles`, `69.016
instructions`) uses a different fixed-struct BPF template. Keep it as
supporting evidence only; do not substitute it for the original top-level or
kernel-map-runtime denominator without explicitly accounting for the source
and control difference.

## Task 1: ARM64 array update closure

First determine how much can be concluded from existing data without a rerun.
Audit source semantics, commits, build options, warm-up, iteration count, map
shape, key/value initialization, and control subtraction.

The current numerical closure hypothesis is:

```text
top-level gap
  = 16.426 - 11.858
  = 4.568 ns/helper

direct helper gap
  = BPFtime direct L3 13.178 - kernel runtime 10.952
  = 2.226 ns/helper

BPFtime JIT-context premium
  = BPFtime JIT helper 14.507 - BPFtime direct L3 13.178
  = 1.329 ns/helper

remaining top-integration difference
  = 4.568 - 2.226 - 1.329
  = 1.013 ns/helper
```

Treat this only as a hypothesis until the harness audit establishes that the
subtractions are compatible. If compatible, report this closure with
uncertainty and exact source references. If incompatible, build the minimum
matched diagnostic needed to reproduce these three boundaries on the same
commit and configuration.

Use the existing BPFtime cumulative layers to explain the helper body:

| BPFtime array-update layer | ARM64 incremental ns | cycles | instructions |
|---|---:|---:|---:|
| map implementation | +2.771 | +4.785 | +48 |
| handler | +6.166 | +10.647 | +60 |
| shared-memory/fd dispatch | +5.941 | +10.257 | +41 |
| LTO-shaped final helper delta | -1.701 | -2.936 | -18 |

L3 is not mechanically L2 plus a wrapper because runtime LTO reshapes the
complete helper. Preserve the negative L3-L2 delta; do not force the layers to
be additive or label every gross BPFtime layer as overhead relative to kernel.

The final answer must distinguish:

- work intrinsic to updating an 8-byte array value;
- BPFtime-only generic handler and fd/variant dispatch;
- JIT-to-helper context premium;
- top-level uprobe/integration residual;
- kernel denominator work.

## Task 2: ARM64 array lookup

Explain the top-level `+10.338 ns/helper` BPFtime excess.

Start from the existing BPFtime layers:

| Layer | ARM64 cumulative net |
|---|---:|
| L0 array implementation | 0.228 ns / 0.393 cycles / 14 instructions |
| L1 handler | 4.989 ns / 8.611 cycles / 64 instructions |
| L2 shm/fd/variant | 9.947 ns / 17.173 cycles / 98 instructions |
| L3 production helper | 8.920 ns / 15.400 cycles / 89 instructions |
| LLVM JIT real-minus-noop | 10.063 ns / 17.374 cycles / 85 instructions |

The kernel runtime denominator is 1.384 ns/helper, but matched kernel
cycles/instructions were not collected in the original archive. Add a matched
kernel control/real PMU run only if required to answer the attribution. Keep
the original loop, warm-up, key semantics, and program shape unchanged.

Close direct L3, JIT helper A/B, and official top-level values. Quantify how
much of the gap is map implementation, handler, shared-memory/fd dispatch,
JIT context, and top integration. Do not rely only on BPFtime/kernel ratios.

## Task 3: ARM64 hash lookup

Explain the top-level `+26.800 ns/helper` BPFtime excess using the existing
hash direct layers and JIT helper A/B.

The existing ARM64 JIT result is approximately:

```text
BPFtime hash lookup-hit: 41.558 ns, 71.754 cycles, 270.905 instructions
kernel hash lookup-hit runtime: 26.769 ns
```

Audit and report the cumulative/incremental costs for lock, map implementation
after lock, handler, shared-memory/fd dispatch, final helper, JIT context, and
top integration. Existing layer data indicate that the map body and generic
handler are substantial; verify the exact ARM64 values from the raw CSV rather
than copying rounded values from a cross-architecture summary.

Collect matched kernel PMU counters only if existing evidence cannot separate
the BPFtime numerator from the kernel denominator. Do not modify the kernel.

## Execution controls

When a rerun is necessary, preserve the established Jetson conditions:

- host execution only, no Docker;
- Jetson Orin Nano, CPU5 pinned, no SMT sibling;
- MAXN_SUPER and `jetson_clocks`, CPU5 fixed at 1.728 GHz;
- LLVM 15, GCC 13, Boost 1.83;
- RelWithDebInfo, LLVM JIT ON, runtime LTO ON;
- probe read/write checks OFF;
- same map sizes, key/value sizes, helper count, warm-up, iterations, rounds,
  alternation order, and semantics as the source experiment;
- one PMU metric at a time, with `enabled == running` and no multiplex;
- `kernel.bpf_stats_enabled=1` only during kernel profiling;
- end with zero BPF links, BPF stats restored, and BPFtime shared memory
  removed.

Do not optimize or modify BPFtime runtime in this phase. Diagnostic programs
may be added only when existing data cannot close a boundary, and they must
preserve production semantics and be clearly identified as diagnostics.

## Deliverable

Create:

```text
uprobe/arm64-bpftime-vs-kernel-paths-20260803/
```

At minimum include:

- `README.md` with answer-first findings;
- `environment.txt`;
- `closure.csv` covering top, JIT, direct L3, and kernel runtime boundaries;
- `layer-attribution.csv` with absolute and adjacent-layer deltas;
- matched PMU CSV and raw output for any new runs;
- exact reproduction and summarization scripts for new data;
- a data-compatibility table listing source commit, program semantics,
  controls, build flags, and whether each subtraction is valid.

For each of array update, array lookup, and hash lookup, report:

- kernel and BPFtime absolute ns/helper, cycles/helper, instructions/helper
  wherever directly measured;
- BPFtime-minus-kernel absolute difference;
- how much is explained by BPFtime map implementation, handler,
  shared-memory/fd dispatch, final helper/JIT context, and top residual;
- which conclusions are measured, inferred from compatible subtraction, or
  still unresolved.

Do not report only relative percentages. Push the completed result to
`benchmark-results/jetson` and report both source and result commit hashes.
