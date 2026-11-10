#!/usr/bin/env bash
# Phase 1: the host-map gate (threadhist with 1512, mem_trace with 1513 on each build) and the
# evidence pass (every micro mode + cuda-counter + kernelretsnoop on the minimal geometry, debug
# level), legacy victim, both builds.
set -u
V=/home/jetson/src/bpftime-gpu-work/compare
cd $V
python3 gen_configs.py G-off official fix legacy gate
python3 gen_configs.py G-dev candidate fix legacy gate
python3 gen_configs.py E-off official fix legacy evidence --level=debug
python3 gen_configs.py E-dev candidate fix legacy evidence --level=debug
bash run_batch.sh official $V/configs/G-off-gate.json 600 || echo "G-off rc=$?"
bash run_batch.sh candidate $V/configs/G-dev-gate.json 600 || echo "G-dev rc=$?"
bash run_batch.sh official $V/configs/E-off-evidence.json 900 || echo "E-off rc=$?"
bash run_batch.sh candidate $V/configs/E-dev-evidence.json 900 || echo "E-dev rc=$?"
R=/home/jetson/src/results/gpu/compare-20261110
python3 verify_runs.py $R/runs/G-off $R/runs/G-dev $R/runs/E-off $R/runs/E-dev > $R/phase1-verify.csv
column -s, -t < $R/phase1-verify.csv | cut -c1-230
