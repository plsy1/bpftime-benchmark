#!/usr/bin/env bash
# Run cancel under the syscall server (fs-cache's tracepoint programs) and
# the transformer with the real agent. Root (the server reads tracefs).
set -u
B=${B:-/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate/build-jetson-daily}
F=/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate/benchmark/fuse
D=$(dirname "$0")
export BPFTIME_GLOBAL_SHM_NAME=cancel_e2e_shm
echo "native: $(timeout 30 "${D}/cancel")"
timeout 120 env LD_PRELOAD=${B}/runtime/syscall-server/libbpftime-syscall-server.so \
	BPFTIME_SHM_MEMORY_MB=256 "${F}/bpf/fs-cache" > "${D}/server.log" 2>&1 &
SP=$!
sleep 3
kill -0 ${SP} || { echo "server died"; exit 1; }
echo "bpftime: $(timeout 30 env AGENT_SO=${B}/runtime/agent/libbpftime-agent.so \
	LD_PRELOAD=${TRANSFORMER:-${B}/attach/text_segment_transformer/libbpftime-agent-transformer.so} \
	"${D}/cancel" 2>"${D}/agent.log")"
echo "rc=$?"
kill -INT ${SP}; sleep 1; kill -9 ${SP} 2>/dev/null; wait
rm -f /dev/shm/cancel_e2e_shm
