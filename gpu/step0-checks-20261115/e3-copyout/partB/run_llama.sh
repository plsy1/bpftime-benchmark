#!/bin/bash
# partB/run_llama.sh OUTDIR: llama-bench tokens/s with CUDA graphs on, with
# and without the copier shim, in interleaved rounds; plus one graphs-off
# control. Writes OUTDIR/partB-llama/{runs.csv, <tag>.json, <tag>.err,
# <tag>.shim.json}. Expected wall: about 5 min (ROUNDS=3, REPS=3).
# Each llama-bench run is bounded by timeout 300 s.
#
# Configurations (STEP0_COPY_BYTES / STEP0_COPY_PERIOD_US):
#   noshim   no LD_PRELOAD at all
#   idle     shim loaded, copier thread retains the context and allocates
#            16 MiB device + 16 MiB registered host memory, copies nothing
#   c1m1ms   1 MiB every 1 ms   (about 1.05 GB/s if every period is met)
#   c16m1ms  16 MiB every 1 ms  (asks 16.8 GB/s; expected to saturate)
#   cont     16 MiB back to back
# Tests: pp512 (-p 512 -n 0) and tg128 (-p 0 -n 128), each -r REPS with
# llama-bench's default warm-up run. Graphs on = GGML_CUDA_DISABLE_GRAPHS unset.
set -u
E3=$(cd "$(dirname "$0")/.." && pwd)
SHIM=$E3/build/libstep0_copier.so
L=${LLAMA_BENCH:-/home/jetson/src/workloads/llama.cpp/build-ptx87/bin/llama-bench}  # LLAMA_BENCH: a stub for dry runs
M=/home/jetson/src/workloads/models/tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf
O=${1:?usage: partB/run_llama.sh OUTDIR}/partB-llama
mkdir -p "$O"
ROUNDS=${ROUNDS:-3}
REPS=${REPS:-3}
CONFIGS=(noshim idle c1m1ms c16m1ms cont)
declare -A BYTES=([idle]=0 [c1m1ms]=1048576 [c16m1ms]=16777216 [cont]=16777216)
declare -A PERIOD=([idle]=0 [c1m1ms]=1000 [c16m1ms]=1000 [cont]=0)
TESTS=("pp512:-p 512 -n 0" "tg128:-p 0 -n 128")
clk() { echo "$(cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq)/$(cat /sys/devices/platform/bus@0/17000000.gpu/devfreq/17000000.gpu/cur_freq)"; }
echo "round,config,test,graphs,rc,wall_s,clk_before,clk_after" > "$O/runs.csv"

run1() { # round config test "args" graphs(on|off) reps
  local r=$1 c=$2 t=$3 args=$4 g=$5 reps=$6
  local tag=r$r-$c-$t
  [ "$g" = off ] && tag=$tag-graphsoff
  local envs=(-u GGML_CUDA_DISABLE_GRAPHS)
  [ "$g" = off ] && envs+=(GGML_CUDA_DISABLE_GRAPHS=1)
  if [ "$c" != noshim ]; then
    envs+=(LD_PRELOAD="$SHIM" STEP0_ONLY_EXE=llama-bench STEP0_COPY_BYTES="${BYTES[$c]}"
           STEP0_COPY_PERIOD_US="${PERIOD[$c]}" "STEP0_LOG=$O/$tag.shim.json")
  fi
  local cb ca t0 t1 rc
  cb=$(clk); t0=$(date +%s.%N)
  # shellcheck disable=SC2086
  timeout -k 5 300 env "${envs[@]}" "$L" -m "$M" $args -r "$reps" -o json > "$O/$tag.json" 2> "$O/$tag.err"
  rc=$?
  t1=$(date +%s.%N); ca=$(clk)
  echo "$r,$c,$t,$g,$rc,$(echo "$t1 - $t0" | bc),$cb,$ca" >> "$O/runs.csv"
  echo "llama $tag rc=$rc"
}

for ((r = 1; r <= ROUNDS; r++)); do
  n=${#CONFIGS[@]}
  for ((i = 0; i < n; i++)); do
    c=${CONFIGS[$(( (i + r - 1) % n ))]}  # rotate the order every round
    for tt in "${TESTS[@]}"; do
      run1 "$r" "$c" "${tt%%:*}" "${tt#*:}" on "$REPS"
    done
  done
done
# control: graphs off, shim idle: the shim must count 0 captures and 0 launches
run1 0 idle tg128 "-p 0 -n 128" off 1
