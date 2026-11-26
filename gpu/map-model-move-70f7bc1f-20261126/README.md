# The map-model prototype moved onto dev 70f7bc1f (2026-11-26)

Follow-up to `gpu/map-lowering-20261118`. The prototype's 11 commits (`proto/map-model`, 52126f33..662bd83e, on the old dev base `dev/uprobe-official-4708f4092d1e`) were moved onto the current dev `dev/uprobe-official-70f7bc1f3363` (c71a9987, official 70f7bc1f plus the 244-commit stack) as `proto/map-model-70f7bc1f` (33d5a3d8). The old branch is kept.

Upstream is not followed further for now (user, 11-26). Official master moved 44 commits past 70f7bc1f on 11-24/25 (60+ commits a week since 11-09, most of them on the locks of the map handler, shared memory, ringbuf and agent start-up). A trial merge of 35015537 into dev conflicted in 30 files. A manual daily run against 35015537 was stopped before its migration step; nothing from it was kept or pushed.

## The move (`rebase/`)

- One conflict, in the first commit (`nv_attach_impl.cpp`, `hack_fatbin`). Upstream f1cfb639 (#619) returns early when a module gets no trampoline; the prototype calls the routing pass at that point. Resolved as: return if no trampoline; route the module's map accesses if map info exists; wrap with the trampoline. That is what the prototype did before for modules that get a trampoline.
- The other ten commits applied as they were (`range-diff.txt`: the third differs only in context lines of the constructor, which upstream #763 reorganized; its own lines are unchanged). Same change set: 19 files, +4658/-1099.
- The build uses the same options as the old one (gcc-13, LLVM 15, RelWithDebInfo, no LTO, CUDA 13.2).
- A read-only review of the upstream and reconcile changes between the two bases against what the prototype relies on had three lenses (nv_attach, the map side, build/loader/agent) and no findings; with none, nothing went to a second check.
- Upstream #853 now turns a map backend's -1 into -errno at the BPF-helper boundary. Every error path of the prototype's map types (`nv_gpu_model_maps.cpp`) sets errno before returning -1, so they fit that contract (read, not tested).

## Checks (`check/`)

| check | on the new base |
|---|---|
| `[sum-map-routing]` unit tests | 20 cases, 239 assertions, pass |
| runtime GPU tests (`[cuda-host-call]`, `[cuda-no-device]`, `[cuda-ringbuf]`, `[cuda-watcher-policy]`, `[gpu-map-host]`, `[map-create]`) | all pass, same case and assertion counts as on 11-17 |
| pt_victim, seven tools (carry, lat, latold, hist, histold, latmix, tlatomic) | all PASS; the 38 count lines equal the old base's (`addpath10`) line for line |
| `bpftime_nv_attach_tests` (all) | 52 of 53 cases pass; `[ptx-failure]` fails |

`[ptx-failure]` fails on plain dev c71a9987 too, built without the prototype (`plain-dev-ptx-failure.txt`). The test (from dev 21921cb0) compiles a broken module that has no probes. Since upstream f1cfb639 such modules are skipped and never compiled, so the error it expects cannot happen. The dev test needs a module with a probe; the prototype does not cause it.

## Generated PTX (`target-shape/ptx-compare.txt`)

The patched PTX of `lat_mmvq` and of the four-kernel `lat` was dumped under the new build and compared with the old one's. Every module is identical line for line except the `// callseq N` comments (8 lines per module, numbering only); ptxas ignores comments, so the GPU runs the same instructions. Separately, the llvmbpf submodule differs between the bases (87bac39 -> 550cf471); whether that renumbers the comments was not checked.

The PTX override forms of 11-18 (`proto/target-shape/variants/*`, e.g. the by-value carry) are keyed by the old sha 41b5bf76, so they no longer apply to the new build's PTX (923ea830). They have to be regenerated from the new dump (`dumpN70-lat_mmvq`) before any override item runs.

## Measured (`meas/`, passes r1/r2, rules in `RULES-sum-map-build.md`, last addendum, written before the passes)

llama-bench TinyLlama Q4_K_M `-r 5`, clocks fixed and checked before every item, items shuffled per pass. `o52`: the prototype on the old base (662bd83e). `n70`: on the new base (33d5a3d8). Per thread = s per run over base / 3,137,339,392 threads (mul_mat_vec_q only).

| tool, workload | old base: t/s (r1 / r2) | new base: t/s (r1 / r2) | s per run | per thread over base | overhead |
|---|---:|---:|---:|---:|---:|
| base, tg128 | 68.00 / 67.96 (shared) | | 1.882-1.884 | - | - |
| `lat_mmvq`, tg128 | 54.54 / 54.52 | 54.54 / 54.52 | 2.347-2.348 both | 0.1481 / 0.1479 -> 0.1481 / 0.1480 ns | 19.8% both |
| `lat` (four kernels), tg128 | 53.99 / 54.04 | 54.02 / 54.04 | 2.368-2.371 both | - | 20.5-20.6% both |
| base, pp512 | 2514.9 / 2512.1 (shared) | | 0.204 | - | - |
| `lat` (four kernels), pp512 | 2475.5 / 2477.3 | 2478.6 / 2475.9 | 0.207 both | - | 1.4-1.6% -> 1.4% |

The difference between the bases is 0.0000 / 0.0001 ns per thread and at most 0.2 points of overhead: within the rule's 0.006 ns and 1 point in both passes, so the move keeps the numbers. Counts are exact in every item and equal between the bases. The 11-18 figures (54.5 t/s, 0.147-0.149 ns, 19.7-19.9%) are reproduced. pp512 `lat` reads 1.4-1.6% today against 2.1-2.7% on 11-18: the base rates overlap (2512-2515 today, 2504-2517 then), but the four-kernel `lat` rate is about 1% higher today (2475-2479 against 2449-2453) with the same PTX. Not explained; a different day.

After the passes, the runner's and the check script's default build was switched to the new base (`bpftime-proto-70f7/build-cuda`); the copies here are the ones that ran (old default; every timed item used a prefix, and the base item has no agent).

## Not done

- The by-value carry of the start (still a candidate, 0.015-0.021 ns per thread on 11-18).
- An independent review of the forwarding allowlist (662bd83e, now 33d5a3d8).
- The dev test `[ptx-failure]` (above).
