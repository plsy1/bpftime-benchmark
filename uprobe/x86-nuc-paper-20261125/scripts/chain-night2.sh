#!/usr/bin/env bash
R=/home/y/src/bpftime-x86-runs/paper-20261124; H=$R/harness; P=$R/plans
until grep -q "build-exit" $R/logs/unittest/build.log; do sleep 15; done
$R/scripts/run-unittests.sh > $R/logs/unittests.out 2>&1; echo "unittests $(cat $R/raw/E0-x86-unittests/summary.txt)"
waitload() { until awk '{exit !($1 < 0.15)}' /proc/loadavg; do sleep 5; done; }
run_retry() { local name=$1; shift
  for i in 1 2 3; do waitload; "$@" "$name-try$i" > "$R/logs/$name-try$i.out" 2>&1; s=$?; echo "$name try$i exit $s"
    [ $s -eq 0 ] && return 0; grep -q "load too high" "$R/logs/$name-try$i.out" || return $s; done; }
sess() { bash -c '"$0" "$2" "$1"' $H/session.sh "$@"; }
run_retry E7-counters-20261123 sess $P/E7-counters.txt
run_retry E5-repro-20261123 sess $P/E5-repro.txt
run_retry E8-sweep-20261123 sess $P/E8-sweep.txt
run_retry PB-loaded-20261123 sess $P/PB-loaded.txt
run_retry E3pc-full10-20261123 bash -c '"$0" "$1" opc dpc dpc opc' $H/e3/run-e3.sh
export PAPER_PINS="2:1700000 4:1700000 6:1700000 8:2500000 9:2500000 10:2500000 11:2500000"
run_retry E9-samefreq-20261123 sess $P/E9-samefreq.txt
unset PAPER_PINS
export E3_KHZ=1700000
run_retry E9-full10-1.7ghz-20261123 bash -c '"$0" "$1" official dev' $H/e3/run-e3.sh
echo CHAIN2-DONE
