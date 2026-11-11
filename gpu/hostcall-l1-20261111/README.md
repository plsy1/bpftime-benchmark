# GPU host calls, layer 1: array maps and pid_tgid without the host (2026-11-11)

Direction 2 of `RESEARCH_DIRECTIONS.md` (main), layer 1, items 1 and 2: GPU probes make no host
call for ordinary array maps and for `bpf_get_current_pid_tgid`. Dev commits on
`dev/uprobe-official-4708f4092d1e`, on top of c115b33d:

- **1b3d9c99** nv_attach: GPU probes use ordinary array maps without host calls. The agent puts
  the device address of an array map's values (the shared segment is registered with CUDA) into
  `map_info`, pins the map with an `array_map_binding` for the attach context's life, and the
  trampoline's lookup/update/delete follow `array_map_impl`'s rules on the GPU.
- **aa6eb973** nv_attach: GPU bpf_get_current_pid_tgid without a host call. The agent writes the
  process's tgid (in both halves) into every module's `__bpftime_pid_tgid`; the host answers the
  same value for a module it did not write (its low half used to be the watcher thread's tid).

Each commit message holds its tests, mutations and numbers.

## Measurements

Sets: **D** dev c115b33d (the worktree's `build-cuda`, as in compare/ablate/xlarge), **N** dev
with both commits (`bpftime-gpu-work/l1/build-cuda`, the tree of aa6eb973, built 12:20 with the
compare round's flags: `scripts/build.sh`). Upstream `benchmark/gpu` runner, legacy-launch
victim, the 1512/1513 micro probe copy (`compare/fix`), the candidate tree's example loaders.
Cases: the ones the change touches (CPU Array map update/lookup, CUDA Counter) and controls.

- **Smoke** (S, 1 round, debug log level, 12:22-12:26 JST): host requests per case counted from
  the watcher's debug lines (`CUDA Received call request`): CPU Array map update and lookup 128 ->
  0, CPU hash cases 128 -> 128, CUDA Counter 1,312 -> 1,056 (2 per thread x 32 threads x 4
  launches fewer). `summary-smoke.md`; logs `runs/S*` (xz).
- **Rounds** (R, 10 interleaved rounds, order alternating, 12:26-12:59 JST): `summary.md`, from
  `scripts/summarize_l1.py`. Checks before every batch (`compare/run_batch.sh`): time window, cpu5
  1728000 and GPU 1020000000, no leftovers, no build, no bpftime segment. All 20 batches rc 0; all
  240 case sections valid (victim's `C[1] = 3.00`, closing line, no failure string; CUDA Counter
  calls=97 and Kernel Retsnoop 128 events in every run): `verify-rounds.csv`.

| case | D (us) | N (us) | N / D | p |
|---|---:|---:|---:|---:|
| CPU Array map update (minimal) | 321.4 | 47.2 | 0.15 | < 0.001 |
| CPU Array map lookup (minimal) | 376.4 | 48.2 | 0.13 | < 0.001 |
| CUDA Counter (minimal) | 3,461.5 | 2,877.5 | 0.83 | < 0.001 |
| Baseline (minimal) | 39.0 | 39.9 | 1.02 | 0.57 |
| CPU Hash map update / lookup / delete | 324.3 / 339.0 / 318.1 | 327.5 / 341.1 / 317.7 | 1.00-1.01 | >= 0.21 |
| Empty probe (tiny), GPU Array map update (small), Kernel Retsnoop | | | 0.97-0.99 | >= 0.35 |

The CPU array cases (32 threads, 3 timed launches, one call each per launch) lose their 32
serialized round trips: about 8 us remain over the baseline, the cost of a probed launch. CUDA
Counter loses 2 of its 10 host calls per thread (-584 us, 17%); its hash map calls and printk
still go to the host.

Wall time is unchanged (96-97 s per batch on both): the runner waits a fixed 10 s per probed case.

## Checks

- Unit tests (`bpftime_runtime_tests` `[cuda-host-call]`, `[cuda-ringbuf]`, `[cuda-no-device]`;
  `bpftime_nv_attach_tests`, 32 cases) pass on N, and on 1b3d9c99 alone (built and run at that
  commit before the tip was rebuilt).
- Mutations (`mutations/mut.py`, results `mut.log`, `mut2.log`): seven, each failing its test:
  the GPU array branch removed; no device address; BPF_NOEXIST accepted; the binding not kept;
  the helper ignoring the module's pid_tgid; the writer writing nothing; the host answering the
  watcher's tid. The sources were restored and their hashes checked after each run.
- Full `bpftime_runtime_tests` (one process, not root): 286 of 294 pass, 6 skipped; the 2
  failures are not from this change: "Test tail calling from userspace to kernel" fails the same
  on c115b33d (needs bpf), "Rate-limited logging picks 1, 10, 100, ..." passes run alone on both
  builds (order-dependent in one process).
- An independent review (a workflow of three reviewers and a refuting verifier per finding) found
  the unpinned address (fixed: the binding), a stale regenerated PTX and a flag case the test did
  not cover (fixed, test added), and counting builds losing GPU array operations (they keep the
  host calls). Two findings were refuted (torn byte-wise updates: bpf's array update is no atomic
  copy either; the pid test thread).

## Files

- `summary.md`, `summary-smoke.md`, `verify-rounds.csv`
- `runs/<id>/`: result json/md and runner logs; ids `R<D|N><round>`, smoke `S<D|N>1`.
- `configs/`, `batch-logs/` (`smoke.log`, `rounds.log`)
- `scripts/`: `gen_l1.py` (configs), `rounds_l1.sh` (interleaved rounds), `build.sh`,
  `summarize_l1.py`
- `mutations/`, `build/` (configure log; build logs xz)
