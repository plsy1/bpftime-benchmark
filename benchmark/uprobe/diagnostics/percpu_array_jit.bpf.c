// SPDX-License-Identifier: GPL-2.0
// Identical consuming workload, with per-CPU data AND result maps. No mixed
// map types or auxiliary CPU helper in the BPF program obscure eligibility.
#define BPFTIME_ARRAY_JIT_MAP_TYPE BPF_MAP_TYPE_PERCPU_ARRAY
#define BPFTIME_ARRAY_JIT_PATH "uprobe/benchmark/uprobe/.output/percpu-array-jit/victim:"
#include "array_jit_poc.bpf.c"
