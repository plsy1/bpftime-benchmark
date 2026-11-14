#!/bin/bash
# step0 e6 part (c): does the agent register and prefault a large segment, how
# long does that take, what does it cost in memory, and does the GPU workload
# still work? Per size: one run.sh run with BPFTIME_SHM_MEMORY_MB=<size>; loader =
# e5_loader e5_fadd.bpf.o, victim = e5_victim 48 256 10 --report-mem --alloc-mib 256
# under the agent (from ../e5-atomics). "Works" = the e5 fetch-add check passes
# (counter == calls, no gaps) and the victim's 256 MiB cudaMalloc+memset succeeds.
# A size runs only if MemAvailable - size >= 1024 MiB and /dev/shm has room.
# env: E6_SIZES ("128 1024 1536 1792 2047 2048"; 2048 MiB = 2^31 B, 2047 tells a 2^31 limit apart), TS
# Wall: ~20-40 s per size.
set -u
H=/home/jetson/src/bpftime-gpu-work
D=$H/step0/e6-drain
E5=$H/step0/e5-atomics
SIZES=${E6_SIZES:-128 1024 1536 1792 2047 2048}
TS=${TS:-$(date +%Y%m%d-%H%M%S)}
O=$D/results/c-$TS
mkdir -p "$O"
NAMES=()
SAMP=""
cleanup() { [ -n "$SAMP" ] && kill "$SAMP" 2>/dev/null; for n in "${NAMES[@]}"; do rm -f "/dev/shm/bpftime_gpu_$n"; done; }
trap cleanup EXIT
export SPDLOG_LEVEL=info
( cd "$E5" && timeout 600 make > "$O/make-e5.log" 2>&1 ) || { echo "e5 build failed"; exit 2; }   # incremental
avail() { awk '/^MemAvailable:/{print int($2/1024)}' /proc/meminfo; }
shm_avail() { df -m --output=avail /dev/shm | tail -1 | tr -d ' '; }
echo "start $(date '+%F %T') sizes=$SIZES" > "$O/meta.txt"
for MB in $SIZES; do
  a=$(avail); s=$(shm_avail)
  echo "size $MB: MemAvailable $a MiB, /dev/shm avail $s MiB" | tee -a "$O/meta.txt"
  free -m > "$O/free-before-$MB.txt"; df -m /dev/shm >> "$O/free-before-$MB.txt"
  if [ $(( a - MB )) -lt 1024 ] || [ "$s" -lt "$MB" ]; then
    echo "skip $MB" | tee -a "$O/meta.txt"
    echo "{\"size_mb\":$MB,\"skipped\":true,\"mem_available_mib\":$a,\"shm_avail_mib\":$s}" >> "$O/c.jsonl"
    continue
  fi
  NAME=step0-e6c-$TS-$MB; NAMES+=("$NAME")
  # MemAvailable/Shmem every 0.2 s (at most 1,000 samples = 200 s)
  ( for i in $(seq 1 1000); do echo "$(date +%s.%N) $(awk '/^MemAvailable:|^Shmem:/{printf "%s %s ", $1, $2}' /proc/meminfo)"; sleep 0.2; done ) > "$O/mem-samples-$MB.txt" 2>/dev/null &
  SAMP=$!
  t0=$(date +%s.%N)
  SRV_ENV="BPFTIME_SHM_MEMORY_MB=$MB E5_OUT=$O/e5-$MB.json E5_LABEL=seg$MB E5_THREADS=12288 E5_LAUNCHES=10" \
    timeout -k 5 200 "$H/run.sh" "$NAME" "$E5" "./e5_loader e5_fadd.bpf.o" "./e5_victim 48 256 10 --report-mem --alloc-mib 256" 150 90 2 > "$O/runsh-$MB.txt" 2>&1
  # server timeout 150 > ready wait (<= 24 s) + victim timeout 90 + drain (2 + <= 10 s)
  rc=$?
  kill "$SAMP" 2>/dev/null; SAMP=""
  R=$H/runs/$NAME
  reg=$(grep -h 'Registered shared memory with CUDA' "$R/client.log" | head -1 | sed 's/"/\\"/g')
  regerr=$(grep -h 'cuMemHostRegister() failed\|Only agent side' "$R/client.log" | head -1 | sed 's/"/\\"/g')
  m_main=$(grep -h '^E5MEM .*"when":"main"' "$R/client.log" | head -1 | sed 's/^E5MEM //')
  m_end=$(grep -h '^E5MEM .*"when":"end"' "$R/client.log" | head -1 | sed 's/^E5MEM //')
  vic=$(grep -h '^E5VICTIM' "$R/client.log" | head -1 | sed 's/^E5VICTIM //')
  cms=$(grep -o 'client_ms=[0-9]*' "$R/meta.txt" | cut -d= -f2)
  e5=$(cat "$O/e5-$MB.json" 2>/dev/null)
  echo "{\"size_mb\":$MB,\"run_rc\":$rc,\"run\":\"$NAME\",\"mem_available_before_mib\":$a,\"client_ms\":${cms:-null},\"registered_line\":\"$reg\",\"register_error\":\"$regerr\",\"victim_main\":${m_main:-null},\"victim_end\":${m_end:-null},\"victim\":${vic:-null},\"e5\":${e5:-null}}" >> "$O/c.jsonl"
  tail -3 "$O/runsh-$MB.txt"
done
echo "end $(date '+%F %T')" >> "$O/meta.txt"
python3 -I "$D/summarize_e6.py" c "$O" | tee "$O/summary.txt"
