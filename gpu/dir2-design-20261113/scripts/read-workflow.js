export const meta = {
  name: 'dir2-design-read',
  description: 'Read the subsystems a per-call-site GPU helper execution design depends on (compile path, trampoline, host side, semantics, analysis tools), then cross-check',
  phases: [
    { title: 'Read', detail: 'five readers, one per subsystem' },
    { title: 'Check', detail: 'cross-check the critical facts and gaps' },
  ],
}

const DEV = '/home/jetson/src/bpftime-daily-state/worktrees/4708f4092d1e/candidate'
const CONTEXT = `
Goal of the larger project ("direction 2" in RESEARCH_DIRECTIONS.md on main of plsy1/bpftime-benchmark): bpftime runs eBPF probes inside CUDA kernels. Some helper calls (lookup/update/delete on ordinary CPU maps and on BPF_MAP_TYPE_GPU_HASH_MAP 1501, bpf_trace_printk 6, puts 501, and before dev 1b3d9c99/aa6eb973 also ordinary ARRAY maps and get_pid_tgid 14) become a synchronous GPU-to-CPU host call: one request slot per process, lanes serialized. We will design a runtime that, at load time, analyses each helper call site and picks one execution: eliminate (no host call; data lives on the GPU), aggregate per warp (one request for a warp's lanes on the same target), defer (buffer on the GPU, apply on the host later with an exact final state), or keep the synchronous round trip. You are NOT designing; you are collecting precise facts the designers will need, with file:line or commit citations.
Code: our dev branch dev/uprobe-official-4708f4092d1e at 52126f33, worktree ${DEV}. Relevant areas: attach/nv_attach_impl/ (nv_attach_impl.cpp, pass/ptxpass_core/src/core.cpp, pass/ptxpass_*/main.cpp, trampoline/default_trampoline.cu, trampoline/gpu_helper_protocol.h, probed_launch_activity.*), runtime/src/attach/bpf_attach_ctx_cuda.cpp and bpf_attach_ctx.cpp, runtime/src/handler/, runtime/src/bpf_map/ (map implementations, incl. gpu maps), runtime/syscall-server/ (syscall_context.cpp: BPF_PROG_LOAD, BPF_MAP_* commands), vm/llvm-jit (llvmbpf submodule; PTX generation, generate_ptx), vm/compat/llvm-vm/*.patch (our patch overlay on llvmbpf), bpftime-verifier (PREVAIL submodule), CLAUDE.md (project invariants).
Inputs from earlier work (read them as needed): /home/jetson/src/results/gpu/callsite-survey-20261113/ (README.md, records.json: every helper call site of 22 bpftime GPU programs and 15 gpu_ext tools, classified; summary.txt), /home/jetson/src/results/gpu/overlap-survey-20261113/ (README.md, workflow-output.json: gpu_ext paper/repo and upstream bpftime branches, incl. official/revision/automatic-warp-execution eligibility rules and a suspected non-atomic += miscount), /home/jetson/src/results/gpu/watcher-policy-20261112/dev/README.md (current host-side waiting).
Upstream refs are available in ${DEV} as remote "official" (official/master, official/revision/*); gh works for eunomia-bpf/bpftime PRs.
Rules: read-only. In ${DEV} never check out, reset, stash, switch branches, edit files or build; only git log/show/grep/ls-tree/diff and reading files. No builds, no GPU runs, no benchmarks, no sudo. Scratch files only under /tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/dir2-read/. Cite every fact (file:line at 52126f33, or commit hash, or PR number). Say when something is inferred rather than read.
`

const READ = {
  type: 'object',
  properties: {
    answers: { type: 'array', items: { type: 'object', properties: {
      question: { type: 'string' }, answer: { type: 'string' }, sources: { type: 'array', items: { type: 'string' } }, inferred: { type: 'boolean' },
    }, required: ['question', 'answer', 'sources', 'inferred'] } },
    facts: { type: 'array', items: { type: 'object', properties: {
      statement: { type: 'string' }, source: { type: 'string' },
    }, required: ['statement', 'source'] } },
    open_questions: { type: 'array', items: { type: 'string' } },
    summary: { type: 'string' },
  },
  required: ['answers', 'facts', 'open_questions', 'summary'],
}

const READERS = [
  { key: 'compile-path', prompt: `${CONTEXT}
Subsystem: how a GPU probe's eBPF program becomes PTX, and where a per-call-site decision could be computed and carried into code generation.
Answer:
1. Trace the full path from the tool's BPF_PROG_LOAD (syscall server, other process) to the agent attaching a CUDA probe (hook_entries in nv_attach_impl, instructions, attach type) to compile_ebpf_to_ptx_from_words (ptxpass_core/src/core.cpp:226) to llvmbpf generate_ptx to the PTX passes (separate executables talking JSON) to wrap_ptx_with_trampoline and the fatbin rebuild. Which process and which step sees: the instruction words; the map fds / map ids and map types used by each LDDW; which other programs (CPU uprobes, other GPU probes) use the same map; the attach point; the target kernel's PTX. Where is map metadata resolved for the device (map_info symbol, constData)?
2. How could an individual call site get its own callee or inline code instead of the shared _bpf_helper_ext_NNNN? Compare concretely: (a) rewriting helper ids (or adding new helper ids) in the bytecode before compilation; (b) an LLVM IR pass or change in llvmbpf via our patch overlay (vm/compat/llvm-vm/*.patch, how overlays are applied and checked); (c) rewriting the PTX text (as ptxpass does). For each: where it would run, what it can see, what breaks (e.g. helper registration in compile_ebpf_to_ptx_from_words registers ids 1,2,3,6,14,25,501-511 only).
3. Is the AOT cache hand-off (commits f37c9502, a73218ad, 7ff8f073: server compiles once, agent links; prog handler fields like aot_insns) a usable precedent for computing an analysis result in the server at BPF_PROG_LOAD and passing it to the agent through the shared segment? Cite the fields and the publication protocol.
4. Does the CUDA path call the PREVAIL verifier or any analysis today? What does the compile step know about map value sizes, key sizes, and map types?` },
  { key: 'device-side', prompt: `${CONTEXT}
Subsystem: the device side (attach/nv_attach_impl/trampoline/default_trampoline.cu, gpu_helper_protocol.h, the GPU map implementations, and how they are compiled for sm_87 on the Orin).
Answer:
1. For each helper id handled on the device (1, 2, 3, 6, 14, 25, 501-511): what the device function does today for each map type (1501, 1502/1512, 1503/1513, 1504, 1527, ordinary ARRAY since 1b3d9c99, ordinary HASH/other CPU maps), whether it makes a host call, and the exact host-call protocol (locks, occupy_flag, request layout, membar, flags, response) with line numbers.
2. How the device resolves a map from the eBPF map fd/id (map_info, constData, map_id encoding like 1ull<<32), what per-map metadata the device has (type, key/value size, pointers), and where that metadata comes from.
3. What warp-level primitives are already used or available in the trampoline (activemask, ballot, shfl, match.any for grouping lanes by key, atomics on host-registered memory vs device memory, __nanosleep) and any constraint from the compile target (PTX version, sm_87, -arch, legacy launch). Is warp-uniform execution assumed anywhere?
4. What memory exists on the device side that a GPU buffer for deferred operations could use: GPU map storage locations (cuMemAlloc device memory vs host-registered shared segment; dev 3823cac5 says integrated GPUs create IPC map types in host memory), existing GPU ringbuf 1527 layout and its drain path, and any per-process GPU-side scratch already allocated by the agent.
5. How dev 1b3d9c99 (ordinary ARRAY direct access) and aa6eb973 (pid as a constant) decide at runtime/compile time that a call can avoid the host: the mechanism and where the decision lives; what semantics they gave up (stated in commit messages / CLAUDE.md).` },
  { key: 'host-side', prompt: `${CONTEXT}
Subsystem: the host side and the cross-process map view.
Answer:
1. serve_cuda_request in runtime/src/attach/bpf_attach_ctx_cuda.cpp: per request type, what it calls on the map handler, how a lookup's returned value pointer is translated for the GPU (gpu_address_of_map_value), and what memory that pointer refers to.
2. Pointer stability: in dev's hash map (runtime/src/bpf_map/ hash map implementation after commits 07aed862, b5e64843, 5567f2a2, ae324e26), does a value pointer returned by lookup stay valid and keep referring to the same key's value if another thread (CPU or GPU via host call) concurrently updates or deletes that key? Quote the code and the CLAUDE.md statements. Same question for array maps and per-CPU maps. This decides whether a GPU probe may keep a pointer and add through it later.
3. The host read path: when the user-space tool (loader, e.g. a libbpf program running with the syscall server preloaded) reads a map (BPF_MAP_LOOKUP_ELEM, BPF_MAP_GET_NEXT_KEY, lookup batch, mmap of arrays), which process executes it and what memory does it read? Confirm whether it reads the shared segment directly in the loader process, with no involvement of the agent in the target process. Is there any existing channel by which the loader process could ask the agent (target process) to do something before a read (e.g. flags in the shared segment, signals, the CUDA watcher, the GPU ringbuf poll path bpftime_poll_gpu_ringbuf_map)? How does the 1527 ringbuf get drained (which process, which code)?
4. CPU-side programs in the same tool: uprobes/kprobes running in the agent on the CPU that touch the same maps as GPU probes (the survey's cpu_side records). How do they access maps, and what ordering exists between a GPU probe's host call and a CPU probe's access?
5. When does the agent learn that a probed kernel finished (probed_launch_activity, events per stream, cudaDeviceSynchronize/stream sync hooks if any)? Which CUDA API calls of the application are hooked (launch, sync, memcpy, graph launch, exit) and could serve as points to flush deferred work before the application or the host tool observes results?` },
  { key: 'semantics', prompt: `${CONTEXT}
Subsystem: semantics, i.e. what "exact" can mean for deferred or aggregated helper calls.
Answer:
1. Linux eBPF semantics that bound what a program may observe, with citations to kernel docs or source (Documentation/bpf/, kernel/bpf/hashtab.c, arraymap.c, helpers.c; cite paths and versions or URLs): bpf_map_lookup_elem returning a pointer into the map; concurrent updates from other CPUs (non-atomic read-modify-write through the pointer is racy in the kernel too); BPF_ATOMIC add/fetch-add/xchg/cmpxchg; update flags BPF_ANY/BPF_NOEXIST/BPF_EXIST and their return codes; delete; per-CPU maps; what ordering user space may assume when it reads a map while programs run; bpf_trace_printk ordering. The point: define reference semantics against which "exact" is measured (not today's bpftime GPU behaviour, which itself loses counts).
2. From /home/jetson/src/results/gpu/callsite-survey-20261113/records.json: enumerate the idioms precisely as instruction-level shapes (e.g. counter idiom = lookup; if null then update(NOEXIST, init) else atomic add through pointer; per-thread record = update(BPF_ANY) with key from thread index; timestamp maps written at entry and read at exit by the same thread; leader-thread publish; printk). For each idiom: how many sites (bpftime vs gpu_ext), whether the lookup's result is used and how (null check, atomic add, plain load/store, passed to another helper), the key's scope (constant, warp-uniform, per-thread), and what the result would be if all lanes of a warp executed it in lockstep through the serialized host call (e.g. the NOEXIST race losing increments).
3. From /home/jetson/src/results/gpu/overlap-survey-20261113/workflow-output.json: the exact eligibility rules of upstream's official/revision/automatic-warp-execution, the claimed exactness, and the suspected non-atomic += miscount; and gpu_ext's stated consistency contract (relaxed, eventual, merge at kernel completion). Also read the auto-warp branch code (git show official/revision/automatic-warp-execution, ptxpass_core core.cpp and the eligibility code) to confirm the rules.
4. Write down candidate reference semantics for exactness (for example: the final map state after a probed kernel completes equals the state after some serial order of the probe executions; or per-key linearizability; or equivalence only at defined observation points such as kernel completion, stream sync, or a host bpf() read) and for each, which idioms it allows to defer or aggregate and what host-visible staleness it permits. Do not pick one; list them with consequences.` },
  { key: 'analysis-tools', prompt: `${CONTEXT}
Subsystem: what analysis machinery exists or can be reused to classify call sites at load time.
Answer:
1. bpftime-verifier (PREVAIL submodule): how bpftime invokes it (where, which process, for which program types; is it used for CUDA probes?), and what per-instruction information it can expose (register types, map fd for pointer-to-map-value registers, value ranges, liveness?). Cite the API used in bpftime.
2. Upstream's GPU SIMT verifier (open PR #624, and its use on official/revision/automatic-warp-execution): what analysis it performs (warp-uniformity propagation from thread-index helpers 503/505/510/511?), where it runs, its data structures and output; can it tell whether a helper's key argument is warp-uniform or lane-varying? Use gh pr view/diff 624 --repo eunomia-bpf/bpftime and git show on the branch.
3. llvmbpf (vm/llvm-jit): after eBPF is lifted to LLVM IR for PTX generation, can standard LLVM analyses (use-def chains, liveness of the call result, recognising an atomicrmw add on the pointer returned by a lookup call, constant keys) be applied? Where in llvmbpf's PTX path (generate_ptx, its pass pipeline) could an analysis or transformation pass be added; is there a hook today? How does llvmbpf lower helper calls and BPF_ATOMIC instructions for NVPTX?
4. For each of these needs, say which machinery fits best and why, with evidence: (a) is the result of a call used (null check only vs dereference vs passed on); (b) is the operation through a returned pointer a commutative atomic add; (c) is the key constant, warp-uniform, or lane-varying; (d) the multi-instruction counter idiom (lookup, null branch to update NOEXIST, atomic add) recognised as one unit; (e) which maps a program touches and with which helpers.` },
]

phase('Read')
const reads = await parallel(READERS.map(r => () =>
  agent(r.prompt, { label: `read:${r.key}`, phase: 'Read', schema: READ }).then(x => x ? { key: r.key, ...x } : null)))
const got = reads.filter(Boolean)
log(`readers returned: ${got.map(g => g.key).join(', ')}`)

phase('Check')
const check = await agent(`${CONTEXT}
You are the cross-checker for five readers' outputs (JSON below). Tasks:
1. Re-open the sources yourself and verify these critical facts, giving a verdict (confirmed / corrected / not supported) with citations:
 (a) where per-call-site decisions could be computed and how they would reach PTX generation (compile-path reader);
 (b) how a call site could get its own callee (bytecode helper-id rewrite vs llvmbpf IR pass vs PTX rewrite);
 (c) that the tool's host-side map reads run in the loader process directly on the shared segment, and whether any channel exists for the loader to make the agent flush first;
 (d) whether a hash-map lookup's value pointer stays valid across a concurrent update/delete of the same key in dev's hash map;
 (e) the auto-warp eligibility rules and the non-atomic += suspicion;
 (f) which CUDA application calls are hooked and could act as flush points.
2. Find contradictions between readers and resolve them from the sources.
3. List important gaps (facts a designer of eliminate/aggregate/defer/sync would need that nobody established) and fill the most important ones yourself, with citations.
Readers' outputs:
${JSON.stringify(got, null, 1)}`, { label: 'check', phase: 'Check', schema: {
  type: 'object',
  properties: {
    verdicts: { type: 'array', items: { type: 'object', properties: {
      item: { type: 'string' }, verdict: { type: 'string', enum: ['confirmed', 'corrected', 'not_supported'] }, detail: { type: 'string' }, sources: { type: 'array', items: { type: 'string' } },
    }, required: ['item', 'verdict', 'detail', 'sources'] } },
    contradictions: { type: 'array', items: { type: 'string' } },
    gaps_filled: { type: 'array', items: { type: 'object', properties: { fact: { type: 'string' }, source: { type: 'string' } }, required: ['fact', 'source'] } },
    gaps_open: { type: 'array', items: { type: 'string' } },
    summary: { type: 'string' },
  },
  required: ['verdicts', 'contradictions', 'gaps_filled', 'gaps_open', 'summary'],
} })
return { readers: got, check }
