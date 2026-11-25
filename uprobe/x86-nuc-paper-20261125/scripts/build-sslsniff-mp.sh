#!/usr/bin/env bash
# I2: build the stage-4 counting loader with one -p per nginx worker as /sslsniff/sslsniff-s4mp.
# Same BPF object and loader as sslsniff-s4 except sslsniff-mp.c's attach loop over several -p.
# Takes the measurement lock (non-blocking). Appends to sslsniff/sha256.txt; existing binaries untouched.
set -euo pipefail
exec 9>/home/y/src/bpftime-x86-runs/paper-20261124/.measure.lock
flock -n 9 || { echo "refusing: measurement lock held"; exit 3; }
R=/home/y/src/bpftime-x86-runs/paper-20261124
SRC=$R/sslsniff-src
T=/home/y/src/bpftime-paper/official-4708f409/example/tracing
OUT=$R/sslsniff
d=$T/sslsniff-paper-s4mp
[ -e "$OUT/sslsniff-s4mp" ] && { echo "refusing: $OUT/sslsniff-s4mp exists"; exit 2; }
rm -rf "$d"; mkdir -p "$d"
cp "$SRC/Makefile" "$SRC/sslsniff.h" "$SRC/sslsniff.bpf.c" "$d/"
cp "$SRC/sslsniff-mp.c" "$d/sslsniff.c"
make -C "$d" CLANG="${CLANG:-clang-19}" -j8 >"$R/logs/sslsniff/s4mp.log" 2>&1
cp "$d/sslsniff" "$OUT/sslsniff-s4mp"
cmp <(llvm-objdump-19 -d --no-show-raw-insn "$d/.output/sslsniff.bpf.o") "$R/logs/sslsniff/s4.objdump" && echo "BPF object identical to s4"
cp "$OUT/sha256.txt" "$OUT/sha256.txt.pre-s4mp"
(cd "$OUT" && sha256sum sslsniff-s4mp >> sha256.txt)
echo "built s4mp"
