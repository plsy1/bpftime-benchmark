#!/usr/bin/env bash
# J6 (ATC, exploratory): working-set pinbench on the sniff core cpu3 (NUC protocol prep/J6/J6-protocol.md, handoff-J6 77f4b57c).
# ./pinbench-ws 3 5 100000 0,64,128,256,512,1024,2048 single=3 pair=2,3 cross=3,4 all=0-5. Usage: J6-run.sh [session-id]
set -euo pipefail
SID=${1:-J-J6-20261125}
MIN=30                                  # expected 10-15 min
KILL_NAMES="pinbench-ws"
ARMS_USED=""
source "$HOME/src/paper-runs-20261124/prep/lib.sh"
start_session
(cd "$P/J6" && sha256sum --check --quiet sha256.txt)
cp "$P/J6/pinbench-ws.c" "$P/J6/J6-protocol.md" "$P/J6/sha256.txt" "$F/"
step "J6 start"
budget 1500
timeout --kill-after=30 "$B" taskset -c 0-5 "$P/J6/pinbench-ws" 3 5 100000 0,64,128,256,512,1024,2048 single=3 pair=2,3 cross=3,4 all=0-5 \
    >"$F/j6.jsonl" 2>"$LOG/j6.stderr" || { echo "J6 exit $?" >>"$LOG/steps.txt"; exit 1; }
step "J6 end"
echo "J6 done: $(wc -l <"$F/j6.jsonl") lines"
