export const meta = {
  name: 'dir1-overlap-gpu-ext',
  description: 'How much gpu_ext and upstream bpftime overlap with direction 1 (GPU maps sized by thread index); read paper, repo, upstream branches, then verify',
  phases: [
    { title: 'Read', detail: 'paper, gpu_ext repo, upstream bpftime branches' },
    { title: 'Verify', detail: 're-open every cited source' },
  ],
}

const CONTEXT = `
Background (our research, "direction 1"): bpftime's GPU maps sized by thread INDEX. PERGPUTD_ARRAY (map types 1502 / 1512 host variant) and GPU_RINGBUF (1527) keep one copy per linear global thread id (getGlobalThreadId in attach/nv_attach_impl/trampoline/default_trampoline.cu). thread_count is fixed when the map is created by the syscall server (default 1024, env BPFTIME_MAP_GPU_THREAD_COUNT), before any launch and in another process, so it cannot know the grid. Threads past thread_count lose data (ringbuf silently returns; official per-thread array wrote past the map). Memory grows with the grid (gpu_ext's own notes: kernelretsnoop ring 892 MiB for 22 MiB of payload). The approaches we consider:
 (1) size_at_launch: allocate/grow per-thread storage per launch from grid x block (as Neutrino and Xtrace do);
 (2) shared_warp_buffer: one shared buffer (or one per SM) with reservation by a warp leader, aggregated per warp, memory independent of the grid;
 (3) hardware_slots: slots by hardware position (SM x warp slot x lane, or GT-Pin-like buckets);
 (4) cheap_fixes: validate thread count, warn at launch when grid exceeds a map's thread count, count every drop by reason.
Also relevant to our "direction 2" (GPU-to-CPU helper calls): a runtime that, by analysing each helper call site at load time, chooses to eliminate the host call, aggregate per warp, defer (buffer on GPU, apply later with exact final state) or keep a synchronous round trip; tag such findings dir2_automatic_warp.
The question: how much do gpu_ext (paper arXiv 2512.12615, repo eunomia-bpf/gpu_ext) and upstream bpftime (eunomia-bpf/bpftime) already cover these approaches, and in what form: a general runtime feature merged in master, a runtime feature only on an unmerged branch, hand-written inside specific tools, described only in the paper, or a measurement/doc note. Be precise; cite file paths with line numbers, commit hashes, or paper section numbers. Record dates (commit dates, paper versions). Report what you looked for and did not find in not_found.
Rules: read-only. Never check out, reset, stash, switch branches or edit files in any git worktree; use git fetch, git log, git show, git ls-tree, git diff only. Never clone gpu_ext fully (several GB). Write any scratch files only under /tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/dir1-overlap/.
`

const FINDINGS = {
  type: 'object',
  properties: {
    findings: { type: 'array', items: { type: 'object', properties: {
      claim: { type: 'string' },
      source: { type: 'string' },
      approach: { type: 'string', enum: ['size_at_launch', 'shared_warp_buffer', 'hardware_slots', 'cheap_fixes', 'dir2_automatic_warp', 'other'] },
      status: { type: 'string', enum: ['runtime_feature_merged', 'runtime_feature_unmerged_branch', 'tool_handwritten', 'paper_only', 'measurement', 'doc_note'] },
      date: { type: 'string' },
    }, required: ['claim', 'source', 'approach', 'status'] } },
    summary: { type: 'string' },
    not_found: { type: 'array', items: { type: 'string' } },
  },
  required: ['findings', 'summary', 'not_found'],
}

const VERIFIED = {
  type: 'object',
  properties: {
    checked: { type: 'array', items: { type: 'object', properties: {
      claim: { type: 'string' },
      verdict: { type: 'string', enum: ['confirmed', 'corrected', 'struck'] },
      correction: { type: 'string' },
      source: { type: 'string' },
    }, required: ['claim', 'verdict', 'source'] } },
    added: { type: 'array', items: { type: 'object', properties: {
      claim: { type: 'string' }, source: { type: 'string' }, approach: { type: 'string' }, status: { type: 'string' },
    }, required: ['claim', 'source'] } },
    summary: { type: 'string' },
  },
  required: ['checked', 'added', 'summary'],
}

const READERS = [
  { key: 'paper', prompt: `${CONTEXT}
Your source: the gpu_ext paper. Read the arXiv HTML https://arxiv.org/html/2512.12615v2 completely (it is about 111k characters: use WebFetch with offset to read all of it), and check https://arxiv.org/abs/2512.12615 for any newer version. Find everything about: per-thread maps (per-GPU-thread array), the GPU ringbuf, how map sizes / thread counts are chosen, what happens on overflow, warp-leader output, hierarchical map shards (shared memory / GPU global / host) and how their sizes are set, any per-launch sizing, any slot-by-hardware-position scheme, and any automatic (analysis-driven) choice of execution per call site. Say for each whether the paper presents it as implemented and evaluated, or only as design.` },
  { key: 'gpu_ext_repo', prompt: `${CONTEXT}
Your source: the gpu_ext repository. A blobless sparse clone is at /tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/survey-src/gpu_ext (HEAD 50a51ad, 2026-09-24). Run git fetch origin there first and list commits after 50a51ad (git log 50a51ad..origin/HEAD, with dates). Read with git show <rev>:<path> and git ls-tree (blobs are fetched on demand; fetch only files you need). Key places: workloads/llama.cpp/observability_overhead/revision-rq4/ (device-array-runtime-notes.md, results-*.md about per-warp device arrays, banked arenas, full-record device buffers, warp-contiguous SoA, ring transports), microbench/ (fig15-device/legacy-evidence-audit.md), docs/, plan.md, AGENTS.md/CLAUDE.md, ARTIFACT.md, and the device-side tools under workloads/llama.cpp (kernelretsnoop, threadhist, launchlate variants). For each buffer design, determine: is it a new map type or runtime change (in which repo/commit), or hand-written in a tool's BPF source; how its size is chosen (fixed constant, per warp count, per grid); what happens when full; and whether any general fix for thread-index sizing exists. Also note any plans or TODOs about sizing by grid, per-launch allocation, or automatic warp execution.` },
  { key: 'bpftime_upstream', prompt: `${CONTEXT}
Your source: upstream bpftime. A worktree with the remote "official" (https://github.com/eunomia-bpf/bpftime.git) is at /home/jetson/src/bpftime-daily-state/worktrees/4708f4092d1e/candidate. It is our dev worktree: DO NOT check out, reset, stash or edit anything there; read-only git only. Run git fetch official first. Then: (a) list official/master commits since 39cc1fce (now about 1e39125a) and say which touch GPU maps (1502/1512/1527/1503/1513), thread counts, ringbuf, warp-leader output, or host calls (make_helper_call, the CUDA watcher); (b) for every branch official/revision/* (git branch -r), give its commits since its merge-base with official/master (dates, subjects) and which of them touch these areas; read the diffs of relevant commits (git show) and describe what they implement precisely. Pay special attention to official/revision/automatic-warp-execution: what exactly does it do (does the runtime automatically run probes once per warp? how does it decide? does it analyse helper call sites, result usage or commutativity? which helpers/maps? exact or approximate results? what does it do for divergent lanes?), and whether it is merged or has a PR (check gh pr list --repo eunomia-bpf/bpftime --search with relevant terms if gh works; otherwise say so). Also check open/closed PRs and issues since 2026-09 mentioning thread_count, max_thread_count, per-thread, ringbuf, warp.` },
]

phase('Read')
const results = await pipeline(
  READERS,
  r => agent(r.prompt, { label: `read:${r.key}`, phase: 'Read', schema: FINDINGS }),
  (found, r) => found ? agent(`${CONTEXT}
You are checking another reader's findings about source "${r.key}". The reader's task was:
---
${r.prompt.slice(CONTEXT.length)}
---
Their findings (JSON):
${JSON.stringify(found, null, 1)}
Re-open the cited source of EVERY finding yourself (same rules: read-only, no checkout) and give a verdict: confirmed, corrected (say what is right) or struck (not supported). Be strict about status (merged runtime feature vs unmerged branch vs tool-handwritten vs paper-only) and about dates and numbers. Then look briefly for important items the reader missed (especially anything that sizes per-thread storage by grid or per launch, any automatic warp execution, and any work dated after 2026-11-11) and add them with sources.`, { label: `verify:${r.key}`, phase: 'Verify', schema: VERIFIED }).then(v => ({ key: r.key, found, verified: v })) : null,
)
return results.filter(Boolean)
