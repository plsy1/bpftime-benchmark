#!/bin/bash
# meas.sh round B only (per-launch time, 1M threads: vec_add_legacy 1048576 200 256 0, victim's own
# median): no agent / empty kprobe / sampling kprobe, 3 rounds interleaved. Usage: meas-b.sh PREFIX
set -u
W=/home/jetson/src/bpftime-gpu-work
V=$W/benchmark/gpu/workload/vec_add_legacy
P=${1:?prefix}
export SPDLOG_LEVEL=info SRV_ENV="BPFTIME_SHM_MEMORY_MB=64 BPFTIME_MAP_GPU_THREAD_COUNT=16"
echo "cpu5=$(cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq) gpu=$(cat /sys/devices/platform/bus@0/17000000.gpu/devfreq/17000000.gpu/cur_freq) start=$(date +%T)"
for r in 1 2 3; do
  NO_AGENT=1 $W/run.sh $P-noagent-r$r $W/example/gpu/kt3-empty ./kernel_trace "$V 1048576 200 256 0" 90 60 2 | grep client_rc
  $W/run.sh $P-empty-r$r $W/example/gpu/kt3-empty ./kernel_trace "$V 1048576 200 256 0" 90 60 2 | grep client_rc
  $W/run.sh $P-sample-r$r $W/example/gpu/kt3-sample ./kernel_trace "$V 1048576 200 256 0" 90 60 2 | grep client_rc
done
echo "end=$(date +%T) cpu5=$(cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq) gpu=$(cat /sys/devices/platform/bus@0/17000000.gpu/devfreq/17000000.gpu/cur_freq)"
for k in noagent empty sample; do
  printf '%-8s' $k; for r in 1 2 3; do printf ' %s' "$(grep -o 'Median: *[0-9.]*' $W/runs/$P-$k-r$r/client.log | awk '{print $2}')"; done; echo
done
