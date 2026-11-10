# Why dev is faster on GPU Array map update (xlarge) (2026-11-10/11)

Follow-up to [`../ablate-20261110`](../ablate-20261110/). That round left one official-vs-dev difference
unexplained. On "GPU Array map update (xlarge)", official took 1,682 us and dev took 1,509 us. Undoing
any one of four dev changes on dev left it fast. Four experiments here look for the cause.

Tables: [summary.md](summary.md). The conclusion is in
[`../compare-20261110/attribution.md`](../compare-20261110/attribution.md), section "Ablations".

## The case

The victim `vec_add 1000000 1000 512 1954` uses the legacy launch and runs 1,000,448 threads.
Every thread runs the kretprobe `cuda__retprobe_gpu_array_update`. It looks up key 0 of the 1513
map `gpu_counter` and then updates key 0 with the value plus one. On the Orin, `gpu_counter` lives
in bpftime's shared-memory segment, which is host memory registered with CUDA. The GPU does not
cache this memory, so about a million threads read and write the same 8 bytes in it per launch,
with no atomics.

The update copies the 8 bytes one byte at a time (8 `st.u8`) and ends with `membar.sys`. Both
builds do this. The case therefore measures contention on one uncached line. How fast it drains
depends on when the warps reach that line.

## Experiments

| # | sets | what changes | rounds |
|---|---|---|---|
| 1 | O official, P official `BPFTIME_SHM_MEMORY_MB=128`, D dev, E dev `BPFTIME_SHM_MEMORY_MB=50` | segment size: 50 MB on official, 128 MB on dev (99845759) | 6 |
| 2 | O, D with `BPFTIME_MAP_GPU_THREAD_COUNT` = 1024..1035 | where the counter sits. The GPU ringbuf, which is allocated before the counter, grows by one 408-byte entry per thread. On dev the 1512 buffer also grows by 8 bytes per thread, so dev's counter moves 416 bytes per thread. The counter moves through 12 positions per build, about 4.5 KB in all. | 2 per position |
| 3 | O, D, R, C | R: official + [patches/oreg.diff](patches/oreg.diff), which uses dev's CUDA registration (`cuInit`, primary context, `cuMemHostRegister(DEVICEMAP\|PORTABLE)`) instead of `cudaSetDeviceFlags(cudaDeviceMapHost)` + `cudaHostRegister(Mapped)`. C: dev + the three ablation patches (`../ablate-20261110/patches/{frame,gate,tramp}.diff`) together, i.e. official's device code on dev's runtime. | 8 |
| 4 | O, D, L, C, K | L: dev with `BPFTIME_CUDA_LOAD_SUBSTITUTION=0`, the late path. K: C with the late path. | 6 |

Experiments 3 and 4 also run the xlarge grid with N=32 (`vec_add 32 1000 512 1954`, "xlarge_n32").
The victim then does almost no adds, but every thread still runs the retprobe, because the call
comes after the bounds branch.

The runs used `../../../bpftime-gpu-work/compare/run_batch.sh`, the same checks as the ablation
round. These are run before every batch:

- the time window;
- cpu5 at 1728000 and the GPU at 1020000000;
- no leftover processes;
- no build running;
- a fresh shared-memory name, removed afterwards.

The order of the sets rotated every round. The runs were on 2026-11-10 from 22:45 to 00:24 JST.
All 134 measurement batches and the two smokes ended with rc 0. A first smoke attempt failed on
a relative config path and was rerun.

## Builds

- **R** (`oreg`): a copy of official 4708f409 with oreg.diff, built with official's flags and its
  CUDA root of symlinks.
- **C** (`fgt`): a copy of the T ablation tree with frame.diff and gate.diff added. It uses dev's flags.

The script is `scripts/build_x.sh`. Logs and library hashes are in `build/`.

## Checks

- **Every run section.** All 530 sections, smoke included, are valid: the victim printed
  `C[1] = 3.00`, and there is no failure string. Every probe section logs an instrumented module.
- **[ptx-check.txt](ptx-check.txt)** compares the patched PTX of the update probe, kept from the
  debug smoke runs (copies in `ptx/`):
  - C's retprobe is identical to official's. C has no load of the probe gate, and its 1512/1513
    helpers are T's. Its kernel frame is 16,504 bytes; official's is 16,488.
  - R's patched PTX is byte for byte official's.
  - The instrumented kernel uses 32 registers in every build.
- **The logs.** P's and E's logs show the swapped segment size. The placement sweep logs the
  counter's device address for every run.

## Results (median over rounds of the run mean, us)

| | large | xlarge | xlarge_n32 | fastest xlarge launch |
|---|---:|---:|---:|---:|
| O official | 223.0 | 1,687 | 1,880 | 1,366 |
| D dev | 246.2 | 1,474 | 2,131 | 983 |
| R official, dev's registration | 220.6 | 1,681 | 1,874 | 1,342 |
| C dev, official's device code | 238.3 | 1,591 | 1,927 | 1,324 |

These rows are from experiment 3. Experiment 4 repeats O, D and C within a few percent; there,
L = D and K = C.

- **Segment size.** Not the cause. P equals O and E equals D.
- **Counter position.** Not the cause. Across 12 positions in two 4 KB pages, the build decides
  the result:
  - official 1,621-1,803 us;
  - dev 1,329-1,540 us.
- **Registration.** Not the cause. R equals O at every size (p 0.65 / 0.72 / 0.65).
- **Launch path.** Not the cause. L equals D and K equals C, alone and together with the device code.
- **Device code.** C moves dev toward official at every size, which is 55% of the xlarge gap. Its
  fastest launch, 1,324 us, is close to official's 1,366. With dev's code, some launches finish in
  under 1,000 us, and the spread doubles (standard deviation about 190 against about 100 us).
- **The rest.** About 90 us remains between C and O at xlarge, 15 at large and 50 at xlarge_n32,
  and it was not isolated. C's retprobe is identical to official's in PTX. Its 1513 paths have
  official's source but are compiled by dev's toolchain, so their PTX differs in small ways (for
  example a 32-bit multiply where official has a 64-bit one). What still differs:
  - **813c3ecb**: the whole trampoline PTX is regenerated from source with clang 15/16, where
    upstream's shipped PTX came from clang 17;
  - **518f05b5**'s device parts that were not undone: the IPC map branches,
    `__bpftime_map_bounds_warned`, and the renamed comm lock;
  - the SASS layout ptxas makes of the whole module.
- **Sign changes with the grid.**
  - At xlarge_n32, dev is **13% slower** than official (2,131 vs 1,880 us). C brings it back to
    1,927.
  - At large, dev is 10% slower.
  - The same code is faster or slower depending on how the work around the hot line is shaped.

## Files

- `summary.md`: the tables of all four experiments.
- `runs/<id>/`: result json/md and runner logs. The ids are:
  - experiment 1: `O1`-`O6`, `P*`, `D*`, `E*`;
  - experiment 2: `O-t<threads>-r<rep>`, `D-*`;
  - experiment 3: `B<set><round>`;
  - experiment 4: `K<set><round>`;
  - smoke: `SC`, `SR`.
- `configs/`: the generated runner configs. `batch-logs/`: the batch logs.
- `scripts/`:
  - `gen_x.py`: configs;
  - `rounds_{shm,place,build,late}.sh`: the experiments;
  - `build_x.sh`: the two builds;
  - `summarize_build.py`, `summarize_place.py`: the tables;
  - `classify_ptx.py`: sorts kept PTX by probe.
- `patches/oreg.diff`, `ptx/`, `ptx-check.txt`, `build/`.
