#!/bin/bash
# step0 e9-hooks (check 9): log every launch-hook geometry under the
# instrumentation build, then compare with the nsys census per kernel.
#   1. preflight;
#   2. controls: the cudagraph example (one direct launch, then
#      cudaGraphAddKernelNode / KernelNodeSetParams / ExecKernelNodeSetParams
#      and 3 replays), default launch and legacy launch, with its probe:
#      the log must name _Z9vectorAddPKfS0_Pf at the direct launch and show
#      which graph-node calls reach the hooks;
#   3. llama-bench "-p 512 -n 0 -r 1 --no-warmup" and "-p 0 -n 128 -r 1
#      --no-warmup", graphs on, the empty kretprobe on rms_norm attached
#      (so the agent's hooks are installed), BPFTIME_EXP_GEOMETRY_LOG set;
#   4. analyze_hooks.py against census/{pp512-graph,tg128-graph}.sqlite.
# Run as:  timeout -k 10 1800 bash run_all.sh
# Expected wall: ~2-3 min (two ~15 s controls; pp512 and tg128 with the empty
# probe, ~16 s agent start-up each; the tg128 smoke's victim took 13 s).
# Per-run caps (victim 300 s, server 360 s, outer 430 s) fire before 1800 s.
set -u
E=/home/jetson/src/bpftime-gpu-work/step0/e9-hooks
W=/home/jetson/src/bpftime-gpu-work
X=/home/jetson/src/step0-bpftime-exp
CENSUS=$W/llama/census
export GPU_BUILD=$X/build-cuda
unset GGML_CUDA_DISABLE_GRAPHS   # llama.cpp tests only whether it is set: graphs on
LB=/home/jetson/src/workloads/llama.cpp/build-ptx87/bin/llama-bench
MODEL=/home/jetson/src/workloads/models/tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf
BENCH="$LB -m $MODEL -r 1 --no-warmup -o json"
STAMP=$(date +%m%d-%H%M%S)
# CONTROLS_ONLY=1: preflight and the two cudagraph controls only (~40 s), into results/controls-only-<stamp>
R=$E/results/${CONTROLS_ONLY:+controls-only-}$STAMP
mkdir -p $R
exec > >(tee -a $R/run_all.log) 2>&1
echo "e9-hooks run_all $STAMP GPU_BUILD=$GPU_BUILD"
ls /dev/shm > $R/shm-before.txt
bash $E/preflight.sh > $R/preflight.txt 2>&1; pf=$?
cat $R/preflight.txt
[ $pf = 0 ] || { echo "preflight failed: nothing run"; exit 3; }

# one run: label exdir server-cmd victim-cmd victim-timeout srv_env [extra victim env]
one() {
  local label=$1 exdir=$2 scmd=$3 vcmd=$4 vt=$5 senv=$6 venv=${7:-}
  local name=step0-e9-$label-$STAMP
  local O=$W/runs/$name
  local st=$((vt + 60))
  echo "== $name ($(date +%T))"
  SPDLOG_LEVEL=info SRV_ENV="$senv" VIC_ENV="BPFTIME_EXP_GEOMETRY_LOG=$O/geometry.tsv $venv" \
    timeout -k 10 $((st + 60)) bash $W/run.sh "$name" "$exdir" "$scmd" "$vcmd" $st $vt 4 | grep -vE '^$'
  echo "geometry lines: $(grep -vc '^#' $O/geometry.tsv 2>/dev/null || echo 0)"
  grep -E 'loaded the patched image|could not be patched|unpatched|refused' $O/client.log | cut -c1-250 | head -5
  ln -sfn $O $R/run-$label
}

# 2. controls (graph-node hooks, names of handles and stubs)
CG=$W/example/gpu/cudagraph
one cg-default $CG ./cuda_probe ./vec_add_graph 60 "BPFTIME_SHM_MEMORY_MB=64" "BPFTIME_CUDAGRAPH_ITERS=3"
one cg-legacy  $CG ./cuda_probe ./vec_add_graph_legacy 60 "BPFTIME_SHM_MEMORY_MB=64" "BPFTIME_CUDAGRAPH_ITERS=3"
for c in cg-default cg-legacy; do
  G=$R/run-$c/geometry.tsv
  {
    echo "== $c: lines by hook / capture / depth"
    grep -v '^#' $G 2>/dev/null | awk -F'\t' '{print $4" cap="$14" depth="$3" name="$5" grid="$7"x"$8"x"$9" block="$10"x"$11"x"$12}' | sort | uniq -c
  } | tee $R/control-$c.txt
done
# V6 instrument check now: no llama run when a control fails (the verdict would be INCONCLUSIVE anyway)
python3 -I $E/analyze_hooks.py --controls-only --out $R/controls \
  --control $R/run-cg-default/geometry.tsv --control $R/run-cg-legacy/geometry.tsv; crc=$?
if [ $crc != 0 ] || [ -n "${CONTROLS_ONLY:-}" ]; then
  [ $crc != 0 ] && echo "a cudagraph control failed: not running llama-bench"
  pgrep -a -x 'cuda_probe|vec_add_graph|vec_add_graph_l' && echo "LEFTOVER PROCESSES ABOVE" || echo "no leftover processes"
  ls /dev/shm > $R/shm-after.txt
  diff $R/shm-before.txt $R/shm-after.txt > $R/shm-diff.txt && echo "shm unchanged" || { echo "shm changed:"; cat $R/shm-diff.txt; }
  echo "results: $R"
  exit $crc
fi

# 3. llama.cpp, graphs on, empty probe on rms_norm
TH=$W/example/gpu/threadhist-llama-empty
one pp512 $TH ./threadhist "$BENCH -p 512 -n 0" 300 "BPFTIME_MAP_GPU_THREAD_COUNT=1024 THREADHIST_N=1024"
one tg128 $TH ./threadhist "$BENCH -p 0 -n 128" 300 "BPFTIME_MAP_GPU_THREAD_COUNT=1024 THREADHIST_N=1024"

# 4. leftovers and analysis
pgrep -a -x 'llama-bench|threadhist|cuda_probe|vec_add_graph|vec_add_graph_l' && echo "LEFTOVER PROCESSES ABOVE" || echo "no leftover processes"
ls /dev/shm > $R/shm-after.txt
diff $R/shm-before.txt $R/shm-after.txt > $R/shm-diff.txt && echo "shm unchanged" || { echo "shm changed:"; cat $R/shm-diff.txt; }
for t in pp512 tg128; do
  cp $R/run-$t/geometry.tsv $R/geometry-$t.tsv 2>/dev/null
  # V6: the cudagraph controls are part of the verdict (INCONCLUSIVE if either fails or is missing)
  python3 -I $E/analyze_hooks.py --log $R/geometry-$t.tsv --sqlite $CENSUS/$t-graph.sqlite --out $R/$t --label $t \
    --control $R/run-cg-default/geometry.tsv --control $R/run-cg-legacy/geometry.tsv
  echo
done
echo "results: $R"
