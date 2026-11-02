#!/usr/bin/env bash
# Per-CPU hash phase 2 gate (11/02): shared counters (per-CPU hash, hash) at 1-6 threads with
# the kernel; churn and tracking table (hash: must be unchanged); the suite's userspace cases.
# dev-e1677f0d = dev 68f19c1f + fbcc1a04, a73f6753, e1677f0d.
set -u
D=/home/jetson/src/results/uprobe/percpu-hash-lockfree-20261102/gate
U=/home/jetson/src/results/uprobe
B=dev-68f19c1f,dev-e1677f0d
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $D/clock-before.txt
date > $D/started.txt
python3 $U/map-table-20261028/run_count.py --backends kernel,$B \
  --objs count-hash.bpf.o,count-percpu-hash.bpf.o,count-control.bpf.o \
  --threads 1,2,4,6 --nkeys 1,64 --reps 3 --out $D/count.jsonl > $D/count.log 2>&1
python3 $U/map-threads-20261028/run_churn.py --backends $B --objs churn-k4-v8.bpf.o,control.bpf.o \
  --threads 1,6 --reps 3 --out $D/churn.jsonl > $D/churn.log 2>&1
python3 $U/map-table-20261028/run_track.py --backends $B --objs track.bpf.o,track-control.bpf.o \
  --threads 1,6 --live 1000,900000 --reps 3 --out $D/track.jsonl > $D/track.log 2>&1
sudo -n taskset -c 5 python3 $U/dev-6ab9cbfe-20261101/hash-bisect/run_hash.py \
  --builds dev-68f19c1f,a7d54d7e,dev-e1677f0d --reps 3 --iters 50000 --out $D/suite.jsonl > $D/suite.log 2>&1
sudo -n chown -R jetson:jetson $D
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq > $D/clock-after.txt
date > $D/finished.txt
