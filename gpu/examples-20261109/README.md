# examples-20261109: bpftime CUDA attach on the Orin Nano: example runs, measurements, problems

Board: Orin Nano 8 GB, L4T R39.2.1, CUDA 13.2 (nvcc 13.2.86), nvgpu integrated GPU (sm_87, 8 SMs),
driver 595.78. Code: the dev candidate a2acb4f8. `attach/nv_attach_impl`,
`runtime/src/attach/bpf_attach_ctx_cuda.cpp`, `runtime/src/bpf_map/gpu`, `cmake/cuda.cmake` and the
llvm-jit submodule (87bac39) are byte-identical to official/master, so every problem below applies
upstream too (the agent's `main()` wrapper and `__cudaRegisterFatBinary` interposer are identical as
well). Follows `../feasibility-20261109` (the survey). Runs 2026-11-09 03:54-04:21 JST (meas.sh
04:16-04:19), a control at 06:02 and gap runs 06:29-06:34 (section 7); read-only verification
04:23-06:28. No repo file was changed.

## Build (`build/`)

`build-cuda` in the candidate worktree: RelWithDebInfo, gcc-13, LLVM 15, LTO off, debug logs compiled
in, `BPFTIME_ENABLE_CUDA_ATTACH=ON`, `BPFTIME_CUDA_ROOT` = a root of symlinks with
`targets/aarch64-linux -> sbsa-linux` (problem 14). Every target linked, including
`libnv_attach_impl_ptx_compiler.so` with `libnvptxcompiler_static.a` inside (53k exported symbols).
`bpftime_nv_attach_tests` passed 21/21 (`logs/nv_attach_tests.log`), but those cases run on the CPU
only (string rewriting, the register guard, PTX generation for sm_60): they call no CUDA,
nvPTXCompiler or Frida API, so they do not show that attaching works. build-cuda was never installed
and served only these runs.

## Harness (`scripts/`)

- `run.sh NAME EXDIR SERVER_CMD VICTIM_CMD`: the loader (an example's userspace program) under
  `libbpftime-syscall-server.so`, then the victim under `libbpftime-agent.so` (`NO_AGENT=1`: plain),
  both from build-cuda, on a segment of its own, every process under `timeout`.
- Victim: `benchmark/gpu/workload/vec_add.cu`, arguments elements, timed launches (after one
  warm-up), threads per block, blocks. It copies the element count into `__constant__ d_N` with
  `cudaMemcpyToSymbol`, prints the median time per launch and `Validation check: C[0], C[1]`
  (correct: 0.00 and 3.00). Builds: `vec_add` (nvcc 13.2 defaults), `vec_add_legacy`
  (`-D__NV_LEGACY_LAUNCH -Xcompiler -D__NV_LEGACY_LAUNCH`), `vec_add_legacy_fs` (plus
  `-ffunction-sections -fdata-sections`), `vec_add_legacy_stripped` (`strip --strip-all`).
- A probe counts as working only on the loader's own output (server.log). Every run's validation
  line was checked.
- `meas.sh`: the timing rounds, SPDLOG_LEVEL=info, cpu5 at 1728000 and the GPU at 1020 MHz before and
  after (`logs/meas.out`).
- `probes/kt3-empty`, `probes/kt3-sample`: kernel_trace's loader with the probe moved to the
  benchmark kernel `_Z9vectorAddPKfS0_Pf`. Empty: `return 0`. Sample: every thread calls the block
  and thread index helpers; thread 0 reads globaltimer and writes one GPU ringbuf event per launch.

## 1. Runs (`runs/<name>/`: meta.txt, server.log, client.log)

| run | loader (probes, maps) | victim | result |
|---|---|---|---|
| krs-legacy | kernelretsnoop (kretprobe, ringbuf 1527) | vec_add_legacy 5 3 5 1 | 20 events = 4 launches x 5 threads; C[1] 3.00 |
| krs-geom | same | vec_add_legacy 7 2 7 1 | 21 = 3 x 7; 3.00 |
| krs-noagent | same | vec_add_legacy, no agent | 0 events; 3.00 |
| krs-default | same | vec_add 5 3 5 1 | **0 events**; 3.00 (the original kernel ran) |
| krs-vec_add_legacy_stripped | same | stripped build | **0 events**; 3.00 |
| krs-vec_add_legacy_fs | same | fs build | 20 events; **C[1] 0.00** |
| krs-fs-noagent | same | fs build, no agent (06:02) | 0 events; 3.00 |
| kt-legacy | kernel_trace (kprobe, thread 0 writes 1527) | its own vec_add, legacy | 4 events, one per launch; victim output lost (buffered stdout, ended by SIGINT) |
| kt3-empty-debug | kt3-empty, DEBUG | vec_add_legacy 5 3 5 1 | 4 patched launches; 0 B stack; 3.00 |
| hmt-legacy | host_map_test (kprobe + kretprobe; maps 1512, 1513; printk) | vec_add_legacy 10 5 10 1 | maps exact (shared_counter 6 per key, call_count 60 from 10 threads); all 120 printk lines name thread 0; **C[1] 0.00**; 21.3 ms per launch |
| cc-legacy | cuda-counter (kprobe + kretprobe; CPU hash maps; printk) | vec_add_legacy 10 5 10 1 | 610 host calls; **C[1] 0.00**; 106 ms per launch |
| cc-noagent | same | no agent | 35.2 us per launch; 3.00 |
| ts-legacy | threadscheduling (kprobe; CPU hash maps) | vec_add_legacy 10 5 10 1 | thread_mapping holds **1 key of 10** after 60 successful updates; 42.3 ms per launch; 3.00 |
| cg-vec_add_graph | cudagraph (kprobe + kretprobe) | default-launch graph victim | the 5 graph launches counted; the one `<<<>>>` launch missed |
| cg-vec_add_graph_legacy | same | legacy | 6 (1 + 5) |
| th-ipc | threadhist (map 1502) | none | loader fails: `cuIpcGetMemHandle` 801, map 1502 not created |
| gdb-server-exit | kernelretsnoop inside gdb | none | backtrace of the exit crash (problem 9) |
| krs-static-legacy | kernelretsnoop | vec_add_legacy built with static cudart (nvcc's default) | **0 events**; 3.00 |
| krs-static-default | same | vec_add, static cudart | **0 events**; 3.00 |
| krs-static-legacy-info | same, info level | static legacy | 0 events; the only agent line is "Attach successfully" |
| gdb-static-vec_add_legacy_static | kernelretsnoop; victim in gdb | static legacy | bpftime's `cuLaunchKernel` hook reached 4 times with a kernel handle: `cuFuncGetName` 400, `cuKernelGetName` names it |
| glob-noagent | kt3-empty | `gaps/globals_legacy`, no agent | 3.00; g, g, m = 5 5 5 |
| glob-agent | kt3-empty (probe on vectorAdd only) | `gaps/globals_legacy` | 3.00; **g, g, m = 0 0 0**: the unprobed `inc()` ran in the patched module |
| krs-autodetect | kernelretsnoop, `BPFTIME_SM_ARCH` unset | vec_add_legacy 5 3 5 1 | "Auto-detected GPU SM arch: sm_87"; 20 events |
| m-A-\*, m-B-\*, m-C-strace | meas.sh | section 2 | |

Every loader died with SIGSEGV at exit (status 139 in every completed run; hmt-legacy 124, its
timeout came first; th-ipc stopped early), with or without a victim: problem 9.

## 2. Measurements (meas.sh, clocks locked, info level)

Victim start-up, exec to exit (`client_ms`, 5 rounds, vec_add_legacy 5 3 5 1): no agent 112-113 ms;
agent with an empty kprobe 1722-1742 ms (+1.61 s). One run under `strace -f -e trace=execve,openat`
(`runs/m-C-strace`, 1791 ms; ms after exec):

| span | what |
|---|---|
| 3-52 | libcuda and device init |
| 52-140 | not resolved by this trace (CUDA init, 64 MiB host registration) |
| 140-193 | three PTX pass libraries, dlmopen'ed with their own libc/libstdc++ |
| 193 | `libnv_attach_impl_ptx_compiler.so` |
| **236-1629** | host symbol cache: the agent's own `.symtab` 734 ms, 29 small libraries 44 ms, the PTX compiler library 614 ms |
| 1630-1691 | two cuobjdump runs, patch, ptxas, module load |
| 1705 | exit_group |

Per launch (victim median, us; vec_add_legacy 1048576 200 256 0 = 4096 x 256 threads, 201 launches):

| | r1 | r2 | r3 |
|---|---:|---:|---:|
| no agent | 162.18 | 162.31 | 162.92 |
| empty kprobe | 178.34 | 175.97 | 171.56 |
| sampling kprobe | 586.04 | 583.57 | 585.59 |

At 5 threads (m-A) the empty kprobe has no measurable cost: 24.4-40.6 us (two clusters) against
34.7-38.1 without the agent. ptxas (`ptx/`): the empty probe's patched kernel has 0 B stack and 12
registers; the sampling probe's and kernelretsnoop's 16,464 B stack frame and 28 registers
(threadscheduling 16,488 B, 38).

## 3. Problems

Mechanism and upstream status as checked by the verifiers (section 5); line numbers are in
`verify/verify-result.json`.

### Silently wrong results

1. **Two probes on one kernel (kprobe + kretprobe) break the victim** (hmt-legacy, cc-legacy:
   C[1] 0.00; one probe or no agent: 3.00). The second link reruns the late bootstrap without
   dropping the first patched module set. Launches go to the last set (last writer wins), while the
   `cudaMemcpyToSymbol` mirror walks the sets from the front and writes `d_N` into the first, so the
   launched kernel reads `d_N` = 0. Both bootstraps finish before `main`, before the copy. Upstream
   issue #708, open PR #709 (the maintainer asked for a wait on in-flight launches before unloading).
2. **Host-call arguments from different GPU threads overwrite each other.** Every host-call helper
   (map lookup/update/delete, printk) writes its key, value or arguments into the one shared request
   buffer before taking the lock, all lanes at once; each lane then sends whatever is there.
   ts-legacy: 60 successful updates with 10 different keys left 1 key; hmt-legacy: 120 printk lines
   all name thread 0 while the per-thread map in the same run holds threads 0-9. PR #503 adds
   fences only; open PR #624's verifier treats host-call arguments that differ between lanes as
   unsupported ("host-bridge payload uniformity"), although the upstream threadscheduling example
   uses per-thread keys.
3. **`-ffunction-sections`/`-fdata-sections` victims lose their `__constant__` copies on aarch64**
   (krs-vec_add_legacy_fs: 20 events, C[1] 0.00; the same binary without the agent 3.00). Since #695
   the host symbol cache keeps STT_NOTYPE symbols, so AArch64 mapping symbols (`$x`, `$d`, size 0)
   share addresses with real symbols. Ties are not broken, and here `d_N` resolved to `$d`
   (established by elimination; the mirror's success path logs nothing), no module exports `$d`,
   and the copy is skipped without a message. The kernel's own address ties with `$x` too; that it
   resolved correctly depends on the sort order of all 556k symbols. Not tracked upstream (x86 has no
   mapping symbols).
4. **llvmbpf bpf-to-bpf calls give the callee a frame that overlaps the caller's** (CPU JIT too;
   `extra/localcall/`). A local call moves r10 by `STACK_SIZE` = 64, a count of 8-byte slots used as
   bytes (vm/llvm-jit/src/compiler.cpp: the call and the return). bpftime never enables the
   kernel-compatible mode, so this is the path every bpftime program takes. Test: main stores a
   value at r10-128, calls a subprogram that zeroes r10-8..r10-64 of its own frame: the value is
   gone (at r10-48 it survives). Any program with a non-inlined subprogram call whose caller keeps
   stack data deeper than 64 bytes across the call is affected. Not GPU-specific; found by the perf
   verifier while reading the GPU stack code.

### Silently not instrumented, or events lost

5. **Victims built with nvcc 13.2 defaults are not instrumented** (krs-default: 0 events). CUDA 13
   host stubs fetch a kernel handle once (`__cudaGetKernel`) and launch with it; the handle (a heap
   pointer) reaches `cudaLaunchKernel`. bpftime names launches by host stub address: the
   registration map is empty under LD_PRELOAD (the agent starts after the app's constructors), the
   late path's symbol lookup turns the heap pointer into the nearest lower symbol of size 0
   (`__end__` here, `_fini` in #646) because size-0 entries skip the range check, and the driver-level
   hook tries only `cuFuncGetName` (400 on a handle). The graph hooks already use `cuKernelGetName`,
   so graph launches of the same binary are counted (cg-vec_add_graph). Upstream issue #646 (GB300,
   aarch64, CUDA 13.2), open, no PR; upstream GPU CI runs CUDA 12 where nvcc still uses the legacy
   stubs.
6. **Stripped victims are not instrumented** (krs-vec_add_legacy_stripped: 0 events). The late path
   can name a host address only from `.dynsym` (no hidden stubs there) or `.symtab` (removed by
   strip); distribution packages ship stripped binaries. Variables have the same dependency, so a fix
   that only resolves launches would turn this into wrong results like problem 3.
7. **The GPU ringbuf (1527) loses events.** m-B-sample: 13 of 201 events arrived (the first 13
   launches). A thread slot holds 16 entries and a full slot drops the event on the GPU; the host
   drains at most one entry per slot per poll and returns at the first slot being written; the
   examples poll once a second. 185 of the 188 lost events were dropped on the GPU within about
   0.12 s (the slot was full); draining every entry per poll would recover only the other 3, so the
   example must also poll faster or reserve more entries, and bpftime should count the drops.
   Publication itself is unsound: no system-scope fence between the
   data and the flags, and the host can consume a slot whose tail moved before its data was written.
   Not tracked upstream.
8. **None of these failures shows at the default log level.** In all agent logs the only warnings
   are "VM factory ... already registered" and "Not a GPU map!" (the printk format array). "Attach
   successfully" appears when nothing will be instrumented (krs-default, stripped), and the
   wrong-result runs log nothing at all.

### Crash

9. **Every loader under the syscall server segfaults at exit** (gdb-server-exit). The segment is
   unmapped by `__destruct_shm`, an ELF destructor in the server library's `.fini_array`. `_dl_fini`
   runs it before the destructors of `libcuda.so.1` and `libnvrm_mem.so`, which the server library
   depends on. `libnvrm_mem`'s destructor then closes the `/dev/nvmap` fd that `cuInit` opened (the
   server always calls cuInit), and the server's `close()` hook takes the handler path on the
   unmapped segment (`clear_id_at`). The dev branch already sets `global_shm_released` there, but the
   close path never reads it; official/master faults one frame earlier. Events are not affected;
   every loader exits with status 139. Not tracked upstream.

### Cost

10. **Each host call costs 1.06 ms, one at a time** (cc 610 calls, 1.062 ms each; hmt 1.063; ts
    1.060). The host watcher sleeps 1 ms after every iteration, also right after serving a request,
    so each request waits one full sleep. The transport itself costs about 5-6 us on this board
    (survey: `hostreg.hsloop-atomdev.txt`, `hostreg.hsmulti.txt`). One request slot, one watcher and
    one GPU spin lock serialize every host call of every thread, so a launch takes at least threads x
    calls x 1.06 ms: one printk per thread at 1M threads would take about 18 minutes per launch.
    Moving the sleep into the idle branch is not enough: the next lane posts only after it has seen
    the answer, so the watcher must keep polling for a short window.
11. **Start-up +1.6 s, 1.39 s of it the host symbol cache.** Before `main`, the agent reads `.symtab`
    and `.dynsym` of every loaded module, one `seekg` and 24-byte read per symbol (about 2.2 us each):
    556,752 entries, 97% from two bpftime libraries that carry no GPU code (the agent with static
    LLVM: 320,386; the PTX compiler library: 217,982), 43% of them mapping symbols. Only the victim
    has a fatbin. The cache is used only to name host stubs and `__constant__`/`__device__` shadows,
    which live in the module that registered the fatbin. Reading only that module: about 0.34 s.
12. **A probe whose stack address escapes gets a 16,464-byte stack frame per thread.** llvmbpf's GPU
    lowering reserves 32 call frames x 512 B + 80 B for every program; it disappears only when LLVM
    can drop the array, as for the empty probe. The sampling probe touches 56 B of it. At full
    occupancy the driver backs 16,464 B x 1536 threads x 8 SMs = 193 MiB of the shared DRAM. How
    much of the sampling probe's +410 us comes from the frame and how much from the per-thread
    helper work is not separated; static bounds suggest the work explains a large share.
13. **Each fatbin is ingested 2-3 times per bootstrap.** `.nv_fatbin` holds two contiguous blobs (an
    empty device-link cubin, then the cubin with PTX), and the wrapper points into the same bytes, so
    cuobjdump runs twice (kernel_trace and cudagraph victims: three times, two wrappers); about
    6.6 ms each. The caches turn the repeats into hits: no extra compile, load or device memory.

Not established: a per-launch cost of the hooks when nothing is redirected. krs-default's 58 us
against krs-noagent's 36 us mixes the launch ABI, the loader, DEBUG output and unlocked clocks; at
locked clocks agent runs were not slower than no-agent runs.

### Build and maps

14. **`cmake/cuda.cmake` hard-codes `targets/aarch64-linux`**; CUDA 13 installs only
    `targets/sbsa-linux`, also on Jetson. Configure succeeds and only checks the PTX compiler
    library; the compile misses `cuda.h` and the link misses `-lcudart` (shown by a preprocess run and
    a probe link in the survey, not by a full stock build). The CUPTI include fallback points at a
    directory that does not exist either. PR #647 mentions the same symlink workaround.
15. **Maps 1501, 1502 and 1503 need CUDA IPC**, which this GPU does not support (801). The HOST
    variants 1512/1513 ("for Tegra") work (hmt-legacy), and the device code already handles them.
    Blocked here: threadhist (1502), atomizer, mem_trace, launchlate, cutlass counter (1503),
    threadscheduling_dynamic_hook (1501), gpu_shared_map (both).

### Found in the gap runs (section 7)

16. **Kernels in a probed module run on their own copies of `__device__` and `__managed__`
    variables** (glob-agent: g, g, m = 0 0 0 instead of 5 5 5, for a kernel that was not probed).
    The late bootstrap loads the rewritten PTX as a separate module and redirects every kernel in it,
    probed or not (prefill registers every entry function); that module has its own storage for
    every global. Only `cudaMemcpy{To,From}Symbol` are mirrored, and only into the patched module;
    `cudaGetSymbolAddress`, managed variables and values that kernels write are not. Any program
    whose kernels keep state in device globals computes wrong results once any kernel of the same
    module is probed.
17. **Victims linked with static cudart, nvcc's default, are not instrumented** (krs-static-\*:
    0 events, only "Attach successfully" at info). The runtime hooks need cudart's dynamic exports.
    The driver-level `cuLaunchKernel` hook is reached (gdb: 4 hits) but gets a kernel handle and
    names it only with `cuFuncGetName` (400); `cuKernelGetName` names it. `cudaMemcpyToSymbol` is
    internal to static cudart, so `__constant__` copies could not be mirrored either.

### Found by reading only (not run)

- A PTX compile or module load error inside the agent ends in `std::terminate` (thrown inside a
  Boost thread pool or a Frida listener): the victim dies with SIGABRT instead of running
  unpatched. Triggers: PTX that nvPTXCompiler refuses for sm_87, e.g. `.version` below 7.4 (never
  raised for sm_87) or library PTX for newer GPUs.
- SM detection falls back to sm_61 (refused by CUDA 13) when driver calls fail, and calls `setenv`
  inside the victim. Auto-detection itself works (krs-autodetect: sm_87, 20 events; the second
  lookup then reads the variable it set).
- The host reads the request flag without an acquire (ARMv8 does not order the following loads); PR
  #503.
- Host pointers from map lookups are translated to device pointers without a range check; a value
  outside the registered segment (kernel-shared arrays, thread-local buffers) becomes an illegal
  address.
- Not handled: `cudaLaunchKernelEx`/`cuLaunchKernelEx`, cooperative launches, launches during
  stream capture (run unpatched), and `cudaFuncSetAttribute` settings (a patched launch needing
  them fails and falls back).
- Several CUDA processes on one segment share one request slot (each agent memsets it at start) and
  answer each other's requests; the GPU lock is per patched module, so two module sets in one
  process are not serialized against each other either.
- With cuBLAS/cuBLASLt/cuSPARSE loaded, the bootstrap would extract and compile every fatbin
  (about 2,800 wrappers, 480 PTX files for sm_120) whether or not a probe touches it. PR #619, issue
  #552.
- LLVM 15 does not know sm_87: a warning on the direct-run path only.
- `bpftime attach` to a running CUDA process was not tried (Frida injection on aarch64).
- `default_trampoline.cu` says `GPU_HELPER_MAX_BUF = 1 << 24`; the shipped PTX and the host use
  `1 << 20`. Regenerating the PTX from the source as it is would break the host-call layout.
- Smaller: two CUevents per redirected launch; a failed patched launch through `cuLaunchKernel`
  returns the error to the app instead of falling back; map 1502 honours
  `BPFTIME_MAP_GPU_THREAD_COUNT` but 1512 and the reported value size do not; a GPU map constructor
  that throws leaks its `cuMemAlloc` buffer and leaves the server's mock switched off; detaching one
  of several CUDA probes keeps its instrumentation until full detach; debug-level runs leave a
  `/tmp/bpftime-fatbin-work.*` directory per fatbin.

Regenerating `trampoline_ptx.h`, which the device-side fixes of problems 2 and 7 need, does not
work on this board as is: clang 15/16 fail on the aarch64 host headers with CUDA 13.2, and nvcc 13.2
rejects the `__atomic` builtins in device code (`extra/trampoline-regen/attempts.txt`; the Makefile
expects clang++-17).

## 4. What works

With legacy-launch, unstripped victims linked with shared cudart, one probe, and kernels that keep
no state in device globals: the kprobe and kretprobe PTX passes,
nvPTXCompiler from sm_75 PTX to sm_87, cuobjdump extraction, module load and launch redirection, the
64 MiB host-registered segment, the globaltimer and thread/block index helpers, the 1527 ringbuf at
low rates, the 1512/1513 host-backed GPU maps, CPU maps and printk through host calls (slow, problems
2 and 10), and CUDA graph launches, including default nvcc builds. Event counts match the launch
geometry exactly. An empty probe costs nothing measurable at 5 threads and 8% at 1M threads.

## 5. Verification (`verify/`)

A workflow of six read-only agents (`verify/workflow-script.js`): five verifiers, each trying to
refute one group of my findings from the logs, the code and the upstream threads (launch, wrong
results, exit crash, cost, build and maps plus the survey's unverified list), and one reader of the
raw logs who did not see my findings. A critic then looks for gaps and contradictions. Full outputs:
`verify/verify-result.json`. Corrections to my first reading:

- The exit crash comes from an ELF destructor run by `_dl_fini` in dependency order, not from a C++
  static destructor before `_dl_fini`; `libnvrm_mem.so` is loaded at start (DT_NEEDED), cuInit only
  makes its destructor close an fd.
- The 1.06 ms per host call is the watcher's sleep, not transport or serialization.
- The 16,464 B frame is 32 call frames, not "the whole eBPF stack", and only survives when a stack
  address escapes.
- The double ingestion wastes a cuobjdump run, not a compile or a load.
- "Never instrumented" for default builds holds for `<<<>>>` and `cudaLaunchKernel`, not for graph
  launches.
- New: problems 2, 4 and 7, which I had not seen.

The critic's first attempt was cut off; the second (06:05-06:27) listed gaps, contradictions and a
priority order (`verify/verify-result.json`, `critic`). Its gaps led to section 7. Its corrections:
the ringbuf loss is mostly GPU-side capacity (problem 7); #624 disagrees with fixing problem 2 at
run time; a test of the exit crash must go through the `close()` interposer (clearing `manager` in
the destructor would be removed by the compiler); #709's wait for in-flight launches is required,
since launching a function of an unloaded module is undefined.

Not run, with the failure the critic predicts: SASS-only fatbins (no PTX: silently unpatched), PTX
older than 7.4, a compile failure (`BPFTIME_SM_ARCH=sm_61`: SIGABRT), CUDA code dlopen'ed after
start-up (the registration path never ran), two translation units with same-named static kernels
or `__constant__`, concurrent streams, `-rdc`, and `bpftime attach` (globals written before the
attach never reach the patched module). The gdb observations of problems 1 and 3 are written out in
the verifier outputs and were not run.

## 6. Caveats

- hmt-legacy and cc-legacy ran before a harness fix: their victims started about 100 s after the
  loader, and hmt's loader was stopped by its own timeout.
- The krs, kt, cc, hmt, ts and cg runs (03:54-04:15) fell in the daily timer's window (it skipped:
  official unchanged at 03:32) and ran with DEBUG output before the clocks were checked; their
  timings are not evidence. Only m-A, m-B and m-C (meas.sh) are.
- kt-legacy's victim output was lost, so its correctness is not verified.
- Example bugs, not bpftime: cuda-counter calls=51 and threadscheduling SM=51 / warps=24 instead of
  60 come from lanes racing on `BPF_NOEXIST` first inserts; cuda-counter's 60 failed deletes use the
  wrong key; the victim's "GB/s" figures are MiB/s.

## 7. Gap runs and the driver's load path (`gaps/`, 06:29-06:34)

`gaps/run-gaps.sh` ran the static-cudart, globals and auto-detection rows of section 1;
`gaps/gdb-static.sh` traced the static victim's launches.

Where cudart hands fatbins to the driver (`gaps/gdb-loadpath.out`, no agent): with shared and with
static cudart, each fatbin is loaded with `cuLibraryLoadData` after `main` begins (at the first
runtime call; the image is the fatbin wrapper), and kernels, globals and managed variables are then
looked up by name with `cuLibraryGetKernel`, `cuLibraryGetGlobal` and `cuLibraryGetManaged`.

Substituting a modified PTX for that image (gdb spike, `gaps/subst/`: `inc()` adds 10, vectorAdd
adds 100) made cudart use it for everything, with shared and with static cudart: the launch ran the
modified kernel (C[1] = 103), `cudaMemcpyToSymbol` reached its `d_N`, and `cudaGetSymbolAddress`,
`cudaMemcpyFromSymbol` and the managed variable all saw the modified `inc()` (50 50 50). A patched
image handed over at this call would need no launch redirection, no host symbol names and no global
mirroring, which is where problems 3, 5, 6, 16 and 17 (and the 1.39 s symbol cache) come from.
cudart passes one library option, `CU_LIBRARY_BINARY_IS_PRESERVED` = 1, and no JIT options
(`gaps/gdb-libopts.out`, both cudart kinds), so a substituted image must stay in memory for the
life of the process. (`globals.mod.ptx` is stored without the terminating NUL the driver needs.)

## Words used here

- **victim**: the CUDA program being traced. **loader**: the example's userspace program that loads
  the eBPF objects under the syscall server.
- **late bootstrap**: with LD_PRELOAD the agent starts after the app's constructors registered its
  fatbins, so it scans the loaded files for fatbins, extracts PTX with cuobjdump, rewrites and
  compiles it, loads it as separate modules and redirects launches by kernel name.
- **legacy launch**: nvcc's host stub from before CUDA 13 (`__NV_LEGACY_LAUNCH`), which passes the
  host stub address to `cudaLaunchKernel`. **kernel handle**: what CUDA 13's default stubs pass
  instead (`cudaKernel_t`, the same type as `CUkernel`).
- **host call**: an eBPF helper that the GPU cannot serve itself (CPU maps, printk, pid): the GPU
  thread posts a request in host-registered shared memory and spins until the agent's watcher
  thread answers.
- **patched module set**: the modules one late bootstrap loads from the rewritten PTX.
- **mapping symbols**: AArch64 assembler labels `$x` (code starts) and `$d` (data starts), size 0,
  at the start of every section.

## Files

- `runs/`: one directory per run; `logs/`: example builds, nvcc commands, `meas.out`,
  `nv_attach_tests.log`; `build/`: configure and build tails, cmake cache subset.
- `ptx/`: patched PTX and ptxas `-v` output for the three probes.
- `probes/`, `scripts/`: the probe variants and the harness.
- `extra/localcall/`: the bpf-to-bpf frame test (source and output); `extra/trampoline-regen/`: the
  attempts to regenerate the trampoline PTX.
- `verify/`: the verification workflow script and its outputs (`critic` holds the second attempt).
- `gaps/`: the gap runs' scripts and outputs, the globals victim, the load-path trace and the
  substitution spike (`subst/`).
