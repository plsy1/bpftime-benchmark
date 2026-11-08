export const meta = {
  name: 'gpu-orin-verify',
  description: 'Adversarially verify the CUDA-attach findings on the Orin (logs, code, upstream) and find gaps before the fix plan',
  phases: [
    { title: 'Verify', detail: 'five skeptic verifiers on finding groups + one fresh-eyes reader of the raw logs' },
    { title: 'Critic', detail: 'completeness critic over all verifier outputs' },
  ],
}

const REPO = '/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate'
const RES = '/home/jetson/src/results/gpu/examples-20261109'
const FEAS = '/home/jetson/src/results/gpu/feasibility-20261109'
const SCR = '/tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad'

const CONTEXT = `Context: an evaluation of bpftime's CUDA attach on a Jetson Orin Nano 8 GB (L4T R39.2.1, CUDA 13.2 at /usr/local/cuda-13.2, nvgpu integrated GPU sm_87, aarch64, glibc 2.39, GCC 13, LLVM 15).
Code: the dev candidate at ${REPO}. All CUDA attach code is byte-identical to official/master (attach/nv_attach_impl, runtime/src/attach/bpf_attach_ctx_cuda.cpp, runtime/src/bpf_map/gpu, cmake/cuda.cmake); \`git -C ${REPO} show official/master:<path>\` works, and \`git -C ${REPO} diff official/master..HEAD -- <path>\` shows the dev branch's own changes elsewhere.
Build used: ${REPO}/build-cuda (BPFTIME_ENABLE_CUDA_ATTACH=ON, LTO off, SPDLOG_ACTIVE_LEVEL=DEBUG, CUDA root = a symlink tree build-cuda/cuda-root with targets/aarch64-linux -> /usr/local/cuda-13.2/targets/sbsa-linux).
Run data: ${RES}: runs/<name>/meta.txt (exact loader and victim commands and env), runs/<name>/server.log (the example loader under libbpftime-syscall-server.so), runs/<name>/client.log (the CUDA victim under libbpftime-agent.so; DEBUG logging unless SPDLOG_LEVEL=info in meta), scripts/ (run.sh harness, build-examples.sh, meas.sh, gdb-server-exit.sh), probes/ (two measurement probe variants), ptx/ (debug-dumped patched PTX and ptxas sm_87 --verbose output), runs/m-C-strace/strace.txt (strace -ttt of one agent start), runs/gdb-server-exit/gdb.log, logs/. Victims: benchmark/gpu/workload/vec_add.cu (args: size iterations threads_per_block num_blocks; 1 warm-up launch + iterations launches; __constant__ int d_N set with cudaMemcpyToSymbol; prints "Validation check: C[0] = 0.00, C[1] = 3.00" when correct) built with nvcc 13.2 -cudart shared either default (handle launches) or with -D__NV_LEGACY_LAUNCH -Xcompiler -D__NV_LEGACY_LAUNCH ("legacy"); sources of the examples are under ${REPO}/example/gpu/<name>.
Earlier read-only survey (before any build): ${FEAS}/README.md, ${FEAS}/workflow-result.json (readers, synthesis, critic), ${FEAS}/gpu-env/ (small CUDA tests, gdb traces, upstream issue_*.txt / pr_*.txt dumps).`

const RULES = `Rules: read-only. You may run read-only commands (grep, sed, awk, readelf, nm, objdump, git show/log/diff, python3 -I scripts that only read, /usr/local/cuda-13.2/bin/ptxas or cuobjdump on files under ${RES} or ${FEAS}). Do NOT run any CUDA program, anything with LD_PRELOAD, any build or cmake, and do not modify anything under ${REPO} or ${RES}. Scratch files only under ${SCR}/verify-<your key>/. Put \`timeout\` on every command. If a claim can only be settled by a new experiment on the GPU, describe it precisely (commands, expected outcomes for each hypothesis) in experiment_needed instead of running it.
Method: try to REFUTE each claim; default to skepticism. Check that the data really shows it (quote log lines with run names), that the controls rule out other causes, trace the code path of the claimed root cause (cite file:line), check whether official/master has the same code and whether upstream already tracks it (issue/PR numbers; ${FEAS}/gpu-env/issue_*.txt and pr_*.txt and the upstream section of workflow-result.json), and sketch the smallest correct fix with its risk and a test that fails before and passes after (it must be mutation-checkable: reverting the fix makes it fail). Be concrete; no generic advice.`

const VERIFY_SCHEMA = {
  type: 'object',
  properties: {
    findings: { type: 'array', items: { type: 'object', properties: {
      id: { type: 'string' },
      verdict: { type: 'string', enum: ['confirmed', 'partly', 'refuted', 'uncertain'] },
      what_i_checked: { type: 'string' },
      corrections: { type: 'string' },
      root_cause: { type: 'string' },
      upstream: { type: 'string' },
      fix_sketch: { type: 'string' },
      fix_risk: { type: 'string' },
      test_idea: { type: 'string' },
      experiment_needed: { type: 'string' },
    }, required: ['id', 'verdict', 'what_i_checked', 'root_cause', 'fix_sketch'] } },
    other_observations: { type: 'array', items: { type: 'string' } },
  },
  required: ['findings'],
}

const GROUPS = [
  { key: 'launch', claims: `
F2. Victims built by nvcc 13.2 with default flags are never instrumented, silently. Run krs-default (kernelretsnoop + default-launch vec_add): 0 events in server.log, client.log has 4x "Late attach launch: resolved __end__ but no patched CUfunction is registered", Validation correct (the original kernel ran). Claimed cause: nvcc 13.2 host stubs call __cudaGetKernel then __cudaLaunchKernel, so cudaLaunchKernel/cuLaunchKernel receive a CUkernel handle (a heap address), not the host stub; bpftime's cudaLaunchKernel replacement resolves that address through host symbols (dladdr, then a sorted .symtab/.dynsym cache that returns a zero-size symbol such as __end__), and the cuLaunchKernel replacement tries only cuFuncGetName (returns 400 for a CUkernel; cuKernelGetName works). Code: attach/nv_attach_impl/nv_attach_impl_frida_setup.cpp (~165-245, ~453-527), nv_attach_impl.cpp (~1634-1675 resolve_host_function_symbol), nv_elf_introspect.cpp (~148-171). gdb evidence: ${FEAS}/gpu-env/attach-probe/gdb_launch_trace.txt. Upstream issue #646 reportedly shows the same symptom on GB300.
F6. Stripped victims are never instrumented, silently: krs-vec_add_legacy_stripped: 0 events, 4x "Late attach launch: unable to resolve host function <addr>", Validation correct.
F9. Even when nothing is redirected the agent adds per-launch cost: krs-default victim median 58.3 us per launch vs 35.7 us without the agent (krs-noagent), but these runs had DEBUG logging with one log line per launch. Decide whether the data supports any per-launch hook cost beyond logging, from code (what runs per launch on the miss path) and data; say what clean measurement would settle it.` },
  { key: 'wrongresults', claims: `
F4. A kprobe and a kretprobe on the same kernel make the victim compute wrong results: runs hmt-legacy (host_map_test) and cc-legacy (cuda-counter) print "Validation check: C[0] = 0.00, C[1] = 0.00" (expected 3), with "new hook added after bootstrap; rerunning late bootstrap" and two "late attach bootstrap ingested" lines; single-hook runs krs-legacy, krs-geom, ts-legacy print 3.00; no-agent runs krs-noagent and cc-noagent print 3.00. Claimed cause (also upstream issue #708 / open PR #709): the second bootstrap adds a second set of patched modules; launches use the last set (prefill overwrites patched_kernel_by_name), while the late fallback in mirror_cuda_memcpy_to_symbol (nv_attach_impl.cpp ~1272-1370) walks fatbin_records from the front and caches the first module that has the global, so cudaMemcpyToSymbol(d_N) reaches the wrong module and the launched kernel reads d_N = 0. Verify the mechanism exactly (including ordering: is cudaMemcpyToSymbol called before or after both bootstraps in this victim?), and whether #709's change is the right fix.
F5. On aarch64, a victim built with -Xcompiler -ffunction-sections,-fdata-sections computes wrong results even with ONE kretprobe: run krs-vec_add_legacy_fs: 20 events (probe fine) but C[1] = 0.00; the same binary without the agent prints 3.00 (verified separately); ${FEAS}/gpu-env/review/ties.py shows _ZL3d_N shares its address with the AArch64 mapping symbol $d (and the kernel stub with $x) in that binary but not in the normal legacy build. Claimed cause: resolve_host_function_symbol returns "$d" for the d_N host shadow, cuModuleGetGlobal("$d") fails in every module, and the mirroring returns silently. The launch itself resolved correctly here (luck of sort order?). Check the cache construction and tie-breaking in nv_elf_introspect.cpp and nv_attach_impl.cpp, and whether $x could also break launch resolution.` },
  { key: 'exitcrash', claims: `
F7. In every run with the CUDA build the loader (syscall-server process) dies with SIGSEGV at exit, also when the victim ran without the agent (krs-noagent, cc-noagent, m-A-noagent-*). runs/gdb-server-exit/gdb.log backtrace: __libc_start_call_main -> exit -> _dl_fini -> _dl_call_fini -> libnvrm_mem.so (destructor) -> close(9) -> syscall-server close() interposition (syscall_server_main.cpp:453) -> syscall_context::handle_close (syscall_context.cpp:280) -> bpftime_close -> bpftime_shm::close_fd -> handler_manager::clear_id_at (handler_manager.cpp:118) -> map_binding_mutex -> segment_manager lock -> SIGSEGV in pthread_mutex_lock. Claimed cause: the global bpftime_shm is destroyed by a C++ static destructor (runs in __run_exit_handlers, before _dl_fini runs ELF destructors of libraries such as L4T's libnvrm_mem.so, which libcuda loads when the server calls cuInit); close_fd then calls manager->clear_id_at on the destroyed object (the segment is unmapped). Claimed upstream too: official/master's handle_close/bpftime_close/close_fd do the same (the dev branch only added map_binding_mutex in clear_id_at). Check the destruction order exactly (where is global shm destruction registered: __cxa_atexit object, destructor attribute, or explicit call), whether official/master would crash at the same place, whether the agent side has the same hazard, and the right fix (e.g. a destroyed flag checked in bpftime_close / handle_close, or disabling mocking before destroying the shm).
F11. Each victim start logs "late attach bootstrap ingested 2 fatbin(s)" for an executable with one fatbin, with two "Patching PTX: temp.1.sm_75.ptx" and two "Compiling PTXs with sm_arch sm_87" but one "Compile of ... done" and one "Loaded module" (e.g. krs-legacy client.log). Is the same fatbin ingested twice (.nv_fatbin and .nvFatBinSegment both scanned?), and does that waste a compile, a module load, or memory? Also check whether the two-hook case (F4) multiplies this.` },
  { key: 'perf', claims: `
F8. GPU->host helper calls (printk, CPU-side maps) cost about 1 ms each and are served one at a time: victim per-launch medians (vec_add_legacy 10 5 10 1: 10 threads, 6 launches): no agent 35 us; kernelretsnoop (device-side only) 64 us; host_map_test 21.3 ms; threadscheduling 42.3 ms; cuda-counter 106.2 ms (runs cc-legacy, hmt-legacy, ts-legacy, krs-legacy, cc-noagent; grep "Median:" in client.log). Watcher: runtime/src/attach/bpf_attach_ctx_cuda.cpp (~111-279; 1 ms sleep ~275-276); one CommSharedMem request slot (runtime/include/bpf_attach_ctx.hpp ~43, 93-102). Count the host calls each example's BPF programs make per thread per launch (${REPO}/example/gpu/{cuda-counter,host_map_test,threadscheduling}/*.bpf.c and which helpers are host calls in the trampoline ${REPO}/attach/nv_attach_impl/trampoline/default_trampoline.cu / trampoline_ptx.h), derive the per-call cost, and verify the serialization claim (GPU side spin lock?).
F12. Per-launch time with 1M threads (vec_add_legacy 1048576 200 256 0; runs m-B-*; 3 rounds; victim "Median:"): no agent 162.2/162.3/162.9 us; empty kprobe 178.3/176.0/171.6; sampling kprobe (helpers 503+505, compare, thread 0 writes a GPU ringbuf) 586.0/583.6/585.6. ptx/*.ptxas-sm87.txt: the sampling probe's patched kernel has a 16,464-byte stack frame and 28 registers, the empty probe 0 bytes and 12 registers; the probe function itself declares __local_depot0[16464]. Claim: llvmbpf's PTX output allocates the whole eBPF stack (~16 KB) per thread once the program uses any stack; the +260% is due to the stack frame and/or the helper calls (not separated). Find where 16464 comes from (llvmbpf in ${REPO}/vm/llvm-jit, the GPU/PTX generation path), whether the probe touches only a few bytes of it, what the per-thread local memory reservation implies on this GPU (resident threads x stack), and design a measurement that separates stack-frame cost from helper-call cost.
F10. Victim start-up: 112-113 ms without the agent vs 1722-1742 ms with the agent and one empty kprobe (runs m-A-*, 5 rounds each, SPDLOG_LEVEL=info). strace (runs/m-C-strace/strace.txt; compute ms from the first line): CUDA init + host registration ~22->140 ms; pass libraries dlmopen ~140->193; host symbol cache ~236->1629 ms (opening libbpftime-agent.so at ~237 and the next file only at ~971; libnv_attach_impl_ptx_compiler.so ~1015->1629; client logs say "cached 556757 host symbols"); cuobjdump ~1630-1633; compile + load to ~1690. Claim: about 1.39 s of the 1.6 s extra is the host symbol cache scanning every loaded module, including the agent itself (static LLVM) and the PTX compiler library, which cannot contain CUDA kernels. Verify from code (who builds the cache, when, over which modules, what it is used for) and the strace timing, and estimate the start-up with the cache limited to modules that carry fatbins or built lazily.` },
  { key: 'buildipc', claims: `
F1. Build: cmake/cuda.cmake maps aarch64 to targets/aarch64-linux; CUDA 13.2 here has only targets/sbsa-linux; configure succeeds but compiles miss cuda.h and links miss -lcudart. Worked around with a symlink root; with it the whole CUDA build linked (libnvptxcompiler_static.a into a shared library included) and bpftime_nv_attach_tests passed 21/21 (logs/, build/ in RES).
F3. No CUDA IPC on this GPU: run th-ipc (threadhist): "Unable to open CUDA IPC handle for nv_gpu_array_map_impl: 801", "Unable to allocate map 1502", "Failed to load and verify BPF skeleton". Map types 1501/1502/1503 export device memory with cuIpcGetMemHandle. Check which code would need to change to back them with host-registered shared memory on integrated GPUs (like 1512/1513), or with cuMemCreate + POSIX FD export, and which examples that unblocks.
Then TRIAGE the "possible, not run" list from the survey and say for each: likely real on this board? matters for real users? cheap to test now (exact experiment)? Items: (a) a PTX compile error inside the agent ends in std::terminate (thrown inside a Boost thread pool: nv_attach_fatbin_record.cpp); (b) SM detection fallback to sm_61, rejected by CUDA 13 (nv_attach_utils.cpp); (c) host-call protocol: the host reads flag1 and the request without an acquire fence (bpf_attach_ctx_cuda.cpp ~121-126; upstream PR #503); (d) GPU ringbuf entries published with a plain dirty flag and no system fence between the data write and dirty=0 (trampoline _bpf_helper_ext_0025, default_trampoline.cu ~360-400) while the host reads them; (e) map lookups via host calls turn host pointers into device pointers without a range check (bpf_attach_ctx_cuda.cpp ~140-185); (f) cudaLaunchKernelEx/cuLaunchKernelEx, launches during stream capture and cudaFuncSetAttribute not handled; (g) more than one CUDA process on one shared memory segment; (h) LLVM 15 NVPTX lacks sm_87 (direct-run path only); (i) late attach to a running CUDA process (bpftime attach / trace --pidof) untested; (j) the late bootstrap patches and compiles the PTX of every loaded module (cuBLAS/cuDNN would be huge); (k) trampoline_ptx.h vs trampoline/default_trampoline.cu disagree on GPU_HELPER_MAX_BUF (1<<20 vs 1<<24). Report each as a finding with id P-a ... P-k.` },
]

phase('Verify')
const thunks = GROUPS.map(g => () => agent(
  `${CONTEXT}\n\n${RULES}\n\nYour key: ${g.key}. Verify these claims (each is one finding id):\n${g.claims}`,
  { label: `verify:${g.key}`, phase: 'Verify', schema: VERIFY_SCHEMA }))

const FRESH_SCHEMA = {
  type: 'object',
  properties: {
    runs: { type: 'array', items: { type: 'object', properties: {
      run: { type: 'string' }, config: { type: 'string' }, shows: { type: 'string' }, anomalies: { type: 'string' },
    }, required: ['run', 'config', 'shows'] } },
    overall: { type: 'array', items: { type: 'string' } },
  },
  required: ['runs', 'overall'],
}
thunks.push(() => agent(
  `${CONTEXT}\n\n${RULES}\n\nYour key: fresh. You get NO conclusions from the person who ran these experiments; form your own. For EVERY directory under ${RES}/runs (and runs/m-C-strace/strace.txt, runs/gdb-server-exit/gdb.log), read meta.txt and the logs and report: the configuration (loader, victim, env), what the run shows (event counts per thread, map values, the victim's Validation line and timing medians, which launch path the agent logged, exit codes), and anything anomalous or inconsistent (errors, warnings, crashes, counts that do not match the launch geometry, duplicated work, odd timings, log lines that contradict each other). Compare runs that differ in one variable. Read the example sources under ${REPO}/example/gpu and the harness ${RES}/scripts/run.sh to know what each run should produce. Finish with an overall list of the most important things these runs show about bpftime's CUDA attach on this board, most important first.`,
  { label: 'verify:fresh-eyes', phase: 'Verify', schema: FRESH_SCHEMA }))

const results = (await parallel(thunks))
const named = results.map((r, i) => ({ key: i < GROUPS.length ? GROUPS[i].key : 'fresh', result: r }))
log(`verifiers returned: ${named.filter(n => n.result).map(n => n.key).join(', ')} (${named.filter(n => n.result).length}/${named.length})`)

phase('Critic')
const CRITIC_SCHEMA = {
  type: 'object',
  properties: {
    gaps: { type: 'array', items: { type: 'object', properties: {
      gap: { type: 'string' }, why_it_matters: { type: 'string' }, how_to_close: { type: 'string' }, severity: { type: 'string', enum: ['high', 'medium', 'low'] },
    }, required: ['gap', 'why_it_matters', 'how_to_close', 'severity'] } },
    contradictions: { type: 'array', items: { type: 'string' } },
    priority_order: { type: 'array', items: { type: 'string' } },
  },
  required: ['gaps', 'contradictions', 'priority_order'],
}
const critic = await agent(
  `${CONTEXT}\n\n${RULES}\n\nYour key: critic (second attempt: the first attempt was cut off because its answer was too long, so BE TERSE: at most 8 gaps, at most 6 contradictions, at most 16 priority_order entries, every string at most 70 words, no restating of verifier text, cite file:line or run names instead of quoting). Below are the outputs of five skeptic verifiers and one fresh-eyes reader of the raw logs. Also judge two items that surfaced only in other_observations: (i) llvmbpf bpf-to-bpf calls move r10 by STACK_SIZE=64 (a slot count used as bytes; vm/llvm-jit/src/compiler.cpp ~552 and ~1460) in non-kernel-compatible mode, which bpftime uses on the CPU too: is the overlap real for CPU uprobe programs?; (ii) the 1527 GPU ringbuf: drain_data (runtime/src/bpf_map/gpu/nv_gpu_ringbuf_map.cpp:56-81) consumes at most one entry per thread slot per call and returns at the first dirty header, and the GPU drops when full (default_trampoline.cu:383-387): 13 of 201 events in m-B-sample. Act as a completeness critic before a fix plan is written: (1) gaps: which claims are still weakly supported, which important behaviours of CUDA attach on this board were never exercised (list them with the exact experiment that would close each, cheapest first), which verifier conclusions you doubt and why (check the code yourself where it matters); (2) contradictions between verifiers, or between a verifier and the data; (3) priority_order: the findings ordered by what a fix plan should address first, considering correctness (silent wrong results and silent non-instrumentation first), crashes, usability, then performance, and noting which fixes are small upstream-PR material.\n\nVerifier outputs (JSON):\n${JSON.stringify(named, null, 1)}`,
  { label: 'critic', phase: 'Critic', schema: CRITIC_SCHEMA })

return { verifiers: named, critic }
