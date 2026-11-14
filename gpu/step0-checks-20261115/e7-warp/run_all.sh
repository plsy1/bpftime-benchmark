#!/bin/bash
# run_all.sh [OUT]: check 7 (standalone half). Builds, checks the SASS, runs every case with the
# correct helper (>= 4M calls per case) and both wrong-helper controls (>= 200k calls per case),
# once from the nvcc binary (native) and once from the PTX loaded by the driver JIT with plain
# "call" instructions (jit), then analyzes. Expected wall time about 15 s.
set -u
D=$(cd "$(dirname "$0")" && pwd)
OUT=${1:-$D/runs/$(date +%Y%m%d-%H%M%S)}
[ -e "$OUT" ] && { echo "exists: $OUT"; exit 3; }
mkdir -p "$OUT"
cd "$D" || exit 2
make -s all > "$OUT/build.log" 2>&1 || { echo "build failed"; cat "$OUT/build.log"; exit 2; }
python3 -I check_sass.py build/warp.sass > "$OUT/check_sass_native.txt" 2>&1
python3 -I check_sass.py build/warp_plaincall.sass > "$OUT/check_sass_jit.txt" 2>&1
tail -1 "$OUT/check_sass_native.txt" "$OUT/check_sass_jit.txt"
# evidence: the plain-call PTX, assembled offline, gives the same instructions as the native build
sass_ins() { grep -E 'Function :|/\*[0-9a-f]{4}\*/' "$1" | sed -E 's#/\* 0x[0-9a-f]+ \*/##g; s/[[:space:]]+/ /g'; }
if diff <(sass_ins build/warp.sass) <(sass_ins build/warp_plaincall.sass) > "$OUT/sass_native_vs_plaincall.diff"; then
  echo "SASS: native and plain-call PTX give identical instructions" | tee -a "$OUT/sass_native_vs_plaincall.txt"
else
  echo "SASS: native and plain-call PTX differ ($(wc -l < "$OUT/sass_native_vs_plaincall.diff") diff lines)" | tee -a "$OUT/sass_native_vs_plaincall.txt"
fi
cp build/warp.sass.helpers.txt "$OUT/helpers_native.sass.txt"; cp build/warp_plaincall.sass.helpers.txt "$OUT/helpers_jit.sass.txt"
ls /dev/shm > "$OUT/shm_before.txt"
{ date "+%F %T"; uname -r; echo "gpu cur_freq: $(cat /sys/class/devfreq/17000000.gpu/cur_freq 2>/dev/null)"; echo "loadavg: $(cat /proc/loadavg)"; ps -eo pid,pcpu,etime,args --sort=-pcpu | head -8 | cut -c1-160; } > "$OUT/env.txt"
rc=0
timeout -k 5 60 ./build/warp all all "$OUT/native" min_calls=4000000 ctl_min_calls=200000 2>&1 | tee "$OUT/native.log"
[ "${PIPESTATUS[0]}" = 0 ] || rc=1
timeout -k 5 60 ./build/warp all all "$OUT/jit" min_calls=4000000 ctl_min_calls=200000 ptx=build/warp_plaincall.ptx 2>&1 | tee "$OUT/jit.log"
[ "${PIPESTATUS[0]}" = 0 ] || rc=1
python3 -I analyze.py "$OUT"
pids=$(grep -ho '"pid": [0-9]*' "$OUT"/*/meta.json 2>/dev/null | awk '{print $2}')
for p in $pids; do
  for f in /dev/shm/cuda.shm.$(printf '%x' "$(id -u)").$(printf '%x' "$p").*; do
    [ -e "$f" ] && { echo "removing leftover $f" | tee -a "$OUT/shm_cleanup.txt"; rm -f "$f"; }
  done
done
diff "$OUT/shm_before.txt" <(ls /dev/shm) > "$OUT/shm_diff.txt" && echo "shm: unchanged" || { echo "shm changed:"; cat "$OUT/shm_diff.txt"; }
exit $rc
