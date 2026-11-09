# fixes-20261109/costs: GPU stack frames, late-path symbols and fatbins (group 五)

Board: Orin Nano 8 GB, CUDA 13.2, sm_87. Code: dev candidate 45743763 on top of c6f0a6b6.
build-cuda, never installed. Runs 2026-11-09 21:55-22:28 JST, cpu5 at 1728000, GPU at 1020 MHz.
Problems as numbered in `../../examples-20261109` (11 start-up, 12 stack frame, 13 fatbin ingested
2-3 times, 3 mapping symbols).

## What changed

- **GPU stack (12).** llvmbpf gave every GPU thread 32 eBPF frames (16,464 bytes of local memory)
  whenever a stack address escapes. The overlay patch (`vm/compat/llvm-vm/local-call-return-block.patch`)
  now gives a GPU program without bpf-to-bpf calls one frame (592 bytes); with a call, and on the
  CPU, 32 as before.
- **Late-path host names (11, 3).** The late bootstrap read the `.symtab` and `.dynsym` of every
  loaded module, one read per symbol. `elf_introspect::host_symbol_resolver` reads only the module
  that contains the asked address, when first asked, one read per table; drops AArch64 mapping
  symbols (`$x`, `$d`); prefers a sized symbol on an address tie; names nothing outside every module.
- **Late-path fatbins (13).** `.nv_fatbin` holds a device-link fatbin (cubin only) and right after
  it the program's (cubin and PTX); the bootstrap read from the section start and from each wrapper,
  and every read takes the whole run of fatbins, so cuobjdump ran 2-3 times on the same bytes
  (`ptx/*.nv_fatbin`, `ptx/*.seg`: the section and wrappers of two victims). `fatbins_to_read`
  keeps one read per run; `fatbin_has_ptx` skips fatbins without a PTX entry.

The default path (load-time substitution, 二) does not use the host names or the bootstrap, so its
start-up is unchanged; the stack change applies to both paths.

## GPU stack

`ptx/kt3-sample.ptxas-v.txt`: the sampling probe's patched kernel `_Z9vectorAddPKfS0_Pf` (from a
debug-level run, `runs/wu-ptxas-sample`) has a 592-byte stack frame; examples-20261109 measured
16,464 bytes (`../../examples-20261109/ptx`). At full occupancy (1536 threads x 8 SMs) that is
about 7 MiB of local memory instead of 193 MiB.

Per launch, victim's median, vec_add_legacy 1048576 200 256 0 (`scripts/meas-b.sh`, `runs/wu-mB-*`):

| | r1 | r2 | r3 | examples-20261109 (r1-r3) |
|---|---:|---:|---:|---:|
| no agent | 162.47 | 161.48 | 164.07 | 162.18-162.92 |
| empty kprobe | 171.40 | 169.00 | 170.37 | 171.56-178.34 |
| sampling kprobe | 579.83 | 577.24 | 572.44 | 583.57-586.04 |

The frame was not where the sampling probe's time goes: about -1.5%. The rest is the per-thread
helper work, as the static bounds in examples-20261109 suggested.

Test: "A GPU program without bpf-to-bpf calls gets one stack frame" (bpftime_nv_attach_tests)
compiles two programs that pass a stack address to a helper, without and with a bpf-to-bpf call,
and compares the PTX local depots: 592 and 16,464 bytes; with the previous patch 16,464 and 16,464
(the test fails; `mutations/stack-frame-old-vs-new.out`).

## Late path

Matrix with `BPFTIME_CUDA_LOAD_SUBSTITUTION=0` (`runs/wu-late-summary.txt`) against the same run on
the 二 build (`runs/mutA-*`, from the earlier part of this round):

| case | start-up before | after | cuobjdump runs before | after |
|---|---:|---:|---:|---:|
| krs-legacy (kernelretsnoop, vec_add_legacy 5 3 5 1) | 1774 ms | 402 ms | 2 | 1 |
| krs-fs (-ffunction-sections -fdata-sections victim) | 1750 ms | 399 ms | 2 | 1 |
| hmt-legacy (two probes: two bootstraps) | 2153 ms | 709 ms | 4 | 2 |
| kt-legacy (victim with two wrappers) | | | 3 (examples-20261109) | 1 |

krs-fs now computes C[1] = 3.00 on the late path (0.00 before: `d_N` resolved to `$d`). The other
late-path failures are the known ones (CUDA 13 kernel handles, stripped and static-cudart victims,
separate globals, two probes on one kernel, upstream #708); the threadhist cases fail there because
their victim is a default nvcc 13 build (kernel handles). On the default path all 19 cases pass
(`runs/wu1-summary.txt`).

Tests `[late-path]` (`test_late_path.cpp`); `mutations/run.sh` applies each mutation in
`mutations/mutate.py`, rebuilds and runs them (`mutations/mut.out`):

| mutation | failing check |
|---|---|
| eager-all-modules (every module read on the first lookup) | modules read == 1 |
| keep-mapping-symbols | no `$` symbol in the table |
| old-naming (no module lookup, unsized symbols name everything above them) | a heap address has no name |
| read-every-candidate | one read per run of fatbins |
| ptx-always | a cubin-only fatbin has no PTX |

All 28 nv_attach test cases pass with the sources restored.
