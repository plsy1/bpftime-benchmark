#!/usr/bin/env bash
# After E2o: init submodules of the dexact worktree, build it with checks on, register it, run E16.
set -uo pipefail
R=/home/y/src/bpftime-x86-runs/paper-20261124; H=$R/harness; P=$R/plans; W=/home/y/src/bpftime-paper/dexact-9ccda7e8
(cd $W && git submodule update --init --recursive third_party/Catch2 third_party/argparse third_party/bpftool third_party/spdlog third_party/ubpf vm/llvm-jit >$R/logs/dexact-submodules.log 2>&1) || { echo SUBMODULE-FAIL; exit 1; }
bash $R/scripts/build-arm.sh dexactpc $W on || { echo BUILD-FAIL; exit 1; }
python3 - <<'PY'
import json
p='/home/y/src/bpftime-x86-runs/paper-20261124/harness/config-x86.json'
c=json.load(open(p)); c['builds']['Dx']='/builds/dexactpc'; json.dump(c,open(p,'w'),indent=2)
PY
waitload() { until awk '{exit !($1 < 0.15)}' /proc/loadavg; do sleep 5; done; }
for i in 1 2 3; do waitload; "$H/session.sh" "E16-exact-20261124-try$i" "$P/E16-exact.txt" > "$R/logs/E16-exact-20261124-try$i.out" 2>&1; s=$?; echo "E16 try$i exit $s"
  [ $s -eq 0 ] && break; grep -q "load too high" "$R/logs/E16-exact-20261124-try$i.out" || break; done
echo E16-DONE
