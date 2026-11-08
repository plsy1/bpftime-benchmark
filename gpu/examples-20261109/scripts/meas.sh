#!/bin/bash
# Timing of CUDA attach on the Orin (after 04:15 JST, clocks locked):
#  A: victim start-up (vec_add_legacy 5 3 5 1, wall ms): no agent / agent + empty kprobe, 5 rounds interleaved
#  B: per-launch time, 1M threads (vec_add_legacy 1048576 200 256 0, victim's own median):
#     no agent / empty kprobe / sampling kprobe (helpers + compare, thread 0 writes the ringbuf), 3 rounds
#  C: strace phase times of one agent start (execve/openat/exit_group, seccomp-bpf filter)
set -u
W=/home/jetson/src/bpftime-gpu-work
B=/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate/build-cuda
V=$W/benchmark/gpu/workload/vec_add_legacy
export SPDLOG_LEVEL=info SRV_ENV="BPFTIME_SHM_MEMORY_MB=64 BPFTIME_MAP_GPU_THREAD_COUNT=16"
echo "cpu5=$(cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq) gpu=$(cat /sys/devices/platform/bus@0/17000000.gpu/devfreq/17000000.gpu/cur_freq) start=$(date +%T)"
for r in 1 2 3 4 5; do
  NO_AGENT=1 $W/run.sh m-A-noagent-r$r $W/example/gpu/kt3-empty ./kernel_trace "$V 5 3 5 1" 60 30 2 | grep client_rc
  $W/run.sh m-A-empty-r$r $W/example/gpu/kt3-empty ./kernel_trace "$V 5 3 5 1" 60 30 2 | grep client_rc
done
for r in 1 2 3; do
  NO_AGENT=1 $W/run.sh m-B-noagent-r$r $W/example/gpu/kt3-empty ./kernel_trace "$V 1048576 200 256 0" 90 60 2 | grep client_rc
  $W/run.sh m-B-empty-r$r $W/example/gpu/kt3-empty ./kernel_trace "$V 1048576 200 256 0" 90 60 2 | grep client_rc
  $W/run.sh m-B-sample-r$r $W/example/gpu/kt3-sample ./kernel_trace "$V 1048576 200 256 0" 90 60 2 | grep client_rc
done
NO_AGENT=1 $W/run.sh m-C-strace $W/example/gpu/kt3-empty ./kernel_trace "strace -f -ttt --seccomp-bpf -e trace=execve,openat,exit_group -o $W/runs/m-C-strace.trace env LD_PRELOAD=$B/runtime/agent/libbpftime-agent.so $V 5 3 5 1" 60 60 2 | grep client_rc
echo "end=$(date +%T) cpu5=$(cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq) gpu=$(cat /sys/devices/platform/bus@0/17000000.gpu/devfreq/17000000.gpu/cur_freq)"
ls /dev/shm | grep '^bpftime_gpu_' || echo shm-clean
