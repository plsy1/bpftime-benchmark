#!/usr/bin/env bash
# H2: build only the latency-measuring loader sslsniff-s4lat (stage-4 BPF object, same defines
# as sslsniff-s4, i.e. none: SSLSNIFF_STAGE defaults to 4) with the same Makefile, clang-19 and
# official 4708f409 third_party (libbpf, bpftool, vmlinux.h) as scripts/build-sslsniff.sh.
# Differences from build-sslsniff.sh, on purpose:
#  - user-space source is sslsniff-src/sslsniff-lat.c (sslsniff.c is untouched);
#  - the build directory is $R/sslsniff-lat-build/tree/example/tracing/sslsniff-paper-s4lat,
#    whose ../../../third_party is a symlink to the official tree's third_party, so nothing is
#    written into /home/y/src/bpftime-paper (read only);
#  - the other loaders are not rebuilt; sslsniff/sha256.txt only ever gets one line appended
#    (never rewritten). An existing sslsniff-<NAME> is never replaced: if NAME is already
#    listed with another sha256 the script refuses; build under a new NAME instead.
# NAME (env, default s4lat2) names the output: sslsniff/sslsniff-$NAME. s4lat was the first
# build (before the H2 review fixes) and stays as it is; run_paper.py's S4LAT_LOADER points at
# the current one.
# Must run under the measurement lock, taken by an ancestor process (checked in /proc/locks):
#   flock -n /home/y/src/bpftime-x86-runs/paper-20261124/.measure.lock \
#       /home/y/src/bpftime-x86-runs/paper-20261124/scripts/build-sslsniff-lat.sh
set -euo pipefail
R=/home/y/src/bpftime-x86-runs/paper-20261124
SRC=$R/sslsniff-src
OFF=/home/y/src/bpftime-paper/official-4708f409
OUT=$R/sslsniff
CLANG=${CLANG:-clang-19}
NAME=${NAME:-s4lat2}
# The lock must be held by this process or one of its ancestors (not by some other session).
LOCK=$R/.measure.lock
ino=$(stat -c %i "$LOCK")
holders=$(awk -v ino="$ino" '$2 == "FLOCK" { n = split($6, a, ":"); if (a[n] == ino) print $5 }' /proc/locks)
mine=0; p=$$
while [ "$p" -gt 1 ]; do
  for h in $holders; do [ "$h" = "$p" ] && mine=1; done
  p=$(awk '/^PPid:/ {print $2}' "/proc/$p/status" 2>/dev/null || echo 1)
done
if [ "$mine" != 1 ]; then
  echo "refusing: the measurement lock is not held by this process tree (holders: ${holders:-none});" \
       "run me under 'flock -n $LOCK'" >&2; exit 3
fi
ROOT=$R/sslsniff-lat-build/tree
d=$ROOT/example/tracing/sslsniff-paper-$NAME
mkdir -p "$ROOT/example/tracing" "$R/logs/sslsniff"
ln -sfn "$OFF/third_party" "$ROOT/third_party"
rm -rf "$d"; mkdir -p "$d"
cp "$SRC/Makefile" "$SRC/sslsniff.h" "$d/"
cp "$SRC/sslsniff-lat.c" "$d/sslsniff.c"
cp "$SRC/sslsniff.bpf.c" "$d/sslsniff.bpf.c"   # s4: no defines prepended
make -C "$d" CLANG="$CLANG" -j8 >"$R/logs/sslsniff/$NAME.log" 2>&1
llvm-objdump-19 -d --no-show-raw-insn "$d/.output/sslsniff.bpf.o" >"$R/logs/sslsniff/$NAME.objdump" 2>/dev/null || true
# The BPF program must be the s4 one: compare the disassembly (minus the file-name header).
if [ -f "$R/logs/sslsniff/s4.objdump" ]; then
  if diff <(tail -n +3 "$R/logs/sslsniff/s4.objdump") <(tail -n +3 "$R/logs/sslsniff/$NAME.objdump") >/dev/null; then
    echo "bpf object: disassembly identical to s4"
  else
    echo "bpf object: disassembly DIFFERS from s4 (see logs/sslsniff/$NAME.objdump)" >&2; exit 4
  fi
fi
new=$(sha256sum "$d/sslsniff" | cut -d' ' -f1)
if grep -q "  sslsniff-$NAME\$" "$OUT/sha256.txt"; then
  old=$(grep "  sslsniff-$NAME\$" "$OUT/sha256.txt" | cut -d' ' -f1)
  if [ "$old" = "$new" ]; then
    cmp -s "$d/sslsniff" "$OUT/sslsniff-$NAME" || { echo "sslsniff-$NAME differs from its listed sha256" >&2; exit 5; }
    echo "unchanged sslsniff-$NAME $new (not copied)"
  else
    echo "sslsniff-$NAME exists with sha256 $old, new build $new; not replaced (use a new NAME)" >&2; exit 5
  fi
elif [ -e "$OUT/sslsniff-$NAME" ]; then
  echo "sslsniff-$NAME exists but is not in sha256.txt; not replaced" >&2; exit 5
else
  cp "$d/sslsniff" "$OUT/sslsniff-$NAME"
  echo "$new  sslsniff-$NAME" >>"$OUT/sha256.txt"
  echo "built sslsniff-$NAME $new"
fi
(cd "$OUT" && sha256sum --check sha256.txt)
