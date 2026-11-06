#!/usr/bin/env bash
# improve-20261106 investigations (only measured; nothing fixed here):
# ringbuf-tail, item3-ab: two questions the final batch raised (see below).
# gap3: ssl-nginx over frozen builds (checks off) between dev-7ad5a152 (11-04) and fix-final,
#   to place the 3% fix-final lost (more-dims n3-tail ab), 5 reps.
# oldsniff: the same ssl-nginx legs with ssl-nginx-2x2's sslsniff (built before the BTF kernel,
#   .BTF.ext removed) and with realtools' CO-RE sslsniff, both on the bridge network: audit
#   (events per request) and 3 reps of throughput each.
set -u
I=/home/jetson/src/results/uprobe/improve-20261106/investigate
S=/home/jetson/src/results/uprobe/ssl-nginx-2x2-20261023
NEW=/home/jetson/src/results/uprobe/realtools-20261104/ssl-nginx/sslsniff
OLD=$S/sslsniff
L=$I/go.log
step() { echo "$(date +%T) $1" >> $L; }
run_in() { # dir sslsniff-dir name legs reps [audit]
  sudo -n docker rm -f imp-inv > /dev/null 2>&1
  sudo -n docker run -d --name imp-inv --privileged --pid=host --shm-size=1g -v /sys:/sys -v /lib/modules:/lib/modules:ro -v $1:/bench -v $S/builds:/builds:ro -v $2:/sslsniff:ro bpftime-ssl-nginx:20261023 sleep infinity > $1/container-id.txt
  if [ "${6:-}" = audit ]; then
    timeout 900 sudo -n docker exec imp-inv python3 /bench/run_fair.py --payload 16 --audit --legs $4 --out /bench/audit-16.json > $1/audit-16.log 2>&1
  fi
  timeout 2400 sudo -n docker exec imp-inv python3 /bench/run_fair.py --payload 16 --reps $5 --legs $4 --out /bench/$3.json > $1/$3.log 2>&1
  sudo -n docker rm -f imp-inv > /dev/null 2>&1
}
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $I/clock-before.txt
step ringbuf-tail
# The final batch showed 300-400 us tails for 4-thread ring buffer bursts in one of two reps
# under 3f8981e4 (sizes that never pad too); more reps, with the builds around it.
(cd /home/jetson/src/results/uprobe/output-dims-20261106 && timeout 2400 python3 run.py --backends fix-final-probecheck,dev-0f9c3654-probecheck,exp-0f9c3654-nap3,dev-3f8981e4-probecheck --objs out-rbres-248.bpf.o,out-rbres-8184.bpf.o --threads 4 --reps 6 --secs 4 --workload burst --burst 100 --pause-us 10000 --period-ns 4000 --latency --out $I/ringbuf-tail.jsonl > $I/ringbuf-tail.log 2>&1)
step item3-ab
# The final batch: 3f8981e4 3.2% below fix-final-probecheck on ssl-nginx; the builds between.
run_in $I/item3-ab $NEW ab-16 kernel,fix-final-probecheck,dev-a48f6777-probecheck,dev-0f9c3654-probecheck,exp-0f9c3654-nap3,dev-3f8981e4-probecheck 5
step gap3
run_in $I/gap3 $NEW ab-16 kernel,dev-7ad5a152,dev-25f4134f,dev-6a15a271,dev-74d0ab8b,dev-b60b99c3,fix-rbwake2,fix-final 5
step oldsniff-old
run_in $I/oldsniff-old $OLD ab-16 kernel,fix-final,dev-3f8981e4-probecheck 3 audit
step oldsniff-new
run_in $I/oldsniff-new $NEW ab-16 kernel,fix-final,dev-3f8981e4-probecheck 3 audit
step startup
(cd $I/startup && timeout 1800 python3 breakdown.py > breakdown.log 2>&1)
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $I/clock-after.txt
step done
