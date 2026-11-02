#!/bin/bash
# One process per key pattern (bounded), 100k max_entries, churn 2 x max_entries at each load.
cd "$(dirname "$0")"
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > clock-before.txt
for i in $(seq 0 11); do
  timeout 600 taskset -c 5 ./structure 100000 $i 2 >> structure.jsonl 2>> structure.stderr
  echo "pattern $i rc=$?" >> structure.stderr
  rm -f /dev/shm/bpftime_hash_structure_*
done
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > clock-after.txt
echo done > finished
