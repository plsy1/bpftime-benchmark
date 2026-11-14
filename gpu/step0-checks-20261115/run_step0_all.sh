#!/bin/bash
# run_step0_all.sh: the real step-0 runs, one check after another, on a quiet board.
# Timed checks first (2, 6, 3 and 6's llama variant), then the correctness checks.
# Each step runs its own script with its own timeout; this script records the board
# state before and after each step (clocks, load, busiest processes, apt activity,
# memory, /dev/shm) so overlaps can be found afterwards.
# Usage: timeout -k 30 12600 bash run_step0_all.sh [step ...]   (default: all steps)
set -u
S=/home/jetson/src/bpftime-gpu-work/step0
STAMP=$(date +%Y%m%d-%H%M%S)
LOG=$S/logs/main-$STAMP.log
mkdir -p $S/logs
exec > >(tee -a "$LOG") 2>&1

state() {
  echo "  time $(date '+%F %T') cpu5=$(cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq)/$(cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_min_freq) gpu=$(cat /sys/devices/platform/bus@0/17000000.gpu/devfreq/17000000.gpu/cur_freq) load=$(cut -d' ' -f1-3 /proc/loadavg)"
  echo "  mem $(free -m | awk '/^Mem:/{print "used="$3" avail="$7}')  shm_entries=$(ls /dev/shm | wc -l) step0/bpftime_gpu shm: $(ls /dev/shm | grep -E '^(step0_|bpftime_gpu_)' | tr '\n' ' ')"
  echo "  apt: $(pgrep -a -f 'apt|unattended-upgr|dpkg' | grep -v pgrep | cut -c1-120 | tr '\n' ';')"
  echo "  busiest:"; ps -eo pid,pcpu,etime,args --sort=-pcpu | grep -v -e 'stream-json' -e 'ps -eo' | sed -n '2,5p' | cut -c1-150 | sed 's/^/    /'
}

run_step() {
  local name=$1 dir=$2; shift 2
  echo "===== STEP $name ($dir) start"
  state
  local t0=$(date +%s)
  ( cd "$S/$dir" && bash -c "$*" )
  local rc=$?
  echo "===== STEP $name rc=$rc wall=$(( $(date +%s) - t0 ))s"
  state
  sleep 3
}

want() { [ $# -eq 0 ] && return 0; return 1; }
STEPS="${*:-e2 e6a e6b e6c e3 e6llama e1 e7warp e5 e8 e4 e7llama e9}"
echo "step0 main run $STAMP, steps: $STEPS"
for st in $STEPS; do
  case $st in
    e2)      run_step e2 e2-cost './build.sh && ./run_all.sh 10' ;;
    e6a)     run_step e6a e6-drain 'timeout -k 5 300 ./run_a.sh' ;;
    e6b)     run_step e6b e6-drain 'timeout -k 5 400 ./run_b.sh' ;;
    e6c)     run_step e6c e6-drain 'timeout -k 5 400 ./run_c.sh' ;;
    e3)      run_step e3 e3-copyout './build.sh && timeout -k 10 1800 ./run_all.sh' ;;
    e6llama) run_step e6llama e6-drain 'timeout -k 5 900 ./run_b_llama.sh' ;;
    e1)      run_step e1 e1-ids 'timeout -k 5 420 ./run_all.sh' ;;
    e7warp)  run_step e7warp e7-warp 'timeout -k 5 140 ./run_all.sh' ;;
    e5)      run_step e5 e5-atomics 'timeout -k 5 120 ./compile_only.sh && timeout -k 5 600 ./run_all.sh' ;;
    e8)      run_step e8 e8-package 'timeout -k 5 300 ./get_llama_ptx.sh && timeout -k 5 600 ./run_all.sh' ;;
    e4)      run_step e4 e4-publish './build.sh && ./run_all.sh full' ;;
    e7llama) run_step e7llama e7-llama 'timeout -k 10 3600 bash run_all.sh' ;;
    e9)      run_step e9 e9-hooks 'timeout -k 10 1800 bash run_all.sh' ;;
    *) echo "unknown step $st" ;;
  esac
done
echo "step0 main run $STAMP done $(date '+%F %T')"
