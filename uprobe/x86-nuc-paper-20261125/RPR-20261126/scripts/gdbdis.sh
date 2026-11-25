#!/usr/bin/env bash
set -uo pipefail
B=$1; TAG=$2; shift 2
S=$(dirname "$0"); cd "$S/root"
sudo -n "$B/bpftimetool" remove >/dev/null 2>&1 || true
sudo -n taskset -c 4 env "$@" BPFTIME_LOG_OUTPUT=console LD_PRELOAD=$B/libbpftime-syscall-server.so benchmark/uprobe/uprobe > loader-g$TAG.log 2>&1 &
L=$!
for i in $(seq 100); do grep -q 'Successfully started' loader-g$TAG.log && break; sleep 0.1; done
sleep 1
cat > cmds-$TAG.gdb <<G
set pagination off
set confirm off
set breakpoint pending on
set startup-with-shell off
G
for kv in "$@" BPFTIME_LOG_OUTPUT=console LD_PRELOAD=$B/libbpftime-agent.so; do echo "set environment ${kv%%=*} ${kv#*=}" >> cmds-$TAG.gdb; done
cat >> cmds-$TAG.gdb <<'G'
break bpftime_probe_read
run 1 2000
set $ra = *(unsigned long*)$rsp
printf "RA %lx\n", $ra
x/60i $ra-150
bt 3
info symbol $ra
kill
quit
G
sudo -n timeout 120 gdb -q -batch -x cmds-$TAG.gdb benchmark/test > $S/gdb-$TAG.txt 2>&1
sudo -n pkill -INT -x uprobe; wait $L 2>/dev/null
sudo -n "$B/bpftimetool" remove >/dev/null 2>&1 || true
