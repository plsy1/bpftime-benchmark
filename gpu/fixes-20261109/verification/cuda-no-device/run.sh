#!/usr/bin/env bash
# CPU probes under the CUDA build's agent, with the GPU visible and without (CUDA_VISIBLE_DEVICES=
# empty: cuInit returns 100): funclatency (libbpf-tools, under build-cuda's syscall server) on
# callloop's target_func for 3 s, as the daily smoke. Counted must equal the calls.
set -u
D=/home/jetson/src/results/gpu/fixes-20261109/verification/cuda-no-device
B=/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate/build-cuda
T=/home/jetson/src/realprog-20261026-btf/libbpf-tools
W=/home/jetson/src/realprog-20261026-btf/workloads-mt/t4
for v in visible none; do
  S=bpftime_cuda_nodev_$v
  VIS=(); [ $v = none ] && VIS=(CUDA_VISIBLE_DEVICES=)
  sudo -n timeout 30 env LD_PRELOAD=$B/runtime/syscall-server/libbpftime-syscall-server.so BPFTIME_SHM_MEMORY_MB=1024 BPFTIME_GLOBAL_SHM_NAME=$S BPFTIME_LOG_OUTPUT=console $T/funclatency -u -d 9 $W/callloop:target_func > $D/$v.tool.txt 2>&1 &
  TP=$!
  sleep 3
  sudo -n timeout 40 env "${VIS[@]}" LD_PRELOAD=$B/runtime/agent/libbpftime-agent.so BPFTIME_GLOBAL_SHM_NAME=$S BPFTIME_LOG_OUTPUT=console $W/callloop 3 > $D/$v.workload.txt 2>&1
  wrc=$?
  wait $TP; trc=$?
  calls=$(grep -o 'total_ops [0-9]*' $D/$v.workload.txt | awk '{print $2}')
  counted=$(grep -E -o -e '->[[:space:]]*[0-9]+[[:space:]]*:[[:space:]]*[0-9]+' $D/$v.tool.txt | awk -F: '{s+=$2} END {print s+0}')
  echo "$v: workload rc $wrc, tool rc $trc, calls $calls, counted $counted"
  grep -a -E 'cuInit|CommSharedMem|CUDA attach|init_fail|Unable|disabled' $D/$v.workload.txt | head -5 | sed 's/^/  /'
  sudo -n rm -f /dev/shm/$S
done
