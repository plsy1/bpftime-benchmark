#!/usr/bin/env bash
# Smoke for E15/E16. A host-side poller records which sslsniff loaders carry SSLSNIFF_PB_WAKEUP in their environment
# and which nginx processes carry BPFTIME_DIAG_EXACT (container runs with --pid=host, so /proc is shared).
set -uo pipefail
R=$HOME/src/paper-runs-20261124; SID=${1:?sid}; mkdir -p "$R/logs/$SID"; P=$R/logs/$SID/env-poll.txt
( sleep 25; end=$((SECONDS+600)); while [ $SECONDS -lt $end ]; do
    pids=$(pgrep -f '/sslsniff/sslsniff-' ; pgrep -x nginx); [ -z "$pids" ] && { sleep 1; continue; }
    for pid in $pids; do
      e=$(sudo -n cat /proc/$pid/environ 2>/dev/null | tr '\0' '\n' | grep -E '^(SSLSNIFF_PB_WAKEUP|BPFTIME_DIAG_EXACT)=' | tr '\n' ' ')
      [ -n "$e" ] && echo "$(date +%T) $pid $(tr '\0' ' ' </proc/$pid/cmdline | cut -c1-60) :: $e"
    done; sleep 1; done ) >"$P" 2>&1 &
POLL=$!
PAPER_EXEC_PATH=/usr/lib/linux-tools/6.8.0-1020-nvidia-tegra:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin \
  timeout 1200 "$R/harness/session.sh" "$SID" "$R/plans/SMOKE-E15E16.txt" 20
rc=$?; kill $POLL 2>/dev/null; sort -u -k2,2 -k4 "$P" | head -40; echo "smoke exit=$rc"
