#!/usr/bin/env bash
# E13 prerequisites: the start-up helpers. Only sources remain in the old directories (no binaries),
# and the libbpf they were built against (bpftime-daily-state/.../build-jetson-daily/libbpf) is gone
# (map-table-20261028/src/inc/bpf dangles). Rebuilt from the same sources with the same commands into
# workloads/E13, against the official build's libbpf 1.7.0 (same version as 10-28..11-08):
#   out/      outattach (static libbpf) + out-control.bpf.o   <- output-dims-20261106/src (build.sh lines 7, 11)
#   n2/       shortlived, spawnloop, many-{1,10,100,1000}     <- more-dims-20261106/n2-startup/src (build.sh)
#   burst/    spawnburst                                      <- startup-20261108/concurrent-start/src
#             (10-08 compiled it into a temp dir at every import; prebuilt here)
#   stamp/    stampmain                                       <- remeasure-20261108/memleak-start/src
#             (P1 memleak-start; its static wrapper is the existing ~/src/rp/stw, recorded)
# Light: a few small C files and one BPF object, under a minute.
set -euo pipefail
R=$HOME/src/paper-runs-20261124
W=$R/workloads/E13
U=$HOME/src/results/uprobe
LB=$HOME/src/bpftime-paper/official-4708f409/build-bench-llvm16/libbpf
mkdir -p "$R/workloads"; mkdir "$W"
mkdir "$W"/{out,n2,burst,stamp,inc}
cp -p "$U"/output-dims-20261106/src/{outattach.c,out.bpf.c,build.sh} "$W/out/"
cp -p "$U"/more-dims-20261106/n2-startup/src/{shortlived.c,spawnloop.c,many.py,build.sh} "$W/n2/"
cp -p "$U"/startup-20261108/concurrent-start/src/spawnburst.c "$W/burst/"
cp -p "$U"/remeasure-20261108/memleak-start/src/{stampmain.c,stampwrap.c} "$W/stamp/"
# Only libbpf's bpf/ headers (its build dir also has a linux/ that shadows the uapi types); copied, not linked.
cp -r "$LB/bpf" "$W/inc/bpf"
{
  cd "$W/out"
  gcc-13 -O2 -g -pthread -I"$LB" -o outattach outattach.c "$LB/libbpf.a" -lelf -lz
  clang-15 -O2 -g -target bpf -D__TARGET_ARCH_arm64 -I"$W/inc" -I/usr/include/aarch64-linux-gnu \
      -DKIND=0 -c out.bpf.c -o out-control.bpf.o
  cd "$W/n2"
  gcc-13 -O2 -g -o shortlived shortlived.c
  gcc-13 -O2 -g -o spawnloop spawnloop.c
  for m in 1 10 100 1000; do python3 -I many.py $m; gcc-13 -O2 -g -o many-$m many-$m.c; done
  cd "$W/burst"
  gcc-13 -O2 -g -o spawnburst spawnburst.c
  cd "$W/stamp"
  gcc-13 -O2 -g -o stampmain stampmain.c
} >"$W/build.log" 2>&1
{ echo "libbpf: $LB ($(grep -h '^Version' "$LB"/pkgconfig/libbpf.pc 2>/dev/null))"; gcc-13 --version | head -1; clang-15 --version | head -1; } >"$W/toolchain.txt"
(cd "$W" && sha256sum out/outattach out/out-control.bpf.o n2/shortlived n2/spawnloop n2/many-{1,10,100,1000} \
    burst/spawnburst stamp/stampmain >sha256.txt)
sha256sum "$HOME/src/rp/stw" "$HOME/src/realprog-20261026-btf/libbpf-tools/memleak" >"$W/external-sha256.txt"
chmod -R a-w "$W"/{out,n2,burst,stamp,inc}
cat "$W/sha256.txt"
