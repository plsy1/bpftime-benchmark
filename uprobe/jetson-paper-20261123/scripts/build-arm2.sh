#!/usr/bin/env bash
# Paper campaign, Jetson Orin: configure + build one frozen arm.
# Adapted from the x86 build-arm.sh (NUC). Differences: LLVM 16 + clang-15,
# Unix Makefiles, targets built one at a time (one LTO link at a time),
# -j4 compile, dropping to -j2 when MemAvailable < 1.5 GB (no swap).
# Usage: [BDIR_TAG=tag] [EXTRA_CMAKE="-D..."] build-arm2.sh <arm-name> <source-root> <bench|checks> [extra targets...]
# v2 (11-25): BDIR_TAG gives a fresh build dir next to an existing one; EXTRA_CMAKE adds cmake options (layout control).
set -euo pipefail
ARM=$1; ROOT=$2; KIND=$3; shift 3
R=$HOME/src/paper-runs-20261124
case $KIND in bench) CK=OFF;; checks) CK=ON;; *) echo "kind must be bench|checks"; exit 2;; esac
B=$ROOT/build-$KIND-llvm16${BDIR_TAG:+-$BDIR_TAG}
OUT=$R/builds/$ARM
LOG=$R/logs/build-$ARM
[ -e "$OUT" ] && { echo "refusing: $OUT exists"; exit 2; }
[ -e "$B" ] && { echo "refusing: $B exists (no build-dir reuse)"; exit 2; }
mkdir -p "$LOG"
EXTRA=()
if grep -q BPFTIME_ENABLE_MAP_PRODUCTION_AB "$ROOT/cmake/StandardSettings.cmake"; then
  EXTRA+=(-DBPFTIME_ENABLE_MAP_PRODUCTION_AB=OFF -DBPFTIME_ENABLE_MAP_USAGE_STATS=OFF -DBPFTIME_PATCH_EXECUTABLE=/usr/bin/patch)
fi
date --iso-8601=seconds >"$LOG/started-at.txt"
{
  echo "arm: $ARM"; echo "commit: $(git -C "$ROOT" rev-parse HEAD)"; echo "branch: $(git -C "$ROOT" rev-parse --abbrev-ref HEAD)"
  echo "log:"; git -C "$ROOT" log --oneline -5
  echo "vm/llvm-jit: $(git -C "$ROOT/vm/llvm-jit" rev-parse HEAD)"
  echo "submodules:"; git -C "$ROOT" submodule status --recursive | awk '{print $1,$2}'
  echo "tracked status (empty = clean):"; git -C "$ROOT" status --short --untracked-files=no
} >"$LOG/source-state.txt"
{
  { gcc-13 --version | head -1; } || true; { g++-13 --version | head -1; } || true
  { clang-15 --version | head -1; } || true; { /usr/lib/llvm-16/bin/llvm-config --version; } || true
  { ldd --version | head -1; } || true; { cmake --version | head -1; } || true; { make --version | head -1; } || true
} >"$LOG/toolchain.txt"

# memory log every 30 s for the whole build
( while sleep 30; do echo "$(date +%T) $(awk '/MemAvailable/{print int($2/1024)}' /proc/meminfo)MB"; done ) >"$LOG/free.log" &
MEMLOG=$!
trap 'kill $MEMLOG 2>/dev/null || true' EXIT

cmake -S "$ROOT" -B "$B" -G "Unix Makefiles" \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_C_COMPILER=/usr/bin/gcc-13 -DCMAKE_CXX_COMPILER=/usr/bin/g++-13 \
  -DBPFTIME_BPF_CLANG=/usr/bin/clang-15 -DDIAGNOSTIC_BPF_CLANG=/usr/bin/clang-15 -DLLVM_DIR=/usr/lib/llvm-16/cmake \
  -DBPFTIME_LLVM_JIT=ON -DBPFTIME_UBPF_JIT=ON -DBPFTIME_ENABLE_LTO=ON -DBPFTIME_ENABLE_UNIT_TESTING=OFF \
  -DBPFTIME_ENABLE_CUDA_ATTACH=OFF -DENABLE_EBPF_VERIFIER=OFF \
  -DENABLE_PROBE_READ_CHECK=$CK -DENABLE_PROBE_WRITE_CHECK=$CK \
  -DBUILD_BPFTIME_DAEMON=OFF -DBUILD_LLVM_AOT_CLI=OFF \
  -DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO "${EXTRA[@]}" ${EXTRA_CMAKE:-} >"$LOG/configure.log" 2>&1
grep -E '^(BPFTIME_[A-Z_]+|ENABLE_[A-Z_]+|SPDLOG_ACTIVE_LEVEL|CMAKE_BUILD_TYPE|LLVM_DIR|CMAKE_C_COMPILER|CMAKE_CXX_COMPILER|CMAKE_C_FLAGS|CMAKE_CXX_FLAGS|BUILD_[A-Z_]+)[:=]' "$B/CMakeCache.txt" \
  | grep -v ADVANCED | sed -E 's/^([A-Za-z_]+):[A-Z]+=/\1=/' | sort >"$LOG/cmake-options.txt"

: >"$LOG/build.log"
for t in bpftime-agent bpftime-syscall-server bpftimetool simple-benchmark-with-embed-ebpf-calling "$@"; do
  avail=$(awk '/MemAvailable/{print int($2/1024)}' /proc/meminfo)
  J=4; [ "$avail" -lt 1536 ] && J=2
  echo "=== $(date +%T) target $t -j$J (avail ${avail}MB)" | tee -a "$LOG/targets.log" >>"$LOG/build.log"
  cmake --build "$B" --parallel $J --target "$t" >>"$LOG/build.log" 2>&1
done

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
