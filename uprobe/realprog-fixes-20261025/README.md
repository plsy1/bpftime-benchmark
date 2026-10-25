# Fixes for the real-program audit failures (2026-10-25)

Dev branch `codex/uprobe-official-177a1dc465ff`, on top of bca2ebce. Order requested: uretprobe
stacks, hash map flags, JIT crash with the missing helper. Following each failure led to more
fixes than planned; every one is a separate commit with a unit test that fails without it.

| Commit | Fix | Test (fails without the fix) |
|---|---|---|
| 97a837bc | Stack helpers work in uretprobes (memleak recorded no stacks) | frida "Uretprobe callbacks can generate a backtrace" |
| 28637b6b | Programs are skipped after the exit-time destructor unmapped the shm (the stack fix exposed a SIGSEGV at exit; official + stack fix crashes the same way) | `[shm_release]` |
| a43f6b85 | A child forked before the syscall server started passes syscalls through (memleak `-c`: the child started a second server whose new session reset the parent's maps: "Expected fd 4 to be a map fd") | `[allocation][syscall_server][fork]` |
| d0e40e41 | Hash map honours `BPF_NOEXIST`/`BPF_EXIST`, reports full map and missing delete (malloc example over-counted 10-20%) | `[maps][hash][flags]` |
| 86f98862 | JIT registers the return block of a bpf-to-bpf call at pc 0 (crash in the traced process; parent-owned llvmbpf patch, submodule untouched) | `[jit_load]` call at pc 0 |
| df865b3e | A program whose LLVM JIT compile failed is disabled once instead of recompiling and logging on every call | `[jit_load]` disabled program |
| 2448aeaf | The uretprobe CPU context is read from the current gum invocation instead of a thread-local set in on_leave (97a837bc cost uretprobe +1.9%) | same frida test |

The JIT crash was not caused by the missing helper: libbpf-tools funclatency's kretprobe starts
with a call to its non-inlined body, which hit an off-by-one in llvmbpf. The missing helper 37
(`bpf_current_task_under_cgroup`) only makes the JIT fail cleanly.

## Validation

- Daily filter plus `[shm_release]`, `[jit_load]` (`tests.log`, head 2448aeaf): 97 cases /
  254,800 assertions, all pass. Frida attach suite (`frida-tests.log`): 16 cases pass;
  "Test with backtrace" is excluded, it fails on this LTO build before and after these commits
  (the outer fixtures are inlined).
- Full ten rounds, userspace + embedded, no kernel arm:
  - `full10-df865b3e`: uretprobe +1.9%, uprobe + uretprobe +2.8% vs bca2ebce (the thread-local
    in on_leave; `__tls_get_addr` per access in the agent). Fixed by 2448aeaf.
  - `full10-2448aeaf`: every row within +/-2.1% of bca2ebce; uretprobe 267.7 ns (-1.3%),
    uprobe + uretprobe 324.4 ns (0.0%). Array lookup +2.1% is 0.025 ns on a row that does not
    consume the result. Hash lookup/update are 6-10% faster in both runs; not claimed as a gain
    (different runs, possibly code layout).
- ssl-nginx fair scope, 2 rounds x 10, legs rotated (`bench/summary.md`, `bench/run-ab.sh`):

  | Payload | bca2ebce req/s | 2448aeaf req/s | vs kernel | paired 2448aeaf vs bca2ebce |
  |---|---:|---:|---|---|
  | 16 B | 13,927 | 14,065 | +17.9% -> +19.1% | +0.90%, 13/20 faster |
  | 256 KB | 1,353.5 | 1,356.0 | +12.2% -> +12.4% | +0.57%, 12/20 faster |

  No regression (differences within the 2-4% CV). Audit at 256 KB: 17.9 / 18.0 / 17.9 sslsniff
  events per request for kernel / bca2ebce / 2448aeaf.

## Real-program audit after the fixes (`audit.jsonl`, df865b3e; 2448aeaf re-checked for memleak)

| Tool | kernel | bca2ebce | df865b3e |
|---|---|---|---|
| funclatency (libbpf-tools) | 1,921,000 / 1,918,537 | workload SIGSEGV | runs, 0 events (helper 37 missing, program disabled once) |
| funclatency (bpftime example) | match | match | match |
| gethostlatency | 234,335 / 233,866 | 345,034 / 350,788 | 343,908 / 346,568 |
| memleak `-c` | 1,027 outstanding | 0 (48 map errors) | 1,274 outstanding, main+0xf0 stack correct |
| malloc example | 2,129,030 / 2,128,278 | 8,547,012 / 8,545,989 | 8,439,010 / 8,439,336 |
| bashreadline | 200/200 | 200/200 | 200/200 |
| usdt_minimal | 8/8 | 7/8 | 7/8 |

## Still open

- memleak under bpftime runs at ~20k allocations/s (kernel 326k): gum's accurate backtracer
  re-reads ELF unwind data on every call (profile: `_Uelf64_find_section`, mmap/munmap, TLB
  flushes). Its stacks also stop after the first frame (printed twice) because libc's saved
  return addresses are PAC-signed. A kernel-style frame-pointer walk (pc, then frame records with
  PAC stripped, bounded to the thread's stack) would fix both.
- The extra ~250 outstanding allocations under bpftime are the runtime's own allocations seen by
  the malloc probe.
- funclatency (libbpf-tools) needs `bpf_current_task_under_cgroup` and `BPF_MAP_TYPE_CGROUP_ARRAY`.
- `stack_trace_map_impl::fill_stack_trace` compares two stacks of equal length by hash only (the
  content check is inverted), and returns the existing id instead of `-EEXIST` on a collision.
- Children of a started syscall server still destroy shared objects when they close inherited
  fds.
