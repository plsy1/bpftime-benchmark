#!/usr/bin/env bash
# Host-call latency against the gap between launches: run_wake.sh BUILD_DIR OUT_DIR ITER GAP...
# Per gap: the hostwake loader (syscall server preloaded) attaches, the victim (agent preloaded)
# launches vec_add on 1 thread ITER times with GAP us before each launch, the loader is stopped
# with SIGINT and prints every host call's latency (ns, timed on the GPU). Checks first: time
# window, cpu5 and GPU clocks, leftovers, no build, no bpftime segment.
set -u
B=$1; OUT=$2; ITER=$3; shift 3
H=/home/jetson/src/bpftime-gpu-work/hostwake
hm=$(date +%H%M); if [ "$hm" -ge 0325 ] && [ "$hm" -le 0415 ]; then echo "ABORT: window"; exit 2; fi
cpu=$(cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq); gpu=$(cat /sys/class/devfreq/17000000.gpu/cur_freq)
[ "$cpu" = 1728000 ] && [ "$gpu" = 1020000000 ] || { echo "ABORT: clocks $cpu $gpu"; exit 2; }
if pgrep -a -x 'vec_add|vec_add_gap|vec_add_legacy|cuda_probe|hostwake|kernelretsnoop' >/dev/null; then echo "ABORT: leftovers"; exit 2; fi
if pgrep -a -x 'cc1plus|cc1|ld|make|cmake|ninja|nvcc' >/dev/null; then echo "ABORT: build running"; exit 2; fi
if ls /dev/shm | grep -q -E '^(bpftime_maps_shm|bpftime_cmp_|bpftime_wake_)'; then echo "ABORT: bpftime segment present"; exit 2; fi
mkdir -p $OUT
for g in "$@"; do
  shm=bpftime_wake_$$_$g
  L=$OUT/gap$g.loader; V=$OUT/gap$g.victim
  BPFTIME_GLOBAL_SHM_NAME=$shm BPFTIME_LOG_OUTPUT=console SPDLOG_LEVEL=info \
    timeout -k 5 120 env LD_PRELOAD=$B/runtime/syscall-server/libbpftime-syscall-server.so $H/probe/hostwake > $L.out 2> $L.err &
  lp=$!
  for i in $(seq 100); do grep -q "BPF probes attached" $L.err && break; sleep 0.1; done
  BPFTIME_GLOBAL_SHM_NAME=$shm BPFTIME_LOG_OUTPUT=console SPDLOG_LEVEL=info \
    timeout -k 5 300 env LD_PRELOAD=$B/runtime/agent/libbpftime-agent.so $H/victim/vec_add_gap 1 $ITER 1 1 $g > $V.out 2> $V.err
  echo "gap $g victim rc=$?"
  kill -INT $lp; wait $lp; echo "gap $g loader rc=$?"
  sleep 0.5
  [ -e /dev/shm/$shm ] && { fuser -s /dev/shm/$shm && echo "segment $shm in use" || rm -f -- /dev/shm/$shm; }
done
pgrep -a -x 'vec_add_gap|hostwake' && echo LEFTOVERS
ls /dev/shm | grep -E '^bpftime' || echo shm-clean
