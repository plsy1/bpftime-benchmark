#!/usr/bin/env bash
# One run of the watcher-policy test:
#   run_one.sh SET PROBE LOAD SPIN_US LAUNCHES THREADS JITTER_US GAP_US GRAPH PROCS OUT_PREFIX
#   SET: P (no bpftime), W (dev 46f5a7cb, WFE), M100 (max sleep 100 us, slack 1 us, no tracking),
#        A, AC, AB, ABC (experimental policies, BPFTIME_CUDA_WATCHER_POLICY); controls A0 (A without
#        the 200 us busy poll after a request) and AC0 (AC without the 8 deferral sleeps); AC2: AC v2
#        (window 2x the gap, at most 50 us; unseen latch by two hits within 1 s, unlatched after 1 s
#        without a request served while no noted kernel ran); AC3 / AC4: AC2's latch with window mode 3 (doubling up to
#        50 us, closed by two gaps >= 50 us in a row) / 4 (doubling up to 50 us, halved by a gap
#        >= 50 us); AC5: AC3 with the gap measured to the last empty
#        check (BPFTIME_CUDA_WATCHER_GAP_LB=1); ACn: AC2 with the window off (deferral sleeps only)
#   PROBE: host (hostspin, one host call per lane at spinKernel's exit) or none (nohost)
#   LOAD: off (tegrastats every 100 ms), on3 / on6 (3 / 6 load threads), duty6 (6 threads,
#         5 ms on / 5 ms off)
#   PROCS: 1 or 2 victims at once, each with its own loader and segment
# Writes OUT_PREFIX.env, OUT_PREFIX.load / .power, and per process i: OUT_PREFIX.i.{victim,
# victim.err,loader,loader.err} plus OUT_PREFIX.stats (watcher counters). Exit 2 = abort.
set -u
S=$1; PR=$2; LD=$3; D=$4; L=$5; T=$6; J=$7; GAP=$8; GR=$9; NP=${10}; O=${11}
G=/home/jetson/src/bpftime-gpu-work
EXP_ENV=()
case $S in
  P) B=;;
  W) B=$G/wake/build-cuda;;
  M100) B=$G/policy/build-cuda; EXP_ENV=(BPFTIME_CUDA_WATCHER_MAX_SLEEP_US=100);;
  A|AC|AB|ABC) B=$G/policy/build-cuda; EXP_ENV=(BPFTIME_CUDA_WATCHER_POLICY=$S BPFTIME_CUDA_WATCHER_STATS=$O.stats);;
  A0) B=$G/policy/build-cuda; EXP_ENV=(BPFTIME_CUDA_WATCHER_POLICY=A BPFTIME_CUDA_WATCHER_HOT_US=0 BPFTIME_CUDA_WATCHER_STATS=$O.stats);;
  AC0) B=$G/policy/build-cuda; EXP_ENV=(BPFTIME_CUDA_WATCHER_POLICY=AC BPFTIME_CUDA_WATCHER_K_DEFER=0 BPFTIME_CUDA_WATCHER_STATS=$O.stats);;
  AC2) B=$G/policy/build-cuda; EXP_ENV=(BPFTIME_CUDA_WATCHER_POLICY=AC BPFTIME_CUDA_WATCHER_V2=1 BPFTIME_CUDA_WATCHER_STATS=$O.stats);;
  AC3) B=$G/policy/build-cuda; EXP_ENV=(BPFTIME_CUDA_WATCHER_POLICY=AC BPFTIME_CUDA_WATCHER_V2=1 BPFTIME_CUDA_WATCHER_WIN_MODE=3 BPFTIME_CUDA_WATCHER_STATS=$O.stats);;
  AC4) B=$G/policy/build-cuda; EXP_ENV=(BPFTIME_CUDA_WATCHER_POLICY=AC BPFTIME_CUDA_WATCHER_V2=1 BPFTIME_CUDA_WATCHER_WIN_MODE=4 BPFTIME_CUDA_WATCHER_STATS=$O.stats);;
  AC5) B=$G/policy/build-cuda; EXP_ENV=(BPFTIME_CUDA_WATCHER_POLICY=AC BPFTIME_CUDA_WATCHER_V2=1 BPFTIME_CUDA_WATCHER_WIN_MODE=3 BPFTIME_CUDA_WATCHER_GAP_LB=1 BPFTIME_CUDA_WATCHER_STATS=$O.stats);;
  ACn) B=$G/policy/build-cuda; EXP_ENV=(BPFTIME_CUDA_WATCHER_POLICY=AC BPFTIME_CUDA_WATCHER_V2=1 BPFTIME_CUDA_WATCHER_WIN_MAX=0 BPFTIME_CUDA_WATCHER_STATS=$O.stats);;
  *) echo "bad set $S"; exit 2;;
esac
case $PR in host) PB=$G/load/probe/hostspin;; none) PB=$G/load/probe/nohost;; *) echo "bad probe"; exit 2;; esac

state() {
  echo "$1 $(date +%T) cpu5=$(cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq)" \
    "cpu0=$(cat /sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq)" \
    "gpu=$(cat /sys/class/devfreq/17000000.gpu/cur_freq)" \
    "temps=$(cat /sys/class/thermal/thermal_zone{0,1,5,6,7,8}/temp 2>/dev/null | paste -sd,)"
}
hm=$(date +%H%M); if [ "$hm" -ge 0325 ] && [ "$hm" -le 0415 ]; then echo "ABORT: window"; exit 2; fi
cpu=$(cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq); gpu=$(cat /sys/class/devfreq/17000000.gpu/cur_freq)
[ "$cpu" = 1728000 ] && [ "$gpu" = 1020000000 ] || { echo "ABORT: clocks $cpu $gpu"; exit 2; }
if pgrep -a -x 'spin|spin2|spin3|load|load2|hostspin|nohost|tegrastats|vec_add|vec_add_gap|vec_add_legacy|cuda_probe|hostwake|kernelretsnoop' >/dev/null; then echo "ABORT: leftovers"; exit 2; fi
if pgrep -a -x 'cc1plus|cc1|ld|make|cmake|ninja|nvcc' >/dev/null; then echo "ABORT: build running"; exit 2; fi
if ls /dev/shm | grep -q -E '^(bpftime_maps_shm|bpftime_cmp_|bpftime_wake_|bpftime_load_|bpftime_sleep_|bpftime_pol_)'; then echo "ABORT: bpftime segment present"; exit 2; fi

{ echo "set=$S probe=$PR load=$LD spin_us=$D launches=$L threads=$T jitter_us=$J gap_us=$GAP graph=$GR procs=$NP build=${B:-none} env=${EXP_ENV[*]:-default}"; state before; } > $O.env
lp=; tp=
case $LD in
  on3) timeout -k 5 300 $G/policy/load2 3 $O.load & lp=$!;;
  on6) timeout -k 5 300 $G/policy/load2 6 $O.load & lp=$!;;
  duty6) timeout -k 5 300 $G/policy/load2 6 $O.load 5 5 & lp=$!;;
  off) ;;
  *) echo "bad load"; exit 2;;
esac
[ -n "$lp" ] && sleep 1
pps=()
if [ "$S" != P ]; then
  for i in $(seq $NP); do
    BPFTIME_GLOBAL_SHM_NAME=bpftime_pol_$$_$i BPFTIME_LOG_OUTPUT=console SPDLOG_LEVEL=info \
      timeout -k 5 200 env LD_PRELOAD=$B/runtime/syscall-server/libbpftime-syscall-server.so $PB > $O.$i.loader 2> $O.$i.loader.err &
    pps+=($!)
  done
  for i in $(seq $NP); do
    ok=0; for k in $(seq 200); do grep -q "BPF probes attached" $O.$i.loader.err 2>/dev/null && { ok=1; break; }; sleep 0.1; done
    [ $ok = 1 ] || echo "loader $i not attached after 20 s" >> $O.env
  done
fi
[ "$LD" = off ] && { timeout -k 5 300 tegrastats --interval 100 > $O.power 2>&1 & tp=$!; }
vps=()
for i in $(seq $NP); do
  if [ "$S" = P ]; then
    timeout -k 5 150 $G/policy/victim/spin3 $L $D $J $T $GAP $GR > $O.$i.victim 2> $O.$i.victim.err &
  else
    env "${EXP_ENV[@]}" BPFTIME_GLOBAL_SHM_NAME=bpftime_pol_$$_$i BPFTIME_LOG_OUTPUT=console SPDLOG_LEVEL=info \
      timeout -k 5 150 env LD_PRELOAD=$B/runtime/agent/libbpftime-agent.so $G/policy/victim/spin3 $L $D $J $T $GAP $GR > $O.$i.victim 2> $O.$i.victim.err &
  fi
  vps+=($!)
done
for i in $(seq $NP); do wait ${vps[$((i-1))]}; echo "victim $i rc=$?" >> $O.env; done
for i in $(seq $NP); do
  [ "$S" != P ] && { kill -INT ${pps[$((i-1))]}; wait ${pps[$((i-1))]}; echo "loader $i rc=$?" >> $O.env; }
done
[ -n "$tp" ] && { kill -INT $tp; wait $tp 2>/dev/null; }
[ -n "$lp" ] && { kill -INT $lp; wait $lp; echo "load rc=$?" >> $O.env; }
state after >> $O.env
sleep 0.5
for i in $(seq $NP); do
  shm=bpftime_pol_$$_$i
  [ -e /dev/shm/$shm ] && { fuser -s /dev/shm/$shm && echo "segment $shm in use" >> $O.env || rm -f -- /dev/shm/$shm; }
done
exit 0
