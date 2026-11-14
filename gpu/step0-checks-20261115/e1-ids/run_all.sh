#!/bin/bash
# run_all.sh [OUT]: check 1, all cases, then the analysis. OUT defaults to runs/<date-time>.
# Every GPU command has its own timeout; expected wall time about 25 s (worst case with every
# device cap firing: about 4 min; the step timeouts sum to 390 s).
set -u
D=$(cd "$(dirname "$0")" && pwd)
OUT=${1:-$D/runs/$(date +%Y%m%d-%H%M%S)}
[ -e "$OUT" ] && { echo "exists: $OUT"; exit 3; }
mkdir -p "$OUT"
cd "$D" || exit 2
make -s all > "$OUT/build.log" 2>&1 || { echo "build failed"; cat "$OUT/build.log"; exit 2; }
python3 -I check_sass.py build > "$OUT/check_sass.txt" 2>&1 || { echo "SASS check failed"; tail -3 "$OUT/check_sass.txt"; exit 2; }
cp build/ids.sass build/idmod.sass build/hog.sass build/idmod.ptx "$OUT/" 2>/dev/null
ls /dev/shm > "$OUT/shm_before.txt"
{
  date '+%F %T'; uname -r
  echo "gpu cur_freq: $(cat /sys/class/devfreq/17000000.gpu/cur_freq 2>/dev/null)"
  echo "cpu5 cur_freq: $(cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq 2>/dev/null)"
  echo "loadavg: $(cat /proc/loadavg)"
  echo "other processes holding the GPU (nvidia-smi):"; nvidia-smi --query-compute-apps=pid,name --format=csv,noheader 2>&1
  ps -eo pid,pcpu,etime,args --sort=-pcpu | head -8 | cut -c1-160
} > "$OUT/env.txt"
rc=0
# step LOG TIMEOUT CMD...: one bounded GPU step, output to OUT/LOG and the terminal
step() {
  local log=$1 t=$2; shift 2
  echo "== $*" | tee "$OUT/$log"
  timeout -k 5 "$t" "$@" >> "$OUT/$log" 2>&1
  local r=$?
  cat "$OUT/$log"
  [ $r != 0 ] && { echo "rc=$r: $*" | tee -a "$OUT/$log"; rc=1; }
  return 0
}
step a.log 60 ./build/ids a "$OUT/a"
step b.log 30 ./build/ids b "$OUT/b"
step c.log 30 ./build/ids c "$OUT/c"
step d.log 30 ./build/ids d "$OUT/d"
step e_ctl.log 70 ./run_e.sh "$OUT/e_ctl" ctl 3
step e_hog.log 100 ./run_e.sh "$OUT/e_hog" hog 3 6 50
step e_prio.log 70 ./run_e.sh "$OUT/e_prio" ctl 3 0 0 prio=10
python3 -I analyze.py "$OUT"
ls /dev/shm > "$OUT/shm_after.txt"
# CUDA may leave /dev/shm/cuda.shm.<uid hex>.<pid hex>.* behind when a process is killed:
# remove only entries whose pid is one of ours
pids=$(grep -ho '"pid": [0-9]*' "$OUT"/*/*_meta.json 2>/dev/null | awk '{print $2}'; grep -ho 'hog: pid [0-9]*' "$OUT"/*/hog.log 2>/dev/null | awk '{print $3}')
for p in $pids; do
  for f in /dev/shm/cuda.shm.$(printf '%x' "$(id -u)").$(printf '%x' "$p").*; do
    [ -e "$f" ] && { echo "removing leftover $f" | tee -a "$OUT/shm_cleanup.txt"; rm -f "$f"; }
  done
done
diff "$OUT/shm_before.txt" <(ls /dev/shm) > "$OUT/shm_diff.txt" && echo "shm: unchanged" || { echo "shm changed:"; cat "$OUT/shm_diff.txt"; }
exit $rc
