#!/bin/bash
# step0 e9-hooks smoke (short): the cudagraph control (default launch) and
# one llama-bench pp32 run with the geometry log, analyzed against
# census/pp32-graph.sqlite, the legacy-launch cudagraph control (stub
# pointers: cudaGetFuncBySymbol path), and (tg128) one decode run with graphs on,
# analyzed against census/tg128-graph.sqlite. Wall ~2 min.
# Run as:  timeout -k 10 600 bash smoke_run.sh   (PARTS="cg cg-legacy pp32 tg128" by default)
set -u
E=/home/jetson/src/bpftime-gpu-work/step0/e9-hooks
W=/home/jetson/src/bpftime-gpu-work
export GPU_BUILD=/home/jetson/src/step0-bpftime-exp/build-cuda
unset GGML_CUDA_DISABLE_GRAPHS
BENCH="/home/jetson/src/workloads/llama.cpp/build-ptx87/bin/llama-bench -m /home/jetson/src/workloads/models/tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf -r 1 --no-warmup -o json"
STAMP=$(date +%m%d-%H%M%S)
R=$E/smoke/run-$STAMP
mkdir -p $R
exec > >(tee -a $R/smoke.log) 2>&1
pgrep -a -x llama-bench && { echo "a llama-bench is running: not starting"; exit 3; }
ls /dev/shm > $R/shm-before.txt
one() { # label exdir scmd vcmd vt senv venv
  local name=step0-e9smoke-$1-$STAMP
  local O=$W/runs/$name
  SPDLOG_LEVEL=info SRV_ENV="$6" VIC_ENV="BPFTIME_EXP_GEOMETRY_LOG=$O/geometry.tsv ${7:-}" \
    timeout -k 10 $(($5 + 120)) bash $W/run.sh "$name" "$2" "$3" "$4" $(($5 + 60)) $5 4 | grep -vE '^$'
  echo "geometry lines: $(grep -vc '^#' $O/geometry.tsv 2>/dev/null || echo 0)"
  ln -sfn $O $R/run-$1
}
PARTS=${PARTS:-cg cg-legacy pp32 tg128}
has() { case " $PARTS " in *" $1 "*) return 0;; esac; return 1; }
if has cg; then
one cg-default $W/example/gpu/cudagraph ./cuda_probe ./vec_add_graph 30 "BPFTIME_SHM_MEMORY_MB=64" "BPFTIME_CUDAGRAPH_ITERS=3"
grep -v '^#' $R/run-cg-default/geometry.tsv | awk -F'\t' '{print $4" cap="$14" depth="$3" name="$5" grid="$7"x"$8"x"$9" block="$10"x"$11"x"$12}' | sort | uniq -c
fi
if has cg-legacy; then
one cg-legacy $W/example/gpu/cudagraph ./cuda_probe ./vec_add_graph_legacy 30 "BPFTIME_SHM_MEMORY_MB=64" "BPFTIME_CUDAGRAPH_ITERS=3"
grep -v '^#' $R/run-cg-legacy/geometry.tsv | awk -F'\t' '{print $4" cap="$14" depth="$3" name="$5" grid="$7"x"$8"x"$9" block="$10"x"$11"x"$12}' | sort | uniq -c
grep -E 'loaded the patched image|could not be patched|unpatched' $R/run-cg-legacy/client.log | cut -c1-200 | head -3
grep -c 'C\[1\]=3.0' $R/run-cg-legacy/client.log | sed 's/^/correct results lines: /'
fi
has pp32 && one pp32 $W/example/gpu/threadhist-llama-empty ./threadhist "$BENCH -p 32 -n 0" 60 "BPFTIME_MAP_GPU_THREAD_COUNT=1024 THREADHIST_N=1024"
has tg128 && one tg128 $W/example/gpu/threadhist-llama-empty ./threadhist "$BENCH -p 0 -n 128" 90 "BPFTIME_MAP_GPU_THREAD_COUNT=1024 THREADHIST_N=1024"
pgrep -a -x 'llama-bench|threadhist|cuda_probe|vec_add_graph|vec_add_graph_l' && echo "LEFTOVER PROCESSES ABOVE" || echo "no leftover processes"
ls /dev/shm > $R/shm-after.txt; diff $R/shm-before.txt $R/shm-after.txt && echo "shm unchanged"
for t in pp32 tg128; do
  has $t || continue
  cp $R/run-$t/geometry.tsv $R/geometry-$t.tsv
  cargs=()
  for c in cg-default cg-legacy; do [ -f $R/run-$c/geometry.tsv ] && cargs+=(--control $R/run-$c/geometry.tsv); done
  python3 -I $E/analyze_hooks.py --log $R/geometry-$t.tsv --sqlite $W/llama/census/$t-graph.sqlite --out $R/$t --label $t-smoke "${cargs[@]}"
done
