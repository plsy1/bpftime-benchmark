#!/bin/bash
# Correctness of the prototype's tools on pt_victim (proto/map-model).
# usage: run_test_set.sh LABEL [tools...]   (tools: carry lat latold hist histold)
set -u
W=/home/jetson/src/bpftime-gpu-work
T=$W/proto/tools
LABEL=$1; shift
TOOLS="${*:-carry lat latold hist histold}"
# default: the prototype on dev 70f7bc1f (since 2026-11-26; before it bpftime-proto)
export GPU_BUILD=${GPU_BUILD:-/home/jetson/src/bpftime-proto-70f7/build-cuda}
for t in $TOOLS; do
  N=proto-$LABEL-$t
  SRV_ENV="BPFTIME_MAP_GPU_THREAD_COUNT=131072" SPDLOG_LEVEL=info VIC_ENV="BPFTIME_MAP_GPU_THREAD_COUNT=131072 ${EXTRA_VIC_ENV:-}" \
    timeout -k 10 300 bash $W/run.sh $N $T "./pt_loader bpf/${t}_test.bpf.o $W/runs/$N" "./pt_victim 10 100" 120 90 3 > /dev/null 2>&1
  echo "== $t rc=$? $(grep -h client_rc $W/runs/$N/meta.txt)"
  grep -h "status:" $W/runs/$N/server.log
  grep -h "final delivery" $W/runs/$N/client.log | sed 's/^.*GPU map/  GPU map/'
  python3 -I $W/proto/check/check_test_set.py $t $W/runs/$N ${EXTRA_VIC_ENV:+in-call}
done
