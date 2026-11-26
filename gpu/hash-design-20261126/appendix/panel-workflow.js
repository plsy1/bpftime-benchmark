export const meta = {
  name: 'gpu-hash-design',
  description: 'Design panel for hash maps in the GPU map model: four independent designs, three judges, one synthesis',
  phases: [
    { title: 'Design', detail: 'four designers, different starting points' },
    { title: 'Judge', detail: 'three lenses score every design' },
    { title: 'Synthesis', detail: 'recommended design and first checks' },
  ],
}

const CTX = `
Read-only design task: do NOT edit files, build, run anything on the GPU, or touch git refs/worktrees. Reading, grep and git show are fine.
Setting: bpftime runs eBPF programs as probes inside CUDA kernels on a Jetson Orin Nano (sm_87, integrated GPU, 8 SMs, no CUDA IPC). We built a "GPU map model" prototype with array-shaped map types: 1530 thread-local (a per-thread .local area, now passed by value), 1531 sum array (device memory from cuMemAlloc, (SM, lane) shards, atomic adds only, a delivery thread snapshots and sums the shards into the host map the CPU tool reads), 1532 device per-thread array. A routing pass rewrites the probes' PTX (inline lookups, global-space atomics). The user now wants HASH maps in this model. Today, every GPU hash operation is a host call.
Read these first (they are the evidence; quote them):
- /home/jetson/src/results/gpu/hash-current-20261126/README.md and survey.md (today's hash path and costs: 10-13 us per op back to back, about 18-21 us sparse, serialized; correctness of idioms; how examples use hash maps: counters keyed by a constant/pid/SM/warp/block, start time keyed by block or thread, per-launch sums; 7 of 22 GPU example programs use one; keys bounded).
- /home/jetson/src/results/gpu/dir1-design-space-20261115/README.md (the earlier design space: hard constraints such as map_info fixed per module in __constant__ memory, no host step before each launch, CUDA graphs, memory placement; launch sizes on llama.cpp).
- /home/jetson/src/results/gpu/map-model-proto-20261116/README.md, map-lowering-20261118/README.md, map-model-byvalue-20261126/README.md (how 1530/1531/1532 and their lowering work, delivery semantics, measured per-thread costs: lat on mul_mat_vec_q now 0.125 ns per thread, 17.2-17.5% on tg128; map part about 0.010 ns).
- /home/jetson/src/results/gpu/probe-cost-20261117/README.md and step0-checks-20261115, step0b-checks-20261116 (measured atomic costs in device vs registered host memory, warp-combine findings, what is documented vs not, e.g. activemask agreement not documented; CAS written by a tool is broken because the JIT drops the cmpxchg result).
- git -C /home/jetson/src/bpftime show origin/main:RESEARCH_DIRECTIONS.md (current conclusions; hash maps were "not in focus" because no GPU-native tool was known to need them).
- Code: /home/jetson/src/bpftime-proto-70f7 (attach/nv_attach_impl/nv_thread_local_routing.cpp, trampoline/default_trampoline.cu, runtime/src/bpf_map/gpu/nv_gpu_model_maps.cpp, nv_gpu_shared_hash_map.cpp (the x86-only 1501 device table), runtime/src/bpf_map/bpftime_hash_map.hpp (the CPU hash table)).
Rules the user holds us to: correctness only on documented guarantees (PTX ISA / CUDA programming guide; quote the section) or on measurement; do not change the target application or the tool logic to win (sampling etc. is not allowed); the map model goal (user, 11-15): low memory AND every thread can use its own copy where it needs one; name designs plainly (no branding). Distinguish measured facts from inference.
Shell is zsh: no word splitting of unquoted variables.`

const DESIGNERS = [
  { key: 'device-table', prompt: `Starting point: ONE hash table resident in device memory (cuMemAlloc), shared by all GPU threads of the process, with lock-free insert (CAS on the key slot), values updated in place with atomics, deletes, and a delivery thread that snapshots it into the host map the CPU tool reads (as 1531 does). Work out: layout (open addressing vs chaining, key sizes, bucket count vs max_entries), insert/lookup/delete protocols and their memory-model argument (PTX ISA memory consistency model: atom.cas, release/acquire, morally strong), what a lookup returns (a pointer valid until when?), BPF_NOEXIST/BPF_EXIST/BPF_ANY semantics, a full table (E2BIG, counted), deletes and reuse (tombstones, ABA), how the CPU sees inserts/deletes (delivery, get_next_key), and the per-op cost under contention (e.g. every thread of a launch hitting one key) using our measured atomic costs. Then: which of the surveyed idioms it serves exactly.` },
  { key: 'shards', prompt: `Starting point: SHARDED tables, the hash analogue of the 1531 sum array: each (SM, lane) or each SM has its own small hash table in device memory, threads insert and add only into their own shard (little or no contention), and the delivery merges shards by key into the host map. Work out: which operations this can support exactly (atomic adds to counters keyed by anything: yes; plain stores, lookups of values other threads wrote, deletes: what semantics?), shard capacity and memory (shards x capacity, overflow and its accounting), how a key inserted in several shards is merged, what the CPU tool sees and when, per-op cost (no CAS contention across SMs?), and the documented basis for smid/laneid-based sharding (what is guaranteed about smid during the life of a thread). Also consider per-warp or per-block shards. Then: which idioms it serves exactly and which it cannot.` },
  { key: 'route-first', prompt: `Starting point: RECOGNIZE THE KEY. Many GPU hash uses have keys that are a function the routing pass can see in the probe PTX (a constant; smid; the thread or block index from helpers 503/505; pid, a constant on the GPU). Design a load-time classification that maps each hash map (or each call site) to the cheapest storage that keeps the program semantics exactly: constant key -> a single cell; key = f(thread index) used as entry-to-exit scratch -> the thread-local area (1530, by value); key = f(block index) -> per-block storage; key with small known range -> an array or sum array; anything else -> a general hash table (left open or pick one). Work out: what must be proven about the key expression and the use (no other code reading the map, the CPU view the tool expects: e.g. a tool that iterates keys with get_next_key, or reads per-thread start times, must still see a hash map with those keys), fail-closed rules, and how the CPU-side view is built for each class. Quantify how many of the surveyed example uses each class covers.` },
  { key: 'host-cache', prompt: `Starting point: KEEP THE CPU HASH MAP AS THE OWNER (so CPU tools and get_next_key work unchanged and semantics stay the kernel htab semantics), and remove most host calls: a device-side cache of looked-up value pointers (registered host memory) per key, so repeated lookups of the same key by many threads do not go to the host; inserts and deletes batched or deferred through a device-side queue drained by the watcher; or warp-level aggregation of identical requests. Work out what is exact vs not (a lookup that misses the cache, an insert racing a delete, the BPF_NOEXIST first-touch race we measured), the documented basis (registered host memory is not cached by the GPU on the Orin; membar.sys; host-device coherence), and expected cost given our measurements (host call 10-21 us; atomics in registered memory about 80 ns; per-thread probe budget about 0.1 ns on mul_mat_vec_q). Then: which idioms it serves and at what rate (per thread / per block / per launch) it becomes usable.` },
]

const DESIGN = {
  type: 'object',
  properties: {
    name: { type: 'string', description: 'a plain descriptive name' },
    summary: { type: 'string' },
    layout: { type: 'string' },
    operations: { type: 'string', description: 'lookup/update/delete/flags/atomic add: exact semantics and what differs from the kernel htab' },
    concurrency_basis: { type: 'string', description: 'memory-model argument with PTX ISA / CUDA guide sections quoted; mark inference' },
    cpu_view: { type: 'string', description: 'what the CPU tool sees and when (bpf() lookups, get_next_key, delivery)' },
    capacity_and_loss: { type: 'string' },
    cost: { type: 'string', description: 'expected per-op GPU cost per rate (thread/block/launch) with the measured numbers it rests on; mark inference' },
    idioms_served: { type: 'string', description: 'which surveyed uses it serves exactly, which approximately, which not' },
    implementation: { type: 'string', description: 'which files change (routing pass, trampoline, map handler, agent delivery, syscall server), new map type numbers' },
    risks: { type: 'string' },
    checks_first: { type: 'array', items: { type: 'string' }, description: 'measurements or hand-written forms to run before building, each with its decision rule' },
  },
  required: ['name', 'summary', 'layout', 'operations', 'concurrency_basis', 'cpu_view', 'capacity_and_loss', 'cost', 'idioms_served', 'implementation', 'risks', 'checks_first'],
}

phase('Design')
const designs = (await parallel(DESIGNERS.map(d => () =>
  agent(CTX + `\n\nYou are one of four independent designers.\n` + d.prompt, { label: 'design:' + d.key, phase: 'Design', schema: DESIGN })
    .then(x => x && ({ key: d.key, ...x }))))).filter(Boolean)
log('designs: ' + designs.length)

const JUDGES = [
  { key: 'correctness', prompt: `Lens: correctness. For each design: are its concurrency and semantic claims backed by documented guarantees (check the PTX ISA / CUDA guide sections it cites; say which claims are inference), does it keep eBPF hash-map semantics a CPU-style tool relies on (or say exactly what changes), are losses counted, are there races (first touch, delete/reuse, pointer lifetime, delivery vs adds), and does it fail closed when its assumptions do not hold.` },
  { key: 'cost', prompt: `Lens: cost at llama.cpp scale. Using our measurements (mul_mat_vec_q tg128: 3.14e9 threads per run, a per-thread probe now 0.125 ns, base 1.88 s per run; host call 10-21 us; atomics in device vs registered memory from step0; contention on one address), estimate each design per rate (every thread, one per block, one per launch) for the surveyed idioms, and say which numbers are measured and which inferred. Flag designs whose common case still makes a host call per thread or a contended global atomic per thread on one key.` },
  { key: 'buildability', prompt: `Lens: fit and buildability. Against the code (routing pass, trampoline, map_info in __constant__ memory fixed per module, delivery thread in nv_gpu_model_maps.cpp, syscall server map creation, the CPU tool reading via bpf() in another process without CUDA IPC): how much each design changes, what it reuses from 1530/1531/1532, what it cannot do under the hard constraints (no host step before launches, CUDA graphs, map_info fixed), and whether it serves the map model goal (low memory, every thread its own copy where needed).` },
]

const JUDGEMENT = {
  type: 'object',
  properties: {
    scores: { type: 'array', items: { type: 'object', properties: { design: { type: 'string' }, score: { type: 'integer', description: '1 (poor) to 10 (strong) on this lens' }, reasons: { type: 'string' }, must_fix: { type: 'array', items: { type: 'string' } } }, required: ['design', 'score', 'reasons', 'must_fix'] } },
    best_combination: { type: 'string', description: 'which parts of which designs to combine, on this lens' },
  },
  required: ['scores', 'best_combination'],
}

phase('Judge')
const designText = JSON.stringify(designs, null, 1)
const judgements = (await parallel(JUDGES.map(j => () =>
  agent(CTX + `\n\nYou judge four designs for GPU hash maps on ONE lens. Verify their claims against the evidence and code yourself; do not trust them.\n` + j.prompt + `\n\nThe designs:\n` + designText, { label: 'judge:' + j.key, phase: 'Judge', schema: JUDGEMENT })
    .then(x => x && ({ lens: j.key, ...x }))))).filter(Boolean)

phase('Synthesis')
const SYN = {
  type: 'object',
  properties: {
    recommendation: { type: 'string', description: 'the recommended design in markdown: what it is, per idiom/rate what storage and path, semantics and their documented basis, CPU view, capacity and loss accounting, expected costs (measured vs inferred), what is out of scope, and how it relates to 1530/1531/1532' },
    rejected: { type: 'string', description: 'what was rejected and why, briefly' },
    checks: { type: 'array', items: { type: 'object', properties: { check: { type: 'string' }, why: { type: 'string' }, decision_rule: { type: 'string' } }, required: ['check', 'why', 'decision_rule'] } },
    open_questions: { type: 'array', items: { type: 'string' } },
  },
  required: ['recommendation', 'rejected', 'checks', 'open_questions'],
}
const syn = await agent(CTX + `\n\nSynthesize ONE recommended hash-map design for the GPU map model from four designs and three judgements. Prefer what is exact on documented guarantees and cheap at llama.cpp scale; keep only what the judges did not refute; graft good parts across designs. Then list the checks (measurements, hand-written PTX forms) to run before building anything, each with a decision rule written now, ordered by what decides the most. Be explicit about what is inferred.\n\nDesigns:\n` + designText + `\n\nJudgements:\n` + JSON.stringify(judgements, null, 1), { label: 'synthesis', phase: 'Synthesis', schema: SYN })
return { designs, judgements, synthesis: syn }
