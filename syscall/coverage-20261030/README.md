# aarch64 system call rewriting: what it misses, and the fixes (2026-10-30)

Plan: the implementation limits of our svc-hook-style rewriting (K23's pitfalls P2, P3, P5 on
ARM64), measured first, then fixed; a seccomp lazy mode as an opt-in research mode.
Dev branch `dev/uprobe-official-177a1dc465ff`, build `build-jetson-daily`, cpu5 at 1728000
before and after every measurement.

## Commits (dev, on top of f4fdcbcd)

| Commit | What |
|---|---|
| e430ea53 | Coverage tests, `[!shouldfail]` until fixed: dlopen-ed svc, vDSO fallback, literal-pool data, JIT code |
| 014935f6 | **Crash fix**: the hook's own system calls are not hooked again (re-entrancy guard) |
| 38d92964 | Only ELF code sections are scanned; anonymous mappings only with `BPFTIME_SYSCALL_REWRITE_ANON=1`; mprotect restore checked |
| 74c215a7 | Code mapped executable later is rewritten (rescan after mmap/mprotect); agent namespace loaded unhooked |
| d9c4f1b6 | The vDSO's fallback svc are rewritten (not `__kernel_rt_sigreturn`) |
| 166c6016 | Hook common path kept short (rescan out of line, no PLT) |
| 7722084c | Rescans decided from the call's arguments (an mprotect read /proc/self/maps: 190 us) |
| d807ef93 | `BPFTIME_SYSCALL_LAZY=1`: seccomp SIGSYS for svc never rewritten, rewritten at first use |

Each fix has a test that fails without it (mutations listed in the commit messages).

## 1. Real-tool check (`audit.py`, `workload.py`)

Reference: `strace -f -c` on the program without bpftime. bpftime: libbpf-tools syscount on the
syscall server, the program with the transformer. All as root (syscount needs tracefs).

- **python3 with 4 threads crashed (SIGSEGV) under syscount** (`audit-before.jsonl`): a new
  thread's first traced call allocates the agent's dynamic TLS through ld.so with the main libc's
  malloc, whose new arena reads sysfs through the rewritten libc, re-entering the hook before the
  TLS exists, until the stack overflows. Without programs the dispatch fast path never touches
  TLS, which is why nginx workers (processes) and earlier tests did not show it. Fixed by 014935f6.
- After all fixes (`audit-a2.jsonl`), python3 strace 2227 vs bpftime 2147 calls; ls 7068 vs 6999.
  What is still missing:
  - `clock_gettime` 200/200 before d9c4f1b6 (vDSO fallback for CPU-time clocks), 0 after;
  - `clone3` 4: by design (direct path, never traced);
  - about 80 calls before the agent is loaded (ld.so: openat, read, mmap, mprotect, execve,
    set_tid_address, rseq ...): document-only;
  - about 30 futex/mmap/munmap: a new thread's malloc arena setup, now done by the agent's TLS
    allocation inside the hook (untraced, bpftime's own work).

- nginx (`nginx-audit.jsonl`, `../syscount-nginx-20261029/audit.py` in the
  `bpftime-syscount-nginx:20261029` container, removed after): runs under the final transformer
  with syscount attached (114,533 requests in 5 s, no crash). That script only checks that
  something is traced: the bpftime leg's 10 s syscount window ends before wrk does (about 0.6 of
  the per-request calls), and the kernel leg's `-p` counts a subset (no epoll, no read). Not a
  per-call comparison; the strace audits above are.
- Daily candidate gate on the dev worktree at d807ef93: all 7 suites green (runtime focused
  120 cases, kernel_user, daemon, attach replace, frida, transformer 20 + 1 expected
  `[!shouldfail]`, syscall trace); no root-owned shm left.

## 2. Spikes (`sigsys.c`, `vdso.c`)

- SIGSYS (`SECCOMP_RET_TRAP`) on this kernel: `si_call_addr` and the saved pc are the **svc + 4**;
  writing `regs[0]` in the handler sets the result. Round trip 1783 ns vs getpid 292 ns.
- A seccomp filter comparing the instruction pointer: getpid 299.2 -> 406.4 ns (+107 ns).
- The vDSO text can be mprotect-ed writable and patched (copy-on-write); calls keep working, in
  fork children too.

## 3. Cost of the default path (`perf/`, writeloop, 5 alternating rounds, medians)

| Leg (ns per write; untraced 352.3) | 1aa74c4c (before) | 166c6016 | Change |
|---|---:|---:|---:|
| Rewritten, dummy agent | 389.9 | 394.9 | +5.0 |
| Agent, no program for write | 417.8 | 424.8 | **+7.0** |
| User instructions per write (20M writes) | 227 | 258 | +31 |

- The +31 instructions are the re-entrancy guard (a thread-local flag set and cleared around the
  hook and around the real call). The first version cost +43 instructions / +10 ns: the rescan was
  inlined into `syscall_hooker_cxx` (ten registers saved, stack canary, PLT call).
- So the dispatch overhead without programs is about 35 ns, was 28 ns.
- With programs (agent-exit) the leg is bimodal across runs (old 491-531 ns); instruction counts
  show the same +31.

## 4. Lazy mode (`perf/lazy-*.jsonl`, `perf/jitbench*`)

| | Off | On | Change |
|---|---:|---:|---:|
| Rewritten, dummy agent | 397.7 | 459.7 | +62.0 |
| Agent, no program | 426.2 | 486.5 | +60.3 |
| Agent, one empty exit program | 525.2 | 588.2 | +63.0 |

jitbench (1000 distinct JIT sites, `mov x8,#getppid; svc; ret`, dummy agent; medians of 5):

| Mode | mprotect to RX | First call | Later calls |
|---|---:|---:|---:|
| No transformer | 7.2 us | 310.6 ns | 308.0 ns |
| Transformer, default (JIT not covered) | 6.8 us | 308.6 ns | 305.7 ns |
| `BPFTIME_SYSCALL_REWRITE_ANON=1` (word scan at mprotect) | 706.4 us | 383.0 ns | 379.3 ns |
| `BPFTIME_SYSCALL_LAZY=1` | 8.3 us | **13.2 us** | 493.8 ns |

- First call = SIGSYS trap + gate allocation + two writes through /proc/self/mem. With
  /proc/self/maps and mprotect instead it was 90 us.
- Before 7722084c the default transformer's mprotect cost 189 us (a /proc/self/maps read).
- **exec**: the filter survives execve and nothing handles SIGSYS there, so an exec-ed program
  dies at its first system call. Reproduced by accident: `LD_PRELOAD=... taskset -c 5 jitbench`
  preloaded taskset, which exec-ed jitbench (rc 159). Decided (user, 10-30): refuse. The follow-up
  dev commit makes `execve`/`execveat` return EPERM with a warning naming the path while lazy
  mode is active (test: execve, fexecve, posix_spawn; mutation without the refusal: the test
  child dies of SIGSYS, rc 159). The hook's common path gains two compares (execve 221 shares
  mmap's range check; execveat 281 one `ccmp`). Making exec work (fixed-address allowed page
  plus a loader, or a ptrace helper as K23 uses on x86) is not done.
- python3 and ls under lazy mode (`audit-lazy*.jsonl`): same coverage as the default mode
  (python3 2144-2149 of 2225-2231): neither has JIT code, and what is left (calls before the
  agent, clone3) the filter cannot see either.

## Comparison with the other implementations (svc-hook paper, Cortex-A72; ours on the Orin)

| Mechanism | Per call | Source |
|---|---:|---|
| ptrace | 30,789 ns | svc-hook paper, table 2 |
| seccomp SIGSYS on every call | 3,157 ns (Orin: 1,783 ns round trip) | paper / spike |
| brk trap | 3,045 ns | paper |
| svc-hook | 28 ns (11-14 ns over LD_PRELOAD) | paper / supplement |
| Ours, rewritten, no program | +72 ns over untraced (424.8 vs 352.3) | this dir |
| Ours, lazy mode, steady state | about +60 ns more per call | this dir |

## Files

- `audit.py`, `workload.py`, `audit-*.jsonl`: per-syscall comparison (before, guard, a1, a2, lazy).
- `sigsys.c`, `vdso.c`: spikes.
- `perf/`: `run_layers.py` (copy of `../breakdown-20261029`, `BPFTIME_TRANSFORMER` and
  `LAYER_EXTRA_ENV`), `run_layers_perf.py` (perf stat), `go*.sh`, transformer builds
  (old = 1aa74c4c, new/new2/new3 = intermediate, lazy = d807ef93), `jitbench.c`, results.
