#!/usr/bin/env bash
# statsnoop (syscall server) + forker (transformer, agent). Root.
# AGENT, TRANSFORMER, SERVER: the builds to use; OUT: the tool's output.
set -u
T=/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate/example/tracing/statsnoop/statsnoop
D=$(dirname "$0")
export BPFTIME_GLOBAL_SHM_NAME=clone_e2e_shm
rm -f /dev/shm/clone_e2e_shm
timeout 120 env LD_PRELOAD=${SERVER} BPFTIME_SHM_MEMORY_MB=256 ${T} > "${OUT}" 2> "${OUT}.err" &
SP=$!
sleep 3
kill -0 ${SP} || { echo "server died"; tail -3 "${OUT}.err"; exit 1; }
timeout 120 env AGENT_SO=${AGENT} LD_PRELOAD=${TRANSFORMER} "${D}/cloner" 2> "${OUT}.agent"
echo "forker rc=$?"
sleep 3
kill -INT ${SP}; sleep 1; kill -9 ${SP} 2>/dev/null; wait
rm -f /dev/shm/clone_e2e_shm
