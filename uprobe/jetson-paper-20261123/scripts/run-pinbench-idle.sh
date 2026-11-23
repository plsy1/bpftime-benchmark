#!/usr/bin/env bash
# E2 pinbench, idle state (Jetson). Runs on the sslsniff core (cpu3) at 1728 MHz.
set -euo pipefail
R=$HOME/src/paper-runs-20261124; OUT=$R/raw/${1:?session id}; mkdir "$OUT"
$R/harness/check_env.sh start "$OUT/log" 10
(cd $R/bench-extra && sha256sum --check --quiet sha256.txt)
taskset -c 3 $R/bench-extra/pinbench 3 1728 10 1000000 single=3 pair=3,2 xpair=3,4 clusterA=0-3 all=0-5 >"$OUT/pinbench.jsonl" 2>"$OUT/pinbench.stderr"
$R/harness/check_env.sh end "$OUT/log"
