export const meta = {
  name: 'gpu-callsite-survey',
  description: 'Classify every helper call site of GPU eBPF programs (bpftime examples, gpu_ext) by what it needs from the host',
  phases: [{ title: 'Classify' }, { title: 'Verify' }],
}
const BT = '/home/jetson/src/bpftime-daily-state/worktrees/4708f4092d1e/candidate'
const GX = '/tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/survey-src/gpu_ext'
const CTX = `Research question: in real GPU eBPF tools (eBPF programs that bpftime runs ON THE GPU, attached to CUDA kernels via SEC("kprobe/<mangled kernel>") / "kretprobe/..." / "cuda__/..." / "kprobe/__memcapture"), how many helper call sites truly need a synchronous round trip to the CPU ("host call"), and how many could be eliminated, deferred or aggregated without changing what the program observes?

Background (bpftime dev tree at ${BT}, commit 52126f33; read code there to confirm):
- On the GPU, a helper is either GPU-local (bpf_get_globaltimer 502, bpf_get_block_idx 503, bpf_get_block_dim 504, bpf_get_thread_idx 505 and similar 5xx index helpers; operations on GPU map types: per-thread arrays 1502/1512, GPU arrays 1503/1513, 1504, GPU ringbuf 1527) or a HOST CALL through one request slot in host memory: lookup/update/delete on ordinary CPU maps (BPF_MAP_TYPE_HASH, LRU_HASH, PERCPU_*), every op on BPF_MAP_TYPE_GPU_HASH_MAP 1501, bpf_trace_printk (6, bpf_printk), puts (501). Since dev 1b3d9c99, ordinary BPF_MAP_TYPE_ARRAY lookup/update go directly from the GPU (no host call); since aa6eb973 bpf_get_current_pid_tgid (14) is a per-module constant (no host call). Confirm in ${BT}/attach/nv_attach_impl/trampoline/default_trampoline.cu and the map type enum (runtime/include/bpftime_shm.hpp or similar); the direction notes table: git -C /home/jetson/src/results show origin/main:RESEARCH_DIRECTIONS.md (section 2, "What goes through the host").
- gpu_ext (${GX}, only sources checked out; it carries its own bpftime variant elsewhere) may use helpers or map types not in our tree: then read its sources/READMEs for what they do; if you cannot tell whether it needs the host, say host_call_today = "unknown".

For EVERY helper call site in the device-side programs of your files (count each textual call; ignore CPU-side uprobe/uretprobe/tracepoint sections in the same file but list them with class "cpu_side"), fill one record:
- file (path), line, section (SEC name, shortened), helper (name or id), map (name) and map_type (as declared),
- host_call_today: "yes" | "no" | "unknown" (in our dev 52126f33),
- result_use: "unused" | "null_check_only" (pointer only tested for NULL) | "pointer_rmw" (pointer used for an atomic or plain read-modify-write of the value) | "value_used" (value read for computation, branching, output or another map key) ,
- key_scope: "constant" | "per_kernel" | "per_block" | "per_warp" | "per_thread" | "data_dependent" | "none",
- operation: "read" | "overwrite" | "accumulate" (counter/sum/histogram: commutative) | "insert_if_absent" (BPF_NOEXIST) | "update_if_exists" (BPF_EXIST) | "delete" | "output" (printk/puts/ring output) | "id_or_time" | "other",
- cpu_reads_map: "yes" | "no" | "unknown": does the user-space loader/tool (the .c/.cpp/.py next to the program) read or write this map? Look at the loader sources; "no" means only GPU probes touch it,
- idiom: short name if the call is part of a pattern, e.g. "lookup+atomic_add+noexist_insert counter", "entry ts store / exit ts read",
- class: one of
  * "gpu_local_already": no host call even today,
  * "eliminate": host call today, but the data or answer can live on the GPU (map touched only by GPU probes; a constant answer; ARRAY-like data),
  * "defer": host call today, the probe does not need the result now (unused, or an accumulate idiom whose pointer is only used to add), so it can be buffered on the GPU and applied later with an exact final state; output-only calls (printk/puts) count here too (note ordering),
  * "sync": the probe needs the result now (value_used for computation/branch), or immediate cross-thread visibility matters (insert_if_absent/update_if_exists whose outcome is used, deletes another thread depends on), and the map is shared with the CPU so it cannot simply live on the GPU,
  * "cpu_side": not executed on the GPU,
- aggregatable: true if lanes of a warp would use the same key/target in this call (key constant/per_kernel/per_block; output with identical payload), so one request per warp would do,
- frequency: "per_thread_per_launch" | "per_launch" | "per_memory_access" | "per_thread_per_event" | "cpu_side" | "unknown",
- reason: one sentence why this class.
Also report per file: device_side (true/false), what the tool does (one line), and whether it is a copy/variant of another file in your list.
Read-only: write nothing outside /tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/, no builds, no CUDA runs, no network.`
const REC = { type: 'object', properties: {
  file: { type: 'string' }, line: { type: 'integer' }, section: { type: 'string' }, helper: { type: 'string' }, map: { type: 'string' }, map_type: { type: 'string' },
  host_call_today: { type: 'string', enum: ['yes', 'no', 'unknown'] },
  result_use: { type: 'string', enum: ['unused', 'null_check_only', 'pointer_rmw', 'value_used', 'n/a'] },
  key_scope: { type: 'string', enum: ['constant', 'per_kernel', 'per_block', 'per_warp', 'per_thread', 'data_dependent', 'none'] },
  operation: { type: 'string', enum: ['read', 'overwrite', 'accumulate', 'insert_if_absent', 'update_if_exists', 'delete', 'output', 'id_or_time', 'other'] },
  cpu_reads_map: { type: 'string', enum: ['yes', 'no', 'unknown', 'n/a'] },
  idiom: { type: 'string' },
  class: { type: 'string', enum: ['gpu_local_already', 'eliminate', 'defer', 'sync', 'cpu_side'] },
  aggregatable: { type: 'boolean' },
  frequency: { type: 'string', enum: ['per_thread_per_launch', 'per_launch', 'per_memory_access', 'per_thread_per_event', 'cpu_side', 'unknown'] },
  reason: { type: 'string' } },
  required: ['file', 'line', 'helper', 'host_call_today', 'result_use', 'key_scope', 'operation', 'cpu_reads_map', 'class', 'aggregatable', 'frequency', 'reason'] }
const OUT = { type: 'object', properties: {
  files: { type: 'array', items: { type: 'object', properties: { file: { type: 'string' }, device_side: { type: 'boolean' }, what: { type: 'string' }, variant_of: { type: 'string' } }, required: ['file', 'device_side', 'what'] } },
  records: { type: 'array', items: REC },
  changes: { type: 'array', items: { type: 'string' } } }, required: ['files', 'records'] }
const GROUPS = [
  { key: 'bpftime-A', files: ['benchmark/gpu/micro/cuda_probe.bpf.c', 'benchmark/gpu/micro/cuda_probe_kernel_gpu.bpf.c', 'example/gpu/cuda-counter/cuda_probe.bpf.c', 'example/gpu/cudagraph/cuda_probe.bpf.c', 'example/gpu/rocm-counter/rocm_probe.bpf.c', 'example/gpu/host_map_test/host_map_test.bpf.c', 'example/gpu/gpu_shared_map/gpu_shared_map.bpf.c'].map(f => `${BT}/${f}`) },
  { key: 'bpftime-B', files: ['example/gpu/atomizer/atomizer.bpf.c', 'example/gpu/threadhist/threadhist.bpf.c', 'example/gpu/faiss-test/threadhist.bpf.c', 'example/gpu/llama-cpp-test/threadhist.bpf.c', 'example/gpu/pytorch-test/threadhist.bpf.c', 'example/gpu/threadhist-gpu-kernel-shared-map/threadhist.bpf.c', 'example/gpu/cutlass/counter/cutlass_launch_counter.bpf.c', 'example/gpu/mem_trace/mem_trace.bpf.c', 'example/gpu/kernelretsnoop/kernelretsnoop.bpf.c', 'example/gpu/kernel_trace/kernel_trace.bpf.c', 'example/gpu/launchlate/launchlate.bpf.c', 'example/gpu/launchlate-kernel-gpu-shared-map/launchlate.bpf.c', 'example/gpu/threadscheduling/threadscheduling.bpf.c', 'example/gpu/threadscheduling_dynamic_hook/threadscheduling.bpf.c', 'example/gpu/directly_run_on_gpu/directly_run.bpf.c'].map(f => `${BT}/${f}`) },
  { key: 'gpu_ext-1', files: ['microbench/fig15-device/map_probe.bpf.c', 'microbench/fig15-device/strict-uniform-map/uniform_probe.bpf.c', 'microbench/fig15-device/strict-warp-map-scaling/warp_map_probe.bpf.c', 'microbench/trampoline-scaling/probe.bpf.c', 'workloads/bpftime-device-smoke/probe.bpf.c', 'workloads/cross-layer-raw-map/raw_map.bpf.c', 'workloads/sass-kretprobe/bpf/kretprobe_sass.bpf.c', 'workloads/xsched/bpftime_hpf.bpf.c', 'workloads/hummingbird/hostdev/hb_map.bpf.c'].map(f => `${GX}/${f}`) },
  { key: 'gpu_ext-2', files: ['device-verifier-s0/raw/s0-575-02/gpubpf_tool_build/kernelretsnoop/kernelretsnoop.bpf.c', 'device-verifier-s0/raw/s0-575-02/gpubpf_tool_build/threadhist/threadhist.bpf.c', 'full-record-device-buffer/full-record-device-buffer.bpf.c', 'onevalue-array-candidate/onevalue-array.bpf.c', 'raw/preflight-575-06/gpubpf_tool_build/kernelretsnoop/kernelretsnoop.bpf.c', 'raw/preflight-575-06/gpubpf_tool_build/launchlate/launchlate.bpf.c', 'raw/preflight-575-07/gpubpf_tool_build/launchlate/launchlate.bpf.c', 'results-launchlate-perf-plt-575-01/gpubpf_tool_build/launchlate/launchlate.bpf.c', 'results-table1-warp-plt-575-03/gpubpf_tool_build/kernelretsnoop/kernelretsnoop.bpf.c'].map(f => `${GX}/workloads/llama.cpp/observability_overhead/revision-rq4/${f}`) },
]
const results = await pipeline(GROUPS,
  g => agent(`${CTX}\n\nYour files (loaders and sources sit next to them):\n${g.files.join('\n')}`, { label: `classify:${g.key}`, phase: 'Classify', schema: OUT }),
  (r, g) => agent(`${CTX}\n\nAnother agent classified these files:\n${g.files.join('\n')}\n\nIts result (JSON):\n${JSON.stringify(r)}\n\nCheck it adversarially against the sources: every call site present (add missed ones), helper and map type right, host_call_today right for our dev tree, result_use and key_scope right, cpu_reads_map checked in the loader, class consistent with the definitions (be strict: "defer" only if the probe truly does not need the result; "eliminate" only if no CPU-side code touches the map), aggregatable right. Return the corrected full result (same schema) and list every change you made in "changes".`, { label: `verify:${g.key}`, phase: 'Verify', schema: OUT, effort: 'high' }).then(v => ({ key: g.key, first: r, verified: v })))
const tally = {}
for (const res of results.filter(Boolean)) {
  const src = res.key.startsWith('bpftime') ? 'bpftime' : 'gpu_ext'
  for (const x of (res.verified && res.verified.records) || []) {
    const k = `${src}|${x.class}`
    tally[k] = (tally[k] || 0) + 1
  }
}
return { tally, results }
