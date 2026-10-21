#!/usr/bin/env bash
# Unpatched official 177a1dc with the baseline's LLVM 16 options, then the
# consuming array workloads: kernel / official / optimized 5466f0d, 6 rounds.
set -euo pipefail
OUT=/home/jetson/src/results/uprobe/consuming-official-vs-optimized-20261021
ROOT=/home/jetson/src/bpftime-official-177a1dc-llvm16-20261021
BUILD="$ROOT/build-jit"
BASE=/home/jetson/src/results/uprobe/baseline-177a1dc-jit-llvm16-20261021
trap 'exit 130' INT
trap 'exit 143' TERM
trap 'status=$?; printf "%s\n" "$status" >"$OUT/exit-status.txt"; date --iso-8601=seconds >"$OUT/finished-at.txt"' EXIT
date --iso-8601=seconds >"$OUT/started-at.txt"
test ! -e "$ROOT"
git -C /home/jetson/src/bpftime-jetson-daily worktree add --detach "$ROOT" 177a1dc465ff6e4122e3bda9b280018c9a324b80 >"$OUT/worktree.log" 2>&1
git -C "$ROOT" submodule update --init --recursive --jobs 6 >>"$OUT/worktree.log" 2>&1
cmake -S "$ROOT" -B "$BUILD" \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_C_COMPILER=/usr/bin/gcc-13 -DCMAKE_CXX_COMPILER=/usr/bin/g++-13 \
    -DBPFTIME_BPF_CLANG=/usr/bin/clang-15 -DLLVM_DIR=/usr/lib/llvm-16/cmake \
    -DBPFTIME_ENABLE_LTO=ON -DBPFTIME_ENABLE_UNIT_TESTING=ON \
    -DBPFTIME_ENABLE_CUDA_ATTACH=OFF -DENABLE_EBPF_VERIFIER=OFF \
    -DENABLE_PROBE_READ_CHECK=OFF -DENABLE_PROBE_WRITE_CHECK=OFF \
    -DBUILD_BPFTIME_DAEMON=OFF -DBUILD_LLVM_AOT_CLI=OFF \
    -DSPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO >"$OUT/configure.log" 2>&1
grep -E '^(BPFTIME_[A-Z_]+|ENABLE_[A-Z_]+|SPDLOG_ACTIVE_LEVEL|CMAKE_BUILD_TYPE|LLVM_DIR|CMAKE_C_COMPILER|CMAKE_CXX_COMPILER|CMAKE_C_FLAGS|CMAKE_CXX_FLAGS|BUILD_[A-Z_]+|TEST_LCOV)[:=]' "$BUILD/CMakeCache.txt" \
    | grep -v ADVANCED | sed -E 's/^([A-Za-z_]+):[A-Z]+=/\1=/' | sort >"$OUT/cmake-options.txt"
diff "$BASE/cmake-options.txt" "$OUT/cmake-options.txt" >"$OUT/cmake-options-vs-baseline.diff" || true
cmake --build "$BUILD" --parallel 6 --target bpftime-agent bpftime-syscall-server bpftimetool >"$OUT/build.log" 2>&1
A="$OUT/official-artifacts"
for item in runtime/agent/libbpftime-agent.so runtime/syscall-server/libbpftime-syscall-server.so tools/bpftimetool/bpftimetool; do
    mkdir -p "$A/$(dirname "$item")"; cp "$BUILD/$item" "$A/$item"
done
sha256sum "$A"/runtime/*/*.so "$A"/tools/bpftimetool/bpftimetool >"$OUT/official-artifact-sha256.txt"
{ echo "official: $(git -C "$ROOT" rev-parse HEAD)"; git -C "$ROOT" status --short --untracked-files=no; } >"$OUT/official-source-state.txt"
test -z "$(git -C "$ROOT" status --short --untracked-files=no)"
sha256sum --check --quiet "$BASE/artifact-sha256.txt"
test ! -e /dev/shm/bpftime_maps_shm
python3 -u "$OUT/run-ordinary.py" --output "$OUT/ordinary" --runs 6 --iterations 50000 >"$OUT/ordinary.log" 2>&1
python3 -u "$OUT/run-percpu.py" --output "$OUT/percpu" --runs 6 --iterations 50000 >"$OUT/percpu.log" 2>&1
