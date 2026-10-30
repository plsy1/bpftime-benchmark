#!/usr/bin/env bash
# Functional check of the fuse benchmark on the Jetson: one native and one
# bpftime run of each filebench workload against the passthrough FUSE mount.
# Run as root (the syscall server reads tracefs). Bounded by timeouts.
set -u
B=/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate
F=${B}/benchmark/fuse
BUILD=${B}/build-jetson-daily
W=${WORK:-/home/jetson/src/results/syscall/fuse-20261030/work}
OUT=${OUT:-/home/jetson/src/results/syscall/fuse-20261030/check.log}
export BPFTIME_GLOBAL_SHM_NAME=fusebench_shm
mkdir -p "${W}/src/dir" "${W}/mnt"
for i in $(seq 1 20); do echo "file ${i}" > "${W}/src/dir/f${i}.txt"; done
echo "hello world" > "${W}/src/test.txt"

timeout 900 taskset -c 4 "${F}/fs/passthrough" "${W}/src" "${W}/mnt" > "${W}/fuse.log" 2>&1 &
FUSE_PID=$!
for i in $(seq 1 50); do mountpoint -q "${W}/mnt" && break; sleep 0.1; done
mountpoint -q "${W}/mnt" || { echo "mount failed"; cat "${W}/fuse.log"; exit 1; }

run() { # label, then the filebench arguments
	local label=$1; shift
	local line
	line=$(timeout 120 taskset -c 5 "$@" 2>&1 | grep "Average time usage" | tail -1)
	echo "${label}: ${line}" | tee -a "${OUT}"
}
AGENT=(env AGENT_SO=${BUILD}/runtime/agent/libbpftime-agent.so
       LD_PRELOAD=${BUILD}/attach/text_segment_transformer/libbpftime-agent-transformer.so)
: > "${OUT}"
run "native getdents64" "${F}/filebench" getdents64 "${W}/mnt/dir"
run "native open" "${F}/filebench" open "${W}/mnt/dir"
(cd "${W}/mnt" && run "native newfsstatat" "${F}/filebench" newfsstatat "${W}/mnt" test.txt)

run "transformer only getdents64" "${AGENT[@]}" "${F}/filebench" getdents64 "${W}/mnt/dir"

timeout 900 taskset -c 0 env LD_PRELOAD=${BUILD}/runtime/syscall-server/libbpftime-syscall-server.so \
	BPFTIME_SHM_MEMORY_MB=256 "${F}/bpf/fs-cache" > "${W}/server.log" 2>&1 &
SERVER_PID=$!
sleep 3
kill -0 ${SERVER_PID} 2>/dev/null || { echo "server died" | tee -a "${OUT}"; tail -5 "${W}/server.log" | tee -a "${OUT}"; }
run "bpftime getdents64" "${AGENT[@]}" "${F}/filebench" getdents64 "${W}/mnt/dir"
run "bpftime open" "${AGENT[@]}" "${F}/filebench" open "${W}/mnt/dir"
(cd "${W}/mnt" && run "bpftime newfsstatat" "${AGENT[@]}" "${F}/filebench" newfsstatat "${W}/mnt" test.txt)
# one verbose run for the record
timeout 120 taskset -c 5 "${AGENT[@]}" "${F}/filebench" getdents64 "${W}/mnt/dir" > "${W}/agent-getdents.log" 2>&1

kill -INT ${SERVER_PID}; sleep 1; kill -9 ${SERVER_PID} 2>/dev/null
kill -INT ${FUSE_PID}; sleep 1
mountpoint -q "${W}/mnt" && fusermount -u "${W}/mnt"
wait
cat "${W}/fuse.log" | tail -15 >> "${OUT}"
rm -f /dev/shm/fusebench_shm
