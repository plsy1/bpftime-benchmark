#!/usr/bin/env bash
set -euo pipefail
ROOT=/home/jetson/src/bpftime-array-jit-integrated-20261020
RESULTS=/home/jetson/src/results/uprobe/array-jit-inline-integrated-20261020
OUT="$RESULTS/full10"
FROZEN="$RESULTS/artifacts"
RUNNER="$RESULTS/full-runner/bpftime"
test "$(<"$RESULTS/validation-accepted-exit-status.txt")" = 0
test -f "$RESULTS/full-inputs-reviewed"
test ! -e /dev/shm/bpftime_maps_shm
mkdir "$OUT"
mkdir -p "$FROZEN/runtime/agent" "$FROZEN/runtime/syscall-server" \
    "$FROZEN/tools/bpftimetool" "$FROZEN/benchmark" "$RUNNER"
finish()
{
    status=$?
    trap - EXIT
    if test -x "$FROZEN/tools/bpftimetool/bpftimetool"; then
        sudo -n "$FROZEN/tools/bpftimetool/bpftimetool" remove >>"$OUT/cleanup.log" 2>&1 || true
    fi
    printf '%s\n' "$status" >"$OUT/exit-status.txt"
    date --iso-8601=seconds >"$OUT/finished-at.txt"
    exit "$status"
}
trap finish EXIT
date --iso-8601=seconds >"$OUT/started-at.txt"
python3 - "$RESULTS" <<'PY'
from pathlib import Path
import hashlib,json,sys
r=Path(sys.argv[1]); env=json.loads((r/'ab6-accepted/environment.json').read_text())
assert env['status']=='success'
reviewed=json.loads((r/'full-inputs-reviewed').read_text())
assert reviewed['matched']=='ab6-accepted'
assert reviewed['binary']==env['binary_sha256']
source=Path('/home/jetson/src/bpftime-array-jit-integrated-20261020')
for name,digest in reviewed['source'].items():
    assert hashlib.sha256((source/name).read_bytes()).hexdigest()==digest,name
for file,digest in env['binary_sha256'].items():
    assert hashlib.sha256(Path(file).read_bytes()).hexdigest()==digest,file
for suite in ('array','hash'):
    report=json.loads((r/f'{suite}-regression3/summary.json').read_text())
    for row in report:
        if row['historically_comparable']:
            assert row['candidate']['median']/row['baseline']['median']<=1.03,row
print('Matched-artifact and non-target regression gates passed.',flush=True)
PY
cp "$ROOT/build-jit/runtime/agent/libbpftime-agent.so" "$FROZEN/runtime/agent/"
cp "$ROOT/build-jit/runtime/syscall-server/libbpftime-syscall-server.so" "$FROZEN/runtime/syscall-server/"
cmake --build "$ROOT/build-jit" --target simple-benchmark-with-embed-ebpf-calling -j2 \
    >"$OUT/build-embed.log" 2>&1
# Dependencies are now current. Refresh the CLI via its generated target rules
# without another libbpf install that would touch all runtime headers again.
gmake --no-print-directory -C "$ROOT/build-jit" \
    -f tools/bpftimetool/CMakeFiles/bpftimetool.dir/build.make \
    tools/bpftimetool/bpftimetool >"$OUT/build-tool.log" 2>&1
cp "$ROOT/build-jit/tools/bpftimetool/bpftimetool" "$FROZEN/tools/bpftimetool/"
cp "$ROOT/build-jit/benchmark/simple-benchmark-with-embed-ebpf-calling" "$FROZEN/benchmark/"
ln -s "$FROZEN" "$RUNNER/build"
ln -s "$ROOT/benchmark" "$RUNNER/benchmark"
sha256sum "$FROZEN/runtime/agent/libbpftime-agent.so" \
    "$FROZEN/runtime/syscall-server/libbpftime-syscall-server.so" \
    "$FROZEN/tools/bpftimetool/bpftimetool" \
    "$FROZEN/benchmark/simple-benchmark-with-embed-ebpf-calling" \
    "$ROOT/benchmark/test" "$ROOT/benchmark/uprobe/uprobe" \
    "$ROOT/benchmark/uprobe/.output/uprobe.bpf.o" >"$OUT/runtime-sha256.txt"
sudo -n /usr/sbin/nvpmodel -q >"$OUT/nvpmodel-before.txt" 2>&1
sudo -n jetson_clocks --show >"$OUT/jetson-clocks-before.txt" 2>&1
python3 - <<'PY'
from pathlib import Path
p=Path('/sys/devices/system/cpu/cpu5/cpufreq')
assert all(int((p/f'scaling_{k}_freq').read_text())==1728000 for k in ('min','max','cur'))
PY
cd "$RUNNER"
sudo -n taskset -c 5 env PYTHONUNBUFFERED=1 BPFTIME_LOG_OUTPUT=console \
    BPFTIME_ARRAY_JIT_INLINE=1 /usr/bin/python3 -u benchmark/uprobe/benchmark.py --iter 10 \
    2>&1 | tee "$OUT/runner.log"
for artifact in benchmark-output.json results.md benchmark-logs.txt; do
    test "$ROOT/benchmark/uprobe/$artifact" -nt "$OUT/started-at.txt"
    cp "$ROOT/benchmark/uprobe/$artifact" "$OUT/"
done
test "$(rg -c '\[array-jit-inline\] inlined ' "$OUT/runner.log")" -ge 10
python3 - "$OUT/benchmark-output.json" <<'PY'
import json,math,sys
result=json.load(open(sys.argv[1]))
for group,count in [('kernel_uprobe',17),('userspace_uprobe',17),('embed',1)]:
    assert len(result[group])==count
    for name,value in result[group].items():
        assert len(value['raw_values'])==10,(group,name)
        assert all(math.isfinite(x) and x>0 for x in value['raw_values'])
print('Full-suite sample validation passed.',flush=True)
PY
sha256sum --check "$OUT/runtime-sha256.txt"
sudo -n jetson_clocks --show >"$OUT/jetson-clocks-after.txt" 2>&1
python3 "$RESULTS/summarize-full.py" >"$OUT/ten-operation-comparison.md"
