#!/usr/bin/env bash
R=/home/y/src/bpftime-x86-runs/paper-20261124
waitload() { until awk '{exit !($1 < 0.15)}' /proc/loadavg; do sleep 5; done; }
run_retry() { # name cmd...
  local name=$1; shift
  for i in 1 2 3; do
    waitload; "$@" "$name-try$i" > "$R/logs/$name-try$i.out" 2>&1; s=$?
    echo "$name try$i exit $s"
    [ $s -eq 0 ] && return 0
    grep -q "load too high" "$R/logs/$name-try$i.out" || return $s
  done
}
run_retry E3-full10-20261123 bash -c '"$0" "$1" official dev dev official' $R/harness/e3/run-e3.sh
run_retry E4-headline-20261123 bash -c '"$0" "$2" "$1"' $R/harness/session.sh $R/plans/E4-headline.txt
run_retry E6-fig1-20261123 bash -c '"$0" "$2" "$1"' $R/harness/session.sh $R/plans/E6-fig1.txt
echo CHAIN-DONE
