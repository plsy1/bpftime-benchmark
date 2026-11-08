#!/usr/bin/env bash
# Per-process cost of preparing programs: JIT (what every traced process does now) against loading
# an AOT object compiled once (LLJIT + JITLink, helpers resolved by name in the process).
# bench_aot.cpp, linked against build-probecheck (dev 9c088441 runtime, checks on) with the AOT
# tool's link line; one fresh process per measurement, pinned to CPU 5, modes alternated.
set -u
N=/home/jetson/src/results/uprobe/remeasure-20261108/aot-link
BIN=$1; OBJ=$2; REPS=${3:-20}
LT=/home/jetson/src/realprog-20261026-btf/libbpf-tools/.output
CTRL=/home/jetson/src/results/uprobe/output-dims-20261106/src/out-control.bpf.o
# memleak: the 12 entry programs only. Its exit programs call a subprogram (gen_alloc_exit2), which
# libbpf appends at load; bpf_object__open alone leaves the call unresolved and the JIT refuses it.
ML=malloc_enter,free_enter,calloc_enter,realloc_enter,mmap_enter,munmap_enter,mremap_enter,posix_memalign_enter,aligned_alloc_enter,valloc_enter,memalign_enter,pvalloc_enter
export BPFTIME_LOG_OUTPUT=console SPDLOG_LEVEL=warn
sets=("control $CTRL on_count" "funclatency $LT/funclatency.bpf.o dummy_kprobe,dummy_kretprobe"
      "opensnoop $LT/opensnoop.bpf.o tracepoint__syscalls__sys_enter_openat,tracepoint__syscalls__sys_exit_openat"
      "memleak $LT/memleak.bpf.o $ML")
for s in "${sets[@]}"; do
  read -r name elf progs <<< "$s"
  mkdir -p $OBJ/$name
  timeout 300 $BIN build $elf $progs $OBJ/$name | grep -E '^programs' | sed "s/^/build $name /"
  for i in $(seq $REPS); do
    for mode in jit load; do
      timeout 300 taskset -c 5 $BIN $mode $elf $progs $OBJ/$name 2>/dev/null | grep -E '^programs' | sed "s/^/$mode $name rep $i /"
    done
  done
done
