#!/bin/bash
# step0 e7-llama (check 7, llama part): run everything, then analyze.
#   1. preflight (build done, instrumentation present, nothing left over);
#   2. controls: e7_ctrl (e7ctrl_partial: 48-thread blocks; e7ctrl_diverge:
#      odd lanes leave early, even lanes wait ~200 us; e7ctrl_merge: the same
#      without the wait, reported) with their probes, PTX kept and checked
#      (helper calls, atomic add, scoped SASS, separate probe call sites);
#   3. llama-bench pp512 with a probe on each of rms, mmq, mmvq, ropek, and
#      tg128 (graphs on) with a probe on each of rms, mmvq, ropek: 7 runs,
#      one object and one run per kernel (ALL4=1 adds one pp512 and one
#      tg128 run with all four programs in one object, exploratory);
#   4. analyze.py -> results/<stamp>/summary.txt, summary.json, hist.csv, waste.csv.
# Run as:  timeout -k 10 3600 bash run_all.sh   (ALL4=1: timeout -k 10 4500)
# Expected wall: ~8 min. Each llama run pays the agent start-up: ~16 s, ~35 s
# when the probed kernel's module is large (mul_mat_q, mul_mat_vec_q: patching
# and compiling). The heaviest run, tg128 mmvq (98M probe calls), is expected
# at ~90 s from the tg8 smoke (6.1M calls in under 3 s after a 38 s start).
# Per-run caps (victim 300 s, server 360 s, outer 430 s) fire before the
# outer 3600 s.
# Counts only, no timing: the board need not be quiet, but run it alone.
set -u
E=/home/jetson/src/bpftime-gpu-work/step0/e7-llama
W=/home/jetson/src/bpftime-gpu-work
X=/home/jetson/src/step0-bpftime-exp
export GPU_BUILD=$X/build-cuda
unset GGML_CUDA_DISABLE_GRAPHS   # llama.cpp tests only whether it is set: graphs on
LB=/home/jetson/src/workloads/llama.cpp/build-ptx87/bin/llama-bench
MODEL=/home/jetson/src/workloads/models/tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf
BENCH="$LB -m $MODEL -r 1 --no-warmup -o json"
STAMP=$(date +%m%d-%H%M%S)
# CONTROLS_ONLY=1: preflight, target check and the control run only (~30 s), into results/controls-only-<stamp>
R=$E/results/${CONTROLS_ONLY:+controls-only-}$STAMP
mkdir -p $R
exec > >(tee -a $R/run_all.log) 2>&1
echo "e7-llama run_all $STAMP GPU_BUILD=$GPU_BUILD"
ls /dev/shm > $R/shm-before.txt
bash $E/preflight.sh > $R/preflight.txt 2>&1; pf=$?
cat $R/preflight.txt
[ $pf = 0 ] || { echo "preflight failed: nothing run"; exit 3; }
# the probed names are the ones the census counts (a name that matches nothing never fires)
python3 -I $E/check_targets.py > $R/targets.txt 2>&1; tc=$?
tail -1 $R/targets.txt
[ $tc = 0 ] || { cat $R/targets.txt; echo "target check failed: nothing run"; exit 3; }
printf 'label\ttest\tkidx\trun_dir\tjson\tctrl_launches\tctrl_blocks\n' > $R/runs.tsv

# one run under run.sh: label test obj kidx(es) victim-cmd victim-timeout [vic_env]
one() {
  local label=$1 test=$2 obj=$3 kidxs=$4 vcmd=$5 vt=$6 venv=${7:-}
  local name=step0-e7-$label-$test-$STAMP
  local st=$((vt + 60))
  echo "== $name ($(date +%T))"
  SPDLOG_LEVEL=info VIC_ENV="$venv" timeout -k 10 $((st + 60)) \
    bash $W/run.sh "$name" "$E" "./e7_loader $obj $W/runs/$name/e7.json" "$vcmd" $st $vt 4 | grep -vE '^$'
  local O=$W/runs/$name
  grep -E 'final|^kernel|lanes=' $O/server.log | head -40
  grep -E 'loaded the patched image|could not be patched|unpatched|refused|outside map' $O/client.log | cut -c1-300 | head -8
  for k in $kidxs; do
    printf '%s\t%s\t%s\t%s\t%s\t10\t64\n' "$label" "$test" "$k" "$O" "$O/e7.json" >> $R/runs.tsv
  done
  ln -sfn $O $R/run-$label-$test
}

# 2. positive control (PTX kept: the agent logs where it wrote it)
# both control kernels run in one victim; the control object holds both programs
one ctrl ctrl bpf/e7_ctrl.bpf.o "4 5 6" "$E/e7_ctrl 10 64" 60 "BPFTIME_CUDA_KEEP_PTX=1"
C=$W/runs/step0-e7-ctrl-ctrl-$STAMP
mkdir -p $C/kept-ptx
for d in $(grep -oE '/tmp/bpftime-fatbin-work\.[A-Za-z0-9]{6}' $C/client.log | sort -u); do
  [ -d "$d" ] && [ -O "$d" ] && mv "$d" $C/kept-ptx/ && echo "kept $d -> $C/kept-ptx/"
done
P=$(grep -l "e7ctrl_partial" $C/kept-ptx/*/patched.* 2>/dev/null | head -1)
if [ -n "$P" ]; then
  python3 -I $E/check_ptx.py "$P" e7ctrl_partial $C/ptxcheck --ptxas /usr/local/cuda-13.2/bin/ptxas > $C/ptxcheck.out 2>&1
  echo "ptx check (partial) rc=$? ($C/ptxcheck.check.json)"; grep -E '"pass"' $C/ptxcheck.check.json
  # the divergence control must keep its two exits as two probe call sites in SASS
  python3 -I $E/check_ptx.py "$P" e7ctrl_diverge $C/ptxcheck-div --ptxas /usr/local/cuda-13.2/bin/ptxas --min-sass-probe-calls 2 > $C/ptxcheck-div.out 2>&1
  echo "ptx check (diverge) rc=$? ($C/ptxcheck-div.check.json)"; grep -E '"pass"' $C/ptxcheck-div.check.json
  python3 -I $E/check_ptx.py "$P" e7ctrl_merge $C/ptxcheck-merge --ptxas /usr/local/cuda-13.2/bin/ptxas --min-sass-probe-calls 2 > $C/ptxcheck-merge.out 2>&1
  echo "ptx check (merge, reported) rc=$? ($C/ptxcheck-merge.check.json)"; grep -E '"pass"' $C/ptxcheck-merge.check.json
else
  echo "ptx check: no patched PTX for e7ctrl_partial found (see $C/client.log)"
fi
grep -E '^e7_ctrl:' $C/client.log
# rules 1 and 2 now: no llama run when a control or a compiled-code check fails (all would be INCONCLUSIVE)
python3 -I $E/analyze.py $R > $R/controls-analysis.txt 2>&1
grep -E '^(ctrl|overall|control_pass|ptx_check_pass|merge_observation)' $R/controls-analysis.txt
if ! python3 -I -c 'import json,sys; v=json.load(open(sys.argv[1]))["verdicts"]; sys.exit(0 if v["control_pass"] and v["ptx_check_pass"] else 1)' $R/summary.json; then
  echo "a control or a PTX/SASS check failed: stopping before the llama runs"
  exit 4
fi
if [ -n "${CONTROLS_ONLY:-}" ]; then
  pgrep -a -x 'e7_loader|e7_ctrl' && echo "LEFTOVER PROCESSES ABOVE" || echo "no leftover processes"
  ls /dev/shm > $R/shm-after.txt
  diff $R/shm-before.txt $R/shm-after.txt > $R/shm-diff.txt && echo "shm unchanged" || { echo "shm changed:"; cat $R/shm-diff.txt; }
  echo "controls only: $R"
  exit 0
fi

# 3. llama.cpp
one rms   pp512 bpf/e7_rms.bpf.o   0 "$BENCH -p 512 -n 0" 300
one mmq   pp512 bpf/e7_mmq.bpf.o   1 "$BENCH -p 512 -n 0" 300
one mmvq  pp512 bpf/e7_mmvq.bpf.o  2 "$BENCH -p 512 -n 0" 300
one ropek pp512 bpf/e7_ropek.bpf.o 3 "$BENCH -p 512 -n 0" 300
one rms   tg128 bpf/e7_rms.bpf.o   0 "$BENCH -p 0 -n 128" 300
one mmvq  tg128 bpf/e7_mmvq.bpf.o  2 "$BENCH -p 0 -n 128" 300
one ropek tg128 bpf/e7_ropek.bpf.o 3 "$BENCH -p 0 -n 128" 300
if [ -n "${ALL4:-}" ]; then
  one all4 pp512 bpf/e7_all4.bpf.o "0 1 2 3" "$BENCH -p 512 -n 0" 300
  one all4 tg128 bpf/e7_all4.bpf.o "0 1 2 3" "$BENCH -p 0 -n 128" 300
fi

# 4. leftovers and analysis
pgrep -a -x 'llama-bench|e7_loader|e7_ctrl' && echo "LEFTOVER PROCESSES ABOVE" || echo "no leftover processes"
ls /dev/shm > $R/shm-after.txt
diff $R/shm-before.txt $R/shm-after.txt > $R/shm-diff.txt && echo "shm unchanged" || { echo "shm changed:"; cat $R/shm-diff.txt; }
python3 -I $E/analyze.py $R
echo "results: $R"
