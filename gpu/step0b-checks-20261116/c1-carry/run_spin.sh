#!/bin/bash
# run_spin.sh OUT <hog|ctl> [HOG_S] [KERNEL_MS] [c1check spin options ...]
#   ctl: c1check spin alone. hog: build/hog (copied from step0/e1-ids) first, in
#   a second process (its own context, full-occupancy kernels of KERNEL_MS
#   each for HOG_S seconds), so the GPU time-slices between the two contexts;
#   then c1check spin while it runs. Bounded: hog <= HOG_S + 25 s, c1check
#   <= 60 s. As step0/e1-ids/run_e.sh.
set -u
D=$(cd "$(dirname "$0")" && pwd)
OUT=$1; MODE=${2:-hog}; HOG_S=${3:-8}; KMS=${4:-50}
shift 4 2>/dev/null || shift $#
mkdir -p "$OUT"
cd "$D" || exit 2
if [ "$MODE" = ctl ]; then
	timeout -k 5 60 ./build/c1check spin "$OUT" "$@" 2>&1 | tee "$OUT/c1check.log"
	exit "${PIPESTATUS[0]}"
fi
rm -f "$OUT/hog.ready"
timeout -k 5 $((HOG_S + 25)) ./build/hog "$HOG_S" "$KMS" "$OUT/hog.ready" "$OUT" > "$OUT/hog.log" 2>&1 &
HP=$!
echo "$HP" > "$OUT/hog.pid"
ok=0
for i in $(seq 1 100); do
	[ -e "$OUT/hog.ready" ] && { ok=1; break; }
	kill -0 "$HP" 2>/dev/null || break
	sleep 0.1
done
if [ $ok != 1 ]; then echo "hog not ready" | tee "$OUT/c1check.log"; wait "$HP"; exit 3; fi
sleep 0.5 # let the hog's first kernel occupy the GPU
timeout -k 5 60 ./build/c1check spin "$OUT" "$@" 2>&1 | tee "$OUT/c1check.log"
rc=${PIPESTATUS[0]}
# the hog must still be running when c1check ends, or the last launches were not contended
if kill -0 "$HP" 2>/dev/null; then echo "hog_alive_at_end=1" >> "$OUT/c1check.log"; else echo "hog_alive_at_end=0" >> "$OUT/c1check.log"; fi
wait "$HP"; echo "hog_rc=$?" >> "$OUT/c1check.log"
cat "$OUT/hog.log"
exit "$rc"
