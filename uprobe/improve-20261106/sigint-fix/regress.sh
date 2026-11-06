#!/usr/bin/env bash
# sigint-fix regression on dev 9c088441 (candidate build-probecheck, checks on): every runtime
# test case as root in a process of its own (regression/each.sh, copied from ../regression),
# then the daily smoke (tracker.smoke_real_tools: funclatency exact count, memleak outstanding
# allocations, SIGUSR1 detach under load, each tool stopped with SIGINT).
set -u
D=/home/jetson/src/results/uprobe/improve-20261106/sigint-fix/regression
C=/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate
mkdir -p $D
cp /home/jetson/src/results/uprobe/improve-20261106/regression/each.sh $D/each.sh
date +%T > $D/started
timeout 5400 bash $D/each.sh $C/build-probecheck/runtime/unit-test/bpftime_runtime_tests > $D/each.out 2>&1
cd /home/jetson/src/bpftime-jetson-daily/tools/jetson-upstream || exit 1
timeout 600 python3 -c '
import sys
from pathlib import Path
import tracker
config, repo = tracker.load_config(Path("config.json").resolve())
tracker.smoke_real_tools(config, Path(sys.argv[1]), Path(sys.argv[2]))
' $C $C/build-probecheck > $D/smoke.log 2>&1
echo "smoke rc=$?" >> $D/smoke.log
date +%T > $D/finished
