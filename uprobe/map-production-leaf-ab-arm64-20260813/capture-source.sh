#!/usr/bin/env bash
set -euo pipefail

DIAG=${DIAG:-/home/jetson/src/.worktrees/map-production-ab-arm64}
OUT=${OUT:-/home/jetson/src/benchmark-results/uprobe/map-production-leaf-ab-arm64-20260813}
tmp=$(mktemp)
trap 'rm -f "$tmp"' EXIT

git -C "$DIAG" diff --binary >"$tmp"
if [[ -f "$DIAG/runtime/src/bpf_map/map_production_ab.hpp" ]] && \
   ! git -C "$DIAG" ls-files --error-unmatch runtime/src/bpf_map/map_production_ab.hpp >/dev/null 2>&1; then
  git -C "$DIAG" diff --no-index --binary /dev/null \
    "$DIAG/runtime/src/bpf_map/map_production_ab.hpp" >>"$tmp" || true
fi
sed "s#b$DIAG/#b/#g; s#a$DIAG/#a/#g" "$tmp" >"$OUT/source.patch"

{
  echo "commit=$(git -C "$DIAG" rev-parse HEAD)"
  echo "branch=$(git -C "$DIAG" branch --show-current || true)"
  echo 'status_begin'
  git -C "$DIAG" status --short
  echo 'status_end'
  echo 'compiler_begin'
  gcc-13 --version | head -1
  /usr/lib/llvm-15/bin/clang --version | head -1
  rg '^#define BOOST_LIB_VERSION' /usr/include/boost/version.hpp
  echo 'compiler_end'
  echo 'cmake_cache_begin'
  rg '^(CMAKE_BUILD_TYPE|CMAKE_CXX_COMPILER|CMAKE_C_COMPILER|BPFTIME_LLVM_JIT|BPFTIME_ENABLE_LTO|ENABLE_PROBE_READ_CHECK|ENABLE_PROBE_WRITE_CHECK|Boost_INCLUDE_DIR):' \
    "$DIAG/build-map-production-ab-arm64/CMakeCache.txt"
  echo 'cmake_cache_end'
} >"$OUT/source-environment.txt"
