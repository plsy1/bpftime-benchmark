# fixes-20261109: CUDA attach on the Orin Nano, the fix round

Board: Orin Nano 8 GB, CUDA 13.2, sm_87, kernel 6.8.12-1021-tegra-btf. Dev branch
`dev/uprobe-official-177a1dc465ff`, 13 commits from e94e2c04 to af4a1a8d on top of a2acb4f8 (the
start-up round). 2026-11-09 08:05 (plan approved) to 2026-11-10. The problems are those of
`../examples-20261109` (numbers 1-17 there, and its list "found by reading only"). build-cuda
(RelWithDebInfo, `BPFTIME_CUDA_ROOT=/usr/local/cuda-13.2`) is never installed and never used for
the daily gate or for CPU benchmarks.

Each commit message holds its own evidence: the end-to-end runs, the test, and the mutations that
make the test fail. The three directories here hold the data of the later groups:

| directory | group | dev commits |
|---|---|---|
| (commit messages; matrix runs in `bpftime-gpu-work`) | 〇 llvmbpf frame step, 一 build and messages, 二 load path, exit crash, maps | e94e2c04, 3695e13c, 17427685, f28c42a7, acdd7a77, 718fefb1, b9e83c42, 788fdb88, 648fef28 |
| `protocol/` | 三 host calls, GPU ringbuf, GPU array bounds | d670246b, c6f0a6b6 |
| `costs/` | 五 GPU stack frame, late-path symbols and fatbins | 45743763 |
| `robustness/` | 六 environment, locale, lookup pointers, launch events | af4a1a8d |
| `verification/` | the final checks below | af4a1a8d |

Group 四 (attaching to a running CUDA process, `bpftime attach`) was deferred.

## The problems and where they stand

Default path = load-time substitution (acdd7a77): the agent loads the patched image when cudart
loads a fatbin, so the process has one module per fatbin. Late path = the old bootstrap (patched
copies beside the originals, launches redirected by name), still used when CUDA was initialized
before the agent, or with `BPFTIME_CUDA_LOAD_SUBSTITUTION=0`.

| # | problem (examples-20261109) | default path | late path | commit |
|---|---|---|---|---|
| 1 | kprobe + kretprobe on one kernel break the victim (upstream #708, PR #709) | fixed (one module) | open | acdd7a77 |
| 2 | host-call arguments of all threads overwrite each other | fixed | fixed | d670246b |
| 3 | `-fdata-sections` victims lose `__constant__` copies (`$d` symbols) | fixed (no copies) | fixed | acdd7a77, 45743763 |
| 4 | bpf-to-bpf callee frame overlaps the caller's (CPU JIT too) | fixed | fixed | e94e2c04 |
| 5 | nvcc 13.2 default builds (kernel handles) not instrumented | fixed | open | acdd7a77 |
| 6 | stripped victims not instrumented | fixed | open | acdd7a77 |
| 7 | GPU ringbuf loses events | fixed (drops counted) | fixed | d670246b |
| 8 | failures silent at the default log level | fixed | fixed | 788fdb88 |
| 9 | every loader segfaults at exit | fixed | fixed | 17427685 |
| 10 | 1.06 ms per host call | about 10 us | about 10 us | d670246b |
| 11 | start-up +1.6 s (host symbol cache) | no cache read | 1774 -> 402 ms | acdd7a77, 45743763 |
| 12 | 16,464-byte GPU stack frame per thread | 592 B without local calls | same | 45743763 |
| 13 | each fatbin ingested 2-3 times | read once at load | once | acdd7a77, 45743763 |
| 14 | `cmake/cuda.cmake` misses CUDA 13's sbsa-linux | fixed | | 3695e13c |
| 15 | maps 1501-1503 need CUDA IPC (not on Tegra) | host-memory variants | same | b9e83c42 |
| 16 | probed modules have their own `__device__`/`__managed__` copies | fixed (one module) | open | acdd7a77 |
| 17 | static cudart (nvcc's default) not instrumented | fixed | open | acdd7a77 |

From the list "found by reading only": PTX errors ending in `std::terminate` (f28c42a7); the
trampoline source disagreeing with the shipped PTX (648fef28, the PTX is now built from source);
the host's request flag read without acquire (d670246b); SM detection's `setenv` and sm_61
fallback, the lookup pointer range, the two CUevents per launch, the patched `cuLaunchKernel`
without fallback and the `/tmp` work directories (af4a1a8d); 1512's thread count and the GPU map
constructor's leak and mock switch (b9e83c42). Also found and fixed: the patched images' probes kept
running after the last detach (718fefb1), the PTX compiler switching the application's locale
(af4a1a8d), GPU array maps written past their end by threads beyond `max_thread_count`
(d670246b).

Still open: the late path's problems above (it is reached by an injected agent, or CUDA
initialized in a static constructor); several CUDA processes on one segment; `cudaLaunchKernelEx`,
cooperative launches, stream capture, `cudaFuncSetAttribute`; SASS-only fatbins, `-rdc`,
driver-API `cuModuleLoad*`, several GPUs; detaching one of several probes (they stop at the last
detach only); LLVM 15 not knowing sm_87 (a warning); a CUDA build's agent in a process without a
usable GPU disables itself entirely, CPU probes included (found in the verification, older than
this round). Not tested: the fence placement of the host
call and ringbuf protocol (no mutation), discrete GPUs (the system-memory CAS lock, 1502 in device
memory), map 1504, the `cuLaunchKernel` fallback branch.

## Example matrix (`fix-matrix.sh`, 19 cases)

Before the load-path changes (matrix `base1`, on 17427685: groups 〇 and the exit fix in,
`verification/matrix/base1-summary.txt`): 3 of 18 passed. After the round (`robustness/runs`,
`liu1-summary.txt`, af4a1a8d): 19 of 19 on the default path (threadhist-small, added in 六, is the
19th). Late path (`BPFTIME_CUDA_LOAD_SUBSTITUTION=0`, `robustness/runs/liu-late-summary.txt`):
8 of 19 (krs-legacy, krs-fs, krs-autodetect, the two printk cases, ts-legacy, cg-legacy,
kt-legacy); the 11 others fail by the open late-path problems: kernel handles (krs-default,
cg-default, and threadhist's victim, a default nvcc 13 build), stripped, static cudart (two krs
cases, glob-static), separate globals (glob-shared), two probes on one kernel (hmt-legacy,
cc-legacy).

## Costs, before and after

| | examples-20261109 | after the round | where |
|---|---:|---:|---|
| one host call | 1.06 ms | about 10 us (32 warps waiting) | `protocol/` |
| hmt-legacy per launch (10 x 5 threads) | 21.3 ms | 0.35 ms | `robustness/runs/liu1-*` |
| ts-legacy per launch | 42.3 ms | 0.47 ms | same |
| cc-legacy per launch | 106 ms | 1.07 ms | same |
| sampling probe's stack frame | 16,464 B | 592 B | `costs/` |
| late-path start-up, krs-legacy | 1774 ms | 402 ms | `costs/` |
| default-path start-up, krs-legacy | (late path only then: 1722-1742 ms) | 419 ms | `robustness/runs` |
| ringbuf, sampling probe, 201 launches | 13 events | 186 records + 15 counted drops | `protocol/` |

## Verification (`verification/`, 2026-11-10 06:02-06:25 JST, dev af4a1a8d)

### CPU regression (`regression/`, build-probecheck rebuilt at af4a1a8d, CUDA off, checks on)

The round changed CPU code too: the llvmbpf overlay (the r10 frame step, the stack frame count),
the syscall server's exit path, cmake. As in `uprobe/startup-20261108`: every runtime test case as
root in a process of its own (`each.sh`): 282 cases, the same 3 map usage counter cases failing in
this build (no counters compiled in) as before, the rest passed; `[kernel_user]` 2/2 and the daemon
tests 10/10 as root; transformer 54 passed and the 1 expected failure; syscall trace 7/7 (the
daily filter); Frida attach 26/26 in 5 of 5 runs; the daily smoke: funclatency 5,216,000 calls
counted exactly, memleak 9 outstanding allocations, a SIGUSR1 detach under load with the workload
running to its end.

A full `cmake --build build-probecheck` fails in two diagnostics programs,
`benchmark/uprobe/diagnostics/per_cpu_hash_{path,delete}_layers.cpp` (the `per_cpu_hash_map_impl`
constructor call is ambiguous since 7670cee5, 11-03, not this round); the test targets build.

### CUDA build (`regression/`, build-cuda at af4a1a8d, never installed)

`bpftime_nv_attach_tests` 32/32 (148 assertions). The whole runtime suite in one non-root process
(`cuda-runtime.log`, 289 cases): 271 passed, 6 skipped, 12 failed. Rerun one by one on build-cuda
and build-probecheck (`cuda-reruns.sh`, `cuda-reruns.out`, `reruns/`):

- 4 "File exists" (shm progs attach, shm hash maps with sub process, shm json import/export,
  external hash map ops): `each.sh`'s root runs had just left root-owned segments with these
  tests' fixed names; alone, after removing them, they pass on both builds.
- 2 "Syscall server keeps shared memory while ... pid is alive": the script exported
  `BPFTIME_LOG_OUTPUT=console`, and these tests require their helper's output to be empty (each.sh
  runs them through sudo, which drops the variable); without it they pass on both builds.
- "A helper error on every call logs at 1, 10, 100, 1000": the rate limiter's count is per call
  site, and an earlier case of the same process had hit that site; alone it passes on both.
- "Test tail calling from userspace to kernel": needs root, as on every run.
- 4 fail only in the CUDA build, alone too ("Typed hash helpers serve hash maps next to arrays and
  global data", "Typed hash delete helper serves hash maps in plain and bound programs", the two
  `[aot-cache]` cases that construct an attach context): `bpf_attach_ctx`'s constructor throws
  "Unable to get device pointer for CommSharedMem" in a process that created the segment itself
  (only an agent registers it with CUDA). Not new: the constructor of a2acb4f8 threw there too
  (`cuda-no-device/hostptr.out`: `cudaHostGetDevicePointer` on memory nobody registered is
  `invalid argument`; af4a1a8d's `cuMemHostGetDevicePointer` returns 3, not initialized). These
  tests date from 11-08 and had not been run in a CUDA build.

Found with it (`cuda-no-device/`): the same exception disables a CUDA build's whole agent in a
process without a usable GPU. funclatency on callloop with build-cuda's agent and server: GPU
visible, 7,157,000 calls counted 7,157,000; `CUDA_VISIBLE_DEVICES=` (cuInit 100): 1,123,346,000
calls counted 0, the agent's CPU probes never attached (the agent's catch-all turns an exception
from `bpf_attach_ctx`'s constructor into `init_fail`). Not fixed.

### x86 (NUC)

af4a1a8d with the daily's settings, run by this session on the NUC (NUC session told before and
after); the files are on the x86-nuc branch (`uprobe/x86-nuc-dev-af4a1a8d-20261110`, NUC
session). Whole runtime suite in one process: 281 cases, 274 passed, 6 skipped (3 need root, 3 map
usage statistics not compiled in), 1 failed (the user-to-kernel tail call, root); `[jit_load]` and
the syscall server exit test 3 times each, `[aot-cache]`, the array JIT groups and Frida (3 runs)
all pass.

### GPU timing (`gpu-meas/`, `meas-final.sh`, the rounds of examples-20261109's `meas.sh`)

cpu5 at 1728000 and the GPU at 1020 MHz before and after; 06:22-06:24, info level.

Victim start-up, exec to exit (ms, vec_add_legacy 5 3 5 1, 5 rounds):

| | examples-20261109 | af4a1a8d |
|---|---:|---:|
| no agent | 112-113 | 115-126 |
| agent, empty kprobe | 1722-1742 | 347-419 (the first run 419) |

Every agent run loaded one patched image and printed C[1] = 3.00. Under strace (`fin-C-strace`,
376 ms; ms after exec): 3-137 libcuda, device init and the 64 MiB host registration, 137-188 the
three PTX pass libraries, 188-231 the PTX compiler library, 241-245 cuobjdump, 245-310 patch,
compile, load (examples-20261109: 1705 ms, 1.39 s of it the host symbol cache).

Per launch, victim's median (us, vec_add_legacy 1048576 200 256 0):

| | examples-20261109 r1-r3 | af4a1a8d r1-r3 |
|---|---|---|
| no agent | 162.18 162.31 162.92 | 163.50 163.40 162.12 |
| empty kprobe | 178.34 175.97 171.56 | 172.49 172.55 169.70 |
| sampling kprobe | 586.04 583.57 585.59 | 583.54 575.89 574.42 |

The sampling probe's ringbuf: 187, 184, 184 records delivered and 14, 17, 17 counted drops: 201
launches each run (examples-20261109: 13 of 201, nothing counted).
