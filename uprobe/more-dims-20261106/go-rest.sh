#!/usr/bin/env bash
# Runs the remaining batches one after another (each bounded), after output-dims round 3:
# builds, output-dims round 4 and the profile, r8 gethostlatency, n2 start-up, n4 many probes,
# n3 nginx latency, n5 footprint. Steps and times in go-rest.log. The soak (n6) is separate.
set -u
M=/home/jetson/src/results/uprobe/more-dims-20261106
O=/home/jetson/src/results/uprobe/output-dims-20261106
SCRATCH=/tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad
L=$M/go-rest.log
step() { echo "$(date +%T) $*" >> $L; }
step wait-r3
timeout 1800 bash -c "until [ -f $O/r3-finished ]; do sleep 2; done"
step build
( $O/src/build.sh && $M/n2-startup/src/build.sh && $M/n5-footprint/src/build.sh ) >> $L 2>&1
step output-r4
timeout 3000 $O/go-r4.sh
step prof
cd $O && SCRATCH=$SCRATCH timeout 1800 python3 prof.py > $O/prof/prof.log 2>&1
step r8-gethostlatency
cd $O/r8-gethostlatency
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > clock-before.txt
timeout 900 python3 run.py --reps 3 --burst 50 --period-us 20 --pause-us 10000 --out burst50.jsonl --sendlog $SCRATCH/sendlog.txt > r8.log 2>&1
timeout 900 python3 run.py --reps 3 --burst 1 --period-us 20 --pause-us 10000 --out single.jsonl --sendlog $SCRATCH/sendlog.txt >> r8.log 2>&1
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > clock-after.txt
step n2-startup
cd $M/n2-startup
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > clock-before.txt
timeout 1800 python3 run.py --out startup.jsonl > startup.log 2>&1
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > clock-after.txt
step n4-many-probes
cd $M/n4-many-probes
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > clock-before.txt
timeout 2400 python3 run.py --out many.jsonl > many.log 2>&1
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > clock-after.txt
step n3-tail
timeout 2700 $M/n3-tail/go.sh
step n5-footprint
cd $M/n5-footprint
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > clock-before.txt
timeout 2400 python3 run.py --out footprint.jsonl > footprint.log 2>&1
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > clock-after.txt
step done
