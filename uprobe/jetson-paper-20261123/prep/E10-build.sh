#!/usr/bin/env bash
# E10 prerequisites: rebuild the consuming-lookup workloads (loader, victim, .bpf.o) for the
# ordinary and per-CPU arrays. The 10-21 binaries and their tree
# (~/src/bpftime-baseline-177a1dc-20261021) are gone. Same commands as
# results/uprobe/baseline-177a1dc-jit-llvm16-20261021/run-build.sh lines 60-75, sources from the
# frozen D tree (official 4708f409 has no benchmark/uprobe/diagnostics), output in a new directory.
# The .bpf.o's SEC path is relative ("uprobe/benchmark/uprobe/.output/array-jit-poc/victim:"), so the
# output keeps the tree layout and the runner uses the directory as cwd (ROOT).
# Uses read-only from the D tree: benchmark/uprobe/.output/{libbpf.a,bpf/,bpftool/bootstrap/bpftool}
# (built for the frozen bench uprobe), third_party/vmlinux/arm64, third_party/libbpf/include/uapi.
# Light: 2 BPF objects + 4 small C programs, about 1 min, no bpftime build.
set -euo pipefail
R=$HOME/src/paper-runs-20261124
T=$HOME/src/bpftime-paper/dev-9ccda7e8
W=$R/workloads/E10
mkdir -p "$R/workloads"; mkdir "$W"          # refuses an existing build
S=$T/benchmark/uprobe/diagnostics
I=$T/benchmark/uprobe/.output                 # libbpf.a, bpf/ headers, bpftool (read only)
O=$W/benchmark/uprobe/.output
git -C "$T" rev-parse HEAD >"$W/source.txt"
git -C "$T" status --short --untracked-files=no -- benchmark/uprobe/diagnostics >>"$W/source.txt"
for kind in ordinary percpu; do
    if [[ $kind == ordinary ]]; then dir=array-jit-poc; obj=array_jit_poc; loader=array_jit_poc_loader.c
    else dir=percpu-array-jit; obj=percpu_array_jit; loader=percpu_array_jit_loader.c; fi
    mkdir -p "$O/$dir"
    /usr/bin/clang-15 -g -O2 -target bpf -D__TARGET_ARCH_arm64 \
        -I"$T/third_party/vmlinux/arm64" -I"$I" -I"$T/third_party/libbpf/include/uapi" \
        -c "$S/$obj.bpf.c" -o "$O/$dir/$obj.bpf.o"
    "$I/bpftool/bootstrap/bpftool" gen skeleton "$O/$dir/$obj.bpf.o" >"$O/$dir/$obj.skel.h"
    /usr/bin/gcc-13 -g -O2 -I"$O/$dir" -I"$I" "$S/$loader" "$I/libbpf.a" -lelf -lz -o "$O/$dir/loader"
    /usr/bin/gcc-13 -g -O2 -fno-ipa-icf -rdynamic "$S/array_jit_poc_victim.c" -o "$O/$dir/victim"
done >"$W/build.log" 2>&1
{ /usr/bin/clang-15 --version | head -1; /usr/bin/gcc-13 --version | head -1; } >"$W/toolchain.txt"
(cd "$W" && sha256sum benchmark/uprobe/.output/*/{loader,victim,*.bpf.o} >sha256.txt)
chmod -R a-w "$W/benchmark"
cat "$W/sha256.txt"
echo "E10 workloads built in $W"
