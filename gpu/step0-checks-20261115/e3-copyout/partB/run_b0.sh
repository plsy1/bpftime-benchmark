#!/bin/bash
# partB/run_b0.sh OUTDIR: the copier shim next to a capture-and-replay loop
# (captest), per capture mode and copier operation. Writes
# OUTDIR/partB0/{captest.jsonl, <case>.shim.json, runs.csv}.
# 14 cases of SECS (default 3) s each; expected wall about 1 min.
set -u
E3=$(cd "$(dirname "$0")/.." && pwd)
B=$E3/build
SHIM=$B/libstep0_copier.so
O=${1:?usage: partB/run_b0.sh OUTDIR}/partB0
mkdir -p "$O"
SECS=${SECS:-3}
ulimit -c 0  # ctxsync-relaxed crashed libcuda in a smoke; no core files
echo "case,mode,stream,shim_env,rc" > "$O/runs.csv"
# case mode stream "shim env (empty = no shim)"
CASES=(
  "noshim relaxed nonblocking -"
  "idle-relaxed relaxed nonblocking STEP0_COPY_BYTES=0"
  "async-relaxed relaxed nonblocking STEP0_COPY_BYTES=1048576"
  "async-global global nonblocking STEP0_COPY_BYTES=1048576"
  "async-tlocal thread_local nonblocking STEP0_COPY_BYTES=1048576"
  "asyncevent-relaxed relaxed nonblocking STEP0_COPY_BYTES=1048576,STEP0_WAIT=event"
  "asyncevent-global global nonblocking STEP0_COPY_BYTES=1048576,STEP0_WAIT=event"
  "legacysync-relaxed relaxed nonblocking STEP0_COPY_BYTES=1048576,STEP0_API=sync"
  "legacysync-relaxed-blk relaxed blocking STEP0_COPY_BYTES=1048576,STEP0_API=sync"
  "legacysync-global global nonblocking STEP0_COPY_BYTES=1048576,STEP0_API=sync"
  "memalloc-relaxed relaxed nonblocking STEP0_COPY_BYTES=1048576,STEP0_UNSAFE=memalloc"
  "memalloc-global global nonblocking STEP0_COPY_BYTES=1048576,STEP0_UNSAFE=memalloc"
  "ctxsync-relaxed relaxed nonblocking STEP0_COPY_BYTES=1048576,STEP0_UNSAFE=ctxsync"
  "ctxsync-global global nonblocking STEP0_COPY_BYTES=1048576,STEP0_UNSAFE=ctxsync"
)
for c in "${CASES[@]}"; do
  read -r name mode stream senv <<< "$c"
  envs=()
  if [ "$senv" != - ]; then
    envs=(LD_PRELOAD="$SHIM" STEP0_COPY_PERIOD_US=0 "STEP0_LOG=$O/$name.shim.json")
    IFS=, read -r -a kv <<< "$senv"
    envs+=("${kv[@]}")
  fi
  # the JSON line of captest carries no case name: write it first
  printf '{"case":"%s"}\n' "$name" >> "$O/captest.jsonl"
  timeout -k 5 "$(awk -v s="$SECS" 'BEGIN{printf "%d", s + 30}')" env "${envs[@]}" "$B/captest" --mode "$mode" --stream "$stream" \
    --seconds "$SECS" --out "$O/captest.jsonl" > "$O/$name.out" 2> "$O/$name.err"
  rc=$?
  # shim_env holds commas: quote it so that rc stays the last column
  echo "$name,$mode,$stream,\"$senv\",$rc" >> "$O/runs.csv"
  echo "b0 $name rc=$rc"
done
