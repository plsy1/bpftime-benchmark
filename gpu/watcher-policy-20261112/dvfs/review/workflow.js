export const meta = {
  name: 'dvfs-claims-review',
  description: 'Check the clock-regime README claims against the raw data, then refute each finding',
  phases: [{ title: 'Review' }, { title: 'Verify' }],
}
const CTX = `Context: a measurement study of bpftime's CUDA host-call watcher policies on a Jetson Orin Nano, run under two clock regimes (on = jetson_clocks, off = DVFS and CPU idle states). The write-up under review is /home/jetson/src/results/gpu/watcher-policy-20261112/dvfs/README.md. Data: /home/jetson/src/results/gpu/watcher-policy-20261112/dvfs/{on,off}/summary.md and verify.csv, perround.txt (per-round values and residency medians, produced by scripts/perround.py), scripts/ (clocks.sh, rounds7.sh, resid.py, run_one.sh, summarize.py and their diffs). Raw runs: /home/jetson/src/bpftime-gpu-work/policy/logs/dvfs/{on,off}/r1..r4/<cell>_<set>.* (.env, .victim, .loader, .stats, .power = tegrastats at 100 ms, .resid, .load) plus clock snapshots clocks-*.txt and rounds.log. Earlier rounds for comparison: /home/jetson/src/results/gpu/watcher-policy-20261112/README.md, ../dev/README.md, ../v3/README.md. You may read files and run small python/grep/awk commands to recompute numbers (you can import /home/jetson/src/bpftime-gpu-work/policy/resid.py, or run scripts/perround.py from /home/jetson/src/bpftime-gpu-work/policy). STRICTLY read-only: do not modify or create files outside your own scratch use under /tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/, do not run sudo, clocks.sh, run_one.sh, rounds7.sh, tegrastats, CUDA programs or any benchmark (the board must stay quiet).`
const FIND = { type: 'object', properties: { findings: { type: 'array', items: { type: 'object', properties: {
  claim: { type: 'string', description: 'the README sentence or table cell, quoted' },
  problem: { type: 'string' }, evidence: { type: 'string', description: 'numbers/commands that show it' },
  severity: { type: 'string', enum: ['wrong', 'overclaim', 'missing-caveat', 'minor'] },
  fix: { type: 'string' } }, required: ['claim', 'problem', 'evidence', 'severity', 'fix'] } } }, required: ['findings'] }
const VERDICT = { type: 'object', properties: { holds: { type: 'boolean' }, reason: { type: 'string' }, corrected_fix: { type: 'string' } }, required: ['holds', 'reason'] }
const LENSES = [
  { key: 'numbers', prompt: `${CTX}\n\nLens: NUMBERS. Recompute every number, range and paired difference stated in README.md from the per-round data (perround.txt and, where needed, the raw files). Report each number that does not match (wrong range ends, wrong medians, wrong counts, a range attributed to the wrong set/cell/regime). Quote the exact README text. Only report mismatches you verified by computation.` },
  { key: 'claims', prompt: `${CTX}\n\nLens: OVERCLAIMS. For each conclusion in README.md (the bold section titles and every bullet), ask whether the data support it as worded: rankings stated where per-round ranges overlap, causes stated without a test, generalizations beyond 8 cells / 4 rounds / one board, comparisons across regimes of CPU seconds, power differences within the noise of 100 ms tegrastats sampling. Report only concrete wording problems with the evidence.` },
  { key: 'method', prompt: `${CTX}\n\nLens: METHOD AND CONFOUNDS. Check whether the off regime was really in place for every off run and the on regime for every on run (clocks= lines and checks in .env, the clock snapshots per round, residency), whether the regime switch-overs could leak (first cells after a switch: sleep 5 s after switching), whether summarize.py's validity check for off is weaker than for on and what that could hide, whether the power window and the residency window cover the same interval, whether EMC stuck at 204 MHz or tegrastats itself biases any comparison between sets, and whether 'load vs P' or 'cost per call' comparisons between sets inside the off regime are confounded by frequency in ways the README does not say. Report concrete problems with the README's text and evidence.` },
]
const results = await pipeline(LENSES,
  l => agent(l.prompt, { label: `review:${l.key}`, phase: 'Review', schema: FIND }),
  (r, l) => {
    const fs = (r && r.findings) || []
    const top = fs.filter(f => f.severity !== 'minor').slice(0, 3)
    const rest = fs.filter(f => !top.includes(f))
    if (rest.length) log(`${l.key}: ${rest.length} finding(s) not sent to verification (minor or beyond 3), returned unverified`)
    return parallel(top.map(f => () => agent(`${CTX}\n\nA reviewer claims this problem in README.md. Try to REFUTE it by recomputing from the data. Default to holds=false if the evidence does not reproduce.\n\nClaim quoted: ${f.claim}\nProblem: ${f.problem}\nEvidence given: ${f.evidence}\nProposed fix: ${f.fix}`,
      { label: `verify:${l.key}`, phase: 'Verify', schema: VERDICT }).then(v => ({ ...f, lens: l.key, verdict: v }))))
      .then(vs => ({ verified: vs.filter(Boolean), unverified: rest.map(f => ({ ...f, lens: l.key })) }))
  })
return results.filter(Boolean)
