#!/bin/bash
# run.sh NAME EXDIR 'SERVER_CMD' 'VICTIM_CMD' [SERVER_T=90] [VICTIM_T=60] [DRAIN_S=6]
#   One loader under the syscall server and one victim under the agent, both from build-cuda,
#   on a shared memory segment of their own (bpftime_gpu_NAME, removed afterwards).
#   After the victim exits: wait until the server log stops growing (at least DRAIN_S s),
#   then SIGINT the loader (it polls once more and exits).
#   env: SRV_ENV / VIC_ENV = 'K=V ...' for the loader / victim; NO_AGENT=1 runs the victim plain;
#   NO_SMARCH=1 leaves BPFTIME_SM_ARCH unset (SM auto-detection).
set -u
R=/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate
B=$R/build-cuda
W=/home/jetson/src/bpftime-gpu-work
NAME=$1; EXDIR=$2; SCMD=$3; VCMD=$4; ST=${5:-90}; VT=${6:-60}; DRAIN=${7:-6}
O=$W/runs/$NAME; S=bpftime_gpu_$NAME
[ -e "$O" ] && { echo "run dir exists: $O"; exit 3; }
[ -e /dev/shm/$S ] && { echo "shm exists: /dev/shm/$S"; exit 4; }
mkdir -p "$O"
export BPFTIME_GLOBAL_SHM_NAME=$S BPFTIME_LOG_OUTPUT=console
cd "$EXDIR" || exit 2
{ echo "name=$NAME exdir=$EXDIR"; echo "server: SRV_ENV='${SRV_ENV:-}' $SCMD"; echo "victim: VIC_ENV='${VIC_ENV:-}' NO_AGENT='${NO_AGENT:-}' $VCMD"; echo "start $(date '+%F %T.%N')"; } > "$O/meta.txt"
timeout -k 5 -s INT "$ST" env ${SRV_ENV:-} LD_PRELOAD=$B/runtime/syscall-server/libbpftime-syscall-server.so $SCMD > "$O/server.log" 2>&1 &
SP=$!
# ready: the segment exists, then the server log quiet for 1 s or 4 s passed
# (loaders that print every second never go quiet)
for i in $(seq 1 100); do [ -e /dev/shm/$S ] && break; sleep 0.2; done
for i in 1 2 3 4; do
  s1=$(stat -c %s "$O/server.log"); sleep 1; s2=$(stat -c %s "$O/server.log")
  [ "$s1" = "$s2" ] && break
done
kill -0 $SP 2>/dev/null || { echo "server exited early"; wait $SP; echo "server_rc=$?"; tail -5 "$O/server.log"; exit 5; }
echo "server ready $(date '+%T.%N')" >> "$O/meta.txt"
if [ -n "$VCMD" ]; then
  PRE="LD_PRELOAD=$B/runtime/agent/libbpftime-agent.so"; [ -n "${NO_AGENT:-}" ] && PRE=
  t0=$(date +%s%N)
  SMA="BPFTIME_SM_ARCH=sm_87"; [ -n "${NO_SMARCH:-}" ] && SMA=
  timeout -k 5 -s INT "$VT" env $SMA SPDLOG_LEVEL=${SPDLOG_LEVEL:-debug} ${VIC_ENV:-} $PRE $VCMD > "$O/client.log" 2>&1
  crc=$?; t1=$(date +%s%N)
  echo "client_rc=$crc client_ms=$(( (t1-t0)/1000000 ))" | tee -a "$O/meta.txt"
fi
# drain: at least DRAIN s, then until the server log stops growing for 2 s (max 10 s more)
sleep "$DRAIN"
for i in 1 2 3 4 5; do s1=$(stat -c %s "$O/server.log"); sleep 2; s2=$(stat -c %s "$O/server.log"); [ "$s1" = "$s2" ] && break; done
kill -INT $SP 2>/dev/null; wait $SP; src=$?
echo "server_rc=$src" | tee -a "$O/meta.txt"
rm -f /dev/shm/$S
echo "end $(date '+%F %T.%N')" >> "$O/meta.txt"
pgrep -a -x 'vec_add|vec_add_legacy|vec_add_graph|vec_add_graph_l|cuda_probe|kernelretsnoop|host_map_test|kernel_trace|threadhist|threadscheduling' || echo no-leftovers
ls /dev/shm | grep "^bpftime_gpu_" || echo shm-clean
echo "logs: $O"
