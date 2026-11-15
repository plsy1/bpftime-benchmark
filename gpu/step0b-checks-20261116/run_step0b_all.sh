#!/bin/bash
# run_step0b_all.sh: one serial pass of the step-0 follow-up checks on a quiet board.
# Timed checks first (C3, C1, C2, C4), then C5's capture remedy and the audit's self-check.
# Records the board state before and after every step (clocks, load, busiest processes,
# apt, memory, /dev/shm). Timed steps wait (at most 40 min) for apt's daily services.
# Usage: timeout -k 30 7200 bash run_step0b_all.sh PASS_LABEL [step ...]
set -u
S=/home/jetson/src/bpftime-gpu-work/step0b
PASS=${1:?pass label}; shift
STAMP=$(date +%Y%m%d-%H%M%S)
LOG=$S/logs/main-$PASS-$STAMP.log
mkdir -p $S/logs
exec > >(tee -a "$LOG") 2>&1

state() {
  echo "  time $(date '+%F %T') cpu5=$(cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq)/$(cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_min_freq) gpu=$(cat /sys/devices/platform/bus@0/17000000.gpu/devfreq/17000000.gpu/cur_freq) load=$(cut -d' ' -f1-3 /proc/loadavg)"
  echo "  mem $(free -m | awk '/^Mem:/{print "used="$3" avail="$7}')  step0 shm: $(ls /dev/shm | grep -E '^(step0|bpftime_gpu_)' | tr '\n' ' ')"
  echo "  apt: $(for u in apt-daily.service apt-daily-upgrade.service; do printf '%s=%s ' $u "$(systemctl is-active $u)"; done) procs: $(pgrep -l -x 'apt|apt-get|apt.systemd.dai|unattended-upgr|dpkg' | tr '\n' ' ')"
  echo "  busiest:"; ps -eo pid,pcpu,etime,args --sort=-pcpu | grep -v -e 'stream-json' -e 'ps -eo' | sed -n '2,5p' | cut -c1-150 | sed 's/^/    /'
}
apt_busy() { systemctl is-active -q apt-daily.service || systemctl is-active -q apt-daily-upgrade.service || pgrep -x 'apt|apt-get|unattended-upgr|dpkg' > /dev/null; }
wait_apt() {
  local i=0
  while apt_busy && [ $i -lt 240 ]; do [ $i -eq 0 ] && echo "  waiting for apt to finish $(date +%T)"; sleep 10; i=$((i+1)); done
  [ $i -gt 0 ] && echo "  apt wait ended $(date +%T) after $((i*10)) s"
  return 0
}
run_step() {
  local name=$1 dir=$2; shift 2
  case $name in c3|c1|c2|c4) wait_apt ;; esac
  echo "===== STEP $name ($dir) start"
  state
  local t0=$(date +%s)
  ( cd "$S/$dir" && bash -c "$*" )
  local rc=$?
  echo "===== STEP $name rc=$rc wall=$(( $(date +%s) - t0 ))s"
  state
  sleep 3
}

STEPS="${*:-c3 c1 c2 c4 c5cap c5audit}"
echo "step0b pass $PASS $STAMP, steps: $STEPS"
for st in $STEPS; do
  case $st in
    c3)      run_step c3 c3-costparts './build.sh && ./run_all.sh 10' ;;
    c1)      run_step c1 c1-carry './run_all.sh' ;;
    c2)      run_step c2 c2-claim './run_all.sh' ;;
    c4)      run_step c4 c4-batch-fence './build.sh && ./run_all.sh full' ;;
    c5cap)   run_step c5cap c5-guarantees/capture-remedy "./build.sh && python3 -I selftest_analyze.py && timeout -k 5 300 ./run_all.sh OUT/$PASS-$STAMP 3" ;;
    c5audit) run_step c5audit c5-guarantees 'timeout -k 5 600 ./run_all.sh' ;;
    *) echo "unknown step $st" ;;
  esac
done
echo "step0b pass $PASS $STAMP done $(date '+%F %T')"
