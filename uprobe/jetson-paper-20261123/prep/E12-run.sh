#!/usr/bin/env bash
# E12 Jetson 18-tool exact audit (plan 2.2; ATC P0, before CP2): audit.py --backends kernel,O,D --secs 4,
# 3 runs. The 18 tools are audit.py's TOOLS without syscount (passed explicitly). javagc runs through
# each arm's own `bpftime -i builds/<arm> attach` (both O and D are frozen with the CLI, plan 1.2).
# Criterion (analysis, not here): delivered + reported == expected; get_stack_id has no exact answer,
# memleak is approximate (~1024).
# Unpinned as 10-26 (tools and workloads float over cpus 0-5; this shell is on cpu2 after start_session,
# so the runner is started with taskset -c 0-5 to undo the inherited mask).
# Prerequisite: prep/E12-build.sh (official example/*-core). Usage: E12-run.sh [session-id]
set -euo pipefail
SID=${1:-J-E12-20261124}
MIN=60                                  # expected ~32 min (10-26: ~190 s per backend per run)
KILL_NAMES="memleak funclatency gethostlatency bashreadline malloc usdt_minimal mallocloop callloop resolveloop victim sslsniff readline sslloop get_stack_id_ex bpftrace javagc opensnoop openloop java"
ARMS_USED="O D"
source "$HOME/src/paper-runs-20261124/prep/lib.sh"
TOOLS=funclatency-libbpf,funclatency-bpftime-example,gethostlatency,memleak,malloc-example,bashreadline,usdt_minimal,sslsniff-example,bashreadline-example,get_stack_id-example,bpftrace-uprobe-count,bpftrace-uretprobe-hist,bpftrace-ustack,bpftrace-usdt-jvm-gc,bpftrace-bashreadline,bpftrace-gethostlatency,javagc,opensnoop
[ "$(tr ',' '\n' <<<"$TOOLS" | wc -l)" -eq 18 ]
EXL=$R/logs/E12-examples
start_session
(cd "$HOME/src/bpftime-paper/official-4708f409/example" && sha256sum --check --quiet "$EXL/examples-sha256.txt")
(cd "$HOME/src/realprog-20261026-btf" && sha256sum --check --quiet <(grep -v ' /' "$EXL/realprog-inputs-sha256.txt"))
(cd / && sha256sum --check --quiet <(grep ' /' "$EXL/realprog-inputs-sha256.txt"))      # ~/src/rp/sw
for a in O D; do [ -x "$R/builds/$a/bpftime" ] && [ -f "$R/builds/$a/libbpftime-agent-transformer.so" ] || { echo "arm $a lacks CLI/transformer"; exit 1; }; done
cp "$P/E12/audit.py" "$F/audit.py.used"
for run in 1 2 3; do
    assert_clean
    step "audit run $run start"
    budget 1500
    (cd "$F" && timeout --kill-after=30 "$B" taskset -c 0-5 python3 -u "$P/E12/audit.py" \
        --backends kernel,O,D --tools "$TOOLS" --secs 4 --out "$F/run$run.jsonl") >"$LOG/run$run.log" 2>&1 \
        || { echo "audit run $run exit $?" >>"$LOG/steps.txt"; exit 1; }
    step "audit run $run end"
done
python3 -I - "$F" <<'PY'
import json, sys, pathlib
f = pathlib.Path(sys.argv[1])
for run in (1, 2, 3):
    rows = [json.loads(l) for l in (f / f'run{run}.jsonl').read_text().splitlines()]
    print(f'run{run}', len(rows), 'rows (expect 54);', sum('error' in r for r in rows), 'harness errors')
PY
echo "E12 done"
