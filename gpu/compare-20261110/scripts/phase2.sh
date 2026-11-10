#!/usr/bin/env bash
# Phase 2: A (official, fix probe, legacy victim) and B (dev, same) interleaved for 3 rounds,
# micro + examples each; then C (dev, stock probe, default victim) and D (official, stock probe,
# default victim, a few rows: what an as-is official run reports).
set -u
V=/home/jetson/src/bpftime-gpu-work/compare
R=/home/jetson/src/results/gpu/compare-20261110
cd $V
for r in 1 2 3; do
  python3 gen_configs.py A$r official fix legacy micro; python3 gen_configs.py A$r official fix legacy examples
  python3 gen_configs.py B$r candidate fix legacy micro; python3 gen_configs.py B$r candidate fix legacy examples
done
python3 gen_configs.py C candidate stock default micro; python3 gen_configs.py C candidate stock default examples
python3 gen_configs.py D official stock default micro --cases="Baseline (tiny),Baseline (minimal),Empty probe (tiny),GPU Array map update (tiny),CPU Array map update (minimal)"
python3 gen_configs.py D official stock default examples --cases="Baseline (minimal),CUDA Counter (minimal),Kernel Retsnoop (minimal)"
for r in 1 2 3; do
  for x in A B; do
    b=official; [ $x = B ] && b=candidate
    bash run_batch.sh $b $V/configs/$x$r-micro.json 1800 || echo "$x$r micro rc=$?"
    bash run_batch.sh $b $V/configs/$x$r-examples.json 1200 || echo "$x$r examples rc=$?"
  done
done
bash run_batch.sh candidate $V/configs/C-micro.json 1800 || echo "C micro rc=$?"
bash run_batch.sh candidate $V/configs/C-examples.json 1200 || echo "C examples rc=$?"
bash run_batch.sh official $V/configs/D-micro.json 600 || echo "D micro rc=$?"
bash run_batch.sh official $V/configs/D-examples.json 600 || echo "D examples rc=$?"
python3 verify_runs.py $R/runs/A1 $R/runs/A2 $R/runs/A3 $R/runs/B1 $R/runs/B2 $R/runs/B3 $R/runs/C $R/runs/D > $R/phase2-verify.csv
echo done $(date +%T)
