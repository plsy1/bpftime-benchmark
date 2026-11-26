#!/usr/bin/env bash
# E6/I5 x86: bpftime examples built CO-RE in "-core" copies inside the official 4708f409 tree's example/
# (same recipe as the Jetson's prep/E12-build.sh, clang-19 instead of clang-15). Tracked files untouched.
set -uo pipefail
E=/home/y/src/bpftime-x86-runs/paper-20261124/e6
T=/home/y/src/bpftime-paper/official-4708f409
EX=$T/example
L=$E/build/examples
mkdir -p "$L"
git -C "$T" status --short --untracked-files=no >"$L/tracked-before.txt"
for d in malloc tracing/funclatency minimal/usdt_minimal tracing/bashreadline tracing/sslsniff \
         bpf_features/get_stack_id_example tracing/opensnoop tracing/syscount; do
    c=$EX/$d-core
    [ -e "$EX/$d" ] || { echo "no $d" >>"$L/build.log"; continue; }
    [ ! -e "$c" ] || { echo "$c exists; refusing to overwrite"; exit 1; }
    cp -r "$EX/$d" "$c"
    echo "=== $d-core" >>"$L/build.log"
    nice make -C "$c" CLANG=/usr/bin/clang-19 LLVM_STRIP=llvm-strip-19 -j4 >>"$L/build.log" 2>&1
    echo "rc=$?" >>"$L/build.log"
done
git -C "$T" status --short --untracked-files=no >"$L/tracked-after.txt"
cmp -s "$L/tracked-before.txt" "$L/tracked-after.txt" && echo "tracked unchanged" >>"$L/build.log"
echo EXAMPLES-DONE
