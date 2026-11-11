#!/usr/bin/env bash
# One run of the WFE-under-load test: run_one.sh SET PROBE LOAD SPIN_US LAUNCHES OUT_PREFIX
#   SET: P (no bpftime), N (dev aa6eb973, old watcher), W (dev 46f5a7cb, WFE), W0 (W with
#        BPFTIME_CUDA_WATCHER_WFE=0)
#   PROBE: host (hostspin, one host call at spinKernel's exit) or none (nohost); ignored for P
#   LOAD: on (6 unpinned load threads) or off
# Writes OUT_PREFIX.{victim,loader,load,env}. Exit 2 = abort (checks failed).
set -u
S=$1; PR=$2; LD=$3; D=$4; L=$5; O=$6
G=/home/jetson/src/bpftime-gpu-work/load
case $S in
  P) B=;; N) B=/home/jetson/src/bpftime-gpu-work/l1/build-cuda;;
  W|W0) B=/home/jetson/src/bpftime-gpu-work/wake/build-cuda;;
  *) echo "bad set $S"; exit 2;;
esac
case $PR in host) PB=$G/probe/hostspin;; none) PB=$G/probe/nohost;; *) echo "bad probe"; exit 2;; esac
WFE_ENV=; [ "$S" = W0 ] && WFE_ENV=BPFTIME_CUDA_WATCHER_WFE=0

state() {
  echo "$1 $(date +%T) cpu5=$(cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq)" \
    "gpu=$(cat /sys/class/devfreq/17000000.gpu/cur_freq)" \
    "temps=$(cat /sys/class/thermal/thermal_zone{0,1,5,6,7,8}/temp 2>/dev/null | paste -sd,)"
}
hm=$(date +%H%M); if [ "$hm" -ge 0325 ] && [ "$hm" -le 0415 ]; then echo "ABORT: window"; exit 2; fi
cpu=$(cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq); gpu=$(cat /sys/class/devfreq/17000000.gpu/cur_freq)
[ "$cpu" = 1728000 ] && [ "$gpu" = 1020000000 ] || { echo "ABORT: clocks $cpu $gpu"; exit 2; }
if pgrep -a -x 'spin|load|hostspin|nohost|vec_add|vec_add_gap|vec_add_legacy|cuda_probe|hostwake|kernelretsnoop' >/dev/null; then echo "ABORT: leftovers"; exit 2; fi
if pgrep -a -x 'cc1plus|cc1|ld|make|cmake|ninja|nvcc' >/dev/null; then echo "ABORT: build running"; exit 2; fi
if ls /dev/shm | grep -q -E '^(bpftime_maps_shm|bpftime_cmp_|bpftime_wake_|bpftime_load_)'; then echo "ABORT: bpftime segment present"; exit 2; fi

{ echo "set=$S probe=$PR load=$LD spin_us=$D launches=$L build=${B:-none} env=${WFE_ENV:-default}"; state before; } > $O.env
lp=
if [ "$LD" = on ]; then
  timeout -k 5 300 $G/load 6 $O.load & lp=$!
  sleep 1
fi
shm=bpftime_load_$$
if [ "$S" = P ]; then
  timeout -k 5 120 $G/victim/spin $L $D > $O.victim 2>&1; vrc=$?
else
  BPFTIME_GLOBAL_SHM_NAME=$shm BPFTIME_LOG_OUTPUT=console SPDLOG_LEVEL=info \
    timeout -k 5 200 env LD_PRELOAD=$B/runtime/syscall-server/libbpftime-syscall-server.so $PB > $O.loader 2> $O.loader.err &
  pp=$!
  ok=0; for i in $(seq 200); do grep -q "BPF probes attached" $O.loader.err && { ok=1; break; }; sleep 0.1; done
  [ $ok = 1 ] || echo "loader not attached after 20 s" >> $O.env
  env $WFE_ENV BPFTIME_GLOBAL_SHM_NAME=$shm BPFTIME_LOG_OUTPUT=console SPDLOG_LEVEL=info \
    timeout -k 5 120 env LD_PRELOAD=$B/runtime/agent/libbpftime-agent.so $G/victim/spin $L $D > $O.victim 2> $O.victim.err; vrc=$?
  kill -INT $pp; wait $pp; echo "loader rc=$?" >> $O.env
fi
echo "victim rc=$vrc" >> $O.env
[ -n "$lp" ] && { kill -INT $lp; wait $lp; echo "load rc=$?" >> $O.env; }
state after >> $O.env
sleep 0.5
[ -e /dev/shm/$shm ] && { fuser -s /dev/shm/$shm && echo "segment $shm in use" >> $O.env || rm -f -- /dev/shm/$shm; }
exit 0
