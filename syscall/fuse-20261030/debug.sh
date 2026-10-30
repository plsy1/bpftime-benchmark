#!/usr/bin/env bash
# Which fs-cache programs fire for one getdents64 run (printk to a file).
set -u
B=/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate
F=${B}/benchmark/fuse
BUILD=${B}/build-jetson-daily
W=/home/jetson/src/results/syscall/fuse-20261030/work
DIR=${1:-${W}/src/dir}
export BPFTIME_GLOBAL_SHM_NAME=fusebench_shm
rm -f "${W}/printk.log"
timeout 300 taskset -c 0 env LD_PRELOAD=${BUILD}/runtime/syscall-server/libbpftime-syscall-server.so \
	BPFTIME_SHM_MEMORY_MB=256 "${F}/bpf/fs-cache" > "${W}/server-debug.log" 2>&1 &
SERVER_PID=$!
sleep 3
kill -0 ${SERVER_PID} || { echo server died; exit 1; }
timeout 120 taskset -c 5 env AGENT_SO=${BUILD}/runtime/agent/libbpftime-agent.so \
	LD_PRELOAD=${BUILD}/attach/text_segment_transformer/libbpftime-agent-transformer.so \
	BPFTIME_TRACE_PRINTK_OUTPUT=${W}/printk.log SPDLOG_LEVEL=debug \
	"${F}/filebench" getdents64 "${DIR}" > "${W}/agent-debug.log" 2>&1
echo agent rc=$?
kill -INT ${SERVER_PID}; sleep 1; kill -9 ${SERVER_PID} 2>/dev/null; wait
rm -f /dev/shm/fusebench_shm
