#!/bin/bash
# Gap runs asked for by the verification critic (2026-11-09): static cudart victims,
# device globals next to a probed kernel, SM auto-detection. Sequential, every run bounded by run.sh.
set -u
W=/home/jetson/src/bpftime-gpu-work
V=$W/benchmark/gpu/workload
K=$W/example/gpu/kernelretsnoop
E=$W/example/gpu/kt3-empty
export SRV_ENV='BPFTIME_SHM_MEMORY_MB=64 BPFTIME_MAP_GPU_THREAD_COUNT=16'
echo "start $(date +%T) cpu5=$(cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq)"
SPDLOG_LEVEL=debug $W/run.sh krs-static-legacy $K ./kernelretsnoop "$V/vec_add_legacy_static 5 3 5 1" 90 60 4
SPDLOG_LEVEL=debug $W/run.sh krs-static-default $K ./kernelretsnoop "$V/vec_add_static 5 3 5 1" 90 60 4
SPDLOG_LEVEL=info  $W/run.sh krs-static-legacy-info $K ./kernelretsnoop "$V/vec_add_legacy_static 5 3 5 1" 90 60 4
NO_AGENT=1 SPDLOG_LEVEL=info $W/run.sh glob-noagent $E ./kernel_trace "$W/gaps/globals_legacy" 90 60 3
SPDLOG_LEVEL=debug $W/run.sh glob-agent $E ./kernel_trace "$W/gaps/globals_legacy" 90 60 3
NO_SMARCH=1 SPDLOG_LEVEL=debug $W/run.sh krs-autodetect $K ./kernelretsnoop "$V/vec_add_legacy 5 3 5 1" 90 60 4
echo "end $(date +%T)"
