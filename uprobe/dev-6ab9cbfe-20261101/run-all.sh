#!/bin/bash
# 11/01: ssl-nginx fair round, then the complete ten-round uprobe suite, one after another.
set -u
N=/home/jetson/src/results/uprobe/dev-6ab9cbfe-20261101
python3 -I - <<'PY'
from pathlib import Path
p=Path('/sys/devices/system/cpu/cpu5/cpufreq')
assert all(int((p/f'scaling_{k}_freq').read_text())==1728000 for k in ('min','max','cur')), 'run jetson_clocks'
PY
[ $? -eq 0 ] || exit 1
date; uname -r
timeout 4800 bash $N/ssl-nginx/run.sh > $N/ssl-nginx/run.console.log 2>&1
echo "ssl-nginx exit $?"; date
sudo -n docker rm -f ssl-6ab9 >/dev/null 2>&1
timeout 3600 bash $N/full10-6ab9cbfe/run-full.sh > $N/full10-6ab9cbfe/full-console.log 2>&1
echo "full10 exit $?"; date
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq
