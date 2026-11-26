#!/usr/bin/env bash
# J6 v2 (ATC, exploratory, NUC 11-26): pinbench-ws v2 (direct per-call timing) on cpu3; protocol prep/J6v2/J6v2-protocol.md.
# ./pinbench-ws 3 5 100000 0,128,256,512,1024,2048,4096 single=3 pair=2,3 all=0-5. Usage: J6v2-run.sh [session-id]
set -euo pipefail
SID=${1:-J-J6v2-20261126}
MIN=70                                  # expected 30-40 min (4 MB rows dominate)
KILL_NAMES="pinbench-ws"
ARMS_USED=""
source "$HOME/src/paper-runs-20261124/prep/lib.sh"
start_session
(cd "$P/J6v2" && sha256sum --check --quiet sha256.txt)
cp "$P/J6v2/pinbench-ws.c" "$P/J6v2/J6v2-protocol.md" "$P/J6v2/sha256.txt" "$F/"
step "J6v2 start"
budget 3900
timeout --kill-after=30 "$B" taskset -c 0-5 "$P/J6v2/pinbench-ws" 3 5 100000 0,128,256,512,1024,2048,4096 single=3 pair=2,3 all=0-5 \
    >"$F/j6v2.jsonl" 2>"$LOG/j6.stderr" || { echo "J6v2 exit $?" >>"$LOG/steps.txt"; exit 1; }
step "J6v2 end"
echo "J6v2 done: $(wc -l <"$F/j6v2.jsonl") lines"
