#!/usr/bin/env bash
# Start-up of a process traced by libbpf-tools memleak (uprobe mode, -c), as
# remeasure-20261108/memleak-start: memleak forks /home/jetson/src/rp/stw (stampwrap, static),
# which stamps CLOCK_MONOTONIC and execs stampmain with LD_PRELOAD=$SW_PRELOAD; stampmain stamps
# again on entering main. main - exec = the traced process's start (agent: shared memory, its
# programs, attaching). exec - launch = the tool's set-up until it starts the command (opening
# and loading memleak's programs: with the AOT cache, the server compiles them there).
#
# Paper campaign copy (prep/E13) of startup-20261108/memleak-start/run.sh. Changed: cells are named
# on the command line (kernel, <arm>, <arm>:noaot) instead of the fixed kernel/old/new/new-noaot;
# BD = the frozen builds/; stampmain prebuilt in workloads/E13/stamp (was compiled at every run);
# stamps and logs go to OUTDIR (was the script's own directory). Commands otherwise the same.
# usage: memleak_start.sh OUTDIR REPS CELL...      e.g. memleak_start.sh out 10 kernel O D D:noaot
set -u
N=${1:?outdir}; REPS=${2:?reps}; shift 2; CELLS=("$@")
mkdir -p "$N"
BD=$HOME/src/paper-runs-20261124/builds
ML=/home/jetson/src/realprog-20261026-btf/libbpf-tools/memleak
SM=$HOME/src/paper-runs-20261124/workloads/E13/stamp/stampmain
for i in $(seq "$REPS"); do
  for cell in "${CELLS[@]}"; do
    tag=${cell/:/-}
    S=$N/stamp-$tag-$i.txt; rm -f "$S"
    arm=${cell%%:*}
    case $cell in
      kernel) pre=(); swp=; B= ;;
      *:noaot) B=$BD/$arm; pre=(LD_PRELOAD=$B/libbpftime-syscall-server.so BPFTIME_LOG_OUTPUT=console BPFTIME_SHM_MEMORY_MB=1024 BPFTIME_AOT_CACHE=0); swp=$B/libbpftime-agent.so ;;
      *) B=$BD/$arm; pre=(LD_PRELOAD=$B/libbpftime-syscall-server.so BPFTIME_LOG_OUTPUT=console BPFTIME_SHM_MEMORY_MB=1024); swp=$B/libbpftime-agent.so ;;
    esac
    t0=$(python3 -c 'import time; print(time.clock_gettime_ns(time.CLOCK_MONOTONIC))')
    timeout 90 sudo -n env "${pre[@]}" SW_PRELOAD=$swp SW_CMD=$SM SW_ARGS= STAMP_FILE=$S \
      $ML -c /home/jetson/src/rp/stw 1 > "$N/out-$tag-$i.log" 2>&1
    rc=$?
    [ -n "$B" ] && sudo -n "$B/bpftimetool" remove > /dev/null 2>&1
    sudo -n pkill -9 -x memleak > /dev/null 2>&1
    e=$(awk '/^exec/{print $2}' "$S" 2>/dev/null); m=$(awk '/^main/{print $2}' "$S" 2>/dev/null)
    if [ -n "$e" ] && [ -n "$m" ]; then
      echo "$cell rep $i rc $rc start_us $(( (m - e) / 1000 )) tool_setup_us $(( (e - t0) / 1000 ))"
    else echo "$cell rep $i rc $rc no-stamp"; fi
  done
done
