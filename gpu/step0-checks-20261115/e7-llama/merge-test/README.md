# merge-test: do two divergent exit groups merge inside a shared `__noinline__` function?

Added by the e7 review (2026-11-15), plain CUDA, no bpftime. It explains the `e7ctrl_merge` result of the
e7 control run (every warp made one exit-probe call of 32 lanes although odd and even lanes leave through
different `ret`s).

- `merge.cu`: odd lanes call a `__noinline__` function that reads `activemask` and then leave through an
  inline `ret`; even lanes call it from the kernel's final `ret`. kA: the same function from both sites;
  kB: even lanes sleep ~200k cycles first (capped loop, cap hits counted); kC: two different functions.
  The function is a few instructions with no stall.
- `merge2.cu`: the same two exit sites, but the function starts like bpftime's probe: a load of a global
  flag (`ld.volatile`, which ptxas precedes with `YIELD`; see `merge2.sass`), then `activemask`
  (kLd), or the flag, `__nanosleep` and then `activemask` (kSleep). 64 blocks of 64 threads.
- `merge.out` (2026-11-15, board shared, counts only):
  - kA, kB, kC: every lane reports its own group (`0x55555555` or `0xaaaaaaaa`): groups that do not meet
    stay apart, and `activemask` reports the caller's group.
  - kLd, kSleep: every lane of all 64 blocks reports `0xffffffff`: the two groups meet inside the
    function after the `YIELD`/global load and run it as one warp.
- Build and run: `nvcc -O2 -gencode arch=compute_87,code=sm_87 -o merge merge.cu` (same for merge2), then
  `timeout -k 5 20 ./merge; timeout -k 5 20 ./merge2` (< 1 s of GPU time).

Reading for check 7: bpftime's exit probe starts with `YIELD` and a global load of the probe switch
(`__bpftime_probes_enabled`), so lanes that leave a warp at different `ret`s at about the same time can
be counted as one call. The e7 lane histograms therefore describe the masks a helper inside the exit probe
sees, which is what a per-call reservation (design C, D) would see; they do not show that the kernel's
own exits were converged. The merge depends on timing and is not guaranteed by any documented rule.
