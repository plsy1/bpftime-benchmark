# A runner that waits for the loader instead of 10 s (2026-11-11)

Upstream's `benchmark/gpu/run_cuda_bench.py` starts each probed case's loader and sleeps a fixed
10 s before starting the victim (`time.sleep(10)`). On the 24-case subset that is 18 x 10 s per
build and batch: about 60 of the 65 minutes of a 10-round dev-vs-dev comparison.

`compare/runner/run_cuda_bench_ready.py` (copy in `../baseline-sub-20261111/scripts/runner/`, diff
against upstream in `vs-upstream.diff` there) waits until the loader is ready, never longer than
10 s: the micro loader's "BPF probes attached" line; for the example loaders, which print no
such line, their own output after the syscall server's "Created kprobe/kretprobe perf event
handler" line, or 1 s of quiet log after it; or the loader exiting. It logs the wait and the
reason per case. `compare/run_batch.sh` takes the runner from `RUNNER` (default upstream's).

## Check (one batch each, 14:02-14:10 JST)

Dev c115b33d (D) and dev aa6eb973 (N), each with the upstream runner and the ready runner, the
24-case subset (`../baseline-sub-20261111/scripts/gen_sub.py`), `scripts` as there
(`validate_ready.sh`):

- Batch time: 3 min 16 s (upstream runner) against 21 s (ready runner), both builds.
- Waits: 0.15-0.2 s on the 14 micro cases (ready line), 1.1-1.15 s on the 4 examples.
- All 92 case sections valid (`verify.csv`); every probed case instrumented (the memtrace cases'
  patched-image line lists no kernel, as before); loader numbers identical with both runners:
  CUDA Counter calls=97, Kernel Retsnoop 128 events, Thread Histogram 7 threads with counts,
  Memory Trace counter[0]=28.
- Per-case times agree within single-run spread (`compare.md`), e.g. CPU Array map update 327.0
  vs 323.1 us (D), 45.1 vs 47.3 us (N).

Runs: `runs/V<D|N><o|r>1/` (o upstream runner, r ready runner); `configs/`, `batch-logs/`.
