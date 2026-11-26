#!/bin/bash
# C1: short runs only to dump the patched PTX (not measurements).
# Each run through run.sh with build tvf-7d02d677 and its own dump dir.
# Commands copied from fix-matrix.sh (cc-legacy, ts-legacy, cg-default),
# results gpu/llama-census-20261113/runs/llama-hostcall-pp32/meta.txt
# (th-hostcall) and results gpu/hash-current-20261126 (pt_hash).
# Usage: run_dumps.sh CASE...   (cases: cc ts cg th ptcnt ptrmw pttid)
set -u
W=/home/jetson/src/bpftime-gpu-work
C=$W/proto/hashdesign/c1
V=$W/benchmark/gpu/workload
E=$W/example/gpu
export GPU_BUILD=$W/proto/builds/tvf-7d02d677
export SPDLOG_LEVEL=info
LL="/home/jetson/src/workloads/llama.cpp/build-ptx87/bin/llama-bench -m /home/jetson/src/workloads/models/tinyllama-1.1b-chat-v1.0.Q4_K_M.gguf -p 32 -n 0 -r 1 -o json"
one() { # CASE RUN EXDIR SRV_ENV SCMD VIC_ENV VCMD VT
	local d=$C/ptx/$2
	mkdir -p "$d"
	SRV_ENV="$4" VIC_ENV="$6 BPFTIME_CUDA_PTX_DUMP_DIR=$d" timeout 400 "$W/run.sh" "$2" "$3" "$5" "$7" 90 "$8" > "$C/ptx/$2.runsh.txt" 2>&1
	echo "$1 $2 rc=$? dumps=$(ls "$d" | wc -l)"
}
for c in "$@"; do
	case $c in
	cc) one cc c1-cc "$E/cuda-counter" 'BPFTIME_SHM_MEMORY_MB=64' ./cuda_probe 'BPFTIME_TRACE_PRINTK_OUTPUT=stdout' "$V/vec_add_legacy 10 5 1 10" 60 ;;
	ts) one ts c1-ts "$E/threadscheduling" 'BPFTIME_SHM_MEMORY_MB=64' ./threadscheduling '' "$V/vec_add_legacy 10 5 10 1" 60 ;;
	cg) one cg c1-cg "$E/cudagraph" 'BPFTIME_SHM_MEMORY_MB=64' ./cuda_probe 'BPFTIME_CUDAGRAPH_ITERS=5' ./vec_add_graph 60 ;;
	th) one th c1-th "$E/th-hostcall" '' ./threadhist 'GGML_CUDA_DISABLE_GRAPHS=1' "$LL" 150 ;;
	ptcnt) one ptcnt c1-ptcnt "$W/proto/tools" 'BPFTIME_MAP_GPU_THREAD_COUNT=131072' "./pt_loader bpf/hs_cnt_thread_kc_test.bpf.o $W/runs/c1-ptcnt" 'BPFTIME_MAP_GPU_THREAD_COUNT=131072' './pt_victim 1 0' 60 ;;
	ptrmw) one ptrmw c1-ptrmw "$W/proto/tools" 'BPFTIME_MAP_GPU_THREAD_COUNT=131072' "./pt_loader bpf/hs_rmw_thread_kc_test.bpf.o $W/runs/c1-ptrmw" 'BPFTIME_MAP_GPU_THREAD_COUNT=131072' './pt_victim 1 0' 60 ;;
	pttid) one pttid c1-pttid "$W/proto/tools" 'BPFTIME_MAP_GPU_THREAD_COUNT=131072' "./pt_loader bpf/hs_tid_thread_kc_test.bpf.o $W/runs/c1-pttid" 'BPFTIME_MAP_GPU_THREAD_COUNT=131072' './pt_victim 1 0' 60 ;;
	*) echo "unknown case $c" ;;
	esac
done
