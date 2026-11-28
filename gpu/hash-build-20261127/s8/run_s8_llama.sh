#!/bin/bash
# run_s8_llama.sh PASS_LABEL [item ...]: S8 parts B2 and B3 (S8-RULES.md), from
# run_hash_llama.sh (hash-current) with the 1533 tools and the worktree build.
# Original header:
# run_hash_llama.sh: one serial pass of the hash maps
# current state on llama.cpp (RULES-hash-current.md). From run_meas3.sh:
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
OUT=$W/proto/hashdesign/build/s8/out/$PASS-$STAMP
mkdir -p $OUT
exec > >(tee -a "$OUT/driver.log") 2>&1
LB=/home/jetson/src/workloads/llama.cpp/build-ptx87/bin/llama-bench
MODEL=/home/jetson/src/workloads/models/tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf
THREADS=1048576
ITEMS_ALL="p32:base p32:link p32:dlink p32:blk p32:dblk tg:base tg:ldrf tg:ldr tg:dldr"
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

echo "hash llama pass $PASS $STAMP, order: $ORDER"
clocks_ok || { echo "clocks not fixed (jetson_clocks): nothing run"; state; exit 3; }
B=${S8_BUILD:-/home/jetson/src/bpftime-proto-70f7/build-cuda}
for item in $ORDER; do
  wl=${item%%:*}; var=${item##*:}
  case $wl in
    p32) ARGS="-p 32 -n 0" ;;
    pp) ARGS="-p 512 -n 0" ;;
    tg) ARGS="-p 0 -n 128" ;;
  esac
  PF=; NOAG=; VENV=
  case $var in *-ng) VENV="GGML_CUDA_DISABLE_GRAPHS=1" ;; esac
  case ${var%-ng} in
    base) OBJ=noop_test; NOAG=1 ;;
    link) OBJ=hs_pre_thread_kc_rms; PF="PT_PREFILL=h:1" ;;
    blk)  OBJ=hs_cnt_block_kc_rms ;;
    ldr)  OBJ=hs_cnt_leader_kc_llama4 ;;
    ldrf) OBJ=hs_none_leader_kc_llama4 ;;
    dlink) OBJ=hd_pre_thread_kc_rms; PF="PT_PREFILL=h:1" ;;
    dblk)  OBJ=hd_cnt_block_kc_rms ;;
    dldr)  OBJ=hd_cnt_leader_kc_llama4 ;;
    dlinkn) OBJ=hdn_pre_thread_kc_rms; PF="PT_PREFILL=h:1" ;;
    linkn) OBJ=hsn_pre_thread_kc_rms; PF="PT_PREFILL=h:1" ;;
    dnone) OBJ=hd_none_leader_kc_llama4 ;;
  esac
  N=s8l-$PASS-$STAMP-$wl-$var
  wait_apt
  clocks_ok || { echo "clocks changed: stopping"; state; exit 4; }
  echo "===== ITEM $item ($N) start"
  state
  t0=$(date +%s)
  GPU_BUILD=$B NO_AGENT=$NOAG VIC_ENV="$VENV" SRV_ENV="$PF BPFTIME_MAP_GPU_THREAD_COUNT=$THREADS" SPDLOG_LEVEL=info \
    timeout -k 10 2400 bash $W/run.sh $N $T "./pt_loader bpf/$OBJ.bpf.o $W/runs/$N" "$LB -m $MODEL -r 5 -o json $ARGS" 2300 2200 4 > $OUT/$wl-$var.run.txt 2>&1
  echo "===== ITEM $item rc=$? wall=$(( $(date +%s) - t0 ))s $(grep -h client_rc $W/runs/$N/meta.txt) devhash_lines=$(grep -c 'Device hash maps in' $W/runs/$N/client.log) compile_errors=$(grep -c 'Unable to compile' $W/runs/$N/client.log)"
  ln -sfn $W/runs/$N $OUT/run-$wl-$var
  grep -h '"samples_ts"' $W/runs/$N/client.log | sed 's/^/  /'
  state
  sleep 5
done
echo "hash llama pass $PASS done $(date '+%F %T')"
