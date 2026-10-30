#!/usr/bin/env bash
# fuse benchmark on the Jetson: filebench workloads against the passthrough
# FUSE mount, native vs bpftime (fs-cache programs), alternating rounds.
# Run as root (the syscall server reads tracefs). Every step is bounded.
#   BUILD: bpftime build to use (agent, syscall server, transformer)
#   OUT:   jsonl file, one record per run
set -u
B=/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate
F=${B}/benchmark/fuse
BUILD=${BUILD:-${B}/build-fuse-exp}
D=/home/jetson/src/results/syscall/fuse-20261030
W=${D}/work
OUT=${OUT:-${D}/runs.jsonl}
ROUNDS=${ROUNDS:-5}
export BPFTIME_GLOBAL_SHM_NAME=fusebench_shm

rm -rf "${W}/src" && mkdir -p "${W}/src/dir" "${W}/src/fuse/data/arch/alpha" "${W}/mnt"
for i in $(seq 1 20); do echo "file ${i}" > "${W}/src/dir/f${i}.txt"; done
echo "hello world" > "${W}/src/test.txt"

AGENT=(env AGENT_SO=${BUILD}/runtime/agent/libbpftime-agent.so
       LD_PRELOAD=${BUILD}/attach/text_segment_transformer/libbpftime-agent-transformer.so
       BPFTIME_TRACE_PRINTK_OUTPUT=none)

mount_fuse() {
	timeout 3600 taskset -c 4 "${F}/fs/passthrough" "${W}/src" "${W}/mnt" > "${W}/fuse-$1.log" 2>&1 &
	FUSE_PID=$!
	for i in $(seq 1 50); do mountpoint -q "${W}/mnt" && return 0; sleep 0.1; done
	echo "mount failed" >&2; exit 1
}
umount_fuse() {
	kill -INT ${FUSE_PID}; sleep 1
	mountpoint -q "${W}/mnt" && fusermount -u "${W}/mnt"
	wait ${FUSE_PID} 2>/dev/null
}
start_server() {
	timeout 3600 taskset -c 0 env LD_PRELOAD=${BUILD}/runtime/syscall-server/libbpftime-syscall-server.so \
		BPFTIME_SHM_MEMORY_MB=256 "${F}/bpf/fs-cache" > "${W}/server.log" 2>&1 &
	SERVER_PID=$!
	sleep 3
	kill -0 ${SERVER_PID} 2>/dev/null || { echo "server died" >&2; tail -5 "${W}/server.log" >&2; exit 1; }
}
stop_server() {
	kill -INT ${SERVER_PID}; sleep 1; kill -9 ${SERVER_PID} 2>/dev/null
	wait ${SERVER_PID} 2>/dev/null
	rm -f /dev/shm/${BPFTIME_GLOBAL_SHM_NAME}
}
# leg, workload, round, then the command (run in the mount)
one() {
	local leg=$1 wl=$2 round=$3; shift 3
	local line ns
	line=$(cd "${W}/mnt" && timeout 300 taskset -c 5 "$@" 2>&1 | grep "Average time usage" | tail -1)
	ns=$(echo "${line}" | awk '{print $4}')
	echo "{\"leg\": \"${leg}\", \"workload\": \"${wl}\", \"round\": ${round}, \"ns\": ${ns:-null}}" | tee -a "${OUT}"
}
workloads() { # leg, round, prefix command...
	local leg=$1 round=$2; shift 2
	one "${leg}" getdents64 "${round}" "$@" "${F}/filebench" getdents64 dir
	one "${leg}" open "${round}" "$@" "${F}/filebench" open dir
	one "${leg}" open-denied "${round}" "$@" "${F}/filebench" open fuse/data/arch/alpha
	one "${leg}" newfstatat "${round}" "$@" "${F}/filebench" newfsstatat "${W}/mnt" test.txt
}

mount_fuse bench
for r in $(seq 1 "${ROUNDS}"); do
	workloads native "${r}"
	start_server
	workloads bpftime "${r}" "${AGENT[@]}"
	stop_server
done
umount_fuse

# Correctness: the listings and the FUSE requests each leg makes
mount_fuse native-check
(cd "${W}/mnt" && timeout 120 taskset -c 5 "${F}/filebench" getdents64 dir) > "${W}/native-listing.log" 2>&1
umount_fuse
mount_fuse bpftime-check
start_server
(cd "${W}/mnt" && timeout 120 taskset -c 5 "${AGENT[@]}" "${F}/filebench" getdents64 dir) > "${W}/bpftime-listing.log" 2>&1
(cd "${W}/mnt" && timeout 120 taskset -c 5 env AGENT_SO=${BUILD}/runtime/agent/libbpftime-agent.so \
	LD_PRELOAD=${BUILD}/attach/text_segment_transformer/libbpftime-agent-transformer.so \
	BPFTIME_TRACE_PRINTK_OUTPUT=${W}/printk.log "${F}/filebench" open fuse/data/arch/alpha) > /dev/null 2>&1
stop_server
umount_fuse
