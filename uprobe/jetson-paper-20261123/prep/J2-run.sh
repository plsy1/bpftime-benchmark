#!/usr/bin/env bash
# J2 (ATC P2', 11-25) Jetson real-tool throughput, single-threaded: 6 tools x 9 backends x 5 rounds, --secs 10.
# Tools (NUC 11-25, = x86 E6): gethostlatency, malloc-example, funclatency-libbpf, memleak, bpftrace-uretprobe-hist,
# bpftrace-gethostlatency. Backend order shuffled per (round, tool), seed sha256(session:round:tool). Needs builds/OlL3x730 (J7).
# Expected O rows: funclatency-libbpf crash (traced process rc -11), memleak 0 leaks (10-26), recorded as is.
# Unpinned as 10-26 (taskset -c 0-5 undoes the cpu2 mask this shell has after start_session); pinning
# would break comparability with the 10-26 table.
# Prerequisite: prep/E12-build.sh (same examples and audit copy). Usage: E14-run.sh [session-id]
set -euo pipefail
SID=${1:-J-J2-20261125}
MIN=130                                 # expected ~85 min (270 runs x ~18 s)
KILL_NAMES="memleak funclatency gethostlatency malloc mallocloop callloop resolveloop victim bpftrace"
ARMS_USED="O L3port Ol Olpc OlL3x730 Dn Dnpc"
source "$HOME/src/paper-runs-20261124/prep/lib.sh"
EXL=$R/logs/E12-examples
start_session
(cd "$HOME/src/bpftime-paper/official-4708f409/example" && sha256sum --check --quiet "$EXL/examples-sha256.txt")
(cd "$HOME/src/realprog-20261026-btf" && sha256sum --check --quiet <(grep -v ' /' "$EXL/realprog-inputs-sha256.txt"))
(cd / && sha256sum --check --quiet <(grep ' /' "$EXL/realprog-inputs-sha256.txt"))      # ~/src/rp/sw
cp "$P/J2/measure.py" "$F/measure.py.used"; cp "$P/E12/audit.py" "$F/audit.py.used"
assert_clean
step "measure start"
budget 7200
(cd "$F" && timeout --kill-after=30 "$B" taskset -c 0-5 python3 -u "$P/J2/measure.py" --session "$SID" \
    --backends none,kernel,O,L3port,Ol,Olpc,OlL3x730,Dn,Dnpc --tools gethostlatency,malloc-example,funclatency-libbpf,memleak,bpftrace-uretprobe-hist,bpftrace-gethostlatency --rounds 5 --secs 10 --out "$F/measure.jsonl") >"$LOG/measure.log" 2>&1
step "measure end"
python3 -I - "$F/measure.jsonl" <<'PY'
import json, sys
rows = [json.loads(l) for l in open(sys.argv[1])]
print(len(rows), 'rows (expect 270);', sum('error' in r for r in rows), 'harness errors')
PY
echo "J2 done"
