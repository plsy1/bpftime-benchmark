#!/usr/bin/env bash
# One batch of the official GPU benchmark on the Orin: run_batch.sh BUILD CONFIG TIMEOUT_S
#   BUILD official|candidate (runner view whose build/ is that tree's build-cuda)
# Checks before: time window, cpu5 and GPU clocks, leftovers, no build running, no bpftime segment.
# The timeout wraps the whole python runner (outside LD_PRELOAD); afterwards the batch's own
# segment is removed (fuser-checked) and leftovers checked again.
set -u
V=/home/jetson/src/bpftime-gpu-work/compare
B=$1; CFG=$2; TO=$3
SHM=$(python3 -c "import json,sys;print(json.load(open(sys.argv[1]))['global_env']['BPFTIME_GLOBAL_SHM_NAME'])" "$CFG")
PROCS='vec_add|vec_add_legacy|cuda_probe|kernelretsnoop|mem_trace|threadhist'
hm=$(date +%H%M)
if [ "$hm" -ge 0325 ] && [ "$hm" -le 0415 ]; then echo "ABORT: 03:25-04:15 window"; exit 2; fi
cpu=$(cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq)
gpu=$(cat /sys/class/devfreq/17000000.gpu/cur_freq 2>/dev/null || cat /sys/devices/platform/bus@0/17000000.gpu/devfreq/17000000.gpu/cur_freq)
[ "$cpu" = 1728000 ] || { echo "ABORT: cpu5 at $cpu"; exit 2; }
[ "$gpu" = 1020000000 ] || { echo "ABORT: GPU at $gpu"; exit 2; }
if pgrep -a -x "$PROCS" >/dev/null; then echo "ABORT: leftovers"; pgrep -a -x "$PROCS"; exit 2; fi
if pgrep -af run_cuda_bench.py | grep -v -F "$0" | grep -q python3; then echo "ABORT: another runner"; exit 2; fi
if pgrep -a -x 'cc1plus|cc1|ld|make|cmake|ninja|nvcc' >/dev/null; then echo "ABORT: build running"; pgrep -a -x 'cc1plus|cc1|ld|make|cmake|ninja|nvcc' | head -3; exit 2; fi
if ls /dev/shm | grep -q -E '^(bpftime_maps_shm|bpftime_cmp_)'; then echo "ABORT: bpftime segment present"; ls -la /dev/shm | grep bpftime; exit 2; fi
echo "start $(date '+%F %T') build=$B cfg=$(basename $CFG) cpu5=$cpu gpu=$gpu shm=$SHM"
cd $V/view/$B || exit 2
ulimit -c 0
timeout -k 30 "$TO" python3 benchmark/gpu/run_cuda_bench.py "$CFG" > "$CFG.stdout" 2>&1
rc=$?
echo "end $(date '+%F %T') rc=$rc cpu5=$(cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq)"
sleep 2
if pgrep -a -x "$PROCS"; then echo "LEFTOVERS after batch"; fi
if [ -e "/dev/shm/$SHM" ]; then
  fuser -s "/dev/shm/$SHM" && echo "segment $SHM in use" || { rm -f -- "/dev/shm/$SHM"; echo "removed leftover segment $SHM"; }
fi
ls /dev/shm | grep -E '^(bpftime|BPFTIME)' || echo shm-clean
exit $rc
