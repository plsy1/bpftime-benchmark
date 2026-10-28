# aarch64 system call rewriting: end-to-end check (2026-10-28)

First run of bpftime's userspace system call tracing on ARM64 (Jetson Orin, BTF kernel 6.8.12).
Before this, the aarch64 branch of `attach/text_segment_transformer` was a TODO (upstream since
2024-01, PR #151) and the transformer left `call_orig_syscall`, `syscall_addr` and
`syscall_hooker_asm` undefined, so `bpftime start -s` could not even load it. Design and the
svc-hook comparison: `summry/jetson` `syscall/arm64-syscall-trampoline-design-20261028.md`;
implementation: dev branch, CLAUDE.md "aarch64 System Call Rewriting".

`src/tp_count.bpf.c` counts `sys_enter_write` calls with fd 42 and 7 bytes, and
`sys_exit_write` results of 7; `src/writeloop.c` issues N = 1,000,000 such writes to
`/dev/null` dup'ed to fd 42 and times them; `src/tp_attach.c` loads and attaches by section and
prints the counters on SIGINT. Legs (`run_e2e.sh`): none, kernel (kernel tracepoints), bpftime
(syscall-server in the loader; transformer + agent preloaded into writeloop, as
`bpftime start -s`). Build: dev worktree `build-jetson-daily` (GCC 13, LLVM 15, LTO).
jetson_clocks on, cpu5 at 1728000 before and after. 3 rounds (`e2e.log`).

| Leg | enter count | exit count | ns per write (3 rounds) | over no tracing |
|---|---:|---:|---|---:|
| none | - | - | 353.4 / 352.8 / 352.5 | - |
| kernel | 1,000,000 | 1,000,000 | 523.8 / 524.1 / 529.3 | +172 |
| bpftime | 1,000,000 | 1,000,000 | 681.0 / 677.5 / 682.9 | +327 |

- Every write was seen at entry and exit, with the right arguments and result, as with the
  kernel.
- Two (empty-ish) programs per write cost bpftime about twice the kernel's overhead here. The
  official x86 benchmark shows the same direction (bpftime 446 ns vs kernel 365 ns). Not yet
  analysed: trampoline (register save of ~800 bytes), dispatch, and program execution shares.
- Writes after the agent's teardown at exit (the victim's final stdout flush) are not traced.
