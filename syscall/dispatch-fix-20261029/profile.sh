#!/bin/bash
# perf profile of one leg: the loader attaches OBJ (none for rewrite/none legs),
# writeloop runs N writes; after 2 s, perf records the writeloop process for 5 s
# (cycles, frame-pointer call graphs) and perf stat counts the whole run.
# Usage: profile.sh LEG  (none | rewrite | agent-other | agent-exit)
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
B=${BPFTIME_BUILD:-/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate/build-jetson-daily}
PERF=${PERF:-/usr/lib/linux-nvidia-tegra-tools-6.8.0-1020/perf}
N=${N:-20000000}
leg=$1
out=$HERE/perf
cleanup() {
  sudo -n pkill -9 -x tp_attach 2>/dev/null; sudo -n pkill -9 -x writeloop 2>/dev/null
  sudo -n "$B/tools/bpftimetool/bpftimetool" remove >/dev/null 2>&1
}
cleanup
loader_log=$(mktemp)
case $leg in
  agent-other) obj=tp_other.bpf.o ;;
  agent-exit) obj=tp_exit_empty.bpf.o ;;
  *) obj= ;;
esac
if [ -n "$obj" ]; then
  sudo -n timeout 600 env LD_PRELOAD="$B/runtime/syscall-server/libbpftime-syscall-server.so" \
    BPFTIME_LOG_OUTPUT=console "$HERE/src/tp_attach" "$HERE/src/$obj" > "$loader_log" 2>&1 &
  for i in $(seq 100); do grep -q attached "$loader_log" && break; sleep 0.1; done
fi
env_args=()
case $leg in
  rewrite) env_args=(AGENT_SO="$HERE/src/libdummy_agent.so" LD_PRELOAD="$B/attach/text_segment_transformer/libbpftime-agent-transformer.so") ;;
  agent-*) env_args=(AGENT_SO="$B/runtime/agent/libbpftime-agent.so" LD_PRELOAD="$B/attach/text_segment_transformer/libbpftime-agent-transformer.so" BPFTIME_LOG_OUTPUT=console) ;;
esac
sudo -n timeout 300 "$PERF" stat -e cycles,instructions,branch-misses,cache-misses -o "$out/$leg.stat.txt" -- \
  env "${env_args[@]}" "$HERE/src/writeloop" "$N" > "$out/$leg.writeloop.txt" 2>&1 &
stat_pid=$!
sleep 2
pid=$(pgrep -n -x writeloop)
sudo -n timeout 60 "$PERF" record -F 2999 -g -p "$pid" -o "$out/$leg.perf.data" -- sleep 5 > /dev/null 2>&1
wait $stat_pid  # not the loader, which runs until cleanup
sudo -n chown jetson:jetson "$out/$leg.perf.data" "$out/$leg.stat.txt" 2>/dev/null
"$PERF" report -i "$out/$leg.perf.data" --no-children --sort dso,sym --stdio -g none --percent-limit 0.7 > "$out/$leg.report.txt" 2>/dev/null
grep -E "ns_per_write|writes" "$out/$leg.writeloop.txt"
rm -f "$loader_log"
cleanup
