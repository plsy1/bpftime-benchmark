#!/bin/bash
# run_all.sh: check 3 end to end on a quiet board. Expected wall about 9 min:
#   preflight + check_sass ~5 s, part A ~2 min, part B0 ~60 s,
#   part B llama ~5 min, nsys cross-check ~1.5 min, part C ~30 s, summary ~5 s.
# Run (binaries must exist: ./build.sh first):
#   timeout -k 10 1800 ./run_all.sh
# Env: OUT=<dir> (default out/<date-time>, must not exist); SKIP_A, SKIP_B0,
# SKIP_LLAMA, SKIP_NSYS, SKIP_C = 1 to leave a part out; ALLOW_DIRTY=1 to run
# despite a failed preflight; ROUNDS, REPS, SECS pass through to the parts.
set -u
E3=$(cd "$(dirname "$0")" && pwd)
OUT=${OUT:-$E3/out/$(date +%Y%m%d-%H%M%S)}
[ -e "$OUT" ] && { echo "exists: $OUT"; exit 3; }
mkdir -p "$OUT"
cleanup() {
  # the programs unlink their shm names right after mmap; this only catches
  # a process killed in between. Only check-3 names (step0_e3_) are touched.
  for f in /dev/shm/step0_e3_*; do
    [ -e "$f" ] && rm -f -- "$f"
  done
}
trap cleanup EXIT
bash "$E3/preflight.sh" "$OUT" || exit 1
bash "$E3/check_sass.sh" > "$OUT/check_sass.txt" || { cat "$OUT/check_sass.txt"; echo "check_sass failed: run ./build.sh"; exit 1; }
T0=$(date +%s)
[ "${SKIP_A:-0}" = 1 ] || bash "$E3/partA/run.sh" "$OUT"
[ "${SKIP_B0:-0}" = 1 ] || bash "$E3/partB/run_b0.sh" "$OUT"
[ "${SKIP_LLAMA:-0}" = 1 ] || bash "$E3/partB/run_llama.sh" "$OUT"
[ "${SKIP_LLAMA:-0}" = 1 ] || [ "${SKIP_NSYS:-0}" = 1 ] || bash "$E3/partB/run_nsys.sh" "$OUT"
[ "${SKIP_C:-0}" = 1 ] || bash "$E3/partC/run.sh" "$OUT"
{
  echo "wall_s $(( $(date +%s) - T0 ))"
  echo "cpu5_khz_after $(cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq)"
  echo "gpu_hz_after $(cat /sys/devices/platform/bus@0/17000000.gpu/devfreq/17000000.gpu/cur_freq)"
  echo "step0 shm after: $(ls /dev/shm | grep '^step0_' | tr '\n' ' ')"
} > "$OUT/postflight.txt"
cat "$OUT/postflight.txt"
python3 -I "$E3/summarize.py" "$OUT" > /dev/null
echo "summary: $OUT/summary.md"
