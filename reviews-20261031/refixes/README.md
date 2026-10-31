# Fixes for the second review's findings (2026-10-31)

Findings: `../rereview/README.md` (dev f2d6ae7d). Dev branch
`dev/uprobe-official-177a1dc465ff`, f2d6ae7d..9aeb2863. Every fix has a test
that fails with the fix removed (the mutation is in each commit message).

## Commits

| Commit | Finding | Fix |
|---|---|---|
| a1633b51 | 13: a fork during fork-handler registration leaves the child spinning | every registration in a static initializer (array binding, map usage stats moved), wait bounded |
| 040ab4ed | 10: possible CPUs counted as highest + 1 | `bpftime_num_possible_cpus()` = CPUs in the mask (libbpf), `bpftime_cpu_index_limit()` for indexing; slot i = CPU ids[i] |
| a6fdd740 | 11: `BPF_MAP_LOOKUP_AND_DELETE_ELEM` ignored the key | queues/stacks pop; hash, per-CPU hash, LRU hash remove the key in one step; others ENOTSUP |
| bf1af650, 7553abb8 | 12: size overflow, degenerate sizes | kernel `map_alloc_check` rules; overflow and kernel limits E2BIG, larger than the segment ENOMEM; `set_handler` reports `map_init` failure |
| faba3c39 | minor: per-CPU hash bpf() flags | low 32 bits, as the program path; alignment test value sizes 1-8, 12, 20 |
| e3e7d721 | 5, 8, minor drain order | shard search compares the pid namespace; a shard that cannot be allocated is ENOSPC, not an exception in the host; drains rotate their start |
| 5920d5f9 | 7: dead producer wedges the ringbuf | reservation owner table (pid, tid, namespace); the tool's epoll wait discards a dead owner's record |
| dd3ee34d, 9aeb2863 | 6: vfork child shares the identity cache | the aarch64 hook bumps `bpftime_identity_epoch` before a CLONE_VM clone without CLONE_SETTLS; cached identities older than the epoch are re-read, cached only for the process's own pid |
| 28528721 | 1: lazy dlopen vs fork deadlock | one set of fork handlers, rescan order |
| de09b3dc | 2: application's seccomp TRAP filter | our traps carry SECCOMP_RET_DATA 0xb7e5; other SIGSYS go to the application's action |
| 632969a8 | 3: lazy mode patched MAP_SHARED code | shared mappings made untraced (last mapping cached) |
| dae7d025 | 4: logging from a possible signal handler | refused execve reported by the next rescan |
| 41eb8e9d | minor: page-crossing gates, SIGSYS left installed, inode-only file check | two-page mprotect, previous action restored, device and inode |
| 592b0edd | documented only | literal pools in .text, noexcept `run_callbacks` and forced unwind, EPAN, hash homes and token parking |

Not fixed: the agent namespace libc's own fork handlers. glibc 2.39 exports
neither `__run_prefork_handlers` nor the malloc fork locks (only
`__libc_fork`), so the review's dlvsym idea does not apply.

One regression of my own was caught by the gate: bf1af650 returned E2BIG for
a map larger than the segment, while the syscall server's contract (test
"Syscall server reports map allocation failure instead of using the kernel")
is ENOMEM. Fixed by 7553abb8.

## Verification

- Candidate gate on 7553abb8 and on the final HEAD 9aeb2863: runtime
  focused 133 passed + 1 skipped, transformer 42 cases (41 passed, 1
  expected failure), syscall trace 7.
- Runtime cases each in its own process as root, on 9aeb2863: 219 cases, all
  passed except the 3 map-usage-counter cases (not built). On 41eb8e9d the
  ENOMEM case failed too (fixed by 7553abb8).
- The review's reproductions, rerun on the final transformer:
  - `deadlock` (lazy, dlopen loop + fork loop): 20 s, 11,769 forks, 19,612
    dlopens (stalled within 3 s before);
  - `appfilter` (own RET_TRAP filter, no handler): native 159, default mode
    159, lazy 159 (139, stack overflow, before).
- `vfork-e2e/`: statsnoop with a parent and 4 vfork children stat-ing paths
  naming their pid, under the syscall-trace agent. Old agent (f2d6ae7d's
  tree): all 800 child events carried the parent's pid. New: 1,601 events,
  every pid right.
- `fuse-final.*`: getdents64 cache 11.44x, denied open 14.30x faster than
  native (11.35x / 14.68x before these fixes); listings equal.

## Cost (`output-ab/`, cpu5 at 1728000, ns per call, medians of 5)

A uprobe on a counting loop (`countloop` of `uprobe/map-table-20261028`),
program `out.bpf.c`: perf output, ringbuf output, or control (only
`bpf_get_current_pid_tgid`). Every record was received, none dropped.
Baseline `cpucheck-unsigned` is f2d6ae7d's tree.

- `summary.txt`, `summary2.txt`: dd3ee34d..7553abb8 cost 8 ns on control
  (+3.8%), up to +9% on perf output at 1 thread, +7.4% on 6-thread ringbuf.
- `summary3.txt` (control and perf, 1 thread): with 9aeb2863 (`opt-identity2`:
  inline fast path on one `__thread` struct) control 209.0 -> 213.5 (+2.2%),
  perf output 488.6 -> 498.1 (+1.9%, the leg is bimodal: 493-525).
  `summary2.txt`: owner slot from the tid brings 6-thread ringbuf output to
  +0.2%.
- Round 3 was run directly: `run_out.py --backends
  cpucheck-unsigned,dev-7553abb8,opt-identity,opt-identity2 --objs
  out-control.bpf.o,out-perf.bpf.o --threads 1 --nkeys 1 --reps 5 --secs 6`.
- The hook's write path is unchanged by inspection: only the clone and clone3
  branches of the aarch64 hook gained a call (`note_clone_syscall`); not
  measured.
