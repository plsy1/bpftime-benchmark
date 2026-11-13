export const meta = {
  name: 'dir2-design-recheck',
  description: 'Independent recheck of the revised direction-2 design against the code',
  phases: [{ title: 'Recheck', detail: 'two fresh readers: mechanisms and consistency' }],
}
const DOC = '/home/jetson/src/results/gpu/dir2-design-20261113/DESIGN.md'
const CODE = '/home/jetson/src/bpftime-daily-state/worktrees/4708f4092d1e/candidate'
const COMMON = `You review a design document for bpftime (userspace eBPF runtime) on a Jetson Orin: ${DOC}. The code it cites is the dev worktree ${CODE} at commit 52126f33 (READ-ONLY: never modify, build, or run anything there; do not run anything on the GPU). llvmbpf is the submodule vm/llvm-jit. Read the whole document first. Every path:line in it refers to that worktree. You may run read-only commands (grep, sed, git show, git grep, objdump/llvm-objdump on existing objects, opt-15 on small IR files you write under /tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/recheck/).
Report only real problems: a claim the code contradicts, a mechanism with a concrete failing interleaving, an internal contradiction, or a missing piece without which the stated slice-1 cannot work or cannot be measured. For each give severity (blocking: the design's exactness claim or slice 1 fails as written; major: a section must change; minor: wording or a citation), the section, the problem, the evidence (file:line you read, or the interleaving step by step), and a concrete fix. Do not restate style preferences. Say what you checked and found to hold, briefly.`
const SCHEMA = {
  type: 'object',
  properties: {
    checked_ok: { type: 'array', items: { type: 'string' } },
    issues: { type: 'array', items: { type: 'object', properties: {
      severity: { type: 'string', enum: ['blocking', 'major', 'minor'] },
      section: { type: 'string' }, problem: { type: 'string' }, evidence: { type: 'string' }, fix: { type: 'string' } },
      required: ['severity', 'section', 'problem', 'evidence', 'fix'] } },
    summary: { type: 'string' },
  },
  required: ['checked_ok', 'issues', 'summary'],
}
phase('Recheck')
const lenses = [
  { key: 'mechanisms', prompt: `${COMMON}
Your lens: do the runtime mechanisms keep the stated reference semantics (SR in R1)? Focus on these parts, reading the code they rely on:
1. R3.3 helper 600: the run-time grouping check (shfl.sync/vote.ballot.sync peeling loop), the staging sum under the group's lock entry, lane-turn when the PTX is not accepted, and the claim that nomerge plus this check make grouping exact. Consider independent thread scheduling (sm_70+; Orin is sm_87, trampoline compiled for sm_61 and retargeted), divergence inside the loop, partial warps, return probes (ptxpass_kretprobe), and the existing host_call loop in attach/nv_attach_impl/trampoline/default_trampoline.cu.
2. R3.4 fold with the ver guard, the two-half seqlock read, the fast paths (CPU probes and loader), stragglers.
3. R3.6 late pointer writers: hint store to SYNC, fold, then attach; D5; refresh_attach_session in runtime/agent/agent.cpp.
4. R3.8 owner identity (pid, start time, incarnation), ownership ending at process exit, retire-on-death in slice 1.
5. R3.2 context rule (extra_buffer = 0 when a module's context differs) against nv_attach_impl.cpp write_trampoline_globals / with_cuda_context, and bpf_attach_ctx_cuda.cpp create_map_basic_info.
Try to construct concrete interleavings that lose, duplicate, or misattribute an effect, or that deadlock, under the hardware facts the document lists (H1-H6). If a mechanism holds, say so in checked_ok.` },
  { key: 'consistency', prompt: `${COMMON}
Your lens: is the document consistent and is slice 1 buildable and measurable as written?
1. Re-open every path:line cited in "The model in one page", R3.2, R3.3, R3.4, R3.6, R3.8, R7 and R10 and check that the code there says what the document claims (line drift of a few lines is minor; a wrong claim is major).
2. Internal consistency: the slice-1 build list vs "Not in slice 1" vs the tests slice 1 runs (T1-T6, T10, T11, T14, T17-T22) vs the mutation table vs the exit criteria; deviations D1-D7 vs where each is produced; the exec_mode tallies in "The model in one page" and R7 against ${'/home/jetson/src/results/gpu/dir2-design-20261113/exec-mode.txt'} and records-exec.json (rerun python3 -I /home/jetson/src/results/gpu/dir2-design-20261113/scripts/exec_mode.py, which only reads and writes in that results directory); the CUDA Counter request arithmetic (8 per thread, 3 removed, 5 left) against example/gpu/cuda-counter/cuda_probe.bpf.c; the 14-of-20 Orin sites.
3. Leftovers from before the revision that contradict the revised text (for example a section still relying on the flip kernel, T_flip, the alive-agent set, takeover seeding, or trusting popc(activemask), in slice 1).
4. Whether the measurement plumbing named in R10 is enough to produce every number in the expected-numbers table (benchmark/gpu/micro/cuda_probe.c, cuda_probe.bpf.c, examples_vec_add_config.json, example/gpu/threadscheduling).` },
]
const results = await parallel(lenses.map(l => () => agent(l.prompt, { label: `recheck:${l.key}`, phase: 'Recheck', schema: SCHEMA })))
return lenses.map((l, i) => ({ key: l.key, ...(results[i] || { issues: [], checked_ok: [], summary: 'agent failed' }) }))
