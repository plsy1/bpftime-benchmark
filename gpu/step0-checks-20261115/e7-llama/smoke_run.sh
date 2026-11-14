#!/bin/bash
# step0 e7-llama smoke (short, counts only): the positive control and one
# llama-bench pp32 run with the rms_norm probe, under the instrumentation
# build. Checks the whole chain (helper 520, election, 1513 adds, loader,
# kept PTX, analysis) before the full run_all.sh; tg16 checks the counts
# under CUDA graph replays (45 rms_norm launches per token); tg8mmvq times
# the heaviest probe path (mul_mat_vec_q, 8 decode tokens). Wall ~3 min.
# Run as:  timeout -k 10 900 bash smoke_run.sh   (PARTS="ctrl pp32 tg16 tg8mmvq" by default)
set -u
E=/home/jetson/src/bpftime-gpu-work/step0/e7-llama
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
printf 'label\ttest\tkidx\trun_dir\tjson\tctrl_launches\tctrl_blocks\n' > $R/runs.tsv
one() { # label test obj kidx vcmd vt venv
  local name=step0-e7smoke-$1-$2-$STAMP O
  O=$W/runs/$name
  SPDLOG_LEVEL=info VIC_ENV="${7:-}" timeout -k 10 $(($6 + 120)) bash $W/run.sh "$name" "$E" "./e7_loader $3 $O/e7.json" "$5" $(($6 + 60)) $6 4 | grep -vE '^$'
  grep -E '^kernel|lanes=' $O/server.log | head -12
  printf '%s\t%s\t%s\t%s\t%s\t10\t64\n' "$1" "$2" "$4" "$O" "$O/e7.json" >> $R/runs.tsv
  ln -sfn $O $R/run-$1-$2
}
PARTS=${PARTS:-ctrl pp32 tg16 tg8mmvq}
has() { case " $PARTS " in *" $1 "*) return 0;; esac; return 1; }
if has ctrl; then
one ctrl ctrl bpf/e7_ctrl.bpf.o 4 "$E/e7_ctrl 10 64" 30 "BPFTIME_CUDA_KEEP_PTX=1"
for k in 5 6; do printf '%s\t%s\t%s\t%s\t%s\t10\t64\n' ctrl ctrl $k "$W/runs/step0-e7smoke-ctrl-ctrl-$STAMP" "$W/runs/step0-e7smoke-ctrl-ctrl-$STAMP/e7.json" >> $R/runs.tsv; done
C=$W/runs/step0-e7smoke-ctrl-ctrl-$STAMP
mkdir -p $C/kept-ptx
for d in $(grep -oE '/tmp/bpftime-fatbin-work\.[A-Za-z0-9]{6}' $C/client.log | sort -u); do
  [ -d "$d" ] && [ -O "$d" ] && mv "$d" $C/kept-ptx/ && echo "kept $d"
done
P=$(grep -l "e7ctrl_partial" $C/kept-ptx/*/patched.* 2>/dev/null | head -1)
[ -n "$P" ] && { python3 -I $E/check_ptx.py "$P" e7ctrl_partial $C/ptxcheck --ptxas /usr/local/cuda-13.2/bin/ptxas > $C/ptxcheck.out 2>&1; echo "ptx check (partial) rc=$?";
  python3 -I $E/check_ptx.py "$P" e7ctrl_diverge $C/ptxcheck-div --ptxas /usr/local/cuda-13.2/bin/ptxas --min-sass-probe-calls 2 > $C/ptxcheck-div.out 2>&1; echo "ptx check (diverge) rc=$?";
  python3 -I $E/check_ptx.py "$P" e7ctrl_merge $C/ptxcheck-merge --ptxas /usr/local/cuda-13.2/bin/ptxas --min-sass-probe-calls 2 > $C/ptxcheck-merge.out 2>&1; echo "ptx check (merge, reported) rc=$?"; } || echo "no patched PTX found"
grep -E '^e7_ctrl:' $C/client.log
fi
has pp32 && one rms pp32 bpf/e7_rms.bpf.o 0 "$BENCH -p 32 -n 0" 60
has tg16 && one rms tg16 bpf/e7_rms.bpf.o 0 "$BENCH -p 0 -n 16" 60
# the heaviest probe path: ~6M leader calls (98M in the full tg128 run); sets run_all's timeout
has tg8mmvq && one mmvq tg8 bpf/e7_mmvq.bpf.o 2 "$BENCH -p 0 -n 8" 120
pgrep -a -x 'llama-bench|e7_loader|e7_ctrl' && echo "LEFTOVER PROCESSES ABOVE" || echo "no leftover processes"
ls /dev/shm > $R/shm-after.txt; diff $R/shm-before.txt $R/shm-after.txt && echo "shm unchanged"
python3 -I $E/analyze.py $R
