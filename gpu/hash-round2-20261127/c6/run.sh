#!/bin/bash
# One C6 run: a fresh process under the board's GPU lock, with a total timeout of 900 s.
# usage: run.sh <log name> <variant: A|B|C|D|S|SD> [c6 args...]
# Writes logs/<log name>.log (never overwrites an existing log).
set -u
D=$(cd "$(dirname "$0")" && pwd)
LOCK=/tmp/claude-2002/-home-jetson-src/548cc3e6-4b28-4a87-b830-6e00c1ee829e/scratchpad/gpu.lock
GPUF=/sys/devices/platform/bus@0/17000000.gpu/devfreq/17000000.gpu/cur_freq
cd "$D"
[ $# -ge 2 ] || { echo "usage: run.sh <log name> <variant> [args...]"; exit 5; }
name=$1
v=$2
shift 2
log=logs/$name.log
[ -e "$log" ] && { echo "refusing to overwrite $log"; exit 5; }
[ -x "build/c6_$v" ] || { echo "no build/c6_$v"; exit 5; }
{
  echo "### $name: build/c6_$v $*"
  echo "### queued $(date -Is) cpu5_khz=$(cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq) gpu_hz=$(cat $GPUF)"
  echo "### sha256 $(sha256sum build/c6_$v | cut -d' ' -f1) build/c6_$v"
  echo "### sha256 $(sha256sum src/c6.cu | cut -d' ' -f1) src/c6.cu"
  echo "### other GPU test processes before the run (c4_|c5_|c6_|llama|pt_victim):"
  ps aux | grep -E 'c4_|c5_|c6_|llama|pt_victim' | grep -v grep | sed 's/^/###   /'
} > "$log"
s=$(date +%s.%N)
flock -w 7200 "$LOCK" bash -c '
  echo "### lock held $(date -Is) gpu_hz=$(cat '"$GPUF"')"
  exec timeout 900 "$@"' _ "./build/c6_$v" "$@" >> "$log" 2>&1
rc=$?
echo "### exit=$rc outer_wall_s=$(echo "$(date +%s.%N) - $s" | bc) end $(date -Is)" >> "$log"
grep -E '^(RESULT|ERROR)' "$log"
tail -1 "$log"
exit $rc
