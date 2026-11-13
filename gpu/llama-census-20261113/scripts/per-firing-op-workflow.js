export const meta = {
  name: 'per-firing-op-table',
  description: 'Derive, per GPU eBPF tool program, the map/helper operations executed per firing and their direction (D1/D2/none), twice independently, then reconcile',
  phases: [
    { title: 'Derive', detail: 'two independent readers over the tool sources' },
    { title: 'Reconcile', detail: 'adjudicate disagreements against the sources' },
  ],
}

const C = '/home/jetson/src/bpftime-daily-state/worktrees/4708f4092d1e/candidate'
const G = '/tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/dl/gpu_ext/repo/workloads/llama.cpp/observability_overhead/revision-rq4'
const FILES = [
  `${C}/example/gpu/kernelretsnoop/kernelretsnoop.bpf.c`,
  `${C}/example/gpu/kernel_trace/kernel_trace.bpf.c`,
  `${C}/example/gpu/threadhist/threadhist.bpf.c`,
  `${C}/example/gpu/llama-cpp-test/threadhist.bpf.c`,
  `${C}/example/gpu/threadhist-gpu-kernel-shared-map/threadhist.bpf.c`,
  `${C}/example/gpu/threadscheduling/threadscheduling.bpf.c`,
  `${C}/example/gpu/threadscheduling_dynamic_hook/threadscheduling.bpf.c`,
  `${C}/example/gpu/cuda-counter/cuda_probe.bpf.c`,
  `${C}/example/gpu/launchlate/launchlate.bpf.c`,
  `${C}/example/gpu/launchlate-kernel-gpu-shared-map/launchlate.bpf.c`,
  `${C}/example/gpu/mem_trace/mem_trace.bpf.c`,
  `${G}/results-table1-warp-plt-575-03/gpubpf_tool_build/kernelretsnoop/kernelretsnoop.bpf.c`,
  `${G}/results-table1-warp-plt-575-03/gpubpf_tool_build/threadhist/threadhist.bpf.c`,
  `${G}/results-table1-warp-plt-575-03/gpubpf_tool_build/launchlate/launchlate.bpf.c`,
  `${G}/results-launchlate-perf-plt-575-01/gpubpf_tool_build/launchlate/launchlate.bpf.c`,
  `${G}/full-record-device-buffer/full-record-device-buffer.bpf.c`,
  `${G}/onevalue-array-candidate/onevalue-array.bpf.c`,
]

const RULES = `
Direction rules for bpftime dev 52126f33 on the Orin (integrated GPU). Verify against the trampoline
${C}/attach/nv_attach_impl/trampoline/default_trampoline.cu and ${C}/runtime/src/attach/bpf_attach_ctx_cuda.cpp
and ${C}/runtime/src/handler/map_handler.cpp when in doubt, and say so in notes if a rule looks wrong:
- D1 (thread-indexed storage, sized at map creation by a guessed thread count): any operation on map type
  1502 (per-GPU-thread array; created as 1512 on the integrated GPU) or 1512, and bpf_perf_event_output /
  ringbuf output into map type 1527 (GPU ring buffer with one ring per thread).
- D2 (synchronous GPU-to-CPU host call): any lookup/update/delete on a CPU map (BPF_MAP_TYPE_HASH, LRU,
  PERCPU_*, and 1501 which is an ordinary HASH on the Orin), every delete except on an ordinary ARRAY,
  lookup/update/delete on 1527, bpf_printk/bpf_trace_printk (helper 6), ebpf_puts (helper 501).
- none (GPU-local): ordinary BPF_MAP_TYPE_ARRAY lookup/update (bound into GPU-visible memory since dev 1b3d9c99),
  map types 1503, 1504, 1513, helpers 502-511 (globaltimer, block/thread idx/dim, smid, warpid, laneid),
  bpf_get_current_pid_tgid (14, a constant since aa6eb973), atomic instructions on a pointer returned by a
  GPU-local lookup.
`

const SCHEMA = {
  type: 'object',
  properties: {
    tools: {
      type: 'array',
      items: {
        type: 'object',
        properties: {
          file: { type: 'string' },
          programs: {
            type: 'array',
            items: {
              type: 'object',
              properties: {
                section: { type: 'string', description: 'exact SEC() string' },
                attach_point: { type: 'string', enum: ['entry', 'exit', 'memcapture', 'cpu_uprobe', 'other'] },
                runs_on_gpu: { type: 'boolean' },
                executing_threads: { type: 'string', description: "which threads execute the body past any guard: 'all', 'thread0_of_grid', 'thread0_of_block', 'warp_leader', or 'other: ...'" },
                d1_per_firing: { type: 'number', description: 'D1 operations executed per firing by a thread that passes the guard, on the common path' },
                d2_per_firing: { type: 'number' },
                none_per_firing: { type: 'number', description: 'GPU-local map ops and GPU helper calls' },
                d2_per_firing_all_threads: { type: 'number', description: 'D2 ops executed by EVERY thread, including those that fail the guard (e.g. a lookup before the thread-0 check)' },
                d1_per_firing_all_threads: { type: 'number' },
                ops: {
                  type: 'array',
                  items: {
                    type: 'object',
                    properties: {
                      line: { type: 'integer' },
                      helper: { type: 'string' },
                      map: { type: 'string' },
                      map_type: { type: 'string' },
                      dir: { type: 'string', enum: ['D1', 'D2', 'none'] },
                      count: { type: 'number' },
                      who: { type: 'string', description: 'all threads / guarded threads only' },
                      condition: { type: 'string' },
                    },
                    required: ['line', 'helper', 'dir', 'count', 'who'],
                  },
                },
              },
              required: ['section', 'attach_point', 'runs_on_gpu', 'executing_threads', 'd1_per_firing', 'd2_per_firing', 'none_per_firing', 'd1_per_firing_all_threads', 'd2_per_firing_all_threads', 'ops'],
            },
          },
          output_volume: { type: 'string', description: 'records or bytes produced per launch as a function of threads' },
          notes: { type: 'string' },
        },
        required: ['file', 'programs', 'output_volume', 'notes'],
      },
    },
  },
  required: ['tools'],
}

function derivePrompt(angle) {
  return `You are deriving, for each GPU eBPF tool program below, exactly which map and helper operations one GPU thread executes per firing, and classify each by direction. ${angle}

Files (read each completely; also read the matching user-space loader .c next to it only to learn map types set at run time):
${FILES.map(f => '- ' + f).join('\n')}

${RULES}

Count path-sensitively. For a branch that depends on the thread (e.g. if (tid == 0)), record which threads take it.
For a lookup followed by an update only on miss, count the common steady-state path (after first touch) in d*_per_firing and
explain the first-touch path in condition. Count bpf_map_lookup_elem, bpf_map_update_elem, bpf_map_delete_elem,
output helpers, printk, and GPU helpers (5xx) each as one op. Atomic adds through a returned pointer are not helper calls:
mention them in condition, do not count them as ops. CPU uprobe sections (they run on the CPU) get runs_on_gpu=false.
Report every program (SEC) in every file. Cite line numbers. Do not guess: if a map type is set at run time, find where.`
}

phase('Derive')
const [a, b] = await parallel([
  () => agent(derivePrompt('Work from the eBPF sources directly, top to bottom.'), { label: 'derive-A', phase: 'Derive', schema: SCHEMA }),
  () => agent(derivePrompt('Work from each loader first (which maps exist and their types, which sections attach), then the eBPF source.'), { label: 'derive-B', phase: 'Derive', schema: SCHEMA }),
])

function key(file, sec) { return file.split('/').slice(-3).join('/') + '|' + sec }
function index(r) {
  const m = {}
  for (const t of (r && r.tools) || []) for (const p of t.programs) m[key(t.file, p.section)] = p
  return m
}
const ia = index(a), ib = index(b)
const keys = Array.from(new Set([...Object.keys(ia), ...Object.keys(ib)])).sort()
const FIELDS = ['executing_threads', 'd1_per_firing', 'd2_per_firing', 'none_per_firing', 'd1_per_firing_all_threads', 'd2_per_firing_all_threads', 'runs_on_gpu']
const diffs = []
for (const k of keys) {
  const pa = ia[k], pb = ib[k]
  if (!pa || !pb) { diffs.push({ key: k, missing_in: !pa ? 'A' : 'B', a: pa || null, b: pb || null }); continue }
  const d = FIELDS.filter(f => JSON.stringify(pa[f]) !== JSON.stringify(pb[f]))
  if (d.length) diffs.push({ key: k, fields: d, a: pa, b: pb })
}
log(`${keys.length} programs, ${diffs.length} disagreements`)

phase('Reconcile')
let verdicts = []
if (diffs.length) {
  const VSCHEMA = {
    type: 'object',
    properties: {
      verdicts: {
        type: 'array',
        items: {
          type: 'object',
          properties: {
            key: { type: 'string' },
            correct: { type: 'object', description: 'the corrected program record (same fields as the inputs)' },
            reason: { type: 'string' },
          },
          required: ['key', 'correct', 'reason'],
        },
      },
    },
    required: ['verdicts'],
  }
  const r = await agent(`Two independent readers disagree on these GPU eBPF program records. For each, read the cited source lines yourself and return the correct record with a reason. Key format: last three path parts|SEC. Files are under ${C}/example/gpu/ and ${G}/.

${RULES}

Disagreements (JSON):
${JSON.stringify(diffs, null, 1)}`, { label: 'reconcile', phase: 'Reconcile', schema: VSCHEMA })
  verdicts = (r && r.verdicts) || []
}
return { a, b, keys, diffs, verdicts }
