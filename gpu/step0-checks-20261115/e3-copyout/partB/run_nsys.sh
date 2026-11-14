#!/bin/bash
# partB/run_nsys.sh OUTDIR [APP]: independent cross-check of the shim's graph
# counters, and evidence that the copier's copies overlap the application's
# kernels. Two nsys runs of tg128 (-r 1, default warm-up, graphs on): shim
# idle and shim copying 16 MiB back to back. nsys traces kernels inside
# graphs node by node (--cuda-graph-trace=node), as the census did.
# Writes OUTDIR/partB-nsys/<tag>.{nsys-rep,sqlite,parse.json,shim.json}.
# Expected wall: about 1.5 min. Each nsys step is bounded by timeout 300 s.
# APP=captest runs the same pipeline on build/captest instead (a smoke of the
# script that needs no llama-bench run).
set -u
E3=$(cd "$(dirname "$0")/.." && pwd)
SHIM=$E3/build/libstep0_copier.so
N=/opt/nvidia/nsight-systems/2026.3.1/bin/nsys
L=${LLAMA_BENCH:-/home/jetson/src/workloads/llama.cpp/build-ptx87/bin/llama-bench}  # LLAMA_BENCH: a stub for dry runs
M=/home/jetson/src/workloads/models/tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf
O=${1:?usage: partB/run_nsys.sh OUTDIR [llama|captest]}/partB-nsys
APP=${2:-llama}
mkdir -p "$O"
if [ "$APP" = captest ]; then
  CMD=("$E3/build/captest" --mode relaxed --seconds 1 --out "$O/captest.jsonl")
  EXE=captest
else
  CMD=("$L" -m "$M" -p 0 -n 128 -r 1 -o json)
  EXE=llama-bench
fi
for c in idle:0:0 cont:16777216:0; do
  name=${c%%:*}; rest=${c#*:}; bytes=${rest%%:*}; period=${rest#*:}
  tag=$APP-$name
  # nsys injects through LD_PRELOAD/CUDA_INJECTION64_PATH; the shim is appended
  # to LD_PRELOAD inside the target's shell so nsys's own entries stay.
  timeout -k 5 300 env -u GGML_CUDA_DISABLE_GRAPHS STEP0_ONLY_EXE=$EXE STEP0_COPY_BYTES=$bytes \
    STEP0_COPY_PERIOD_US=$period "STEP0_LOG=$O/$tag.shim.json" \
    "$N" profile -t cuda --cuda-graph-trace=node --force-overwrite true -o "$O/$tag" \
    /bin/sh -c 'export LD_PRELOAD="${LD_PRELOAD:+$LD_PRELOAD:}$1"; shift; exec "$@"' sh "$SHIM" "${CMD[@]}" \
    > "$O/$tag.out" 2> "$O/$tag.err"
  echo "nsys $tag rc=$?"
  timeout -k 5 300 "$N" export --type sqlite --force-overwrite true -o "$O/$tag.sqlite" "$O/$tag.nsys-rep" \
    > /dev/null 2>&1
  echo "export $tag rc=$?"
  python3 -I "$E3/partB/nsys_parse.py" "$O/$tag.sqlite" "$bytes" > "$O/$tag.parse.json"
  echo "parse $tag rc=$?"
done
