#!/usr/bin/env bash
# Start-up of a process traced by libbpf-tools memleak (uprobe mode, -c): memleak forks
# /home/jetson/src/rp/stw (src/stampwrap.c, static), which stamps CLOCK_MONOTONIC and execs
# src/stampmain with LD_PRELOAD=$SW_PRELOAD; stampmain stamps again on entering main.
# main - exec = exec + ld.so + (bpftime) the agent: shared memory, compiling memleak's programs,
# attaching. Cells: kernel; bpftime tool + agent; bpftime tool, child without the agent.
set -u
N=/home/jetson/src/results/uprobe/remeasure-20261108/memleak-start
B=/home/jetson/src/results/uprobe/ssl-nginx-2x2-20261023/builds/dev-9c088441-probecheck
ML=/home/jetson/src/realprog-20261026-btf/libbpf-tools/memleak
REPS=${1:-10}
for i in $(seq $REPS); do
  for cell in kernel bpftime bpftime-noagent; do
    S=$N/stamp-$cell-$i.txt; rm -f $S
    case $cell in
      kernel) pre=(); swp= ;;
      bpftime) pre=(LD_PRELOAD=$B/libbpftime-syscall-server.so BPFTIME_LOG_OUTPUT=console BPFTIME_SHM_MEMORY_MB=1024); swp=$B/libbpftime-agent.so ;;
      bpftime-noagent) pre=(LD_PRELOAD=$B/libbpftime-syscall-server.so BPFTIME_LOG_OUTPUT=console BPFTIME_SHM_MEMORY_MB=1024); swp= ;;
    esac
    timeout 90 sudo -n env "${pre[@]}" SW_PRELOAD=$swp SW_CMD=$N/src/stampmain SW_ARGS= STAMP_FILE=$S \
      $ML -c /home/jetson/src/rp/stw 1 > $N/out-$cell-$i.log 2>&1
    rc=$?
    sudo -n $B/bpftimetool remove > /dev/null 2>&1
    e=$(awk '/^exec/{print $2}' $S 2>/dev/null); m=$(awk '/^main/{print $2}' $S 2>/dev/null)
    if [ -n "$e" ] && [ -n "$m" ]; then echo "$cell rep $i rc $rc start_ms $(( (m - e) / 1000 ))e-3"; else echo "$cell rep $i rc $rc no-stamp"; fi
  done
done
