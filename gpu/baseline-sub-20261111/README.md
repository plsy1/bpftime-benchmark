# Reference rounds on the 24-case subset: official baseline, dev before and after host-call layer 1 (2026-11-11)

The case subset agreed on 2026-11-11 (24 of upstream's 63 Orin cases; `scripts/gen_sub.py`), run
with the ready-waiting runner (`../runner-ready-20261111`), 10 interleaved rounds, set order
rotating per round, 14:10-14:30 JST. All 30 batches rc 0; checks before every batch
(`scripts/run_batch.sh`): time window, cpu5 1728000 and GPU 1020000000, no leftovers, no build,
no bpftime segment.

- **O** official 4708f409 (`build-cuda`, CUDA root of symlinks; compare-20261110). The official
  baseline from now on: re-measured only when the environment (JetPack, driver, CUDA, kernel,
  clocks) or the official commit under study changes; dev changes are compared dev against dev.
- **D** dev c115b33d (before host-call layer 1).
- **N** dev aa6eb973 (host-call layer 1 items 1-2: ordinary array maps on the GPU, 1b3d9c99;
  pid_tgid without a host call, aa6eb973; `../hostcall-l1-20261111`).

Victim: legacy-launch vec_add; micro probe: the 1512/1513 copy. Examples: each tree's cuda-counter
and kernelretsnoop; threadhist and mem_trace as shipped on dev (1502/1503, created as 1512/1513)
and as copies asking for 1512/1513 on official (it cannot create 1502/1503 on the Orin).

## Results

`summary.md` (`scripts/summarize_base.py`): median over rounds of the per-launch mean, us.

- **Changed by layer 1** (N vs D, p < 0.001): CPU Array map update 320.2 -> 51.0 us (0.16),
  lookup 371.3 -> 49.2 (0.13), CUDA Counter 3,487.3 -> 2,928.9 (0.84).
- **Everything else unchanged**: the other 20 cases within 0.96-1.04 of D, p >= 0.076.
- **Against official** (N / O): host-call cases 0.0015 (array), 0.0087 (CUDA Counter); Kernel
  Retsnoop 0.81 (official delivers 32 of 128 events); GPU Array map update large 1.09 and GPU
  Ringbuf small 1.08 (the costs attributed in compare-20261110/attribution.md); the rest
  0.88-1.04.
- Checks (`verify.csv`, 690 case sections): all valid; CUDA Counter calls=97, Thread Histogram 7
  threads, Memory Trace counter[0]=28 on all three; Kernel Retsnoop 128 events on D and N, 32 on
  O, in every round.

## Files

`summary.md`, `verify.csv`, `runs/B<O|D|N><round>/`, `configs/`, `batch-logs/base.log`, `scripts/`
(`gen_sub.py`, `rounds_base.sh`, `validate_ready.sh`, `run_batch.sh`, `summarize_base.py`,
`runner/`).
