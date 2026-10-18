#!/usr/bin/env bash
set -euo pipefail
ROOT=/home/jetson/src/bpftime-daily-state/worktrees/8da867d541f8/candidate
SNAPSHOTS=/tmp/bpftime-percpu-array-lookup-20261018.w2eqFb
RESULTS=/home/jetson/src/results/uprobe/percpu-array-lookup-followup-20261018
PHASE=inline-leaf
finish()
{
	status=$?
	printf '%s\n' "$status" >"$RESULTS/pipeline-exit-status.txt"
	date --iso-8601=seconds >"$RESULTS/pipeline-finished-at.txt"
	exit "$status"
}
trap finish EXIT
cd "$ROOT"
test "$(cat "$RESULTS/$PHASE/exit-status.txt")" = 0
sha256sum --check "$RESULTS/baseline/preserved-hash-source-sha256.txt"
python3 - "$RESULTS/$PHASE/ab5/summary.json" <<'PY'
import json
import sys
with open(sys.argv[1]) as stream:
    result = next(row for row in json.load(stream) if row['case'] == 'percpu_array_lookup')
wins = sum(c < b for b, c in zip(result['baseline']['raw_values'], result['candidate']['raw_values']))
if result['reduction_percent'] < 2 or wins < 4:
    raise SystemExit('Array A/B has insufficient benefit; full run was not started.')
print(f"Array screen passed: {result['reduction_percent']:.2f}% lower median; {wins}/5 paired wins.", flush=True)
PY
printf 'Running hash regression screen with the same frozen libraries.\n'
python3 -u benchmark/uprobe/diagnostics/compare_runtime_builds.py \
	--baseline "$SNAPSHOTS/baseline" --candidate "$SNAPSHOTS/$PHASE" \
	--output "$RESULTS/$PHASE/hash-ab5" --suite hash --runs 5 \
	--iterations 50000 --warmup 3000
python3 - "$RESULTS/$PHASE/hash-ab5/summary.json" <<'PY'
import json
import sys
with open(sys.argv[1]) as stream:
    results = json.load(stream)
failures = []
for row in results:
    ratio = row['candidate']['median'] / row['baseline']['median']
    print(f"{row['case']}: candidate/baseline={ratio:.4f}", flush=True)
    # Ordinary update has known bimodality and is reviewed in the full run.
    if row['case'] != 'hash_update' and ratio > 1.03:
        failures.append(row['case'])
if failures:
    raise SystemExit('Hash screen needs review before full testing: ' + ', '.join(failures))
print('Hash screen passed. Starting full ten-round validation.', flush=True)
PY
bash "$RESULTS/run-full.sh" "$PHASE"
