#!/usr/bin/env bash
R=$HOME/src/paper-runs-20261124; cd $R
export PAPER_EXEC_PATH=/usr/lib/linux-tools/6.8.0-1020-nvidia-tegra:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
waitload(){ ok=0; while [ $ok -lt 3 ]; do if awk '{exit !($1 < 0.15)}' /proc/loadavg; then ok=$((ok+1)); else ok=0; fi; sleep 20; done; echo "load ok $(date +%T)"; }
waitload; harness/session.sh J-E18a-20261124 plans/E18a.txt 60; echo "E18a exit=$?"
until [ "$(date +%s)" -ge "$(date -d '18:25' +%s)" ]; do sleep 30; done
waitload; harness/session.sh J-E18b-20261124 plans/E18b.txt 60; echo "E18b exit=$?"
