#!/usr/bin/env bash
# E15: rebuild only sslsniff-s4 with the SSLSNIFF_PB_WAKEUP loader switch (patch from the NUC, sha 702ab070).
set -euo pipefail
R=$HOME/src/paper-runs-20261124
SRC=$R/sslsniff-src
T=$HOME/src/bpftime-paper/official-4708f409/example/tracing
OUT=$R/sslsniff
CLANG=${CLANG:-clang-15}
mkdir -p "$OUT" "$R/logs/sslsniff"
build() { # name loader(stock|cnt) defines...
  local name=$1 loader=$2; shift 2
  local d=$T/sslsniff-paper-$name
  rm -rf "$d"; mkdir -p "$d"
  cp "$SRC/Makefile" "$SRC/sslsniff.h" "$d/"
  if [ "$loader" = stock ]; then cp "$SRC/sslsniff.c.orig" "$d/sslsniff.c"; cp "$SRC/sslsniff.bpf.c.orig" "$d/sslsniff.bpf.c"
  else cp "$SRC/sslsniff.c" "$d/sslsniff.c"; { for x in "$@"; do echo "#define ${x/=/ }"; done; cat "$SRC/sslsniff.bpf.c"; } >"$d/sslsniff.bpf.c"; fi
  make -C "$d" CLANG="$CLANG" -j4 >"$R/logs/sslsniff/$name.log" 2>&1
  cp "$d/sslsniff" "$OUT/sslsniff-$name"
  llvm-objdump-15 -d --no-show-raw-insn "$d/.output/sslsniff.bpf.o" >"$R/logs/sslsniff/$name.objdump" 2>/dev/null || true
  echo "built $name"
}
build s4 cnt
(cd "$OUT" && sha256sum sslsniff-s1 sslsniff-s2h sslsniff-s2hm sslsniff-s2m sslsniff-s3 sslsniff-s4 sslsniff-stock >sha256.txt)
