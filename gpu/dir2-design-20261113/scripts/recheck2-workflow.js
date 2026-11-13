export const meta = {
  name: 'dir2-design-recheck2',
  description: 'Second independent recheck of the direction-2 design after fixes',
  phases: [{ title: 'Recheck', detail: 'two fresh readers: new mechanisms and whole-document consistency' }],
}
const DOC = '/home/jetson/src/results/gpu/dir2-design-20261113/DESIGN.md'
const CODE = '/home/jetson/src/bpftime-daily-state/worktrees/4708f4092d1e/candidate'
const COMMON = `You review a design document for bpftime (userspace eBPF runtime) on a Jetson Orin: ${DOC}. The code it cites is the dev worktree ${CODE} at commit 52126f33 (READ-ONLY: never modify, build, or run anything there; nothing on the GPU). llvmbpf is the submodule vm/llvm-jit. Read the whole document first, but do NOT read its two issue tables at the end ("Red-team issues and their status", "Independent recheck after the revision") until you have formed your own findings; then use them only to avoid re-reporting something the text already handles. Every path:line refers to that worktree. Read-only commands only; scratch files under /tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/recheck2/.
Report only real problems: a claim the code contradicts, a mechanism with a concrete failing interleaving (step by step), an internal contradiction, or a missing piece without which slice 1 cannot work or cannot be measured. For each give severity (blocking: the exactness claim or slice 1 fails as written; major: a section must change; minor: wording or citation), section, problem, evidence, concrete fix. Also list briefly what you checked and found to hold.`
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
Your lens: do the runtime mechanisms keep the reference semantics SR (R1) and its listed deviations D1-D7, and are they free of deadlock? Concentrate on:
1. R3.3 helper 600 as now written: the peeling loop over m0 with shfl.sync/vote.ballot.sync, the shfl gather sum, one leader per round with __syncwarp(m0), the B == 0 path, lane-turn fallback; nested host_call (default_trampoline.cu) inside the leader branch.
2. R3.4: the FLIPPED flag and hint writers, UNIT_ADD step 3 after a GPU-writer flip (device address returned, GPU atomic add), the sweep, the fold guard and watermark ver = 2c.
3. R3.6: the scan, flip and fold before loop 1 of init_attach_ctx_from_handlers at every entry (refresh_attach_session, the auto-refresh threads, agent start); late CPU and GPU pointer writers; D5 as restated.
4. R3.8: owner liveness via flock on a per-ledger file in /dev/shm with O_CLOEXEC; fork inheritance; exec; retire-on-death with a CAS; refresh rejoining by incarnation id; context-id change.
5. R3.2: the context rule applied by one helper to write_trampoline_globals, fatbin_record::try_loading_ptxs and run_attach_entry_on_gpu.
Construct concrete interleavings that lose, duplicate or misattribute an effect outside D1-D7, or deadlock.` },
  { key: 'consistency', prompt: `${COMMON}
Your lens: is the document consistent with itself and with the code, and is slice 1 buildable and measurable as written?
1. Re-open every path:line cited in "The model in one page", "What changes", R3.2, R3.3, R3.4, R3.6, R3.8, R7 and R10, and check the claim.
2. Internal consistency: slice-1 build list vs "Not in slice 1" vs "Slice 1 runs" vs the test definitions vs the mutation table (every mutation must name a test that can deterministically fail under it) vs the exit criteria; D1-D7 vs where each arises; the deviation list vs the per-idiom table; the exec_mode tallies against /home/jetson/src/results/gpu/dir2-design-20261113/exec-mode.txt; the expected-numbers table vs the measurement plumbing (can every number and every check column be produced?).
3. Leftovers that contradict the current text (staging words, T_flip, alive-set liveness, (pid, start time) liveness, a flip only after loop 1, static grouping, "no sweep in slice 1", etc.).
4. Whether a reader new to the project can follow "The model in one page" and the coined-names table: every coined term used in the body defined there or at first use.` },
]
const results = await parallel(lenses.map(l => () => agent(l.prompt, { label: `recheck2:${l.key}`, phase: 'Recheck', schema: SCHEMA })))
return lenses.map((l, i) => ({ key: l.key, ...(results[i] || { issues: [], checked_ok: [], summary: 'agent failed' }) }))
