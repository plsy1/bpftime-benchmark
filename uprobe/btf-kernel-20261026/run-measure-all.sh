#!/bin/bash
# BTF-kernel baselines (10/26), strictly one after another: real-tool throughput (8 tools,
# none/kernel/official/a7d54d7e, 5 rounds), ssl-nginx fair round, complete ten-round uprobe
# suite with the kernel arm.
set -u
R=/home/jetson/src/results/uprobe/btf-kernel-20261026
H=/home/jetson/src/realprog-20261026-btf/harness
python3 -I - <<'PY'
from pathlib import Path
p=Path('/sys/devices/system/cpu/cpu5/cpufreq')
assert all(int((p/f'scaling_{k}_freq').read_text())==1728000 for k in ('min','max','cur')), 'run jetson_clocks'
PY
[ $? -eq 0 ] || exit 1
date; uname -r
mkdir -p $R/realprog-measure && cd $R/realprog-measure && python3 $H/measure.py --out measure.jsonl > measure.log 2>&1
echo "measure exit $?"; date
bash $R/ssl-nginx/run.sh > $R/ssl-nginx/run.console.log 2>&1
echo "ssl-nginx exit $?"; date
bash $R/full10-a7d54d7e-kernel/run-full.sh > $R/full10-a7d54d7e-kernel/full-console.log 2>&1
echo "full10 exit $?"; date
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq
