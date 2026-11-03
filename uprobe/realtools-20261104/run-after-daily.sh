#!/bin/bash
# 11/04 after the daily job: real-tool validation of the chained hash map (dev 7ad5a152) against
# the kernel and dev a4ac0d31 (before the redesign). Step 2: single-thread throughput of the 8
# tools of measure.py (5 rounds x 10 s); step 3: the multi-thread workloads (workloads-mt, 1/2/4/6
# threads, 5 tools, 3 rounds x 10 s); step 4: ssl-nginx fair round (2 x 10 reps, 16 B and 256 KB).
# One stage at a time; each bounded; cpu5 1728000 and no leftovers before.
set -u
N=/home/jetson/src/results/uprobe/realtools-20261104
H=/home/jetson/src/realprog-20261026-btf/harness
# Wait for the daily job (timer ~03:32, ~40 min) to be over, at most until 05:30.
while :; do
  now=$(date +%H%M)
  active=$(systemctl is-active bpftime-jetson-daily.service 2>/dev/null)
  if [ "$now" -ge 0412 ] && [ "$now" -lt 0900 ] && [ "$active" != active ] && [ "$active" != activating ]; then break; fi
  if [ "$now" -ge 0530 ] && [ "$now" -lt 0900 ]; then echo "daily job still running at 05:30, giving up"; exit 1; fi
  sleep 30
done
date; uname -r
check() {
  python3 -I - <<'PY'
from pathlib import Path
p=Path('/sys/devices/system/cpu/cpu5/cpufreq')
assert all(int((p/f'scaling_{k}_freq').read_text())==1728000 for k in ('min','max','cur')), 'run jetson_clocks'
PY
  [ $? -eq 0 ] || exit 1
  pgrep -af 'keyloop|trackloop|countloop|churnloop|benchmark.py|callloop|mallocloop|nginx|wrk' | grep -v pgrep && { echo leftovers; exit 1; }
  ls -la /dev/shm | awk '$3=="root"' | grep -v -e nvsciipc_init -e ' \.$' -e ' \.\.$'
}
check
mkdir -p $N/measure $N/measure-mt
cd $H && timeout 4200 python3 measure.py --backends none,kernel,dev-a4ac0d31,dev-7ad5a152 --out $N/measure/measure.jsonl > $N/measure/measure.log 2>&1
echo "measure exit $?"; date
check
cd $H && timeout 7200 python3 measure_mt.py --out $N/measure-mt/measure.jsonl > $N/measure-mt/measure.log 2>&1
echo "measure-mt exit $?"; date
check
timeout 5400 bash $N/ssl-nginx/run.sh > $N/ssl-nginx/run.console.log 2>&1
echo "ssl-nginx exit $?"; date
sudo -n docker rm -f ssl-rt 2>/dev/null
sudo -n chown -R jetson:jetson $N
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq
