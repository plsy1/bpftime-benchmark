#!/usr/bin/env bash
# The consumer's CPU time under ssl-nginx (improve-20261106 item 3): one 10 s wrk per leg
# (prof_fair.py's hook), sslsniff's and nginx worker's utime+stime over 5 s while wrk runs.
set -u
R=/home/jetson/src/results/uprobe/improve-20261106/n3-perf-wakeup
P=$R/prof
S=/home/jetson/src/results/uprobe/ssl-nginx-2x2-20261023
RT=/home/jetson/src/results/uprobe/realtools-20261104/ssl-nginx
LEGS=${LEGS:-"kernel dev-a48f6777-probecheck dev-0f9c3654-probecheck exp-0f9c3654-poll2"}
OUT=${OUT:-cpu.txt}
TICK=$(getconf CLK_TCK)
cd $P
rm -f start-*
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > clock-before-cpu.txt
sudo -n docker rm -f imp-cpu > /dev/null 2>&1
sudo -n docker run -d --name imp-cpu --privileged --pid=host --shm-size=1g -v /sys:/sys -v /lib/modules:/lib/modules:ro -v $R:/bench -v $S/builds:/builds:ro -v $RT/sslsniff:/sslsniff:ro bpftime-ssl-nginx:20261023 sleep infinity > /dev/null
(timeout 600 sudo -n docker exec imp-cpu python3 /bench/prof/prof_fair.py --payload 16 --reps 1 --legs ${LEGS// /,} --out /bench/prof/ab-cpu.json > ab-cpu.log 2>&1) &
EXEC=$!
cpu() { sudo -n cat /proc/$1/stat 2>/dev/null | awk '{print $14+$15}'; }
: > $OUT
for leg in $LEGS; do
  for i in $(seq 1 600); do [ -e start-$leg ] && break; sleep 0.2; done
  [ -e start-$leg ] || { echo "no start for $leg"; break; }
  sniff=$(sed -n 's/.*sniff=\([0-9]*\).*/\1/p' start-$leg)
  master=$(sed -n 's/.*nginx_master=\([0-9]*\).*/\1/p' start-$leg)
  worker=$(pgrep -P $master -x nginx | head -1)
  sleep 2
  s0=$(cpu $sniff); w0=$(cpu $worker); t0=$(date +%s.%N)
  sleep 5
  s1=$(cpu $sniff); w1=$(cpu $worker); t1=$(date +%s.%N)
  python3 -c "import sys; s0,s1,w0,w1,t0,t1,tick=map(float,sys.argv[1:8]); dt=t1-t0; print(f'$leg sslsniff {(s1-s0)/tick/dt*100:.0f}% of a core, nginx worker {(w1-w0)/tick/dt*100:.0f}%')" $s0 $s1 $w0 $w1 $t0 $t1 $TICK | tee -a $OUT
  for i in $(seq 1 100); do [ -e start-$leg ] || break; sleep 0.2; done
done
wait $EXEC
sudo -n docker rm -f imp-cpu > /dev/null 2>&1
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > clock-after-cpu.txt
echo done
