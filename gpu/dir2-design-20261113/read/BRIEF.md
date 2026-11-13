# Facts for the direction-2 design (from workflow wf_4e8c582d-f96, checked)

Full reader and checker output (JSON, cite from it): /tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/tasks/w63akje27.output
Dev worktree (read-only): /home/jetson/src/bpftime-daily-state/worktrees/4708f4092d1e/candidate (52126f33)

## Where a per-call-site decision can live
- The tool's BPF_PROG_LOAD (syscall server, loader process) stores cuda__ programs verbatim; no verifier, no AOT for CUDA.
- First place with instructions + map metadata (fd-indexed map_basic_info: type, key/value size) + attach point together: the agent, at CUDA link instantiation, nv_attach_impl::create_attach_with_ebpf_callback (nv_attach_impl.cpp:181-285). One link at a time; map_basic_info is last-link-wins; all GPU programs exist together only at hack_fatbin (module load).
- Carrier to codegen: nv_attach_entry (unused `parameters`/`extras` fields) -> RuntimeInput JSON (part of the patch-cache sha256 key) -> pass .so (dlmopen'ed in-process, LM_ID_NEWLM) -> compile_ebpf_to_ptx_from_words (core.cpp:226; fresh llvmbpf_vm, helpers 1,2,3,6,14,25,501-505,507-511 hard-coded) -> llvmbpf generate_ptx (one optimizeModule, no hook; LLVM 15 legacy PM in all local builds).
- An unregistered helper id or a map-value LDDW makes generate_ptx call exit() (static ExitOnError) inside the traced application: any new helper id must be registered there and defined in the trampoline.
- Ways to give a site its own behaviour: (a) rewrite/add helper ids in bytecode (register + define in trampoline); (b) IR pass via the llvmbpf patch overlay (precedent: array-map-inline.patch direct_helpers, but keyed per map handle; IR carries no eBPF pc today); (c) PTX text rewrite (fd constants shared through registers, no pc link); (d) carry a site id in a free helper argument (lookup r3-r5, update r5, delete r3-r5; printk/perf_event_output use all five).
- memcapture prepends instructions (pcs shift) and compiles the program once per captured access; probe function names carry no program identity (`__probe_func__<kernel>`), several probes chain on one module.
- Upstream PR #624 (open) runs PREVAIL + a forward uniformity analysis over raw eBPF in the agent at link instantiation (per-pc register/stack/pointee uniformity; 505/509/511 varying; lookup result inherits key uniformity; no constant lattice, no control dependence). Usable for "is this key warp-uniform"; dev has ENABLE_EBPF_VERIFIER off.

## Device side
- Host calls today: HASH and every other CPU map (1501 is an ordinary HASH on the Orin, 3823cac5), every delete except ordinary ARRAY, all 1527 lookup/update/delete, printk (6), puts (501), pid (14) until the module global is written; ordinary ARRAY falls back to a host call when its storage cannot be bound.
- Protocol: per-module device lock, then occupy_flag in the ONE CommSharedMem of the segment (shared by all agents on the segment), lanes take turns over a 32-step activemask loop; request byte-wise, membar.sys, flag1, spin on flag2; watcher CAS flag1 (acquire), stores flag2 (release).
- Map lookup on the device: __constant__ map_info[1024] indexed by bpftime map fd (40 B entries), snapshot at link creation, written into each module at load, never refreshed; no generation field; maps created later or fd >= 1024 are missing.
- Warp primitives in the trampoline: activemask, bar.warp.sync, %laneid/%warpid/%smid, unscoped atom.*, membar.sys/gl. No ballot/shfl/match/redux/nanosleep yet. Trampoline and probes compiled for sm_61, .target rewritten to sm_87 before nvPTXCompiler; LLVM-lowered sm_70+ intrinsics are the limit, PTX text is retargeted.
- GPU-visible memory on the Orin: the whole Boost segment (128 MiB default, BPFTIME_SHM_MEMORY_MB) is cuMemHostRegister'ed (DEVICEMAP|PORTABLE) by every agent; uncached by the GPU. The agent can allocate in the segment (Boost interprocess); such a buffer is device-reachable and readable by the loader without the agent (as the 1527 ring is).
- 1b3d9c99 / aa6eb973 decide at run time on the host (extra_buffer, module global), branch on the device; nothing at compile time.

## Host side
- serve_cuda_request uses the same entry points as the loader's bpf() (from_syscall=true). Lookup pointers are device aliases of the live hash element in the segment.
- Hash value pointers are NOT stable: ANY/EXIST update of an existing key links a new element and retires the old as the stripe's spare (overwritten by the next update in that stripe); delete frees to a free list (bpftime_hash_map.hpp:715-781, fix_size is the compiled hash). Array/per-CPU array pointers are stable. No keyed add-or-create op exists for HASH (with_existing_value exists, used only by per-CPU hash).
- Loader reads (lookup, get_next_key, lookup_and_delete, array mmap) run in the loader process directly on the segment; no channel lets the loader make the agent flush first (agent socket: refresh/detach/status only; SIGUSR1 = detach). Posting into CommSharedMem from the CPU is unsafe (occupy_flag is GPU-atomics only).
- 1527 is drained by the loader itself (bpftime_syscall_server__poll_gpu_ringbuf_map -> drain_data).
- CPU probes in the agent use the same map structures; the only ordering with GPU host calls is each map's own locking.
- Kernel completion is known only through the watcher's cuEventQuery over one event per stream (every 1 ms, 100 us with WFE). Hooked: cudaLaunchKernel(_ptsz), cuLaunchKernel, graph-node construction/params, cuLibraryLoadData, fatbin registration, cudaMalloc(Managed), cudaMemcpy{To,From}Symbol(Async). NOT hooked: any synchronize, plain memcpy, graph launch, exit. CUDA 13 kernel-handle launches are not noted.
- A loader close() frees a map; its fd can be reused; only ordinary ARRAY storage is pinned for patched modules.

## Semantics and the survey (59 bpftime host-call sites, gpu_ext 0)
- Linux: atomicity per helper call and per BPF_ATOMIC instruction, not per program; plain RMW through a lookup pointer is racy; lookup-miss then NOEXIST insert loses increments in the kernel too; an update of an existing hash key replaces the element (adds through an older pointer are lost); user-space reads copy one element without a snapshot across keys.
- 8 shapes: counter (NOEXIST insert, atomic add) 10; non-atomic RMW, EXIST self-write, else NOEXIST insert 9; leader-only lookup + update(ANY) 4; entry/exit timestamps 9; per-thread record 2; leader publish 1; printk 21; benchmark hash ops 3.
- result_use: unused 45, pointer_rmw 9, value_used 5; no site uses a helper's integer return. key_scope: constant 26 (21 pid-derived, 3 literal, 2 printk), per_block 14, none 11, per_thread 4, per_warp 4.
- Under today's serialized host call a warp's 32 lookups finish before any lane branches: first-touch lanes all miss and one NOEXIST insert lands; a plain RMW through a shared pointer collapses to one update per warp.
- Candidate reference semantics: S1 per-operation linearizability (Linux's guarantee; today's losses are legal); S2 serializability of probe executions (losses illegal, deferred adds exact); S3 per-key linearizability with idiom-level RMW; S4 equivalence only at observation points (kernel completion, stream sync, host read, periodic flush); S5 gpu_ext-style eventual consistency.
- Upstream auto-warp (official/revision/automatic-warp-execution): whole-program yes/no; rejects atomics, printk/puts/perf output/exit, per-thread map writes; update/delete only on 1501/1503/1504/1513 with uniform key and value; leader lane only. A plain `*val = *val + 1` with a uniform key is admitted and counts once per converged group (equal to today's lockstep collapse, wrong against a per-thread count).
- llvmbpf BPF_CMPXCHG sets R0 from a plain load taken before the cmpxchg (bug, read not run).

## Open (not established; designs must not assume)
- Atomicity of GPU atom.* on registered host memory against concurrent CPU atomics on the Orin (hostNativeAtomicSupported not queried); visibility of plain GPU stores through a host-call pointer to the loader without a fence pairing.
- Whether LLVM 15 O3 merges/sinks helper call sites (breaks pc -> call mapping).
- Whether match.any/redux/nanosleep as PTX text are accepted in the retargeted sm_87 module.
