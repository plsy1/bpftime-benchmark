#!/bin/bash
# run_e.sh OUT [hog|ctl] [REPS] [HOG_S] [KERNEL_MS] [extra ids options ...]
#   ctl: ids case e alone (control). hog: ./build/hog in a second process first (its own context,
#   full-occupancy kernels of KERNEL_MS each for HOG_S seconds), then ids case e while it runs.
#   Total wall time is bounded: hog <= HOG_S + 25 s, ids e <= 60 s.
set -u
D=$(cd "$(dirname "$0")" && pwd)
OUT=$1; MODE=${2:-hog}; REPS=${3:-3}; HOG_S=${4:-12}; KMS=${5:-50}
shift 5 2>/dev/null || shift $#
mkdir -p "$OUT"
cd "$D" || exit 2
if [ "$MODE" = ctl ]; then
  timeout -k 5 60 ./build/ids e "$OUT" reps="$REPS" "$@" 2>&1 | tee "$OUT/ids_e.log"
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
if [ $ok != 1 ]; then echo "hog not ready" | tee "$OUT/ids_e.log"; wait "$HP"; exit 3; fi
sleep 0.5  # let the hog's first kernel occupy the GPU
timeout -k 5 60 ./build/ids e "$OUT" reps="$REPS" "$@" 2>&1 | tee "$OUT/ids_e.log"
rc=${PIPESTATUS[0]}
# the hog must still have been running when ids finished, or the last launches were not contended
if kill -0 "$HP" 2>/dev/null; then echo "hog_alive_at_end=1" >> "$OUT/ids_e.log"; else echo "hog_alive_at_end=0" >> "$OUT/ids_e.log"; fi
wait "$HP"; echo "hog_rc=$?" >> "$OUT/ids_e.log"
cat "$OUT/hog.log"
exit "$rc"
