GPU map design: hard constraints of bpftime's mechanism (dev 52126f33)
Paths: code is relative to /home/jetson/src/bpftime-daily-state/worktrees/4708f4092d1e/candidate. R = /home/jetson/src/results/gpu. L = /home/jetson/src/workloads/llama.cpp/ggml/src/ggml-cuda. RD = origin/main:RESEARCH_DIRECTIONS.md. The PTX ISA quotes come from docs.nvidia.com, fetched 2026-11-15 and read as untrusted data. Anything not read directly is marked [inferred].

A. map_info / MapBasicInfo
1. Layout. MapBasicInfo is {bool enabled; int key_size, value_size, max_entries, map_type; void* extra_buffer; u64 max_thread_count}, 40 B. It is defined twice: default_trampoline.cu:84-92 and nv_attach_impl.hpp:48-56. The table is `__constant__ map_info[1024]` (default_trampoline.cu:101), which is `.const .b8 map_info[40960]` (trampoline_ptx.h:18).
   Consequence: there is no spare field. A new field means editing both structs and regenerating the PTX. The table already takes 40 KiB of the 64 KiB constant bank [64 KiB limit: CUDA docs, inferred], so 24 more bytes per entry would fill the bank before the application's own __constant__ data.
2. Index. The map handle is the fd, compiled into the probe PTX as an immediate. The PTX path registers no map_by_fd, so lddw keeps imm (llvm-jit compiler.cpp:1114-1127; ptxpass core.cpp:234-253). Helpers read `map_info[map]` with no bounds check (default_trampoline.cu:236, 290, 371, 433). The segment allows 6144 fds (bpftime_config.hpp:23), but create_map_basic_info returns {} for fd >= 1024 (bpf_attach_ctx_cuda.cpp:535-540). The globals write then fails and the original image is loaded (nv_attach_impl.cpp:2351-2356, 2457-2463) [inferred chain].
   Consequence: map identity and the table shape are fixed at patch time.
3. Contents. Each CUDA link takes a snapshot of every map handler in the segment (bpf_attach_ctx.cpp:643-644; bpf_attach_ctx_cuda.cpp:505-554). nv_attach_impl keeps the last snapshot (nv_attach_impl.cpp:233).
   - extra_buffer is this process's device address of the storage: 1512 via cuMemHostGetDevicePointer, cached per pid (nv_gpu_array_host_map.cpp:17-57); 1527 the same way (nv_gpu_ringbuf_map.cpp:25-54); ARRAY via bpf_attach_ctx_cuda.cpp:480-503.
   - max_thread_count is the server's thread count at map creation (map_handler.cpp:1625-1660).
4. When written. map_info is written once per patched library, inside the cuLibraryLoadData hook (write_trampoline_globals, nv_attach_impl.cpp:2366-2385, called at :2457). cudart loads lazily at first use (CLAUDE.md:2090-2093). The late path writes it at module load (nv_attach_fatbin_record.cpp:283-292).
   - Nothing rewrites map_info after load. The only write after load is the `.global __bpftime_probes_enabled` at the last detach (nv_attach_impl.cpp:2391-2409).
   - A later session's probes are not patched into these modules (CLAUDE.md:2159-2161).
   Consequence: buffer pointers and sizes are fixed for the module's life. Resizing or swapping storage needs an indirection that the device reads at run time. Whether a __constant__ or .global rewrite reaches kernels already in flight is untested.
5. Shared by all launches. There is one table per module, one module per fatbin (CLAUDE.md:2103), with the same snapshot in every module. Every launch reads the same extra_buffer and max_thread_count: concurrent launches, other kernels, graph replays.
   - The slot is the linear global thread id (getGlobalThreadId, default_trampoline.cu:181-189), so concurrent launches share slots.
   - The ring takes a `busy` flag and drops on collision (default_trampoline.cu:454-459).
   Consequence: map_info cannot carry a per-launch value. Per-launch storage needs a launch key computed on the device (%gridid exists in PTX, but no helper exposes it) or host action before each launch, which graph replays do not allow (B).
6. getGlobalThreadId computes x, y, width and height in int (default_trampoline.cu:183-188). y*width can overflow above 2^31 [inferred]. llama.cpp (at most 4.2M threads per launch) is not affected.
7. Device storage that exists today: per-module `__device__` globals (default_trampoline.cu:122, 127, 193) and the `.global` probe switch (nv_attach_utils.cpp:23-24). Their size is fixed in the PTX text, the driver allocates them in device memory at load, and the host finds them with cuLibraryGetGlobal.
   Consequence: a fixed-size device buffer per module, sized at patch time from the device's SM and warp counts, needs no new allocation path. But it is per module (10 patched modules in llama.cpp, RD), and the CPU tool cannot see it without a copy (D5).

B. Launch hooks
8. Hooked (nv_attach_impl.cpp:488-534): cudaLaunchKernel, cudaLaunchKernel_ptsz, cuLaunchKernel, cuGraphAddKernelNode(_v2), cuGraph(Exec)KernelNodeSetParams(_v2), cuLibraryLoadData, symbol copies.
   Not in the list: cudaGraphLaunch/cuGraphLaunch, cudaGraphExecUpdate, stream begin/end capture, cuLaunchKernelEx/cudaLaunchKernelExC, cudaLaunchCooperativeKernel.
9. What the hooked launch paths see and do:
   - They receive grid, block, shared memory size and stream before the original runs (nv_attach_impl_frida_setup.cpp:94-113, 526-545).
   - In substitution mode they launch first and note afterwards (frida_setup.cpp:202-207, 546-554). No code acts before the kernel.
   - "Probed" means the function's module is a substituted library; any kernel in it counts (nv_attach_impl.cpp:1527-1550).
   - A stub is resolved with cudaGetFuncBySymbol (:1564-1591). CUDA 13 kernel handles fail there and are not identified (:1575-1585; CLAUDE.md:2240-2243). The Orin runs CUDA 13.2 (R/feasibility-20261109/gpu-env/attrs.out.txt:1).
10. Capture. A launch into a capturing stream goes straight through the runtime hook (frida_setup.cpp:111-113), and note() refuses capturing streams (probed_launch_activity.cpp:22-24). Replays reach no hook.
11. llama.cpp
   - Graphs: decode ran 53,848 of its 54,272 launches inside graphs; prefill ran none (R/llama-census-20261113/README.md:47-49). It captures in relaxed mode (L/ggml-cuda.cu:5284, 5475), updates instances with cudaGraphExecUpdate (:3337, :3341) and replays with cudaGraphLaunch (:3386, :5392). None of these is hooked.
   - cudaLaunchKernelEx is used only for kernels with PTX version >= 90 (L/common.cuh:1534-1540; L/ggml-cuda.cu:895-901), so on the Orin they launch with <<<>>> [inferred].
   - softmax uses cudaLaunchCooperativeKernel (L/softmax.cu:363), which is not hooked.
12. Can a design rely on knowing grid x block before every probed launch? No. With graphs on, replays and exec updates are invisible. Geometry is only available at capture time, and today it is discarded. The device always knows its own geometry: helpers 504/508 read blockDim/gridDim (default_trampoline.cu:521-561).
   Consequence: sizing must be decided on the device at run time (geometry or hardware position) or be independent of launches (a budget fixed at map creation or module load).

C. Helpers 509-511
13. 509 returns `mov %smid`, 510 `mov %warpid`, 511 `mov %laneid`. Nothing is computed (default_trampoline.cu:563-588; trampoline_ptx.h:2255, 2275, 2295). They are registered at core.cpp:251-253. No helper reads %nsmid, %nwarpid or %gridid.
14. PTX ISA:
   - %warpid "provides a unique warp number within a CTA but not across CTAs within a grid".
   - %warpid and %smid "may change during execution, e.g. due to rescheduling of threads following preemption"; both are meant for profiling.
   - SM ids are "not guaranteed to be contiguous, so %nsmid may be larger than the physical number of SMs".
   - %nwarpid is the "maximum number of warp identifiers".
   - %gridid is the "temporal grid launch number for this context", 64-bit from sm_30.
   Consequence: a table keyed by (smid, warpid) must be sized %nsmid x %nwarpid (read on the device). Two warps can share a key after preemption, so writes need atomics or a busy flag, never exclusive ownership. Uniqueness of the key across CTAs is not documented.
15. Orin attributes recorded 11-09: 8 SMs, 1536 threads/SM, 16 blocks/SM, computePreemptionSupported=1, concurrentKernels=1 (R/feasibility-20261109/gpu-env/attrs.out.txt:4, 14-23). 48 warps per SM and 384 warp slots are [inferred].
16. Observed. Six threadscheduling runs on 11-09, built from bpftime-gpu-work rather than 52126f33, all with `vec_add_legacy 10 5 10 1`, i.e. 1 block of 10 threads (R/fixes-20261109/protocol/runs/san1-ts-legacy/{meta.txt,server.log}; also costs/wu1, wu-late, robustness/liu1, liu-late, R/examples-20261109/runs/ts-legacy):
   - SM 0 only.
   - The same single warp read %warpid 0, 1, 2 and 3 across launches.
   - lane = threadIdx.x (0-9); the last launch read warp 1.
   - The counts are HASH counters with a race between the first lookup and the BPF_NOEXIST insert: warp rows sum to 24 against 51 for the SM [inferred].
   - Not observed: SMs 1-7, warp ids >= 4, %nsmid/%nwarpid, any id changing inside one kernel.

D. Memory
17. Where each type lives:
   - 1512 (1502 on the Orin): segment bytes laid out as [key][thread][value] (nv_gpu_array_host_map.hpp:22-24, .cpp:59-82).
   - 1513: in the segment (nv_gpu_shared_array_host_map.cpp:127).
   - 1527: in the segment on every GPU. Per thread, a 24 B header plus max_entries x (8 + value_size) (nv_gpu_ringbuf_map.cpp:11-23; gpu_helper_protocol.h:92-103).
   - 1502/1503 on a discrete GPU: the server allocates with cuMemAlloc and exports with cuIpcGetMemHandle (nv_gpu_per_thread_array_map.cpp:61-86; nv_gpu_shared_array_map.cpp:42-63); the agent opens with cuIpcOpenMemHandle (nv_gpu_per_thread_array_map.cpp:28-30).
   - The type swap happens when the server creates the map (syscall_context.cpp:570; bpftime_shm.hpp:142-154). cuIpcGetMemHandle returns 801 on the Orin (R/feasibility-20261109/gpu-env/attach-probe/ipc_test.txt:3).
18. Segment
   - Default 128 MiB (bpftime_config.hpp:81); BPFTIME_SHM_MEMORY_MB accepts 1-10240 (bpftime_config.cpp:119-120).
   - The agent prefaults the whole segment and registers it once with cuMemHostRegister at start (bpftime_shm_internal.cpp:1414-1469).
   - 1527 halves its thread count until it fits (map_handler.cpp:1394-1416); 1512 has no fallback (:1422-1434).
   - The Orin has 7.36 GiB in all, shared with the GPU (attrs.out.txt:4).
   Consequence: all storage the CPU can see comes from one fixed segment. Storage per hardware slot (8 or 384) fits; per-thread rings do not.
19. GPU-side cost
   - The Orin's GPU does not cache registered host memory (RD:214-219, from CUDA for Tegra, not re-read here).
   - Every 1512/1513/ARRAY lookup and update adds a membar.sys, and so does every ring record (default_trampoline.cu:255, 262, 271, 322, 333, 351, 479).
   - Atomics on registered memory work on the Orin (R/feasibility-20261109/gpu-env/hostreg.atom-dual.txt:6-8; hostreg.hsloop-atomdev.txt:17; hostNativeAtomicSupported=1 at attrs.out.txt:21):
     - GPU-only u32/u64 adds were exact.
     - CPU __atomic_fetch_add against GPU atomicAdd_system lost 0.
     - GPU-scope atomicAdd against the CPU lost 0 in one run.
     - A CAS lock in host memory worked.
   - The per-operation cost of host vs device memory on the Orin is not measured.
20. What the CPU tool sees: only the segment, through bpf() intercepted in its own process. For 1527 it has a bpftime-only symbol found with dlsym (example kernelretsnoop.c:86-99, polled every 10 ms).
   - The drain walks all thread_count headers on every call. It acquire-loads tail, release-stores head after each record, sums the per-thread `dropped` counters and logs them (nv_gpu_ringbuf_map.cpp:56-104).
   - One poller per map (CLAUDE.md:2299). Lookup, update and delete on 1527 return ENOTSUP (:110-137).
   - Epoll is wired only for ordinary RINGBUF fds (syscall_context.cpp:1150-1157).
   - Drops reach the tool only as a log line, and the probe only as return code 2/3. A thread outside 1512 prints one message per module and nothing is counted (default_trampoline.cu:195-216).
21. A device-memory buffer on the Orin would need:
   - allocation by the agent, in the application process (the watcher holds the primary context: bpf_attach_ctx_cuda.cpp:271-272; CLAUDE.md:2313-2316);
   - an agent-side copy into the segment, because the reader is another process and there is no IPC.
   Precedent: 1502 on a discrete GPU copies on every bpf() lookup (cuMemcpyDtoH or gdrcopy) into a staging buffer in the segment (nv_gpu_per_thread_array_map.cpp:89-121), in the server's own context (map_handler.cpp:1300-1311).
   Not tried: VMM=1 and POSIX-FD handles=1 (attrs.out.txt:49-50), though the reader would still need CUDA [inferred]. Managed memory has concurrentManagedAccess=0 and directManagedMemAccessFromHost=0 (attrs.out.txt:10, 22).

E. CPU-side view
22. bpf() lookup copies get_userspace_value_size() bytes (syscall_context.cpp:615-622). For 1502/1512 that is value_size x gpu_thread_count, thread-major, unpadded (map_handler.cpp:94-100).
   - No bpf() field tells the tool the thread count. threadhist passes a static 1M-element u64 buffer (threadhist.c:44-49).
   - libbpf's bpf_map__lookup_elem rejects this size: a non-per-CPU type needs value_sz == value_size (third_party/libbpf/src/libbpf.c:10982-10987).
   - The copy is a plain memcpy of a buffer the GPU may be writing (nv_gpu_array_host_map.cpp:84-94), so torn values are possible [inferred].
23. Per-CPU layout: bpftime's per-CPU maps show roundup(value, 8) x possible CPUs (map_handler.cpp:79-85; map_common_def.hpp:155-201). The Orin has 6 (/sys/devices/system/cpu/possible = 0-5).
   - libbpf requires exactly that size for PERCPU types (libbpf.c:10953-10978).
   - BPF_F_CPU/BPF_F_ALL_CPUS (libbpf.c:10961-10972) is not handled anywhere in runtime/.
   Consequence: storage per SM (8) or per warp slot (384) cannot pass as a per-CPU map to unmodified libbpf code. It needs one of: its own type with a slot count the tool knows (as 1512 does), slots summed into one value on read, or a key that names the slot.
24. Lookup returns a pointer, and GPU tools write through it non-atomically: threadhist does `*cnt += 1` (threadhist.bpf.c:21-29), relying on the slot being private to the thread.
   - eBPF atomics become relaxed `atomicrmw` (compiler_utils.cpp:376-388), assembled as a plain `atom.add.u64` (R/dir2-design-20261113/read/cuda-counter-patched-20261110.sm_75.ptx:1965; JIT linked against LLVM 15, build-jetson-daily/CMakeCache.txt).
   - Without a .scope, PTX uses .gpu.
   Consequence: storage shared by several threads makes these plain read-modify-writes race. It needs one sub-slot per lane, or the write must go through a helper.

F. Warp-level instructions
25. The trampoline source is compiled by clang for sm_61 (trampoline/Makefile:3, 30-31). The shipped header says `.version 8.1` / `.target sm_61` (Makefile:12, 18-22; trampoline_ptx.h:6-7). Today it uses activemask (inline asm), bar.warp.sync and %laneid (default_trampoline.cu:30-39, 111-116). There is no shfl, vote or match.
26. Module assembly
   - The trampoline is prepended to the patched PTX, and only the first .version/.target lines are kept, i.e. the trampoline's (nv_attach_impl.cpp:1116-1119; core.cpp:165-190).
   - compile_ptxs rewrites .target to the device (sm_87). The .version stays 8.1 below sm_100 (nv_attach_fatbin_record.cpp:134-135; nv_attach_utils.cpp:31-66).
   - nvPTXCompiler runs with --gpu-name=sm_87 -O3 (nv_attach_fatbin_record.cpp:156-160).
   - Kept PTX shows sm_61 because it is saved before the rewrite (R/dir2-design-20261113/read/cuda-counter-patched-20261110.sm_75.ptx:4-5).
27. PTX ISA requirements:

   | instruction | PTX ISA | target |
   |---|---|---|
   | shfl.sync, vote.sync | 6.0 | sm_30+ |
   | activemask | 6.2 | sm_30+ |
   | match.sync | 6.0 | sm_70+ |
   | redux.sync (integer) | 7.0 | sm_80+ |
   | nanosleep | 6.3 | sm_70+ |

   All are at or below 8.1, so they are legal in the 8.1 / sm_87 module.
   - clang at sm_61 will not emit sm_70+ builtins [inferred], so match and redux need inline asm or a higher PTX_TARGET_ARCH. A higher target drops pre-sm_70 devices, which host_call still handles by letting lanes take turns (default_trampoline.cu:129-133).
   - shfl.sync does assemble in a patched module: llama.cpp rms_norm uses __shfl_xor_sync (L/norm.cu:253; L/common.cuh:490-493), and its patched image loaded on the Orin (R/llama-census-20261113/runs/llama-th-pp512-b/client.log:49). [inferred: that module's PTX was not read]
28. Warp instructions work on registers, so memory placement matters only for atomics. eBPF has no warp instructions, so any aggregation lives in helpers. A new helper must also be registered at core.cpp:235-253; an unknown helper ends the application (llvm_jit_context.cpp:907, 938).
29. Where probes run
   - Entry probe: a bare `call` at the start of the kernel body, run by every thread (ptxpass_kprobe_entry/main.cpp:89-93).
   - Exit probe: before every ret/exit, predicated ones included (ptxpass_kretprobe/main.cpp:50-59).
   - __memcapture: before each load or store, in any control flow (ptxpass_kprobe_memcapture/main.cpp:147).
   - Helpers are __noinline__ and see only __activemask().
   Consequence: aggregation must work on whichever lanes happen to be active (match.any for keys) and never assume 32 lanes. A block-wide barrier is not safe in exit or __memcapture probes [inferred].

Unknowns that need an on-board check
- %nsmid and %nwarpid on the Orin; SM-id and warp-id ranges, and whether they are contiguous, under a full grid (at least 8 x 1536 threads).
- Whether (smid, warpid) is unique among resident warps across CTAs and concurrent streams. Whether it changes inside a kernel under time-slicing with another GPU process or graphics (compute preemption = 1).
- Per-operation cost on the Orin of atomics and membar.sys in registered host memory vs cuMemAlloc memory: a warp-level reservation against the per-thread ring.
- Whether a rewrite of a __constant__ (map_info) or a .global pointer after load is seen by kernels in flight and by graph replays. How an agent-side cuMemcpyDtoH(Async) is ordered against application streams and graphs.
- Whether %gridid is unique per launch, graph replays included, across the modules of one context.
- Constant-bank headroom in patched llama.cpp modules (map_info takes 40 KiB).
- Whether the grid/block of captured nodes can be read back after cudaGraphInstantiate/ExecUpdate, and whether llama.cpp replays change geometry.
- How many of llama.cpp's probed launches (CUDA 13.2 handles, static-cudart paths) reach the hooks at all.
- Whether a second process can map POSIX-FD-exported VMM memory and read it from the CPU on Tegra.
- Whether 1512's bpf() copy tears values in practice.