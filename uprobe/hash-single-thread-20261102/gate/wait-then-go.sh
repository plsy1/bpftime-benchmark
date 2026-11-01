#!/bin/bash
# Start the gate only after today's daily job has run and finished.
D=/home/jetson/src/results/uprobe/hash-single-thread-20261102/gate
for i in $(seq 1 360); do
  last=$(systemctl --user show bpftime-jetson-daily.service -p ExecMainExitTimestamp --value)
  state=$(systemctl --user show bpftime-jetson-daily.service -p ActiveState --value)
  if [ -n "$last" ] && [ "$(date -d "$last" +%s)" -gt "$(date -d 'today 03:00' +%s)" ] && [ "$state" = inactive ]; then
    echo "daily finished at $last"; sleep 30; break
  fi
  sleep 20
done
[ "$(cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq)" = 1728000 ] || { echo "clock not fixed"; exit 1; }
pgrep -af "run_churn|run_track|run_count|run_hash|benchmark/test" | grep -v pgrep && { echo "leftovers"; exit 1; }
bash $D/go.sh
