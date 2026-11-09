# fixes-20261109/protocol: host calls, GPU ringbuf, GPU array bounds (group 三)

Board: Orin Nano 8 GB, CUDA 13.2, sm_87. Code: dev candidate `dev/uprobe-official-177a1dc465ff`,
d670246b (the fix) and c6f0a6b6 (a test setup fix), on top of 648fef28 (the trampoline PTX built
from source). build-cuda (RelWithDebInfo, debug logs compiled in), never installed. Runs
2026-11-09 21:12-21:44 JST, cpu5 at 1728000. Problems as numbered in `../../examples-20261109`.

## What changed

- Problem 2, host-call arguments: every lane wrote its key, value or printk arguments into the one
  request slot before taking the lock, so the calls of a launch carried one thread's arguments. The
  request is now written while the lock is held.
- The lock was per module (one per fatbin). Threads now queue on a per-module lock in GPU memory,
  then take `occupy_flag` in `CommSharedMem` (one per process).
- Problem 10, 1.06 ms per host call: the watcher slept 1 ms after every request. It now takes a
  request with an acquire CAS on `flag1`, answers with a release store of `flag2`, keeps polling for
  200 us after a request and backs off from 50 us to 1 ms when idle.
- Problem 7, GPU ringbuf: the GPU published `tail` before the record and wrote oversize records over
  the next one; the host took one record per thread per poll, stopped at the first thread being
  written, moved `head` before reading and returned 0. Now: the writer takes the thread's `busy`
  word, writes, `membar.sys`, stores `tail`; full, oversize or contended records are counted in
  `dropped`; the host drains every record, stores `head` after reading each, warns with the drop
  count and returns the number of records. kernel_trace and kernelretsnoop poll every 10 ms.
- GPU array maps (1502/1512 per thread, 1503/1504/1513 single copy): a thread beyond
  `max_thread_count` or a key beyond `max_entries` gets NULL / -E2BIG instead of reading and
  writing past the map (found while running threadhist: the device corrupted the shared segment);
  one warning per module.
- Layouts shared by the trampoline and the host: `attach/nv_attach_impl/trampoline/gpu_helper_protocol.h`.

## Host-call cost by launch geometry

`bpftime_runtime_tests "Every GPU thread's host call carries its own arguments"`, the test kernel
does a map update and a map lookup per thread (hash map through host calls). Geometry set by a
temporary override that was removed afterwards; times are one run each, warm module.

| launch | lock only in host memory | per-module lock in front |
|---|---:|---:|
| 1 thread, 500 rounds (1000 calls) | 14.2 us | 15.6 us |
| 1 warp, 16 rounds (1024 calls) | 9.7 us | 10.4 us |
| 8 warps (512 calls) | 74 us | 9.9 us |
| 32 warps (2048 calls) | 300 us | 9.8 us |

Before the fix: 1.06 ms per call (the watcher's sleep). The survey measured the bare transport at
about 5 us (`../../feasibility-20261109/gpu-env/hostreg.hsloop-atomdev.txt`). The committed test
runs 32 warps and requires 2048 calls within 200 ms (about 20 ms measured). `cuPointerGetAttribute`,
called by the watcher on every lookup, costs 0.21 us (`scripts/ptrattr.cpp`).

## Mutations (`mutations/`)

`run.sh` applies one mutation from `mutate.py` to the fixed sources, regenerates the PTX when the
device code changed, builds `bpftime_runtime_tests`, runs `[cuda-ringbuf],[cuda-host-call]`, and
restores the sources (checked byte for byte against a backup afterwards). `mut.out` is the summary,
`test-<mutation>.log` the output.

| mutation | failing test, how |
|---|---|
| none | all 4 pass (1300 assertions) |
| payload-before-lock (request written before the lock) | host calls: 1 of 1024 threads right; every update carried key 608 |
| no-module-lock (spin only on `occupy_flag`) | host calls: 2048 calls in 621 ms (limit 200) |
| watcher-sleeps (1 ms after each request, no polling window) | host calls: 2161 ms |
| no-thread-bounds | per-thread map: 386 failed checks (threads 64-127 got slots, entry 1 and the guard overwritten) |
| no-ringbuf-size-check | ringbuf device: oversize records accepted |
| no-drop-count | ringbuf device: `dropped` stays 0 |
| drain-one-per-poll | both ringbuf tests |

Not mutation-checked: the placement of each fence (`membar.sys` before `flag1`, the host's
acquire/release). Host memory on the Orin is coherent with the GPU, so removing one would likely
still pass here.

## Example matrix (`runs/`, `scripts/fix-matrix.sh`)

`san1-summary.txt`: 17 of 19 cases passed; `san2-summary.txt`: the two others after correcting the
harness (below). All 19 pass:

- hmt-legacy-printk: printk names 10 threads (1 before: problem 2).
- ts-legacy: 10 threads mapped (1 before: problem 2).
- threadhist-small (new case): threadhist with the default thread count (1024) and a victim launching
  512 x 1024 threads. One warning in the victim, server exit 0, 7 threads with counts; the victim
  runs until the harness stops it (124), as in the threadhist case.
- The other 16 (kernelretsnoop variants, globals, cudagraph, kernel_trace, cuda-counter,
  threadhist with the thread count raised) as before.

Harness corrections, not bpftime changes:
- cc-legacy-printk: `x` in cuda-counter's printk is the block index (`bpf_get_block_idx`). The
  victim ran 1 block of 10 threads, so "1 distinct x" was the correct output, before the fix too;
  counting it as a problem-2 failure was my misreading. The case now runs 10 blocks of 1 thread and
  sees x = 0..9.
- threadhist-small first required the victim to exit 0; threadhist's victim launches forever, so
  124 (stopped by the harness) is the expected status.

Per launch (victim's median, these runs, info level, not a formal measurement):

| victim | before (examples-20261109) | now |
|---|---:|---:|
| host_map_test, vec_add_legacy 10 5 10 1 | 21.3 ms | 0.35 ms |
| threadscheduling, vec_add_legacy 10 5 10 1 | 42.3 ms | 0.46 ms |
| cuda-counter | 106 ms (1 block x 10) | 1.06 ms (10 blocks x 1) |

GPU ringbuf, the sampling probe (`runs/san-mB-sample`, the meas.sh m-B-sample command: 1M threads,
201 launches, thread 0 writes one record per launch): 186 records delivered (13 before), 15 dropped
on the GPU and reported by the server's warning (186 + 15 = 201). Thread 0 has 16 records and fills
them in about 17 launches x 0.58 ms, faster than the loader's 10 ms poll; delivering all would need a
faster poll or more entries in the example. Per launch 579 us (584 before).

## Other tests

- `bpftime_nv_attach_tests`: 25 of 25 with the regenerated PTX.
- "CUDA watcher stops before shared-memory teardown" failed in every CUDA build (the segment was
  only opened on the server side, never registered with CUDA, so `CUDAContext` threw; the
  pre-driver-API code fails the same way, `scripts/hgdp.cpp`: invalid argument). c6f0a6b6 opens it
  as the agent does; it passes and checks that the watcher took the request.
- "Test cuda compile" in the same file fails with "VM name cannot be empty", unrelated, not fixed.

## Not covered

- Discrete GPUs: the process-wide lock is now a GPU atomic on host-mapped memory. The survey checked
  CAS locks in host memory on the Orin only (`hostreg.atom-dual.txt`); over PCIe it is untested.
- Map type 1504 (kernel-shared array) now has the key bound but is not in the matrix (its examples
  need root).
- Several CUDA processes on one segment: each agent zeroes `CommSharedMem` at start, and any
  process's watcher may serve a request.
