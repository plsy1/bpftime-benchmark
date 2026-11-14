#!/bin/bash
# step0 e6 part (b): does draining slow the GPU (CPU and GPU share DRAM)?
# One run.sh run: loader = e6_drain toggle (phases off/d40/off/d1m/off/hog1/off/hog4,
# E6_PHASE_S s each, E6_ROUNDS rounds); victim = e6_gpubw, a plain CUDA process
# (NO_AGENT=1) copying device memory for the whole schedule, each batch stamped
# with CLOCK_MONOTONIC. analyze_b.py matches batches to phases afterwards.
# d40 = 40-B lookups from a 64 MiB 1513 pool; d1m = 1-MiB lookups from a 64 MiB
# pool; hog1/hog4 = raw memcpy in 1/4 threads over 2 x 64 MiB per thread (the
# positive control: it must slow the copy kernel).
# env: E6_PHASE_S (5), E6_ROUNDS (4), TS. Segment: BPFTIME_SHM_MEMORY_MB=512.
# Wall: 8 x E6_PHASE_S x E6_ROUNDS + ~25 s; default ~185 s.
set -u
H=/home/jetson/src/bpftime-gpu-work
D=$H/step0/e6-drain
P=${E6_PHASE_S:-5}; R=${E6_ROUNDS:-4}
TS=${TS:-$(date +%Y%m%d-%H%M%S)}
O=$D/results/b-$TS
NAME=step0-e6b-$TS
mkdir -p "$O"
trap 'rm -f /dev/shm/bpftime_gpu_$NAME' EXIT
export SPDLOG_LEVEL=info
cd "$D" || exit 2
timeout 300 make > "$O/make.log" 2>&1 || { echo "build failed"; exit 2; }   # incremental; never run a stale binary
# memory: segment 512 MiB + hog buffers 4 x 2 x 64 MiB + GPU buffers 2 x 256 MiB, plus 1 GiB to spare
NEED=2560; AV=$(awk '/^MemAvailable:/{print int($2/1024)}' /proc/meminfo)
[ "$AV" -ge "$NEED" ] || { echo "skip: MemAvailable $AV MiB < $NEED MiB" | tee "$O/skipped.txt"; exit 0; }
SCHED=$(( 8 * P * R ))
GPU_S=$(( SCHED + 10 ))     # the schedule starts when the copy loop does (E6_GO_FILE)
{ echo "start $(date '+%F %T') phase_s=$P rounds=$R gpu_s=$GPU_S"; free -m; } > "$O/meta.txt"
rm -f "$O/go"
NO_AGENT=1 SRV_ENV="BPFTIME_SHM_MEMORY_MB=512 E6_PHASE_S=$P E6_ROUNDS=$R E6_OUT=$O/drain.jsonl E6_GO_FILE=$O/go" \
VIC_ENV="E6_BW_OUT=$O/bw.jsonl E6_GO_FILE=$O/go" \
  timeout -k 5 $(( GPU_S + 120 )) "$H/run.sh" "$NAME" "$D" "./e6_drain toggle" "./e6_gpubw $GPU_S 256 8" $(( GPU_S + 90 )) $(( GPU_S + 30 )) 2
echo "run.sh rc=$?" | tee -a "$O/meta.txt"
grep -h 'BWSUM' "$H/runs/$NAME/client.log" | tail -1 | tee -a "$O/meta.txt"
echo "end $(date '+%F %T')" >> "$O/meta.txt"
python3 -I "$D/analyze_b.py" "$O" | tee "$O/summary.txt"
