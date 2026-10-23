#!/usr/bin/env bash
# Complete ten-round uprobe suite, userspace + embedded only (no kernel arm),
# on the frozen db3d475f artifacts. Adapted from
# baseline-177a1dc-jit-llvm16-20261021/run-full.sh.
set -euo pipefail
RESULTS=/home/jetson/src/results/uprobe/perf-lost-wakeup-20261023/full10-db3d475f
OUT="$RESULTS/full10"
FROZEN="$RESULTS/artifacts"
RUNNER="$RESULTS/full-runner/bpftime"
test ! -e /dev/shm/bpftime_maps_shm
mkdir "$OUT"
finish()
{
    status=$?
    trap - EXIT
    sudo -n "$FROZEN/tools/bpftimetool/bpftimetool" remove >>"$OUT/cleanup.log" 2>&1 || true
    printf '%s\n' "$status" >"$OUT/exit-status.txt"
    date --iso-8601=seconds >"$OUT/finished-at.txt"
    exit "$status"
}
trap finish EXIT
trap 'exit 130' INT
trap 'exit 143' TERM
date --iso-8601=seconds >"$OUT/started-at.txt"
sha256sum --check --quiet "$RESULTS/artifact-sha256.txt"
sudo -n /usr/sbin/nvpmodel -q >"$OUT/nvpmodel-before.txt" 2>&1
sudo -n jetson_clocks --show >"$OUT/jetson-clocks-before.txt" 2>&1
python3 -I - <<'PY'
from pathlib import Path
p=Path('/sys/devices/system/cpu/cpu5/cpufreq')
assert all(int((p/f'scaling_{k}_freq').read_text())==1728000 for k in ('min','max','cur'))
PY
cd "$RUNNER"
sudo -n taskset -c 5 env -u BPFTIME_ARRAY_JIT_INLINE PYTHONUNBUFFERED=1 BPFTIME_LOG_OUTPUT=console \
    /usr/bin/python3 -u "$RESULTS/run-userspace-benchmark.py" --iter 10 \
    2>&1 | tee "$OUT/runner.log"
for artifact in benchmark-output.json results.md benchmark-logs.txt; do
    test "$RUNNER/benchmark/uprobe/$artifact" -nt "$OUT/started-at.txt"
    cp "$RUNNER/benchmark/uprobe/$artifact" "$OUT/"
done
sudo -n "$FROZEN/tools/bpftimetool/bpftimetool" remove >"$OUT/audit-pre-cleanup.log" 2>&1
python3 "$RESULTS/audit-full-activation.py" >"$OUT/activation-audit.log" 2>&1
python3 -I - "$OUT/benchmark-output.json" <<'PY'
import json,math,sys
result=json.load(open(sys.argv[1]))
assert result['kernel_uprobe']=={'skipped':'kernel arm not run'}
for group,count in [('userspace_uprobe',17),('embed',1)]:
    assert len(result[group])==count
    for name,value in result[group].items():
        assert len(value['raw_values'])==10,(group,name)
        assert all(math.isfinite(x) and x>0 for x in value['raw_values'])
print('Full-suite sample validation passed.',flush=True)
PY
sha256sum --check --quiet "$RESULTS/artifact-sha256.txt"
sudo -n jetson_clocks --show >"$OUT/jetson-clocks-after.txt" 2>&1
python3 -I "$RESULTS/compare-full.py" >"$OUT/comparison.md"
