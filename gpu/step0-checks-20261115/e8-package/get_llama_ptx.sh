#!/bin/bash
# step0 e8, part 1: one threadhist-llama run on llama-bench (pp32, graphs off)
# with BPFTIME_CUDA_KEEP_PTX=1, to keep the patched PTX of the module that holds
# rms_norm (the one with the trampoline and map_info). Copies it to ptx/ and
# removes only the /tmp/bpftime-fatbin-work.* dirs this run's agent log names.
# Wall: ~60-120 s (agent start-up ~15 s, PTX extraction and patching of
# llama.cpp's fatbin, one module compile, pp32). Total timeout 260 s.
set -u
H=/home/jetson/src/bpftime-gpu-work
D=$H/step0/e8-package
L=/home/jetson/src/workloads/llama.cpp/build-ptx87/bin/llama-bench
M=/home/jetson/src/workloads/models/tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf
TS=${TS:-$(date +%Y%m%d-%H%M%S)}
NAME=step0-e8-llamaptx-$TS
trap 'rm -f /dev/shm/bpftime_gpu_$NAME' EXIT
mkdir -p "$D/ptx"
export SPDLOG_LEVEL=info
free -m | tee "$D/ptx/free-before-$TS.txt"
SRV_ENV='BPFTIME_MAP_GPU_THREAD_COUNT=262144 THREADHIST_N=262144' \
VIC_ENV='GGML_CUDA_DISABLE_GRAPHS=1 BPFTIME_CUDA_KEEP_PTX=1' \
  timeout -k 5 260 "$H/run.sh" "$NAME" "$H/example/gpu/threadhist-llama" './threadhist' \
  "$L -m $M -p 32 -n 0 -r 1 --no-warmup -o json" 240 180 4
echo "run.sh rc=$?"
R=$H/runs/$NAME
KEPT=$(grep -ho 'Writing patched PTX to /tmp/bpftime-fatbin-work\.[A-Za-z0-9]*\|Extracted PTX kept in /tmp/bpftime-fatbin-work\.[A-Za-z0-9]*' "$R/client.log" | awk '{print $NF}' | sort -u)
echo "kept dirs: $KEPT" | tee "$D/ptx/kept-$TS.txt"
du -sh $KEPT 2>/dev/null | tee -a "$D/ptx/kept-$TS.txt"
P=""
for k in $KEPT; do
  for f in "$k"/patched.*.ptx; do
    [ -f "$f" ] || continue
    if grep -q '\.entry .*rms_norm_f32' "$f" && grep -q 'map_info' "$f"; then P="$P $f"; fi
  done
done
echo "patched PTX with rms_norm and map_info:$P" | tee -a "$D/ptx/kept-$TS.txt"
n=0
for f in $P; do
  n=$((n+1)); cp "$f" "$D/ptx/rmsnorm-module-$TS.$n.patched.ptx"
done
# the extracted (unpatched) counterpart of the same module, for comparison
for k in $KEPT; do
  for f in "$k"/*.ptx; do
    case $(basename "$f") in patched.*) continue;; esac
    grep -q '\.entry .*rms_norm_f32' "$f" && cp "$f" "$D/ptx/rmsnorm-module-$TS.unpatched.ptx"
  done
done
ls -la "$D/ptx"
grep -h 'lookup=' "$R/server.log" | tail -1
[ -n "$P" ] && for k in $KEPT; do rm -rf "$k"; done
ls /dev/shm | grep "^bpftime_gpu_$NAME" || echo shm-clean
