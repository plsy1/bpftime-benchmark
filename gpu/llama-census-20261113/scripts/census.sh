#!/bin/bash
# census.sh: llama-bench under nsys, no bpftime, no LD_PRELOAD. One run per (test, graphs).
# --no-warmup -r 1: each run is exactly one prefill (pp<N>) or one 128-token decode (tg128).
# Graph runs use --cuda-graph-trace=node so kernels inside CUDA graphs are recorded one by one.
set -u
L=/home/jetson/src/workloads/llama.cpp/build-ptx87/bin
M=/home/jetson/src/workloads/models/tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf
N=/opt/nvidia/nsight-systems/2026.3.1/bin/nsys
O=${O:-/home/jetson/src/bpftime-gpu-work/llama/census}
mkdir -p "$O"
clk() { echo "cpu5=$(cat /sys/devices/system/cpu/cpu5/cpufreq/scaling_cur_freq) gpu=$(cat /sys/devices/platform/bus@0/17000000.gpu/devfreq/17000000.gpu/cur_freq) $(date +%T)"; }
echo "start $(clk)" | tee "$O/clocks.txt"
for g in nograph graph; do
  for t in "pp32:-p 32 -n 0" "pp128:-p 128 -n 0" "pp512:-p 512 -n 0" "tg128:-p 0 -n 128"; do
    name=${t%%:*}; args=${t#*:}
    if [ $g = nograph ]; then E="GGML_CUDA_DISABLE_GRAPHS=1"; GT=; else E="GGML_CUDA_DISABLE_GRAPHS=0"; GT="--cuda-graph-trace=node"; fi
    echo "$name-$g $(clk)" | tee -a "$O/clocks.txt"
    timeout 900 env $E $N profile -t cuda $GT --force-overwrite true -o "$O/$name-$g" \
      "$L/llama-bench" -m "$M" $args -r 1 --no-warmup -o json > "$O/$name-$g.log" 2>&1
    echo "rc=$? $name-$g" | tee -a "$O/clocks.txt"
    timeout 600 $N export --type sqlite --force-overwrite true -o "$O/$name-$g.sqlite" "$O/$name-$g.nsys-rep" > /dev/null 2>&1
  done
done
echo "end $(clk)" | tee -a "$O/clocks.txt"
