#!/usr/bin/env bash
# E14 Jetson real-tool throughput (plan 2.2, P1): measure.py --backends none,kernel,O,D --rounds 5
# --secs 10, its default 8 tools (gethostlatency, malloc-example, funclatency x2, memleak, bpftrace
# uprobe count / uretprobe hist / gethostlatency.bt). Backends rotate per round (as 10-26).
# Expected O rows: funclatency-libbpf crash (traced process rc -11), memleak 0 leaks (10-26), recorded as is.
# Unpinned as 10-26 (taskset -c 0-5 undoes the cpu2 mask this shell has after start_session); pinning
# would break comparability with the 10-26 table.
# Prerequisite: prep/E12-build.sh (same examples and audit copy). Usage: E14-run.sh [session-id]
set -euo pipefail
SID=${1:-J-E14-20261124}
MIN=80                                  # expected ~50 min (160 runs x ~18 s; plan: about 49 min)
KILL_NAMES="memleak funclatency gethostlatency malloc mallocloop callloop resolveloop victim bpftrace"
ARMS_USED="O D"
source "$HOME/src/paper-runs-20261124/prep/lib.sh"
EXL=$R/logs/E12-examples
start_session
(cd "$HOME/src/bpftime-paper/official-4708f409/example" && sha256sum --check --quiet "$EXL/examples-sha256.txt")
(cd "$HOME/src/realprog-20261026-btf" && sha256sum --check --quiet <(grep -v ' /' "$EXL/realprog-inputs-sha256.txt"))
(cd / && sha256sum --check --quiet <(grep ' /' "$EXL/realprog-inputs-sha256.txt"))      # ~/src/rp/sw
cp "$P/E14/measure.py" "$F/measure.py.used"; cp "$P/E12/audit.py" "$F/audit.py.used"
assert_clean
step "measure start"
budget 4200
(cd "$F" && timeout --kill-after=30 "$B" taskset -c 0-5 python3 -u "$P/E14/measure.py" \
    --backends none,kernel,O,D --rounds 5 --secs 10 --out "$F/measure.jsonl") >"$LOG/measure.log" 2>&1
step "measure end"
python3 -I - "$F/measure.jsonl" <<'PY'
import json, sys
rows = [json.loads(l) for l in open(sys.argv[1])]
print(len(rows), 'rows (expect 160);', sum('error' in r for r in rows), 'harness errors')
PY
echo "E14 done"
