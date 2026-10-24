#!/usr/bin/env bash
set -euo pipefail
ROOT=/home/jetson/src/bpftime-percpu-array-jit-20261020
OUT=/home/jetson/src/results/uprobe/array-jit-default-20261021/hash-delete-investigation
BUILD="$ROOT/build-jit"
trap 'exit 130' INT
trap 'exit 143' TERM
trap 'status=$?; printf "%s\n" "$status" >"$OUT/build-exit-status.txt"; date --iso-8601=seconds >"$OUT/build-finished-at.txt"' EXIT
date --iso-8601=seconds >"$OUT/build-started-at.txt"
python3 - "$OUT" "$ROOT" <<'PY'
import hashlib,json,sys
from pathlib import Path
out,source=map(Path,sys.argv[1:])
manifest=json.loads((out.parent/'source-sha256.json').read_text())
changed=[]
for name,digest in manifest.items():
    if hashlib.sha256((source/name).read_bytes()).hexdigest()!=digest:changed.append(name)
assert changed==['runtime/src/bpf_map/array_map_binding.hpp'],changed
print('Only array binding erase call-site differs from frozen current source.')
PY
make_target() {
    gmake --no-print-directory -C "$BUILD" -f "$1" "${1%/build.make}/depend" >>"$OUT/build.log" 2>&1
    gmake --no-print-directory -C "$BUILD" -f "$1" "$2" -j2 >>"$OUT/build.log" 2>&1
}
make_target runtime/CMakeFiles/runtime.dir/build.make runtime/libruntime.a
make_target runtime/agent/CMakeFiles/bpftime-agent.dir/build.make runtime/agent/libbpftime-agent.so
make_target runtime/unit-test/CMakeFiles/bpftime_runtime_tests.dir/build.make runtime/unit-test/bpftime_runtime_tests
env -u BPFTIME_ARRAY_JIT_INLINE timeout 180 "$BUILD/runtime/unit-test/bpftime_runtime_tests" \
    '[array-jit-lifetime],[array-jit-poc],[array-offset-branch],[array-lookup-cold],[percpu-array],[percpu-hash],[maps][hash],[hash-fast4],Test basic operations of array map' >"$OUT/tests.log" 2>&1
env -u BPFTIME_ARRAY_JIT_INLINE timeout 180 "$BUILD/runtime/unit-test/bpftime_runtime_tests" \
    '[allocation],[maps][map_in_map],Test bpftime runtime_config,Allow external maps from the environment,Select Frida*,Test basic operations of external hash map ops' >"$OUT/host-contract.log" 2>&1
mkdir -p "$OUT/artifacts/runtime/agent" "$OUT/artifacts/runtime/syscall-server" "$OUT/artifacts/tools/bpftimetool"
cp "$BUILD/runtime/agent/libbpftime-agent.so" "$OUT/artifacts/runtime/agent/"
ln -s "$OUT/../artifacts/runtime/syscall-server/libbpftime-syscall-server.so" "$OUT/artifacts/runtime/syscall-server/libbpftime-syscall-server.so"
ln -s "$OUT/../artifacts/tools/bpftimetool/bpftimetool" "$OUT/artifacts/tools/bpftimetool/bpftimetool"
cp "$ROOT/runtime/src/bpf_map/array_map_binding.hpp" "$OUT/array_map_binding.diagnostic.hpp"
python3 "$OUT/analyze-native.py" >"$OUT/native-comparison.log"
