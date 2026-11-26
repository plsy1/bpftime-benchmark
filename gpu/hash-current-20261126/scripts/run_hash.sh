#!/bin/bash
# run_hash.sh PASS: one pass of the pt_victim hash matrix (RULES-hash-current.md),
# items shuffled (seed = the pass label), clocks checked before each item.
set -u
W=/home/jetson/src/bpftime-gpu-work
T=$W/proto/tools
B=${HS_BUILD:-$W/proto/builds/tvf-7d02d677}
PASS=${1:?pass}
STAMP=$(date +%Y%m%d-%H%M%S)
OUT=$W/proto/hash/out/$PASS-$STAMP
mkdir -p $OUT
exec > >(tee -a "$OUT/driver.log") 2>&1
# item = tool name; prefill per tool
declare -A OBJ=( [empty]=empty2_test [pre]=hs_pre_thread_kc_test [miss]=hs_miss_thread_kc_test
  [upd]=hs_upd_thread_kc_test [del]=hs_del_thread_kc_test [tid]=hs_tid_thread_kc_test
  [tidnd]=hs_tidnd_thread_kc_test [cnt-t]=hs_cnt_thread_kc_test [cnt-t-sm]=hs_cnt_thread_ksm_test
  [cnt-b-blk]=hs_cnt_block_kblk_test [cnt-b]=hs_cnt_block_kc_test [cnt-l]=hs_cnt_leader_kc_test
  [rmw]=hs_rmw_thread_kc_test )
declare -A PRE=( [pre]=h:2 [upd]=h:2 )
declare -A SIZE=( [D]="1 0" [G]="0 2" [D4]="4 0" [G8]="0 8" )
ITEMS="${*:2}"
if [ -z "$ITEMS" ]; then
  for t in "${!OBJ[@]}"; do for s in D G; do ITEMS="$ITEMS $s:$t"; done; done
fi
ORDER=$(python3 -I -c "import random,sys; it=sorted(sys.argv[2:]); random.Random(sys.argv[1]).shuffle(it); print(' '.join(it))" "$PASS" $ITEMS)
clocks_ok() {
  [ "$(cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq)" = 1728000 ] &&
  [ "$(cat /sys/devices/platform/bus@0/17000000.gpu/devfreq/17000000.gpu/cur_freq)" = 1020000000 ]
}
echo "hash pass $PASS $STAMP build $B order: $ORDER"
for item in $ORDER; do
  s=${item%%:*}; t=${item#*:}
  clocks_ok || { echo "clocks not fixed: stopping"; exit 4; }
  N=hs-$PASS-$STAMP-$s-$t
  PF=${PRE[$t]:-}
  GPU_BUILD=$B SRV_ENV="${PF:+PT_PREFILL=$PF} BPFTIME_MAP_GPU_THREAD_COUNT=131072" VIC_ENV="BPFTIME_MAP_GPU_THREAD_COUNT=131072" SPDLOG_LEVEL=info \
    timeout -k 10 420 bash $W/run.sh $N $T "./pt_loader bpf/${OBJ[$t]}.bpf.o $W/runs/$N" "./pt_victim ${SIZE[$s]}" 400 300 3 > $OUT/$s-$t.run.txt 2>&1
  echo "== $item rc=$? $(grep -h client_rc $W/runs/$N/meta.txt) routing_lines=$(grep -c 'Thread-local maps in' $W/runs/$N/client.log)"
  ln -sfn $W/runs/$N $OUT/run-$s-$t
  sleep 2
done
echo "hash pass $PASS done $(date '+%F %T')"
