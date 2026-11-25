#!/usr/bin/env bash
# R-PR: run the uprobe suite's test binary under a build; print __bench_read/__bench_write/__bench_uprobe per run.
# usage: run.sh BUILD_DIR REPS [extra env...]
set -uo pipefail
B=$1; N=$2; shift 2
S=$(dirname "$0"); cd "$S/root"
sudo -n "$B/bpftimetool" remove >/dev/null 2>&1 || true
sudo -n taskset -c 4 env "$@" BPFTIME_LOG_OUTPUT=console LD_PRELOAD=$B/libbpftime-syscall-server.so benchmark/uprobe/uprobe > loader.log 2>&1 &
L=$!
for i in $(seq 100); do grep -q 'Successfully started' loader.log && break; sleep 0.1; done
sleep 1
for r in $(seq $N); do
  sudo -n taskset -c 2 env "$@" LD_PRELOAD=$B/libbpftime-agent.so benchmark/test 1 20000 2>&1 | awk -v r=$r '/^Benchmarking/{n=$2} /Average time usage/{print r, n, $4}' | grep -E '__bench_(read|write|uprobe) '
done
sudo -n pkill -INT -x uprobe; wait $L 2>/dev/null
sudo -n "$B/bpftimetool" remove >/dev/null 2>&1 || true
