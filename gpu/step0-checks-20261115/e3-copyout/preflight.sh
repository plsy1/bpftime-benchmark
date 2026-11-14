#!/bin/bash
# preflight.sh OUTDIR: read-only checks before a measurement batch.
# Records clocks, possible GPU users and /dev/shm leftovers to OUTDIR/preflight.txt.
# Exit 1 if the clocks are not the census settings (cpu5 1728000 kHz, GPU
# 1020000000 Hz) or step0_* shm segments or a running llama-bench, bpftime,
# nsys or check-3 binary exist, unless ALLOW_DIRTY=1. Changes nothing.
set -u
O=${1:?usage: preflight.sh OUTDIR}
mkdir -p "$O"
F=$O/preflight.txt
cpu5=$(cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq 2>/dev/null || echo NA)
gpu=$(cat /sys/devices/platform/bus@0/17000000.gpu/devfreq/17000000.gpu/cur_freq 2>/dev/null || echo NA)
bad=0
{
  echo "date $(date '+%F %T %Z')"
  echo "uptime $(uptime)"
  echo "cpu5_khz $cpu5 (want 1728000)"
  echo "gpu_hz $gpu (want 1020000000)"
  [ "$cpu5" = 1728000 ] || { echo "WARN cpu5 clock"; bad=1; }
  [ "$gpu" = 1020000000 ] || { echo "WARN gpu clock"; bad=1; }
  # by process name (comm), so shells whose command line merely mentions a
  # path such as bpftime-gpu-work do not count
  others=$(ps -eo pid=,comm=,args= | awk '$2 ~ /^(llama-bench|llama-cli|llama-server|nsys|copyorder|captest|vmmexport|bpftime.*|victim.*|threadhist.*|kernelretsnoop)$/' || true)
  echo "possible GPU users:"; echo "${others:-  none}"
  [ -z "$others" ] || { echo "WARN processes above"; bad=1; }
  shm=$(ls /dev/shm | grep '^step0_' || true)
  echo "step0 shm: ${shm:-none}"
  [ -z "$shm" ] || { echo "WARN step0 shm leftovers"; bad=1; }
  echo "top cpu:"; ps -eo pid,etime,pcpu,comm --sort=-pcpu | head -8
  echo "free:"; free -m
} > "$F" 2>&1
cat "$F"
if [ "$bad" = 1 ] && [ "${ALLOW_DIRTY:-0}" != 1 ]; then
  echo "preflight: not clean (set ALLOW_DIRTY=1 to run anyway)"; exit 1
fi
exit 0
