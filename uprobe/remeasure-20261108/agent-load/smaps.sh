#!/usr/bin/env bash
# Per-mapping memory of a traced process (../aot-link/smaps_sum.py): burstloop (1 thread, a probed
# call every 100 ms for 20 s) with outattach + out-control under the syscall server (default
# 128 MB segment), the agent preloaded; and the same burstloop untraced. Read after 6 s.
set -u
N=/home/jetson/src/results/uprobe/remeasure-20261108/agent-load
B=/home/jetson/src/results/uprobe/ssl-nginx-2x2-20261023/builds/dev-9c088441-probecheck
SRC=/home/jetson/src/results/uprobe/output-dims-20261106/src
SUM=/home/jetson/src/results/uprobe/remeasure-20261108/aot-link/smaps_sum.py
sudo -n pkill -9 -x outattach; sudo -n pkill -9 -x burstloop; sudo -n $B/bpftimetool remove >/dev/null 2>&1
sudo -n env LD_PRELOAD=$B/libbpftime-syscall-server.so BPFTIME_LOG_OUTPUT=console timeout 60 \
  $SRC/outattach $SRC/out-control.bpf.o $SRC/burstloop on_count=count_event > $N/tool.log 2>&1 &
for i in $(seq 100); do grep -q attached $N/tool.log && break; sleep 0.2; done
sudo -n env LD_PRELOAD=$B/libbpftime-agent.so BPFTIME_LOG_OUTPUT=console timeout 40 $SRC/burstloop 1 20 1 100000 0 0 > $N/burst-traced.log 2>&1 &
sleep 6
P=$(pgrep -x burstloop | head -1); echo "traced pid $P"
sudo -n python3 -I $SUM $P > $N/smaps-traced.txt
sudo -n grep -E "^(VmRSS|RssAnon|RssFile|RssShmem)" /proc/$P/status >> $N/smaps-traced.txt
T=$(pgrep -x outattach | head -1)
sudo -n python3 -I $SUM $T > $N/smaps-tool.txt
sudo -n grep -E "^(VmRSS|RssAnon|RssFile|RssShmem)" /proc/$T/status >> $N/smaps-tool.txt
wait %2 2>/dev/null
sudo -n pkill -INT -x outattach; sleep 1; sudo -n pkill -9 -x outattach; sudo -n $B/bpftimetool remove >/dev/null 2>&1
timeout 40 $SRC/burstloop 1 20 1 100000 0 0 > $N/burst-plain.log 2>&1 &
sleep 6
P=$(pgrep -x burstloop | head -1); echo "plain pid $P"
python3 -I $SUM $P > $N/smaps-plain.txt
grep -E "^(VmRSS|RssAnon|RssFile|RssShmem)" /proc/$P/status >> $N/smaps-plain.txt
wait
