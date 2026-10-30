#!/usr/bin/env bash
set -u
B=/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate/build-jetson-daily
F=/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate/benchmark/fuse
D=$(dirname "$0")
export BPFTIME_GLOBAL_SHM_NAME=cancel_e2e_shm
timeout 120 env LD_PRELOAD=${B}/runtime/syscall-server/libbpftime-syscall-server.so BPFTIME_SHM_MEMORY_MB=256 "${F}/bpf/fs-cache" > "${D}/server.log" 2>&1 &
SP=$!
sleep 3
timeout 60 gdb -batch -ex "set startup-with-shell off" -ex "set environment AGENT_SO=${B}/runtime/agent/libbpftime-agent.so" -ex "set environment LD_PRELOAD=${B}/attach/text_segment_transformer/libbpftime-agent-transformer.so" -ex "handle SIGCANCEL nostop noprint pass" -ex "handle SIG32 nostop noprint pass" -ex run -ex "bt 30" -ex "info registers pc" "${D}/cancel" > "${D}/gdb.log" 2>&1
kill -INT ${SP}; sleep 1; kill -9 ${SP} 2>/dev/null; wait
rm -f /dev/shm/cancel_e2e_shm
