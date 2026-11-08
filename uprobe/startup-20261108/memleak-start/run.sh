#!/usr/bin/env bash
# Start-up of a process traced by libbpf-tools memleak (uprobe mode, -c), as
# remeasure-20261108/memleak-start: memleak forks /home/jetson/src/rp/stw (stampwrap, static),
# which stamps CLOCK_MONOTONIC and execs stampmain with LD_PRELOAD=$SW_PRELOAD; stampmain stamps
# again on entering main. main - exec = the traced process's start (agent: shared memory, its
# programs, attaching). exec - launch = the tool's set-up until it starts the command (opening
# and loading memleak's programs: with the AOT cache, the server compiles them there).
# usage: run.sh NEW_BUILD OLD_BUILD [REPS]
# cells: kernel; old (dev before the cache); new; new with BPFTIME_AOT_CACHE=0 (tool side).
set -u
N=/home/jetson/src/results/uprobe/startup-20261108/memleak-start
BD=/home/jetson/src/results/uprobe/ssl-nginx-2x2-20261023/builds
NEW=$1; OLD=$2; REPS=${3:-10}
ML=/home/jetson/src/realprog-20261026-btf/libbpf-tools/memleak
# stampmain built from remeasure-20261108/memleak-start/src/stampmain.c (stw, its static
# wrapper, is /home/jetson/src/rp/stw, from src/stampwrap.c there).
SM=$(mktemp -d)/stampmain
gcc-13 -O2 -g -o $SM /home/jetson/src/results/uprobe/remeasure-20261108/memleak-start/src/stampmain.c || exit 1
for i in $(seq $REPS); do
  for cell in kernel old new new-noaot; do
    S=$N/stamp-$cell-$i.txt; rm -f $S
    case $cell in
      kernel) pre=(); swp=; B= ;;
      old) B=$BD/$OLD; pre=(LD_PRELOAD=$B/libbpftime-syscall-server.so BPFTIME_LOG_OUTPUT=console BPFTIME_SHM_MEMORY_MB=1024); swp=$B/libbpftime-agent.so ;;
      new) B=$BD/$NEW; pre=(LD_PRELOAD=$B/libbpftime-syscall-server.so BPFTIME_LOG_OUTPUT=console BPFTIME_SHM_MEMORY_MB=1024); swp=$B/libbpftime-agent.so ;;
      new-noaot) B=$BD/$NEW; pre=(LD_PRELOAD=$B/libbpftime-syscall-server.so BPFTIME_LOG_OUTPUT=console BPFTIME_SHM_MEMORY_MB=1024 BPFTIME_AOT_CACHE=0); swp=$B/libbpftime-agent.so ;;
    esac
    t0=$(python3 -c 'import time; print(time.clock_gettime_ns(time.CLOCK_MONOTONIC))')
    timeout 90 sudo -n env "${pre[@]}" SW_PRELOAD=$swp SW_CMD=$SM SW_ARGS= STAMP_FILE=$S \
      $ML -c /home/jetson/src/rp/stw 1 > $N/out-$cell-$i.log 2>&1
    rc=$?
    [ -n "$B" ] && sudo -n $B/bpftimetool remove > /dev/null 2>&1
    e=$(awk '/^exec/{print $2}' $S 2>/dev/null); m=$(awk '/^main/{print $2}' $S 2>/dev/null)
    if [ -n "$e" ] && [ -n "$m" ]; then
      echo "$cell rep $i rc $rc start_us $(( (m - e) / 1000 )) tool_setup_us $(( (e - t0) / 1000 ))"
    else echo "$cell rep $i rc $rc no-stamp"; fi
  done
done
