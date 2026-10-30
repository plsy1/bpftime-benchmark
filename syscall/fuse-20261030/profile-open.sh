#!/usr/bin/env bash
# perf profile of the allowed open workload under bpftime (where do +9 us go).
set -u
B=/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate
F=${B}/benchmark/fuse; BUILD=${BUILD:-${B}/build-fuse-exp}
D=/home/jetson/src/results/syscall/fuse-20261030; W=${D}/work
export BPFTIME_GLOBAL_SHM_NAME=fusebench_shm
timeout 600 taskset -c 4 "${F}/fs/passthrough" "${W}/src" "${W}/mnt" > "${W}/fuse-prof.log" 2>&1 &
FP=$!; for i in $(seq 1 50); do mountpoint -q "${W}/mnt" && break; sleep 0.1; done
timeout 600 taskset -c 0 env LD_PRELOAD=${BUILD}/runtime/syscall-server/libbpftime-syscall-server.so \
	BPFTIME_SHM_MEMORY_MB=256 "${F}/bpf/fs-cache" > "${W}/server-prof.log" 2>&1 &
SP=$!; sleep 3
cd "${W}/mnt" && timeout 120 /usr/lib/linux-nvidia-tegra-tools-6.8.0-1020/perf record -q -g -o "${D}/perf-open.data" -- taskset -c 5 env AGENT_SO=${BUILD}/runtime/agent/libbpftime-agent.so \
	LD_PRELOAD=${BUILD}/attach/text_segment_transformer/libbpftime-agent-transformer.so \
	BPFTIME_TRACE_PRINTK_OUTPUT=none "${F}/filebench" open dir > "${W}/agent-prof.log" 2>&1
cd /
kill -INT ${SP}; sleep 1; kill -9 ${SP} 2>/dev/null
kill -INT ${FP}; sleep 1; mountpoint -q "${W}/mnt" && fusermount -u "${W}/mnt"; wait
rm -f /dev/shm/fusebench_shm
