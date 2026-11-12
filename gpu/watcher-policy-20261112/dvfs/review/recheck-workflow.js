export const meta = {
  name: 'dvfs-readme-recheck',
  description: 'Recheck the rewritten clock-regime README against the data',
  phases: [{ title: 'Recheck' }, { title: 'Verify' }],
}
const CTX = `A measurement write-up was rewritten after a review: /home/jetson/src/results/gpu/watcher-policy-20261112/dvfs/README.md (bpftime CUDA host-call watcher policies on a Jetson Orin Nano under two clock regimes: on = jetson_clocks, off = DVFS + CPU idle states). The earlier review's findings are in review/findings.txt next to it. Data: dvfs/{on,off}/summary.md and verify.csv, perround.txt (per-round values; scripts/perround.py), inwin.txt (in-window CPU MHz from tegrastats; scripts/inwin.py), raw runs under /home/jetson/src/bpftime-gpu-work/policy/logs/dvfs/{on,off}/r1..r4/<cell>_<set>.* (.env .victim .loader .stats .power .resid), clock snapshots there (clocks-*.txt). You may run small python/grep commands (scripts run from /home/jetson/src/bpftime-gpu-work/policy, e.g. python3 -B <script>). STRICTLY read-only: write nothing outside /tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/, no sudo, no clocks.sh/run_one.sh/rounds7.sh/tegrastats/CUDA programs.`
const FIND = { type: 'object', properties: { findings: { type: 'array', items: { type: 'object', properties: {
  claim: { type: 'string' }, problem: { type: 'string' }, evidence: { type: 'string' },
  severity: { type: 'string', enum: ['wrong', 'overclaim', 'missing-caveat', 'minor'] }, fix: { type: 'string' } },
  required: ['claim', 'problem', 'evidence', 'severity', 'fix'] } } }, required: ['findings'] }
const VERDICT = { type: 'object', properties: { holds: { type: 'boolean' }, reason: { type: 'string' }, corrected_fix: { type: 'string' } }, required: ['holds', 'reason'] }
const LENSES = [
  { key: 'numbers', prompt: `${CTX}\n\nRecompute EVERY number, range, paired difference and count in README.md from the data. Report only mismatches you verified by computation, quoting the exact README text.` },
  { key: 'wording', prompt: `${CTX}\n\nCheck every statement for overclaims, untested causes stated as fact, contradictions between sections, and whether each confirmed finding of review/findings.txt (those marked HOLDS, and the unverified ones that are correct) is now handled correctly. Report only concrete problems with evidence.` },
]
const out = await pipeline(LENSES,
  l => agent(l.prompt, { label: `recheck:${l.key}`, phase: 'Recheck', schema: FIND }),
  (r, l) => {
    const fs = ((r && r.findings) || []).filter(f => f.severity !== 'minor')
    const minor = ((r && r.findings) || []).filter(f => f.severity === 'minor')
    if (fs.length > 3) log(`${l.key}: ${fs.length - 3} non-minor finding(s) beyond 3 returned unverified`)
    return parallel(fs.slice(0, 3).map(f => () => agent(`${CTX}\n\nTry to REFUTE this claimed problem in README.md by recomputing from the data; holds=false if it does not reproduce.\nClaim: ${f.claim}\nProblem: ${f.problem}\nEvidence: ${f.evidence}\nFix: ${f.fix}`, { label: `verify:${l.key}`, phase: 'Verify', schema: VERDICT }).then(v => ({ ...f, verdict: v }))))
      .then(vs => ({ lens: l.key, verified: vs.filter(Boolean), unverified: fs.slice(3), minor }))
  })
return out.filter(Boolean)
