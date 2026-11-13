export const meta = {
  name: 'dir2-design',
  description: 'Three independent designs for per-call-site execution of GPU eBPF helper calls, judged on two lenses, synthesized, then red-teamed',
  phases: [
    { title: 'Design', detail: 'three designers, different angles' },
    { title: 'Judge', detail: 'correctness/novelty and feasibility/measurability' },
    { title: 'Synthesize', detail: 'one design document' },
    { title: 'Red team', detail: 'try to break the synthesized design' },
  ],
}

const BRIEF = '/tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/dir2-read/BRIEF.md'
const READ_JSON = '/tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/tasks/w63akje27.output'
const DEV = '/home/jetson/src/bpftime-daily-state/worktrees/4708f4092d1e/candidate'
const SCRATCH = '/tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/dir2-design/'

const CONTEXT = `
Project: bpftime runs eBPF probes inside CUDA kernels (Jetson Orin Nano, integrated GPU sm_87, CUDA 13; later also a discrete GPU over PCIe). Some helper calls become a synchronous GPU-to-CPU host call through one request slot, lanes serialized (about 10 us per call on the Orin; total grows with threads x calls). We want a research-grade design (target: a top systems venue) for a runtime that analyses each helper call site at load time and executes it in the cheapest way that keeps eBPF semantics: eliminate (no host call; data on the GPU), aggregate per warp (one request for lanes with the same target), defer (buffer on the GPU, apply later with an exact final state), or synchronous round trip. Hardware must enter only as cost parameters (round-trip cost, memory placement), so the same model applies to integrated and discrete GPUs.
Read first: ${BRIEF} (checked facts about the code, with citations); the full reader/checker JSON is ${READ_JSON}. Earlier inputs: /home/jetson/src/results/gpu/callsite-survey-20261113/ (README.md, records.json with per-site fields file, line, section, helper, map, map_type, host_call_today, result_use, key_scope, operation, cpu_reads_map, aggregatable, frequency, idiom, class; tally bpftime: 109 gpu_local_already, 20 eliminate, 39 defer, 0 sync, 51 aggregatable), /home/jetson/src/results/gpu/overlap-survey-20261113/README.md (gpu_ext, accepted at ASPLOS '27: warp-leader execution and GPU shards merged at kernel completion with relaxed eventual consistency, tools hand-rewritten; upstream official/revision/automatic-warp-execution: whole-program, leader-only, opt-in, unmerged). Dev code (read-only): ${DEV} at 52126f33.
Hard requirements every design must satisfy explicitly (a section each):
R1 Reference semantics: state precisely what "exact" is measured against (NOT today's bpftime behaviour, which itself loses counts under the serialized host call), e.g. a serial order of probe executions, and at which observation points the host-visible map state is guaranteed current. Say what Linux eBPF guarantees and whether you guarantee more, the same, or less, per idiom.
R2 Difference from gpu_ext and upstream auto-warp: concretely, what makes your defer/aggregate exact rather than eventual, and per call site rather than per program.
R3 "Only GPU probes touch this map" is NOT knowable at load time (the loader process reads the shared segment directly, with no channel to the agent; CPU probes in the agent may touch the same map). Give a dynamic answer: e.g. a host-readable representation of pending GPU effects that the loader-side syscall server merges on read, or a detection and reconcile protocol. Account for the facts that loader reads run in another process directly on the segment, the request slot is unsafe for CPU posters, hash value pointers are not stable across update/delete, maps can be closed and fds reused, map_info in a module is a snapshot.
R4 Idioms as units: the counter idiom (lookup; null -> update(NOEXIST, init); else atomic add through the pointer) and the non-atomic RMW idiom (lookup; *v = *v + x; EXIST self-write; else NOEXIST insert) must be recognised as multi-instruction units at the bytecode or IR level, and you must say what each becomes and what result it gives against your reference semantics (including the first-touch race and the lost-update behaviour that Linux itself allows for non-atomic RMW). Cover all 8 shapes in BRIEF.md.
R5 Soundness direction: anything the analysis cannot prove falls back to the synchronous path; the sync path stays (and say whether its waiting rules change).
R6 Where deferred or aggregated data lives on the GPU side, how big, sized by what, what happens when full (direction 1's storage question; do not assume an unbounded buffer), and who drains it when.
R7 Analyzer: where it runs (process, step), its inputs, how its per-site decision reaches code generation (given that generate_ptx has no hook, IR has no eBPF pc, unregistered helper ids exit() the app, memcapture shifts pcs, O3 may merge calls), and its output defined as records in the call-site survey's schema (same field names plus a decision field) so that reproducing the survey's 20 / 39 / 0 and 51 is a mechanical check.
R8 Generality: what changes on a discrete GPU (device memory buffers, PCIe round trip), with cost parameters measured at start-up.
R9 Interactions: several agents on one segment, CPU probes on the same maps, graph launches the hooks do not see, map close / fd reuse, printk ordering, multiple probes on one kernel.
R10 First slice: the smallest end-to-end piece that is measurable on the Orin (what to build, which programs, expected numbers against today, how to measure, how to test exactness, mutation checks).
Rules: read-only; do not edit the dev worktree, do not build, no GPU runs. Scratch files only under ${SCRATCH}. Cite code (file:line at 52126f33) for every claim about the current code. Mark inferences. Write in English.
`

const DESIGN_SCHEMA = {
  type: 'object',
  properties: {
    title: { type: 'string' },
    markdown: { type: 'string', description: 'the full design document, sections R1-R10 plus an overview' },
    decisions: { type: 'array', items: { type: 'object', properties: { topic: { type: 'string' }, choice: { type: 'string' }, why: { type: 'string' } }, required: ['topic', 'choice', 'why'] } },
    risks: { type: 'array', items: { type: 'string' } },
  },
  required: ['title', 'markdown', 'decisions', 'risks'],
}

const ANGLES = [
  { key: 'analysis-first', angle: 'Angle: analysis-first with minimal runtime change. Put the analysis in the agent on eBPF bytecode (reusing or porting the uniformity analysis of upstream PR #624 where it fits), express decisions as bytecode rewrites to new, registered helper ids implemented in the trampoline, and keep the host side as unchanged as possible. Optimise for something buildable in weeks with strong soundness arguments.' },
  { key: 'semantics-first', angle: 'Angle: semantics-first. Start from a precise reference semantics and observation points, design the defer protocol (GPU-side buffers in the shared segment, reader-side merge in the loader process, flush at kernel completion detected by events, ordering against host updates/deletes and CPU probes) so that exactness can be argued like a proof sketch and tested with an oracle. Treat performance second, but keep it plausible.' },
  { key: 'performance-first', angle: 'Angle: performance and code generation first. Do the per-site specialization at the LLVM IR level in llvmbpf (patch overlay) or as PTX text: warp aggregation with match.any / redux / ballot / shfl, per-site inline fast paths, buffers in device memory on discrete GPUs and in the registered segment on the Orin, and a start-up cost model that picks between aggregate, defer and sync per site. Show where cycles go and what the speedups would be against today, with estimates grounded in measured numbers from the results tree.' },
]

phase('Design')
const designs = (await parallel(ANGLES.map(a => () =>
  agent(`${CONTEXT}\n${a.angle}\nProduce your design.`, { label: `design:${a.key}`, phase: 'Design', schema: DESIGN_SCHEMA }).then(d => d ? { key: a.key, ...d } : null)))).filter(Boolean)
log(`designs: ${designs.map(d => d.key).join(', ')}`)

const JUDGE_SCHEMA = {
  type: 'object',
  properties: {
    scores: { type: 'array', items: { type: 'object', properties: {
      design: { type: 'string' }, score: { type: 'number' }, strengths: { type: 'array', items: { type: 'string' } }, flaws: { type: 'array', items: { type: 'string' } },
    }, required: ['design', 'score', 'strengths', 'flaws'] } },
    winner: { type: 'string' },
    grafts: { type: 'array', items: { type: 'string' }, description: 'ideas from other designs the final design should take' },
    must_fix: { type: 'array', items: { type: 'string' } },
  },
  required: ['scores', 'winner', 'grafts', 'must_fix'],
}
const LENSES = [
  { key: 'correctness-novelty', lens: 'Lens: correctness and research value. Check each design against R1-R5 and R9 strictly: is "exact" well defined and actually achieved, including first-touch races, deletes, host writes during a kernel, map close/fd reuse, several agents, CPU probes; does unknown fall back to sync; is the difference from gpu_ext (eventual, per program, hand-written) and upstream auto-warp real and defensible to OSDI/ASPLOS reviewers? Verify claims about the code against the sources.' },
  { key: 'feasibility', lens: 'Lens: feasibility in this codebase and measurability. Check R6-R8 and R10 strictly against the code: can the analysis run where proposed with the information available, can decisions reach code generation (exit() on unregistered helpers, no pc in IR, O3 merging, memcapture, chained passes, sm_61 target), is the buffer sizing realistic in the 128 MiB segment, is the first slice small, testable with an oracle and mutation checks, and measurable on the Orin with numbers that would convince? Verify claims about the code against the sources.' },
]
phase('Judge')
const judged = (await parallel(LENSES.map(l => () =>
  agent(`${CONTEXT}\n${l.lens}\nScore the three designs below (0-10), name a winner, list ideas to graft from the others, and list what the final design must fix.\n\n${JSON.stringify(designs, null, 1)}`, { label: `judge:${l.key}`, phase: 'Judge', schema: JUDGE_SCHEMA }).then(j => j ? { lens: l.key, ...j } : null)))).filter(Boolean)

const FINAL_SCHEMA = {
  type: 'object',
  properties: {
    markdown: { type: 'string', description: 'final design document in English, sections: overview, R1..R10, open questions, what was taken from which draft' },
    first_slice: { type: 'string' },
    open_questions: { type: 'array', items: { type: 'string' } },
  },
  required: ['markdown', 'first_slice', 'open_questions'],
}
phase('Synthesize')
const final = await agent(`${CONTEXT}\nYou write the final design. Start from the judges' winner(s), graft the listed ideas, and fix every must_fix item. Where the judges disagree, decide and say why. Keep every claim about current code cited and verified against the sources. The document must stand alone for a reader who has not seen the drafts. Drafts and judgements:\n\nDESIGNS:\n${JSON.stringify(designs, null, 1)}\n\nJUDGES:\n${JSON.stringify(judged, null, 1)}`, { label: 'synthesize', phase: 'Synthesize', schema: FINAL_SCHEMA })

const RED_SCHEMA = {
  type: 'object',
  properties: {
    issues: { type: 'array', items: { type: 'object', properties: {
      severity: { type: 'string', enum: ['blocking', 'major', 'minor'] }, section: { type: 'string' }, issue: { type: 'string' }, evidence: { type: 'string' }, fix: { type: 'string' },
    }, required: ['severity', 'section', 'issue', 'evidence', 'fix'] } },
    summary: { type: 'string' },
  },
  required: ['issues', 'summary'],
}
phase('Red team')
const reds = final ? (await parallel([
  { key: 'semantics', focus: 'Try to break the design\'s exactness and soundness claims: construct concrete interleavings (GPU lanes, other warps, CPU probes, the loader reading or writing, deletes, map close and fd reuse, two agents, unseen graph launches, buffer full, process exit before a flush) where the host-visible state or the probe-visible results differ from the stated reference semantics, or where an unprovable site is not sent to the sync path.' },
  { key: 'feasibility', focus: 'Try to break the design\'s feasibility and measurement claims against the code: wrong citations, steps that cannot get the information they need, code-generation routes that fail (exit() on helpers, pc mapping, O3, memcapture, chained passes, sm targets), buffer sizes that do not fit, a first slice that is not small or not measurable, expected numbers not grounded in measurements.' },
].map(r => () => agent(`${CONTEXT}\nRed team (${r.key}). ${r.focus}\nVerify against the sources; report only issues you can support.\n\nFINAL DESIGN:\n${final.markdown}\n\nFIRST SLICE:\n${final.first_slice}`, { label: `red:${r.key}`, phase: 'Red team', schema: RED_SCHEMA }).then(x => x ? { key: r.key, ...x } : null)))).filter(Boolean) : []

return { designs: designs.map(d => ({ key: d.key, title: d.title, decisions: d.decisions, risks: d.risks })), judged, final, reds }
