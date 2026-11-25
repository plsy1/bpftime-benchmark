#!/usr/bin/env bash
# R-PR: perf record the victim under a build (loader running); output perf.data.<tag>
set -uo pipefail
B=$1; TAG=$2; shift 2
S=$(dirname "$0"); cd "$S/root"
sudo -n "$B/bpftimetool" remove >/dev/null 2>&1 || true
sudo -n taskset -c 4 env "$@" BPFTIME_LOG_OUTPUT=console LD_PRELOAD=$B/libbpftime-syscall-server.so benchmark/uprobe/uprobe > loader-$TAG.log 2>&1 &
L=$!
for i in $(seq 100); do grep -q 'Successfully started' loader-$TAG.log && break; sleep 0.1; done
sleep 1
sudo -n taskset -c 2 perf record -q -e cycles:u -c 20011 -o $S/perf.data.$TAG -- env "$@" BPFTIME_LOG_OUTPUT=console LD_PRELOAD=$B/libbpftime-agent.so benchmark/test 1 20000 > victim-$TAG.log 2>&1
sudo -n pkill -INT -x uprobe; wait $L 2>/dev/null
sudo -n "$B/bpftimetool" remove >/dev/null 2>&1 || true
sudo -n chown $(id -u):$(id -g) $S/perf.data.$TAG
