#!/usr/bin/env bash
# E11 prerequisites: nothing is compiled. The prebuilt workloads still exist and are copied (frozen,
# with sha256) into workloads/E11 so the runs never read the old results directories:
#   churn/   <- results/uprobe/map-threads-20261028/src  (churn loader: bpftime's libbpf 1.7.0 static,
#               churnloop, churn-k4-v8.bpf.o, control.bpf.o, + sources and build.sh for the record)
#   layers/  <- results/syscall/breakdown-20261029/src   (tp_attach static, writeloop, libdummy_agent.so,
#               tp_other/tp_exit_empty/tp_count.bpf.o, + sources)
# Rebuilding is not possible as-is: both build.sh point at a libbpf build dir that no longer exists
# (bpftime-daily-state/.../build-jetson-daily/libbpf). If a rebuild is ever wanted, use
# ~/src/bpftime-paper/official-4708f409/build-bench-llvm16/libbpf (libbpf 1.7.0, same version).
set -euo pipefail
R=$HOME/src/paper-runs-20261124
W=$R/workloads/E11
mkdir -p "$R/workloads"; mkdir "$W"
C=$HOME/src/results/uprobe/map-threads-20261028/src
L=$HOME/src/results/syscall/breakdown-20261029/src
mkdir "$W/churn" "$W/layers"
cp -p "$C"/{churn,churnloop,churn-k4-v8.bpf.o,control.bpf.o,churn.c,churn.bpf.c,churnloop.c,build.sh} "$W/churn/"
cp -p "$L"/{tp_attach,writeloop,libdummy_agent.so,tp_other.bpf.o,tp_exit_empty.bpf.o,tp_count.bpf.o,tp_attach.c,writeloop.c,dummy_agent.c,tp_other.bpf.c,tp_exit_empty.bpf.c,tp_count.bpf.c,build.sh} "$W/layers/"
printf 'churn from %s\nlayers from %s\ncopied %s\n' "$C" "$L" "$(date --iso-8601=seconds)" >"$W/source.txt"
(cd "$W" && sha256sum churn/* layers/* >sha256.txt)
chmod -R a-w "$W/churn" "$W/layers"
cat "$W/sha256.txt"
