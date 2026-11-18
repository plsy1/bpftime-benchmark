#!/bin/bash
# run_meas.sh PASS_LABEL [item ...]: one serial pass of the prototype's
# llama.cpp measurement (proto/map-model). Items WORKLOAD:VARIANT, run in an
# order shuffled per pass (seed = the label). Board state before and after
# every item; timed items wait (at most 40 min) for apt's daily services.
# Usage: timeout -k 30 10800 bash run_meas.sh PASS_LABEL
set -u
W=/home/jetson/src/bpftime-gpu-work
M=$W/proto/meas
T=$W/proto/tools
PASS=${1:?pass label}; shift
STAMP=$(date +%Y%m%d-%H%M%S)
OUT=$M/out/$PASS-$STAMP
mkdir -p $OUT
exec > >(tee -a "$OUT/driver.log") 2>&1
LB=/home/jetson/src/workloads/llama.cpp/build-ptx87/bin/llama-bench
MODEL=/home/jetson/src/workloads/models/tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf
THREADS=1048576
ITEMS_ALL="pp:base pp:agent pp:lat pp:latmix pp:latold pp:hist pp:histold pp:dev-latold pp:dev-histold tg:base tg:agent tg:lat tg:latmix tg:hist tg:histold tg:dev-histold"
ITEMS="${*:-$ITEMS_ALL}"
ORDER=$(python3 -I -c "import random,sys; it=sys.argv[2:]; random.Random(sys.argv[1]).shuffle(it); print(' '.join(it))" "$PASS" $ITEMS)

state() {
  echo "  time $(date '+%F %T') cpu5=$(cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq)/$(cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_min_freq) gpu=$(cat /sys/devices/platform/bus@0/17000000.gpu/devfreq/17000000.gpu/cur_freq) load=$(cut -d' ' -f1-3 /proc/loadavg)"
  echo "  mem $(free -m | awk '/^Mem:/{print "used="$3" avail="$7}')  shm: $(ls /dev/shm | grep -E '^bpftime' | tr '\n' ' ')"
  echo "  apt: $(for u in apt-daily.service apt-daily-upgrade.service; do printf '%s=%s ' $u "$(systemctl is-active $u)"; done)"
  echo "  busiest:"; ps -eo pid,pcpu,etime,args --sort=-pcpu | grep -v -e 'stream-json' -e 'ps -eo' | sed -n '2,5p' | cut -c1-150 | sed 's/^/    /'
}
clocks_ok() {
  [ "$(cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq)" = 1728000 ] &&
  [ "$(cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_min_freq)" = 1728000 ] &&
  [ "$(cat /sys/devices/platform/bus@0/17000000.gpu/devfreq/17000000.gpu/cur_freq)" = 1020000000 ]
}
apt_busy() { systemctl is-active -q apt-daily.service || systemctl is-active -q apt-daily-upgrade.service || pgrep -x 'apt|apt-get|unattended-upgr|dpkg' > /dev/null; }
wait_apt() {
  local i=0
  while apt_busy && [ $i -lt 240 ]; do [ $i -eq 0 ] && echo "  waiting for apt $(date +%T)"; sleep 10; i=$((i+1)); done
  [ $i -gt 0 ] && echo "  apt wait ended $(date +%T) after $((i*10)) s"
  return 0
}

echo "proto meas pass $PASS $STAMP, order: $ORDER"
clocks_ok || { echo "clocks not fixed (jetson_clocks): nothing run"; state; exit 3; }
for item in $ORDER; do
  wl=${item%%:*}; var=${item##*:}
  case $wl in
    pp) ARGS="-p 512 -n 0" ;;
    tg) ARGS="-p 0 -n 128" ;;
  esac
  # dev-*: the same tool under dev 52126f33's build (today's trampoline)
  # *+sync: deliveries inside the application's synchronize calls
  BUILD=/home/jetson/src/bpftime-proto/build-cuda
  tool=$var
  VENV=
  case $tool in *+sync) tool=${tool%+sync}; VENV="BPFTIME_GPU_DELIVERY_AT_SYNC=1" ;; esac
  # tool@variant: the patched PTX replaced by target-shape/variants/<variant>
  case $tool in *@*) VENV="$VENV BPFTIME_CUDA_PTX_OVERRIDE_DIR=$W/proto/target-shape/variants/${tool#*@}"; tool=${tool%@*} ;; esac
  case $var in dev-*) BUILD=/home/jetson/src/bpftime-daily-state/worktrees/4708f4092d1e/candidate/build-cuda; tool=${var#dev-} ;; esac
  # old-* / new-*: copies of the prototype's agent and server before and
  # after the sum-map change (proto/builds; 2026-11-18)
  case $tool in old-*) BUILD=$W/proto/builds/old-c27412f4; tool=${tool#old-} ;; esac
  case $tool in new-*) BUILD=$W/proto/builds/${NEW_BUILD:-new-00f12354}; tool=${tool#new-} ;; esac
  case $tool in newk-*) BUILD=$W/proto/builds/${NEWK_BUILD:-newk-0890dbdd}; tool=${tool#newk-} ;; esac
  case $tool in cur-*) BUILD=$W/proto/builds/${CUR_BUILD:-cur-7e5861b9}; tool=${tool#cur-} ;; esac
  case $tool in
    base)  OBJ=noop_test; NOAG=1 ;;   # the loader runs, the victim has no agent
    agent) OBJ=noop_test; NOAG= ;;    # the agent, a probe with no map on kernels llama does not have
    *_mmvq) OBJ=$tool; NOAG= ;;   # one-kernel set (attribution 11-17)
    *)     OBJ=${tool}_llama4; NOAG= ;;
  esac
  N=proto-meas-$PASS-$STAMP-$wl-$var
  wait_apt
  clocks_ok || { echo "clocks changed: stopping"; state; exit 4; }
  echo "===== ITEM $item ($N) start"
  state
  t0=$(date +%s)
  GPU_BUILD=$BUILD NO_AGENT=$NOAG VIC_ENV="$VENV" SRV_ENV="BPFTIME_MAP_GPU_THREAD_COUNT=$THREADS" SPDLOG_LEVEL=info \
    timeout -k 10 1500 bash $W/run.sh $N $T "./pt_loader bpf/$OBJ.bpf.o $W/runs/$N" "$LB -m $MODEL -r 5 -o json $ARGS" 1400 1300 4 > $OUT/$wl-$var.run.txt 2>&1
  echo "===== ITEM $item rc=$? wall=$(( $(date +%s) - t0 ))s $(grep -h client_rc $W/runs/$N/meta.txt)"
  ln -sfn $W/runs/$N $OUT/run-$wl-$var
  grep -h '"samples_ts"' $W/runs/$N/client.log | sed 's/^/  /'
  state
  sleep 5
done
echo "proto meas pass $PASS done $(date '+%F %T')"
