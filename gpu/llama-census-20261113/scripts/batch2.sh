#!/bin/bash
# batch2.sh: (1) census with CUDA graphs really on; (2) threadhist on the top kernels, counts vs census;
# (3) threadhist on rms_norm with the default 1,024 slots (records lost, measured).
# All bpftime runs: --no-warmup -r 1 so counts compare 1:1 with the census; graphs off.
set -u
W=/home/jetson/src/bpftime-gpu-work
B='/home/jetson/src/workloads/llama.cpp/build-ptx87/bin/llama-bench -m /home/jetson/src/workloads/models/tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf -r 1 --no-warmup -o json'
clk() { echo "cpu5=$(cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq) gpu=$(cat /sys/devices/platform/bus@0/17000000.gpu/devfreq/17000000.gpu/cur_freq) $(date +%T)"; }
echo "start $(clk)"
MODES=graph bash /home/jetson/src/results/gpu/llama-census-20261113/scripts/census.sh
cd $W
run() { # name dir slots args
  SPDLOG_LEVEL=info SRV_ENV="$5 THREADHIST_N=$3" VIC_ENV='GGML_CUDA_DISABLE_GRAPHS=1' timeout 2400 ./run.sh "$1" "$2" './threadhist' "$B $4" 900 1800 4 | grep -E 'client_rc|exists|leftover'
  grep 'lookup=' runs/$1/server.log | tail -1
}
run llama-th-mmq-pp512 example/gpu/th-mmq 90112 '-p 512 -n 0' 'BPFTIME_MAP_GPU_THREAD_COUNT=90112'
run llama-th-ropek-pp512 example/gpu/th-ropek 524288 '-p 512 -n 0' 'BPFTIME_MAP_GPU_THREAD_COUNT=524288'
run llama-th-mmvq-tg128 example/gpu/th-mmvq 720896 '-p 0 -n 128' 'BPFTIME_MAP_GPU_THREAD_COUNT=720896'
run llama-th-default-pp512 example/gpu/threadhist-llama 1024 '-p 512 -n 0' ''
echo "end $(clk)"
pgrep -a llama-bench || echo no-llama-leftover
