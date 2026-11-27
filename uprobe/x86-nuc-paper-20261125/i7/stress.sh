#!/usr/bin/env bash
# I7' stressor control, called from plan '!' lines: stress.sh start|stop OUTDIR. Starts l2stress (1 MiB each) on
# CPUs 9, 10, 11 as user processes in their own session; pids in OUTDIR/l2stress.pids; stop kills exactly those.
set -euo pipefail
D=$(dirname "$(readlink -f "$0")"); OUT=$2
case $1 in
start)
	: >"$OUT/l2stress.pids"
	for c in 9 10 11; do
		setsid taskset -c $c "$D/l2stress" 1048576 >"$OUT/l2stress-$c.log" 2>&1 < /dev/null &
		echo $! >>"$OUT/l2stress.pids"
	done
	sleep 2
	for p in $(cat "$OUT/l2stress.pids"); do kill -0 "$p"; done
	echo "l2stress started: $(tr '\n' ' ' <"$OUT/l2stress.pids")"
	grep -h ready "$OUT"/l2stress-*.log ;;
stop)
	for p in $(cat "$OUT/l2stress.pids"); do kill "$p" 2>/dev/null || true; done
	sleep 1
	for p in $(cat "$OUT/l2stress.pids"); do if kill -0 "$p" 2>/dev/null; then kill -9 "$p"; fi; done
	echo "l2stress stopped" ;;
esac
