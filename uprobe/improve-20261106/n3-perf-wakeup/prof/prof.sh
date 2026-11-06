#!/usr/bin/env bash
# Where per-sample perf wakeups cost ssl-nginx its throughput (improve-20261106 item 3):
# one 10 s wrk run per leg (kernel, a48f6777 = half-shard rule, f5b388cd = every sample), and
# system-wide on the host while wrk runs: a cycles profile with call graphs (4 s), then
# sched_switch/sched_wakeup (3 s). perf.data in the scratchpad; reports here.
set -u
R=/home/jetson/src/results/uprobe/improve-20261106/n3-perf-wakeup
P=$R/prof
SCR=/tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/perfwake
S=/home/jetson/src/results/uprobe/ssl-nginx-2x2-20261023
RT=/home/jetson/src/results/uprobe/realtools-20261104/ssl-nginx
PERF=/usr/lib/linux-tools/6.8.0-1020-nvidia-tegra/perf
LEGS="kernel dev-a48f6777-probecheck dev-f5b388cd-probecheck"
mkdir -p $SCR
cd $P
rm -f start-*
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > clock-before.txt
date +%T > started
sudo -n docker rm -f imp-prof > /dev/null 2>&1
sudo -n docker run -d --name imp-prof --privileged --pid=host --shm-size=1g -v /sys:/sys -v /lib/modules:/lib/modules:ro -v $R:/bench -v $S/builds:/builds:ro -v $RT/sslsniff:/sslsniff:ro bpftime-ssl-nginx:20261023 sleep infinity > container-id.txt
(timeout 600 sudo -n docker exec imp-prof python3 /bench/prof/prof_fair.py --payload 16 --reps 1 --legs ${LEGS// /,} --out /bench/prof/ab-prof.json > ab-prof.log 2>&1) &
EXEC=$!
for leg in $LEGS; do
  for i in $(seq 1 600); do [ -e start-$leg ] && break; sleep 0.2; done
  [ -e start-$leg ] || { echo "no start for $leg"; break; }
  cat start-$leg > pids-$leg.txt
  sleep 1
  sudo -n $PERF record -a -g -F 999 -o $SCR/cycles-$leg.data -- sleep 4 > /dev/null 2>&1
  sudo -n $PERF record -a -e sched:sched_switch -e sched:sched_wakeup -o $SCR/sched-$leg.data -- sleep 3 > /dev/null 2>&1
  for i in $(seq 1 100); do [ -e start-$leg ] || break; sleep 0.2; done
done
wait $EXEC
sudo -n docker rm -f imp-prof > /dev/null 2>&1
date +%T > finished
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > clock-after.txt
for leg in $LEGS; do
  sudo -n $PERF report -i $SCR/cycles-$leg.data --no-children --sort comm --stdio 2>/dev/null | grep -v "^#" | grep -v "^$" | head -15 > comm-$leg.txt
  sudo -n $PERF report -i $SCR/cycles-$leg.data --no-children --comm nginx --sort sym --stdio -g none 2>/dev/null | grep -v "^#" | grep -v "^$" | head -40 > nginx-sym-$leg.txt
  sudo -n $PERF report -i $SCR/cycles-$leg.data --no-children --comm sslsniff --sort sym --stdio -g none 2>/dev/null | grep -v "^#" | grep -v "^$" | head -40 > sslsniff-sym-$leg.txt
  sudo -n $PERF script -i $SCR/sched-$leg.data -F comm,cpu,event 2>/dev/null > $SCR/sched-$leg.txt
  awk '{c[$1" "$3]++} END {for (k in c) print c[k], k}' $SCR/sched-$leg.txt | sort -rn | head -20 > sched-$leg.txt
  awk '$3 ~ /switch/ {c[$1" "$2]++} END {for (k in c) print c[k], k}' $SCR/sched-$leg.txt | sort -rn | head -30 > sched-cpu-$leg.txt
done
echo done
