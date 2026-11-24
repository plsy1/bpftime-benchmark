#!/usr/bin/env bash
# E10 Jetson consuming lookup (plan 2.2, P1): ordinary and per-CPU array, --runs 6 --iterations 50000,
# arms kernel / O / D, alternating order per round (as 10-21). Loader and victim on cpu5 (the runner's
# own taskset), driver and this Claude session on cpu2.
# Prerequisite: prep/E10-build.sh (workloads/E10). Usage: E10-run.sh [session-id]
# Output: raw/J-E10-<date>/{ordinary,percpu}/ (raw.csv, summary.json, environment.json, per-run logs).
set -euo pipefail
SID=${1:-J-E10-20261124}
MIN=20                                  # guard window; expected ~5 min (10-21: about 1 min per kind)
KILL_NAMES="loader victim"
ARMS_USED="O D"
source "$HOME/src/paper-runs-20261124/prep/lib.sh"
W=$R/workloads/E10
start_session
check_workloads "$W"
for kind in ordinary percpu; do
    assert_clean
    step "$kind start"
    budget 900
    timeout --kill-after=30 "$B" taskset -c 2 python3 -u "$P/E10/run_consuming.py" --kind "$kind" \
        --output "$F/$kind" --runs 6 --iterations 50000 --engines kernel O D >"$LOG/$kind.log" 2>&1
    step "$kind end"
done
check_workloads "$W"
echo "E10 done"
