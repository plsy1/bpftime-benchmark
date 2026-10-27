#!/bin/bash
# DeepFlow variants with goroutines_map/proc_info_map as array-backed tables
# (eunomia-bpf/deepflow style): A without BPF_F_MMAPABLE (as the paper's fork),
# B with it. Legs no-uprobe, kernel-uprobe, user-uprobe; no-probe is in run2.
set -u
B=/home/jetson/src/results/uprobe/deepflow-btf-20261026/bench
T=/home/jetson/src/deepflow-20261026/deepflow-v7.2.3-arraytbl/agent/src/ebpf/samples/rust/socket-tracer/target/release/socket_tracer
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq
cd $B && python3 run_deepflow.py --legs no-uprobe,kernel-uprobe,user-uprobe --tracer $T --out run-arraytbl-A > run-arraytbl-A.log 2>&1
echo "A exit $?"; date
cd $B && python3 run_deepflow.py --legs no-uprobe,kernel-uprobe,user-uprobe --tracer $T --tracer-env DF_TABLES_MMAPABLE=1 --out run-arraytbl-B > run-arraytbl-B.log 2>&1
echo "B exit $?"; date
cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq
