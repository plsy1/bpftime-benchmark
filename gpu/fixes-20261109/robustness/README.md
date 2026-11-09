# fixes-20261109/robustness: effects on the application, lookup pointers, late-path launches (group 六)

Board: Orin Nano 8 GB, CUDA 13.2, sm_87. Code: dev candidate af4a1a8d on top of 45743763. build-cuda,
never installed. Runs 2026-11-09 23:05-23:55 JST. Items from the "found by reading only" list of
`../../examples-20261109`.

## What changed

| item | before | now |
|---|---|---|
| SM architecture detection | `setenv("BPFTIME_SM_ARCH")` in the application; `sm_61` when the driver could not be queried (CUDA 13 refuses it) | kept in a static; empty on failure, `compile_ptxs` throws, the original image is loaded |
| PTX compiler locale | `setlocale(LC_ALL, "")` when the compiler is created: under LD_PRELOAD the whole application switched to its environment's locale | "C" locale for the compiling thread only (`uselocale`) |
| PTX work directories | debug logging kept `/tmp/bpftime-fatbin-work.*` per fatbin and run, never removed (90 on the board) | only with `BPFTIME_CUDA_KEEP_PTX=1` |
| host-call lookup result | the value's distance from `CommSharedMem` added to its device address: an illegal address outside the registered segment | `gpu_address_of_map_value`: device/managed as is, registered host memory through its mapping, NULL otherwise; the watcher thread holds the primary context (the driver call needs one) |
| late-path launch events | each launch created and destroyed two CUevents (the `cudaLaunchKernel` redirect went through the `cuLaunchKernel` hook again), one recorded before the kernel was queued | one event per stream, recorded after each patched launch; a failed patched `cuLaunchKernel` launches the original (branch not tested) |

Not done: several CUDA processes on one segment, `cudaLaunchKernelEx`, stream capture,
`cudaFuncSetAttribute` (examples-20261109's list), and the 90 directories already on the board
(left in /tmp).

## Mutations (`mutations/`)

`run.sh` applies each mutation of `mutate.py`, rebuilds the test binary, runs the tests, restores
the sources (checked byte for byte afterwards). `mut.out`:

| mutation | failing check |
|---|---|
| sm-arch-setenv | `[sm-arch]`: `BPFTIME_SM_ARCH` set after detection |
| sm-arch-sm61 | `[sm-arch]`: the child without visible devices (`CUDA_VISIBLE_DEVICES=`, cuInit 100, `scripts/novis.cpp`) gets sm_61 |
| setlocale-global | `[ptx-compiler]`: with `LC_ALL=C.UTF-8` the global locale is C.UTF-8 after compiling |
| keep-dirs-debug | `[ptx-work-dir]` |
| map-value-unchecked (host pointer passed through) | `[cuda-host-call]`: the GPU faults on the host pointer (`cuCtxSynchronize` fails, later cases fail too) |
| watcher-no-context | `[cuda-host-call]`: 0 of 1024 threads right (every lookup NULL) |

The first version of the lookup change had the last bug (no context in the watcher thread); the
existing host-call test caught it, the new unit test for `gpu_address_of_map_value` (run on a thread
with a context) did not.

## Late-path launch events (`mutations/event-count.out`, `scripts/event-count.sh`)

kernelretsnoop on vec_add_legacy 5 3 5 1 with `BPFTIME_CUDA_LOAD_SUBSTITUTION=0`, the victim under
`scripts/event_count.c` (an LD_PRELOAD shim counting `cuEventCreate` through the dynamic linker,
split by `CU_EVENT_DISABLE_TIMING`; cudart reaches the driver through `cuGetProcAddress` and is not
counted):

| build | events created by bpftime | probe events | C[1] |
|---|---:|---:|---:|
| af4a1a8d | 1 | 20 | 3.00 |
| the two event changes reverted | 8 | 20 | 3.00 |

## Matrix (`runs/`)

Default path (`liu1-summary.txt`): 19 of 19. Late path (`liu-late-summary.txt`): the same results as
`../costs/runs/wu-late-summary.txt`. Start-up unchanged: late krs-legacy 405 ms (402 before), default
krs-fs 400 ms (389), hmt-legacy 599 ms (573); the event-count run's 808 ms has the shim and an extra
`env` in front.
