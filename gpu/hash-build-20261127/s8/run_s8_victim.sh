#!/bin/bash
# run_s8_victim.sh PASS [item ...]: S8 parts A and C (S8-RULES.md): the
# pt_victim matrix, 1533 against the host path, on the worktree build; and
# the delivery cost items. Items SIZE:TOOL, shuffled (seed = the pass label),
# clocks checked before each item. From run_hash.sh (hash-current).
set -u
W=/home/jetson/src/bpftime-gpu-work
T=$W/proto/tools
B=${S8_BUILD:-/home/jetson/src/bpftime-proto-70f7/build-cuda}
PASS=${1:?pass}
STAMP=$(date +%Y%m%d-%H%M%S)
OUT=$W/proto/hashdesign/build/s8/out/$PASS-$STAMP
mkdir -p $OUT
exec > >(tee -a "$OUT/driver.log") 2>&1
declare -A OBJ=( [empty]=empty2_test
  [h-pre]=hs_pre_thread_kc_test [d-pre]=hd_pre_thread_kc_test
  [h-miss]=hs_miss_thread_kc_test [d-miss]=hd_miss_thread_kc_test
  [h-cnt-t]=hs_cnt_thread_kc_test [d-cnt-t]=hd_cnt_thread_kc_test
  [h-cnt-t-sm]=hs_cnt_thread_ksm_test [d-cnt-t-sm]=hd_cnt_thread_ksm_test
  [h-cnt-b-blk]=hs_cnt_block_kblk_test [d-cnt-b-blk]=hd_cnt_block_kblk_test
  [h-cnt-b]=hs_cnt_block_kc_test [d-cnt-b]=hd_cnt_block_kc_test
  [h-cnt-l]=hs_cnt_leader_kc_test [d-cnt-l]=hd_cnt_leader_kc_test
  [d-kmod512]=hd_cnt_thread_kmod512_test [d-kmod1024]=hd_cnt_thread_kmod1024_test
  [hdn-pre]=hdn_pre_thread_kc_test [hdn-miss]=hdn_miss_thread_kc_test [hdn-cnt-t]=hdn_cnt_thread_kc_test
  [hsn-pre]=hsn_pre_thread_kc_test [hdn-kmod512]=hdn_cnt_thread_kmod_test512 [hdn-kmod1024]=hdn_cnt_thread_kmod_test1024 )
declare -A PRE=( [h-pre]=h:2 [d-pre]=h:2 [hdn-pre]=h:2 [hsn-pre]=h:2 )
declare -A SIZE=( [D]="1 0" [G]="0 2" [D4]="4 0" [G8]="0 8" )
ITEMS="${*:2}"
if [ -z "$ITEMS" ]; then
  for t in empty h-pre d-pre h-miss d-miss h-cnt-t d-cnt-t h-cnt-t-sm d-cnt-t-sm h-cnt-b-blk d-cnt-b-blk h-cnt-b d-cnt-b h-cnt-l d-cnt-l; do
    for s in D G D4 G8; do ITEMS="$ITEMS $s:$t"; done
  done
fi
ORDER=$(python3 -I -c "import random,sys; it=sorted(sys.argv[2:]); random.Random(sys.argv[1]).shuffle(it); print(' '.join(it))" "$PASS" $ITEMS)
clocks_ok() {
  [ "$(cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq)" = 1728000 ] &&
  [ "$(cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_min_freq)" = 1728000 ] &&
  [ "$(cat /sys/devices/platform/bus@0/17000000.gpu/devfreq/17000000.gpu/cur_freq)" = 1020000000 ]
}
apt_busy() { systemctl is-active -q apt-daily.service || systemctl is-active -q apt-daily-upgrade.service || pgrep -x 'apt|apt-get|unattended-upgr|dpkg' > /dev/null; }
wait_apt() { local i=0; while apt_busy && [ $i -lt 240 ]; do sleep 10; i=$((i+1)); done; [ $i -gt 0 ] && echo "  apt wait $((i*10)) s"; return 0; }
echo "s8 victim pass $PASS $STAMP build $B order: $ORDER"
for item in $ORDER; do
  s=${item%%:*}; t=${item#*:}
  wait_apt
  clocks_ok || { echo "clocks not fixed: stopping"; exit 4; }
  N=s8v-$PASS-$STAMP-$s-$t
  PF=${PRE[$t]:-}
  GPU_BUILD=$B SRV_ENV="${PF:+PT_PREFILL=$PF} BPFTIME_MAP_GPU_THREAD_COUNT=131072" VIC_ENV="BPFTIME_MAP_GPU_THREAD_COUNT=131072" SPDLOG_LEVEL=info \
    timeout -k 10 420 bash $W/run.sh $N $T "./pt_loader bpf/${OBJ[$t]}.bpf.o $W/runs/$N" "./pt_victim ${SIZE[$s]}" 400 300 3 > $OUT/$s-$t.run.txt 2>&1
  echo "== $item rc=$? $(grep -h client_rc $W/runs/$N/meta.txt) devhash_lines=$(grep -c 'Device hash maps in' $W/runs/$N/client.log) compile_errors=$(grep -c 'Unable to compile' $W/runs/$N/client.log)"
  ln -sfn $W/runs/$N $OUT/run-$s-$t
  sleep 2
done
echo "s8 victim pass $PASS done $(date '+%F %T')"
