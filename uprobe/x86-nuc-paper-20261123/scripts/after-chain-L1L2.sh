#!/usr/bin/env bash
R=/home/y/src/bpftime-x86-runs/paper-20261124; P=/home/y/src/bpftime-paper
until grep -q CHAIN-DONE $R/logs/chain-E3-E4-E6.out; do sleep 30; done
(cd $P/l1-4708f409 && git submodule update --init --recursive third_party/Catch2 third_party/argparse third_party/bpftool third_party/spdlog third_party/ubpf vm/llvm-jit >/dev/null 2>&1)
(cd $P/l2-4708f409 && git submodule update --init --recursive third_party/Catch2 third_party/argparse third_party/bpftool third_party/spdlog third_party/ubpf vm/llvm-jit >/dev/null 2>&1)
$R/scripts/build-arm.sh l1 $P/l1-4708f409 off; echo "l1 build $?"
$R/scripts/build-arm.sh l2 $P/l2-4708f409 off; echo "l2 build $?"
for i in 1 2 3; do
  until awk '{exit !($1 < 0.15)}' /proc/loadavg; do sleep 5; done
  $R/harness/session.sh E6x-ladder-20261123-try$i $R/plans/E6x-ladder.txt > $R/logs/E6x-try$i.out 2>&1; s=$?; echo "E6x try$i exit $s"
  [ $s -eq 0 ] && break; grep -q "load too high" $R/logs/E6x-try$i.out || break
done
echo L1L2-DONE
