#!/usr/bin/env bash
# cpuset control (NUC request 2026-11-24): pinbench on the nginx core (cpu4) inside the measurement container
# (same image and flags as session.sh: --privileged --pid=host) vs on the host, alternating ABAB x 3.
# Masks: single=4, pair=4,5 (policy M's nginx mask), all=0-5. 10 runs x 200000 iterations each invocation.
# Question: is the in-nginx pin cost (~12,500 cycles per sequence, E7) a container cgroup (cpuset) effect?
set -euo pipefail
SID=${1:?session id}
R=$HOME/src/paper-runs-20261124; H=$R/harness; OUT=$R/raw/$SID; LOG=$OUT/log
IMAGE=bpftime-ssl-nginx:20261023; NAME=paper-jetson
mkdir "$OUT" "$LOG"
CLAUDE_PIDS=()
finish()
{
    status=$?
    trap - EXIT
    sudo -n docker rm -f "$NAME" >>"$LOG/cleanup.log" 2>&1 || true
    for p in "${CLAUDE_PIDS[@]}"; do taskset -apc 0-5 "$p" >/dev/null 2>&1 || true; done
    sudo -n chown -R "$(id -u):$(id -g)" "$OUT" 2>/dev/null || true
    "$H/check_env.sh" end "$LOG" >>"$LOG/check-end.txt" 2>&1 || { echo "end guard FAILED" >>"$LOG/warnings.txt"; status=98; }
    printf '%s\n' "$status" >"$LOG/exit-status.txt"
    exit "$status"
}
trap finish EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
"$H/check_env.sh" start "$LOG" 15
(cd "$R/bench-extra" && sha256sum --check --quiet sha256.txt)
p=$$
while [ "$p" -gt 1 ]; do
    c=$(cat /proc/$p/comm 2>/dev/null || true)
    case $c in claude|node|2.1.*|ccd-cli*) CLAUDE_PIDS+=("$p");; esac
    p=$(awk '{print $4}' /proc/$p/stat)
done
for p in "${CLAUDE_PIDS[@]}"; do taskset -apc 2 "$p" >>"$LOG/claude-taskset.txt" 2>&1 || true; done
taskset -pc 2 $$ >/dev/null
sudo -n docker run -d --name "$NAME" --privileged --pid=host --shm-size=1g -v /sys:/sys \
    -v /lib/modules:/lib/modules:ro -v "$R/bench-extra":/pb:ro "$IMAGE" sleep infinity >"$LOG/container-id.txt"
sudo -n docker exec "$NAME" cat /proc/self/cgroup >"$LOG/cgroup-container.txt"
cat /proc/self/cgroup >"$LOG/cgroup-host.txt"
ARGS="4 1728 10 200000 single=4 pair=4,5 all=0-5"
for round in 1 2 3; do
  for where in host container; do
    echo "$(date +%T) round $round $where start" >>"$LOG/steps.txt"
    if [ $where = host ]; then
      timeout 300 taskset -c 4 "$R/bench-extra/pinbench" $ARGS >"$OUT/r$round-host.jsonl" 2>>"$OUT/stderr.txt"
    else
      sudo -n timeout 300 docker exec "$NAME" taskset -c 4 /pb/pinbench $ARGS >"$OUT/r$round-container.jsonl" 2>>"$OUT/stderr.txt"
    fi
  done
done
echo "cpuset control done"
