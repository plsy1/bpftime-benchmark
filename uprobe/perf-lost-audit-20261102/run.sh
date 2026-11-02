#!/bin/bash
# Re-audit of perf event loss (11/02): the NUC saw dev 6ab9cbfe report "lost N events"
# in 2 of 4 16-byte audits on x86 (a7d54d7e none). Same container, builds and CO-RE
# sslsniff as dev-68f19c1f-20261102/ssl-nginx; audit_lost.py keeps sslsniff's stderr.
# Leg order rotated every round. Run from the host.
set -u
R=/home/jetson/src/results/uprobe/perf-lost-audit-20261102
S=/home/jetson/src/results/uprobe/ssl-nginx-2x2-20261023
LEGS=(kernel a7d54d7e dev-6ab9cbfe dev-68f19c1f)
C=perf-lost-audit
uname -r > $R/kernel.txt
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $R/clock-before.txt
sudo -n docker run -d --name $C --privileged --pid=host --shm-size=1g -v /sys:/sys -v /lib/modules:/lib/modules:ro -v $R:/bench -v $S/builds:/builds:ro -v $R/sslsniff:/sslsniff:ro bpftime-ssl-nginx:20261023 sleep infinity
mkdir -p $R/out
rot() { local i=$1 n=${#LEGS[@]}; local o=(); for ((k=0;k<n;k++)); do o+=(${LEGS[$(((i+k)%n))]}); done; (IFS=,; echo "${o[*]}"); }
# set name, payload, conns, secs, rounds
run_set() {
  for ((r=0;r<$5;r++)); do
    sudo -n docker exec $C python3 /bench/audit_lost.py --payload $2 --conns $3 --secs $4 \
      --legs $(rot $r) --out /bench/out/$1-r$((r+1)).json >> $R/out/$1.log 2>&1
  done
}
run_set a16 16 10 3 8
run_set c16 16 100 10 3
run_set a256k 262144 10 3 3
sudo -n docker rm -f $C
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $R/clock-after.txt
date
