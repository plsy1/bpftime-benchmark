#!/bin/bash
# run_all.sh OUTDIR [SECS]: C5's capture-mode remedy check (claim 10 of ../README.md).
# captest captures in GLOBAL mode on a non-blocking stream while the copier shim copies
# 1 MiB DtoH continuously from its own thread; the copier thread's own capture mode is the
# variable. 9 cases of SECS (default 3) s each; expected wall about 1 min at SECS=3
# (about 25 s at SECS=1).
# Run it under a total timeout:  timeout -k 5 300 ./run_all.sh OUT 3
set -u
D=$(cd "$(dirname "$0")" && pwd)
B=$D/build
O=${1:?usage: run_all.sh OUTDIR [SECS]}
SECS=${2:-3}
mkdir -p "$O"
ulimit -c 0 # the ctxsync case may crash libcuda (step 0 saw rc 139); no core files
if pgrep -x captest >/dev/null; then echo "another captest is running; not starting"; exit 3; fi
[ -x "$B/captest" ] && [ -f "$B/libstep0b_copier.so" ] || { echo "run build.sh first"; exit 3; }
: > "$O/captest.jsonl"
echo "case,copier_mode,shim_env,rc" > "$O/runs.csv"
# case  STEP0B_COPIER_MODE ("-" = not set: step 0's copier)  shim env ("-" = no shim)
CASES=(
  "noshim-global - -"
  "async-global-default - STEP0_COPY_BYTES=1048576"
  "async-global-global global STEP0_COPY_BYTES=1048576"
  "async-global-relaxed relaxed STEP0_COPY_BYTES=1048576"
  "async-global-tlocal thread_local STEP0_COPY_BYTES=1048576"
  "asyncevent-global-default - STEP0_COPY_BYTES=1048576,STEP0_WAIT=event"
  "asyncevent-global-relaxed relaxed STEP0_COPY_BYTES=1048576,STEP0_WAIT=event"
  "memalloc-global-relaxed relaxed STEP0_COPY_BYTES=1048576,STEP0_UNSAFE=memalloc"
  "ctxsync-global-relaxed relaxed STEP0_COPY_BYTES=1048576,STEP0_UNSAFE=ctxsync"
)
for c in "${CASES[@]}"; do
  read -r name cmode senv <<< "$c"
  envs=()
  if [ "$senv" != - ]; then
    envs=(LD_PRELOAD="$B/libstep0b_copier.so" STEP0_COPY_PERIOD_US=0 "STEP0_LOG=$O/$name.shim.json")
    IFS=, read -r -a kv <<< "$senv"
    envs+=("${kv[@]}")
    [ "$cmode" != - ] && envs+=("STEP0B_COPIER_MODE=$cmode")
  fi
  printf '{"case":"%s"}\n' "$name" >> "$O/captest.jsonl"
  # timeout outside the env/LD_PRELOAD wrapper
  timeout -k 5 "$((SECS + 30))" env "${envs[@]}" "$B/captest" --mode global --stream nonblocking \
    --seconds "$SECS" --out "$O/captest.jsonl" > "$O/$name.out" 2> "$O/$name.err"
  rc=$?
  echo "$name,$cmode,\"$senv\",$rc" >> "$O/runs.csv"
  echo "c5-capture $name rc=$rc"
done
python3 -I "$D/analyze.py" "$O" | tee "$O/verdict.txt"
