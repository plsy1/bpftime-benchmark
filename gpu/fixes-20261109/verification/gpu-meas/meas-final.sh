#!/bin/bash
# meas.sh of examples-20261109 again, on the round's last build (fixes-20261109 verification),
# with a run-name prefix and a summary. Usage: meas-final.sh PREFIX
#  A: victim start-up (vec_add_legacy 5 3 5 1, wall ms): no agent / agent + empty kprobe, 5 rounds interleaved
#  B: per-launch time, 1M threads (vec_add_legacy 1048576 200 256 0, victim's own median):
#     no agent / empty kprobe / sampling kprobe, 3 rounds interleaved
#  C: strace phase times of one agent start (execve/openat/exit_group, seccomp-bpf filter)
set -u
W=/home/jetson/src/bpftime-gpu-work
B=/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate/build-cuda
V=$W/benchmark/gpu/workload/vec_add_legacy
P=${1:?prefix}
export SPDLOG_LEVEL=info SRV_ENV="BPFTIME_SHM_MEMORY_MB=64 BPFTIME_MAP_GPU_THREAD_COUNT=16"
echo "cpu5=$(cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq) gpu=$(cat /sys/devices/platform/bus@0/17000000.gpu/devfreq/17000000.gpu/cur_freq) start=$(date +%T)"
for r in 1 2 3 4 5; do
  NO_AGENT=1 $W/run.sh $P-A-noagent-r$r $W/example/gpu/kt3-empty ./kernel_trace "$V 5 3 5 1" 60 30 2 | grep client_rc
  $W/run.sh $P-A-empty-r$r $W/example/gpu/kt3-empty ./kernel_trace "$V 5 3 5 1" 60 30 2 | grep client_rc
done
for r in 1 2 3; do
  NO_AGENT=1 $W/run.sh $P-B-noagent-r$r $W/example/gpu/kt3-empty ./kernel_trace "$V 1048576 200 256 0" 90 60 2 | grep client_rc
  $W/run.sh $P-B-empty-r$r $W/example/gpu/kt3-empty ./kernel_trace "$V 1048576 200 256 0" 90 60 2 | grep client_rc
  $W/run.sh $P-B-sample-r$r $W/example/gpu/kt3-sample ./kernel_trace "$V 1048576 200 256 0" 90 60 2 | grep client_rc
done
NO_AGENT=1 $W/run.sh $P-C-strace $W/example/gpu/kt3-empty ./kernel_trace "strace -f -ttt --seccomp-bpf -e trace=execve,openat,exit_group -o $W/runs/$P-C-strace.trace env LD_PRELOAD=$B/runtime/agent/libbpftime-agent.so $V 5 3 5 1" 60 60 2 | grep client_rc
echo "end=$(date +%T) cpu5=$(cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq) gpu=$(cat /sys/devices/platform/bus@0/17000000.gpu/devfreq/17000000.gpu/cur_freq)"
ls /dev/shm | grep '^bpftime_gpu_' || echo shm-clean
echo "== A client_ms (exec to exit)"
for k in noagent empty; do
  printf '%-8s' $k; for r in 1 2 3 4 5; do printf ' %s' "$(grep -o 'client_ms=[0-9]*' $W/runs/$P-A-$k-r$r/meta.txt | cut -d= -f2)"; done; echo
done
echo "== A probe check (empty kprobe: patched images loaded; validation line)"
for r in 1 2 3 4 5; do
  printf 'r%s %s | %s\n' $r "$(grep -c 'loaded the patched image' $W/runs/$P-A-empty-r$r/client.log)" "$(grep -a 'Validation' $W/runs/$P-A-empty-r$r/client.log | tail -n 1)"
done
echo "== B victim median per launch (us)"
for k in noagent empty sample; do
  printf '%-8s' $k; for r in 1 2 3; do printf ' %s' "$(grep -o 'Median: *[0-9.]*' $W/runs/$P-B-$k-r$r/client.log | awk '{print $2}')"; done; echo
done
echo "== B sampling probe: events (server.log); validation"
for r in 1 2 3; do
  printf 'r%s %s | %s\n' $r "$(grep -a 'total events' $W/runs/$P-B-sample-r$r/server.log | tail -n 1)" "$(grep -a 'Validation' $W/runs/$P-B-sample-r$r/client.log | tail -n 1)"
done
echo "== C"; grep -o 'client_ms=[0-9]*' $W/runs/$P-C-strace/meta.txt
