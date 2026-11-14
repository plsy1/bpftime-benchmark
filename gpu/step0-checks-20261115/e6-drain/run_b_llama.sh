#!/bin/bash
# step0 e6 part (b) with llama.cpp as the GPU workload. PREPARED, NOT RUN by W4.
# Arms: off (loader idle), d40 (40-B lookups from a 64 MiB 1513 pool, all the
# time), hog4 (4-thread memcpy, the positive control). Each arm is one run.sh
# run: loader = e6_drain const <arm>, victim = llama-bench pp512 -r 3 with its
# warm-up, as a plain process (NO_AGENT=1, CUDA graphs at llama.cpp's default:
# GGML_CUDA_DISABLE_GRAPHS unset). Arm order rotates per round (ABC, BCA, CAB, ...).
# env: E6_LLAMA_ROUNDS (6), TS. Segment: BPFTIME_SHM_MEMORY_MB=512.
#      E6_LLAMA_CMD: another victim command (wiring smoke only, e.g.
#      ./test-fixtures/fake_llama_bench.sh, which prints a saved llama-bench JSON).
# Memory: each arm runs only if MemAvailable >= its need (segment 512 MiB, llama
# ~1 GiB, hog4 4 x 2 x 64 MiB, 1 GiB to spare); a skipped arm is recorded.
# Wall: ~ 3 arms x rounds x ~25 s (model load ~5 s + warm-up + 3 reps + run.sh
# drain 2 s + ready wait); default ~8 min.
set -u
H=/home/jetson/src/bpftime-gpu-work
D=$H/step0/e6-drain
L=/home/jetson/src/workloads/llama.cpp/build-ptx87/bin/llama-bench
M=/home/jetson/src/workloads/models/tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf
RN=${E6_LLAMA_ROUNDS:-6}
VCMD=${E6_LLAMA_CMD:-$L -m $M -p 512 -n 0 -r 3 -o json}
# CUDA graphs on: llama.cpp only tests whether GGML_CUDA_DISABLE_GRAPHS is set
unset GGML_CUDA_DISABLE_GRAPHS
TS=${TS:-$(date +%Y%m%d-%H%M%S)}
O=$D/results/b-llama-$TS
mkdir -p "$O"
NAMES=()
trap 'for n in "${NAMES[@]}"; do rm -f /dev/shm/bpftime_gpu_$n; done' EXIT
export SPDLOG_LEVEL=info
cd "$D" || exit 2
timeout 300 make > "$O/make.log" 2>&1 || { echo "build failed"; exit 2; }   # incremental; never run a stale binary
{ echo "start $(date '+%F %T') rounds=$RN victim=$VCMD"; free -m; } > "$O/meta.txt"
ARMS=(off d40 hog4)
for r in $(seq 1 "$RN"); do
  for j in 0 1 2; do
    arm=${ARMS[$(( (r - 1 + j) % 3 ))]}
    NEED=2560; [ "$arm" = hog4 ] && NEED=3072
    AV=$(awk '/^MemAvailable:/{print int($2/1024)}' /proc/meminfo)
    if [ "$AV" -lt "$NEED" ]; then
      echo "round $r arm $arm skipped: MemAvailable $AV MiB < $NEED MiB" | tee -a "$O/meta.txt"; continue
    fi
    NAME=step0-e6bl-$TS-r$r-$arm; NAMES+=("$NAME")
    NO_AGENT=1 SRV_ENV="BPFTIME_SHM_MEMORY_MB=512 E6_OUT=$O/drain-r$r-$arm.jsonl" \
      timeout -k 5 240 "$H/run.sh" "$NAME" "$D" "./e6_drain const $arm" \
      "$VCMD" 200 150 2 > "$O/runsh-r$r-$arm.txt" 2>&1
    echo "round $r arm $arm run.sh rc=$?" | tee -a "$O/meta.txt"
    # llama-bench JSON (stdout of the victim) from the run's client.log
    sed -n '/^\[/,/^\]/p' "$H/runs/$NAME/client.log" > "$O/llama-r$r-$arm.json"
  done
done
echo "end $(date '+%F %T')" >> "$O/meta.txt"
python3 -I "$D/analyze_b_llama.py" "$O" | tee "$O/summary.txt"
