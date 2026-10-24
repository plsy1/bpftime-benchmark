#!/usr/bin/env bash
set -euo pipefail
ROOT=/home/jetson/src/bpftime-percpu-array-jit-20261020
RESULTS=/home/jetson/src/results/uprobe/array-jit-default-20261021
OUT="$RESULTS/full10"
FROZEN="$RESULTS/artifacts"
RUNNER="$RESULTS/full-runner/bpftime"
test "$(<"$RESULTS/build-exit-status.txt")" = 0
test "$(<"$RESULTS/short-exit-status.txt")" = 0
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
trap 'exit 130' INT
trap 'exit 143' TERM
date --iso-8601=seconds >"$OUT/started-at.txt"
python3 - "$RESULTS" <<'PY'
from pathlib import Path
import hashlib,json,sys
r=Path(sys.argv[1])
source=Path('/home/jetson/src/bpftime-percpu-array-jit-20261020')
for name,digest in json.loads((r/'source-sha256.json').read_text()).items():
    assert hashlib.sha256((source/name).read_bytes()).hexdigest()==digest,name
for suite in ('percpu-ab6','ordinary-ab6'):
    env=json.loads((r/suite/'environment.json').read_text())
    assert env['status']=='success'
    for file,digest in env['binary_sha256'].items():
        assert hashlib.sha256(Path(file).read_bytes()).hexdigest()==digest,file
    for row in json.loads((r/suite/'summary.json').read_text()):
        if row['case']!='unused':
            assert row['inline_vs_previous_percent']<=3,(suite,row)
assert (r/'standalone.log').exists()
print('Source, exact artifacts, consuming non-regression and standalone gates passed.',flush=True)
PY
cp "$ROOT/build-jit/runtime/agent/libbpftime-agent.so" "$FROZEN/runtime/agent/"
cp "$ROOT/build-jit/runtime/syscall-server/libbpftime-syscall-server.so" "$FROZEN/runtime/syscall-server/"
gmake --no-print-directory -C "$ROOT/build-jit" \
    -f benchmark/CMakeFiles/simple-benchmark-with-embed-ebpf-calling.dir/build.make \
    benchmark/CMakeFiles/simple-benchmark-with-embed-ebpf-calling.dir/depend >"$OUT/build-embed-depend.log" 2>&1
gmake --no-print-directory -C "$ROOT/build-jit" \
    -f benchmark/CMakeFiles/simple-benchmark-with-embed-ebpf-calling.dir/build.make \
    benchmark/simple-benchmark-with-embed-ebpf-calling -j2 >"$OUT/build-embed.log" 2>&1
# Dependencies are now current. Refresh the CLI via its generated target rules
# without another libbpf install that would touch all runtime headers again.
gmake --no-print-directory -C "$ROOT/build-jit" \
    -f tools/bpftimetool/CMakeFiles/bpftimetool.dir/build.make \
    tools/bpftimetool/CMakeFiles/bpftimetool.dir/depend >"$OUT/build-tool-depend.log" 2>&1
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
sudo -n taskset -c 5 env -u BPFTIME_ARRAY_JIT_INLINE PYTHONUNBUFFERED=1 BPFTIME_LOG_OUTPUT=console \
    /usr/bin/python3 -u benchmark/uprobe/benchmark.py --iter 10 \
    2>&1 | tee "$OUT/runner.log"
for artifact in benchmark-output.json results.md benchmark-logs.txt; do
    test "$ROOT/benchmark/uprobe/$artifact" -nt "$OUT/started-at.txt"
    cp "$ROOT/benchmark/uprobe/$artifact" "$OUT/"
done
# The historical timing runner does not request INFO in victim environments.
# Audit activation explicitly, on the same artifacts, outside timing samples.
sudo -n "$FROZEN/tools/bpftimetool/bpftimetool" remove >"$OUT/audit-pre-cleanup.log" 2>&1
python3 "$RESULTS/audit-full-activation.py" >"$OUT/activation-audit.log" 2>&1
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
