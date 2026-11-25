#!/usr/bin/env bash
# Paper campaign, x86 NUC: configure + build one frozen arm.
# Usage: build-arm.sh <arm-name> <source-root> <checks on|off> [extra targets...]
# PAPER_LAYOUT_FLAGS (optional): extra C/C++ flags for a code-layout control build (B5), e.g. -falign-functions=64.
# Takes the measurement lock (non-blocking): refuses to build while a measurement session holds it.
set -euo pipefail
exec 9>/home/y/src/bpftime-x86-runs/paper-20261124/.measure.lock
flock -n 9 || { echo "refusing: measurement lock held"; exit 3; }
ARM=$1; ROOT=$2; CHECKS=$3; shift 3
R=/home/y/src/bpftime-x86-runs/paper-20261124
B=$ROOT/build-paper-$ARM
OUT=$R/builds/$ARM
LOG=$R/logs/build-$ARM
mkdir -p "$LOG"
[ -e "$OUT" ] && { echo "refusing: $OUT exists"; exit 2; }
[ -e "$B" ] && { echo "refusing: $B exists (no build-dir reuse)"; exit 2; }
CK=OFF; [ "$CHECKS" = on ] && CK=ON
EXTRA=()
if [ -n "${PAPER_LAYOUT_FLAGS:-}" ]; then
  EXTRA+=("-DCMAKE_C_FLAGS=$PAPER_LAYOUT_FLAGS" "-DCMAKE_CXX_FLAGS=$PAPER_LAYOUT_FLAGS")
fi
if grep -q BPFTIME_ENABLE_MAP_PRODUCTION_AB "$ROOT/cmake/StandardSettings.cmake"; then
  EXTRA+=(-DBPFTIME_ENABLE_MAP_PRODUCTION_AB=OFF -DBPFTIME_ENABLE_MAP_USAGE_STATS=OFF -DBPFTIME_PATCH_EXECUTABLE=/usr/bin/patch)
fi
date --iso-8601=seconds >"$LOG/started-at.txt"
{
  echo "arm: $ARM"; echo "commit: $(git -C "$ROOT" rev-parse HEAD)"; echo "branch: $(git -C "$ROOT" rev-parse --abbrev-ref HEAD)"
  echo "log:"; git -C "$ROOT" log --oneline -5
  echo "vm/llvm-jit: $(git -C "$ROOT/vm/llvm-jit" rev-parse HEAD)"
  echo "layout flags: ${PAPER_LAYOUT_FLAGS:-none}"
  echo "tracked status (empty = clean):"; git -C "$ROOT" status --short --untracked-files=no
} >"$LOG/source-state.txt"
{
  (gcc-13 --version | head -1; g++-13 --version | head -1; clang-19 --version | head -1) || true
  /usr/lib/llvm-19/bin/llvm-config --version; ldd --version | head -1; cmake --version | head -1; ninja --version
} >"$LOG/toolchain.txt"
cmake -S "$ROOT" -B "$B" -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_C_COMPILER=/usr/bin/gcc-13 -DCMAKE_CXX_COMPILER=/usr/bin/g++-13 \
  -DBPFTIME_BPF_CLANG=/usr/bin/clang-19 -DDIAGNOSTIC_BPF_CLANG=/usr/bin/clang-19 -DLLVM_DIR=/usr/lib/llvm-19/lib/cmake/llvm \
  -DBPFTIME_LLVM_JIT=ON -DBPFTIME_UBPF_JIT=ON -DBPFTIME_ENABLE_LTO=ON -DBPFTIME_ENABLE_UNIT_TESTING=OFF \
  -DBPFTIME_ENABLE_CUDA_ATTACH=OFF -DENABLE_EBPF_VERIFIER=OFF \
  -DENABLE_PROBE_READ_CHECK=$CK -DENABLE_PROBE_WRITE_CHECK=$CK \
  -DBUILD_BPFTIME_DAEMON=OFF -DBUILD_LLVM_AOT_CLI=OFF \
  -DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO "${EXTRA[@]}" >"$LOG/configure.log" 2>&1
grep -E '^(BPFTIME_[A-Z_]+|ENABLE_[A-Z_]+|SPDLOG_ACTIVE_LEVEL|CMAKE_BUILD_TYPE|LLVM_DIR|CMAKE_C_COMPILER|CMAKE_CXX_COMPILER|CMAKE_C_FLAGS|CMAKE_CXX_FLAGS|BUILD_[A-Z_]+)[:=]' "$B/CMakeCache.txt" \
  | grep -v ADVANCED | sed -E 's/^([A-Za-z_]+):[A-Z]+=/\1=/' | sort >"$LOG/cmake-options.txt"
cmake --build "$B" --parallel 14 --target bpftime-agent bpftime-syscall-server bpftimetool simple-benchmark-with-embed-ebpf-calling "$@" >"$LOG/build.log" 2>&1
mkdir -p "$OUT"
cp -a "$B/runtime/agent/libbpftime-agent.so" "$B/runtime/syscall-server/libbpftime-syscall-server.so" "$B/tools/bpftimetool/bpftimetool" "$OUT/"
find "$B" -name simple-benchmark-with-embed-ebpf-calling -type f -exec cp -a {} "$OUT/" \;
for t in "$@"; do
  case $t in
    bpftime_text_segment_transformer) cp -a "$B/attach/text_segment_transformer/libbpftime-agent-transformer.so" "$OUT/";;
    bpftime-cli-cpp|bpftime) f=$(find "$B/tools/cli" -maxdepth 1 -type f -name bpftime | head -1); cp -a "$f" "$OUT/";;
  esac
done
cp "$LOG"/{source-state.txt,toolchain.txt,cmake-options.txt} "$OUT/"
(cd "$OUT" && sha256sum $(ls | grep -v -E '\.txt$') >sha256.txt)
date --iso-8601=seconds >"$LOG/finished-at.txt"
echo "built $ARM"
