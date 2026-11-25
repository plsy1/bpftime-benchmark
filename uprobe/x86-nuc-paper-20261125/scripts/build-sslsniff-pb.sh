#!/usr/bin/env bash
# I4 (H3): build the stage-4 counting loader with SSLSNIFF_PB_PAGES support as /sslsniff/sslsniff-s4pb.
# Same BPF object and loader as sslsniff-s4 except sslsniff-pb.c's perf buffer size from the environment.
# Takes the measurement lock (non-blocking). Appends to sslsniff/sha256.txt; existing binaries untouched.
set -euo pipefail
exec 9>/home/y/src/bpftime-x86-runs/paper-20261124/.measure.lock
flock -n 9 || { echo "refusing: measurement lock held"; exit 3; }
R=/home/y/src/bpftime-x86-runs/paper-20261124
SRC=$R/sslsniff-src
T=/home/y/src/bpftime-paper/official-4708f409/example/tracing
OUT=$R/sslsniff
d=$T/sslsniff-paper-s4pb
[ -e "$OUT/sslsniff-s4pb" ] && { echo "refusing: $OUT/sslsniff-s4pb exists"; exit 2; }
rm -rf "$d"; mkdir -p "$d"
cp "$SRC/Makefile" "$SRC/sslsniff.h" "$SRC/sslsniff.bpf.c" "$d/"
cp "$SRC/sslsniff-pb.c" "$d/sslsniff.c"
make -C "$d" CLANG="${CLANG:-clang-19}" -j8 >"$R/logs/sslsniff/s4pb.log" 2>&1
cp "$d/sslsniff" "$OUT/sslsniff-s4pb"
cmp <(llvm-objdump-19 -d --no-show-raw-insn "$d/.output/sslsniff.bpf.o") "$R/logs/sslsniff/s4.objdump" && echo "BPF object identical to s4"
cp "$OUT/sha256.txt" "$OUT/sha256.txt.pre-s4pb"
(cd "$OUT" && sha256sum sslsniff-s4pb >> sha256.txt)
echo "built s4pb"
