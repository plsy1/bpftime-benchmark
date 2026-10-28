#!/bin/bash
# One run per leg: tp_attach loads src/tp_count.bpf.o, writeloop issues N
# write(2) calls on fd 42; the loader then prints its counters.
# Legs: none (no tracing), kernel (kernel tracepoints), bpftime (syscall-server
# in the loader; transformer + agent in writeloop).
set -u
HERE=$(cd "$(dirname "$0")" && pwd)
B=${BUILD:-/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate/build-jetson-daily}
N=${N:-1000000}
leg=$1
cleanup() {
  sudo -n pkill -9 -x tp_attach 2>/dev/null; sudo -n pkill -9 -x writeloop 2>/dev/null
  sudo -n "$B/tools/bpftimetool/bpftimetool" remove >/dev/null 2>&1
}
cleanup
if [ "$leg" = none ]; then
  timeout 120 "$HERE/src/writeloop" "$N"; exit $?
fi
out=$(mktemp)
if [ "$leg" = kernel ]; then
  sudo -n timeout 300 "$HERE/src/tp_attach" "$HERE/src/tp_count.bpf.o" > "$out" 2>&1 &
else
  sudo -n timeout 300 env LD_PRELOAD="$B/runtime/syscall-server/libbpftime-syscall-server.so" \
    BPFTIME_LOG_OUTPUT=console "$HERE/src/tp_attach" "$HERE/src/tp_count.bpf.o" > "$out" 2>&1 &
fi
for i in $(seq 100); do grep -q attached "$out" && break; sleep 0.1; done
grep -q attached "$out" || { echo "loader did not attach"; tail -5 "$out"; cleanup; exit 1; }
if [ "$leg" = kernel ]; then
  timeout 120 "$HERE/src/writeloop" "$N"
else
  sudo -n timeout 120 env AGENT_SO="$B/runtime/agent/libbpftime-agent.so" \
    LD_PRELOAD="$B/attach/text_segment_transformer/libbpftime-agent-transformer.so" \
    BPFTIME_LOG_OUTPUT=console "$HERE/src/writeloop" "$N"
fi
echo "writeloop rc $?"
sudo -n pkill -INT -x tp_attach; sleep 1
grep -E "^count[0-2] |Rewrote|error|Error" "$out" | head -10
rm -f "$out"
cleanup
