#!/bin/bash
# step0 e6 part (a): drain throughput through intercepted bpf(), no GPU work.
# e6_drain bench runs as the loader under the syscall server; the "victim" is a
# plain `sleep` (NO_AGENT=1) that only keeps run.sh from stopping the loader
# before the bench ends. Segment: BPFTIME_SHM_MEMORY_MB=512 (pools: 64 MiB of
# 40-B values and 32 MiB each of 4-KiB and 1-MiB values, for ARRAY and 1503->1513).
# env: E6_SECONDS (lookup time per config, default 5), TS
# Wall: the sleep victim holds 7 x (E6_SECONDS + 4) + 20 s, plus ready wait and drain; default ~95 s.
set -u
H=/home/jetson/src/bpftime-gpu-work
D=$H/step0/e6-drain
SECS=${E6_SECONDS:-5}
TS=${TS:-$(date +%Y%m%d-%H%M%S)}
O=$D/results/a-$TS
NAME=step0-e6a-$TS
mkdir -p "$O"
trap 'rm -f /dev/shm/bpftime_gpu_$NAME' EXIT
export SPDLOG_LEVEL=info
cd "$D" || exit 2
timeout 300 make > "$O/make.log" 2>&1 || { echo "build failed"; exit 2; }   # incremental; never run a stale binary
HOLD=$(( 7 * (SECS + 4) + 20 ))
{ echo "start $(date '+%F %T') secs=$SECS hold=$HOLD"; free -m; df -h /dev/shm; } > "$O/meta.txt"
NO_AGENT=1 SRV_ENV="BPFTIME_SHM_MEMORY_MB=512 E6_SECONDS=$SECS E6_OUT=$O/a.jsonl" \
  timeout -k 5 $(( HOLD + 60 )) "$H/run.sh" "$NAME" "$D" "./e6_drain bench" "sleep $HOLD" $(( HOLD + 40 )) $(( HOLD + 5 )) 2
echo "run.sh rc=$?" | tee -a "$O/meta.txt"
grep -h 'type 1503 exports\|creating type' "$H/runs/$NAME/server.log" | head -3 > "$O/map-type-lines.txt"
echo "end $(date '+%F %T')" >> "$O/meta.txt"
python3 -I "$D/summarize_e6.py" a "$O" | tee "$O/summary.txt"
