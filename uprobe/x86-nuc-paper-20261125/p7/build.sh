#!/usr/bin/env bash
# P7 (x86 port of the Jetson E13 start-up protocol). Builds the helpers from the Jetson sources (git objects of
# origin/benchmark-results/jetson 8c4ec8ee: output-dims-20261106/src, more-dims-20261106/n2-startup/src,
# startup-20261108/concurrent-start/src, remeasure-20261108/memleak-start/src; copied unchanged into src/) with the
# x86 toolchain and the official tree's static libbpf (as E5/P10). Changes vs prep/E13-build.sh: x86 target and
# include dirs; gcc/clang versions; stw (stampwrap, static) built here (the Jetson used ~/src/rp/stw).
# Run under the measurement lock.
set -euo pipefail
cd "$(dirname "$0")"
LB=/home/y/src/bpftime-paper/official-4708f409/example/tracing/sslsniff-paper-s4/.output
CLANG=${CLANG:-clang-19}
W=workloads
rm -rf "$W"; mkdir -p "$W"/{out,n2,burst,stamp}
cp -p src/out/{outattach.c,out.bpf.c} "$W/out/"
cp -p src/n2/{shortlived.c,spawnloop.c,many.py} "$W/n2/"
cp -p src/burst/spawnburst.c "$W/burst/"
cp -p src/stamp/{stampmain.c,stampwrap.c} "$W/stamp/"
(
  cd "$W/out"
  gcc -O2 -g -pthread -I"$LB" -o outattach outattach.c "$LB/libbpf.a" -lelf -lz
  $CLANG -O2 -g -target bpf -D__TARGET_ARCH_x86 -I"$LB" -I/usr/include/x86_64-linux-gnu -DKIND=0 -c out.bpf.c \
      -o out-control.bpf.o
  cd ../n2
  gcc -O2 -g -o shortlived shortlived.c
  gcc -O2 -g -o spawnloop spawnloop.c
  for m in 1 10 100 1000; do python3 -I many.py $m; gcc -O2 -g -o many-$m many-$m.c; done
  cd ../burst
  gcc -O2 -g -o spawnburst spawnburst.c
  cd ../stamp
  gcc -O2 -g -o stampmain stampmain.c
  gcc -static -O2 -o stw stampwrap.c
) >"$W/build.log" 2>&1
{ echo "libbpf: $LB ($(grep -h '^Version' "$LB"/pkgconfig/libbpf.pc 2>/dev/null))"; gcc --version | head -1; $CLANG --version | head -1; } >"$W/toolchain.txt"
(cd "$W" && sha256sum out/outattach out/out-control.bpf.o n2/shortlived n2/spawnloop n2/many-{1,10,100,1000} \
    burst/spawnburst stamp/stampmain stamp/stw >sha256.txt)
cat "$W/sha256.txt"
