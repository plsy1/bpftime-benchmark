# feasibility-20261109: bpftime CUDA attach on the Orin Nano (first look)

Board: Orin Nano 8 GB, L4T R39.2.1, CUDA 13.2 (`/usr/local/cuda-13.2`, nvcc 13.2.86), nvgpu
integrated GPU, compute capability 8.7 (sm_87), driver 595.78, LLVM 15, GCC 13. Code: the dev
candidate a2acb4f8 (`dev/uprobe-official-177a1dc465ff`), `BPFTIME_ENABLE_CUDA_ATTACH` (off in
every build so far). Read-only survey, 2026-11-09 01:32-03:30 JST: five readers (pipeline, build,
examples, board, upstream), a synthesis, and a critic (`workflow-script.js`). No repo file was
changed and no bpftime CUDA build existed yet. Small CUDA test programs ran in the session
scratch. Their sources and outputs are in `gpu-env/`; binaries and cmake trees are omitted.

## What ran on the GPU

`gpu-env/attach-probe/mini_pipeline.sm87.txt`:
- bpftime's checked-in `trampoline_ptx.h` with a probe, rewritten as bpftime rewrites PTX;
- compiled by nvPTXCompiler 13.2 for sm_87 and loaded with `cuModuleLoadDataEx`;
- 1024 of 1024 probe hits written into a map in host memory registered with `cudaHostRegister`.

The probe was hand-written LLVM IR (`probe.ll`) and the rewrite a Python copy of the entry pass
(`compose.py`). bpftime's own passes (`ptxpass_*`), llvmbpf's GPU lowering and the agent's hooks
have not run on this board yet.

## Confirmed

1. **Build paths.** `cmake/cuda.cmake` uses `targets/aarch64-linux`. CUDA 13.2 here installs only
   `targets/sbsa-linux`: NVIDIA's single layout for Arm servers and Jetson since CUDA 13.
   Configure succeeds, then compiles miss `cuda.h` and links miss `-lcudart`. Workaround without
   a repo edit: a root of symlinks with `targets/aarch64-linux -> .../sbsa-linux`
   (`buildcfg/`).
2. **CUDA 13 launches.** nvcc 13.2's default host stubs call `__cudaGetKernel` and then
   `__cudaLaunchKernel`. The handle that reaches `cudaLaunchKernel` and `cuLaunchKernel` is a
   `CUkernel`, not the host stub address. bpftime names a launch by host stub symbol or
   `cuFuncGetName`, and `cuFuncGetName` returns 400 on that handle (`cuKernelGetName` returns
   `_Z9vectorAddPKfS0_Pf`; `attach-probe/gdb_launch_trace.txt`). So the unpatched kernel runs and
   nothing reports it. Same symptom upstream on GB300 (issue #646). For experiments,
   victims built with `-D__NV_LEGACY_LAUNCH -Xcompiler -D__NV_LEGACY_LAUNCH` use the old stubs,
   which pass the host stub address (`synth-legacy/`).
3. **No CUDA IPC.** `cuIpcGetMemHandle` returns 801 (not supported) on this GPU
   (`attach-probe/ipc_test.txt`). Map types 1501, 1502 and 1503 export device memory that way, so
   the syscall server cannot create them. Usable: 1512/1513 (GPU maps in host-registered shared
   memory, added "for Tegra"), the 1527 GPU ringbuf, and CPU maps reached through host calls.
   - Examples without IPC maps: kernelretsnoop, kernel_trace, host_map_test, cuda-counter,
     cudagraph, threadscheduling.
   - Need root and a bpf map mmap: threadhist-gpu-kernel-shared-map,
     launchlate-kernel-gpu-shared-map.
   - The rest are blocked.
4. **Victims need PTX and shared cudart.** Only PTX is rewritten: `-arch=native` or sm_87-only
   builds carry SASS only (`fatbin.list.txt`). nvcc links cudart statically by default, which
   hides the hooked functions (`link.txt`). cuBLAS ships PTX for sm_120 only.

## Found by reading code (upstream issue #708, open PR #709)

A kprobe and a kretprobe on the same CUDA kernel build two sets of patched modules. Copies to
`__constant__`/`__device__` variables go to the first set; launches use the last set. The app then
computes wrong results without any error (cuda-counter: `C[1] = 0`, expected 3). Single-probe
runs are not affected.

## Possible, not yet run

- The first aarch64 build with CUDA attach: `libnvptxcompiler_static.a` linked into a shared
  library, and three pass libraries with static LLVM 15.
- Frida hooks on `libcudart.so.13` and L4T's `libcuda.so.1`; `dlmopen` of the pass libraries.
- A PTX compile error inside the agent ends in `std::terminate`: it is thrown in a Boost thread
  pool. If SM detection fails, the fallback is sm_61, which CUDA 13 rejects.
- GPU-to-host helper calls: the host reads the request flag without an acquire fence. This is
  the area of upstream PR #503, Orin X.
- Map lookups through host calls turn host pointers into device pointers without a range check.
- The late path's symbol lookup can return AArch64 mapping symbols (`$x`/`$d`). It also needs
  an unstripped victim.
- Not handled: `cudaLaunchKernelEx`, launches during stream capture, and `cudaFuncSetAttribute`
  settings.
- More than one CUDA process on one shared memory segment.
- LLVM 15 does not know sm_87. This matters only for running a program directly on the GPU,
  which needs IPC maps anyway.

## Next

Build `build-cuda` in the candidate: LTO off, debug logs compiled in, the symlink root. Then run
the IPC-free examples with legacy-launch victims, with controls (no agent, other thread counts,
default-launch victim). The plan is in `workflow-result.json` at `result.synthesis.experiment_plan`,
and the critic's corrections are at `result.critic`. Judge probes by server-side data only, and
check the victim's own output too.

## Words used here

- **late bootstrap**: with `LD_PRELOAD`, the agent starts after the app's constructors have
  registered its fatbins. So it scans the loaded files for `.nv_fatbin` sections, extracts PTX
  with `cuobjdump`, rewrites and compiles it, loads it as separate modules, and redirects
  launches by kernel name.
- **legacy launch**: nvcc's host stub from before CUDA 13 (`__NV_LEGACY_LAUNCH`), which calls
  `cudaLaunchKernel` with the host stub address.
- **victim**: the CUDA program being traced.

## Files

- `workflow-result.json`: every reader's findings, the synthesis (verdict, ranked blockers,
  experiment plan, contradictions) and the critic.
- `gpu-env/`: test sources and outputs, e.g. `attrs.out.txt` (device attributes), `hostreg.*.txt`
  (host registration, coherence, GPU atomics on host memory), `fatbin.list.txt`,
  `attach-probe/`, `synth-legacy/`, `review/` (critic checks), `issue_*.txt` and `pr_*.txt`
  (upstream threads).
