# Frame-pointer stacks, stack trace map, cgroup arrays (2026-10-25)

Dev branch `codex/uprobe-official-177a1dc465ff`, three commits on top of 2448aeaf. Kernel-independent
work done before switching to the BTF kernel.

| Commit | Change | Test (fails without the change) |
|---|---|---|
| 64da197c | aarch64 user stacks by frame-pointer walk with the kernel's `perf_callchain_user` result (pc first, x29 records, PAC stripped, bounded to the thread stack) instead of gum's accurate backtracer | frida "Uretprobe callbacks can generate a backtrace" (kernel semantics); "Test with backtrace" now passes too |
| d766e6ad | Stack trace map as `__bpf_get_stackid`: zero-padded values (lookups read value_size bytes; the old code read past the stored vector), truncation, same trace -> same id, `-EEXIST` on collision unless `BPF_F_REUSE_STACKID`, full-hash compare for `BPF_F_FAST_STACK_CMP`; `bpf_get_stackid` keeps errors negative | `[stack_trace]`, updated "Test stack trace map 2" |
| f9d2d9fc | `BPF_MAP_TYPE_CGROUP_ARRAY` and helper 37 `bpf_current_task_under_cgroup` | `[cgroup_array]` (3 cases, incl. fork child) |

## Validation

- Daily filter plus `[stack_trace]`, "Test stack trace map*", `[cgroup_array]` (`tests.log`):
  103 cases / 254,872 assertions pass. Frida suite (`frida-tests.log`): 17 cases pass, nothing
  excluded.
- Full ten rounds (`full10-f9d2d9fc`, vs 2448aeaf): every row within +/-2%.
- Real-program audit (`audit.jsonl`): all 7 tools work on f9d2d9fc. funclatency (libbpf-tools)
  6,180,000 / 6,178,800 events; memleak 1,274 outstanding with the full stack
  main -> libc -> `__libc_start_main` -> `_start` (plus `bpftime_hooked_main`, the agent's wrapper).
- cgroup filter (`cgroup-check*.jsonl`): funclatency `-c user.slice` 1,309,000 / 1,307,444 and
  `-c init.scope` 0, same as the kernel. With the filter, 327k calls/s (kernel 469k); the first
  version (open/read/close per call) did 85k.

## Throughput (`measure-run/`, 5 rounds, median ops/s of the traced workload)

| Tool | none | kernel | 2448aeaf | f9d2d9fc | f9d2d9fc vs kernel | paired vs 2448aeaf |
|---|---:|---:|---:|---:|---|---|
| gethostlatency | 112,535 | 59,153 | 86,930 | 88,324 | 1.49x | +1.4%, 4/5 |
| funclatency, bpftime example | 90.0M | 506,440 | 1,831,140 | 1,866,137 | 3.68x | +2.1%, 5/5 |
| funclatency, libbpf-tools | 90.1M | 479,686 | (0 events) | 1,538,596 | 3.21x | not comparable: 2448aeaf disabled the programs |
| memleak `-c` | 42.4M | 321,972 | 20,058 | 420,190 | 1.31x | +1984% |
| malloc example | 42.4M | 534,095 | 2,122,317 | 1,997,428 | 3.74x | -6.0%, 0/5 (code layout, below) |

malloc example counts are now 0.996-1.001 of the calls instead of exactly 1.000: with `BPF_EXIST`
honoured (d0e40e41), an increment whose key was deleted in between is dropped, as in the kernel.

## The malloc example slowdown is code layout (`malloc-ab/`)

- 10 alternating rounds: f9d2d9fc -5.7% (1/10 faster).
- `perf stat`: instructions per call unchanged (20.41G / 2.03M vs 19.01G / 1.90M, both ~10.0k);
  IPC 2.93 -> 2.74. The work is the same, it runs slower.
- `perf record`: the shares of the JIT code (3.0% -> 5.6%), `bpftime_hash_map_lookup_elem_helper`
  (1.1% -> 2.2%) and gum's `_frida_g_array_remove_range` (1.1% -> 2.1%) doubled; none of them
  changed in these commits, but their addresses moved (e.g. the gum function from mod32 16 to 4).
- Control builds of f9d2d9fc with unused nops in `bpf_helper.cpp` (`builds/layout-pad12`,
  `layout-pad28`; same logic, shifted addresses): -3.1% and -0.9% vs 2448aeaf, f9d2d9fc itself
  -4.1% in the same run. Shifting code alone recovers most of the gap.

Not fixed by padding (it moves again with the next change), as with the earlier layout findings.
