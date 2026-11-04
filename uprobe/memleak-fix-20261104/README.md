# memleak deadlock fix, dev ff5515d8 (2026-11-04)

Plan item 1 of `uprobe/realtools-20261104/README.md`: memleak hung in every
run on a threaded workload (12/12) since 4d9b8a61. dev ff5515d8 (on ea32ad8b,
local, not pushed) fixes it. Frozen build:
`uprobe/ssl-nginx-2x2-20261023/builds/dev-ff5515d8`.

## Cause

A thread's Gum interceptor context (~100 KB: 32 invocation stack entries of
2,888 bytes) is made on first use from Gum's dlmalloc heap, which grows with
`mmap` while holding its spin lock. Inside an invocation
(`_gum_function_context_begin_invocation`) Gum sets its guard before making the
context, so a probed `mmap` there is bypassed. `gum_interceptor_ignore_current_thread`,
`gum_interceptor_attach`, `detach` and `end_transaction` make the context
without the guard (Gum 16.1.2 source, `guminterceptor.c`). With `mmap` probed,
the probe makes a context again and spins on the lock its own thread holds.
4d9b8a61 made the agent's background threads (detach worker, IPC, auto-refresh)
start with `ignore_current_thread`, while probes are attached.

## Fix

`ensure_gum_thread_context()` (frida attach impl) calls a function of our own
that Gum hooks with a no-op listener (attached before the first probe, never
detached): the context is made inside an invocation, with the guard set.
`ignore_probes_on_this_thread()` (moved from the agent) and the attach entry's
attach, replace and detach call it first. Gum is not patched; the probe hot
path is unchanged.

## Test

"Fresh threads can ignore probes while mmap is probed"
(`attach/frida_uprobe_attach_impl/test/test_thread_context_with_probed_mmap.cpp`):
a uprobe on `mmap`, 32 threads alive at once, each ignoring, mapping a page
(no hit), unignoring, mapping (a hit); in a child bounded at 20 s.

- Without the anchor call (`ensure_gum_thread_context` empty, which is the
  code before the fix): hangs 3/3. gdb of the child: `interceptor_thread_context_new`
  -> `g_array_sized_new` -> `sys_alloc` -> probed `mmap` ->
  `begin_invocation` -> `interceptor_thread_context_new` -> `spin_acquire_lock`;
  the other threads wait on the same lock.
- With it: 5/5 pass, 0.25 s.

Gate (`tracker.py test-candidate`, `gate.log`): runtime 168/169 (1 skipped),
Frida 19/19 (one new), text segment transformer 49/50 (1 fails as expected),
the rest all pass. Unchanged from ea32ad8b apart from the new case.

## Real tools (`run.sh dev-ff5515d8`, `audit/`, `measure-mt/`)

Audit, single thread: memleak reports 1,026 outstanding blocks (bpf 1,027;
the agent's own allocations stay out, as with 4d9b8a61); funclatency counts
exact (6,148,000).

memleak on the threaded workloads (3 rounds x 10 s, ops/s summed over
threads, median); before the fix every run hung:

| threads | bpf | dev-ff5515d8 | ratio |
|--:|--:|--:|--:|
| 1 | 329k | 430k | 1.31 |
| 2 | 483k | 601k | 1.24 |
| 4 | 574k | 446k | 0.78 |
| 6 | 688k | 551k | 0.80 |

No errors, no hangs (12/12). At 4 and 6 threads bpftime is below bpf: the
multi-thread costs of the realtools report (Frida's usage counter, shared map
keys). f9d2d9fc (before 4d9b8a61) had 626k at 2 and 532-562k at 6 threads.

funclatency-libbpf (cost check):

| threads | bpf | dev-7ad5a152 | dev-ff5515d8 | ff55/7ad5 |
|--:|--:|--:|--:|--:|
| 1 | 477k | 1.542M | 1.504M | 0.975 |
| 2 | 662k | 2.545M | 2.448M | 0.96 |
| 4 | 786k | 1.872M | 1.847M | 0.99 |
| 6 | 891k | 1.156M | 1.209M | 1.046 |

Layout, not code: the listeners and Gum's begin/end invocation are the same
instructions in both agents (only GOT and page offsets differ), but Gum's code
moved from 0x57c000 to 0x1e1a000 in the library (the attach impl now pulls
Gum's objects in another order). The single-thread audit run gave 1.537M.

## Found while checking detach (not fixed; for the plan)

`detach_mt.py` (memleak -c through the harness, SIGUSR1 to the workload after
4 s), `detach-manual/` (tool and workload started apart), `detach-strace/`,
`gdb/`:

1. **The agent threads were dead on every memleak run before the fix, single
   thread too.** dev-7ad5a152, single-thread workload under memleak: the detach
   worker and the IPC thread spin on Gum's heap lock from the start (100% CPU
   each, gdb in `gdb/7ad5a152.single.gdb.txt`); the main thread does not
   allocate from Gum's heap again, so the workload ran. SIGUSR1 never detached
   (no "Detaching.." line; 3/3 the workload finished at the attached rate).
   So every earlier single-thread memleak result of 4d9b8a61 and later ran
   with two cores spinning and no detach or IPC.
2. **f9d2d9fc (before 4d9b8a61) hangs at detach** with memleak on 6 threads,
   3/3 (40 s timeout): the detach worker's first `gum_interceptor_detach` makes
   its context unguarded. ff5515d8 covers detach too.
3. **Detach with uretprobes in flight crashes the workload, on every build.**
   funclatency, SIGUSR1: SIGBUS (rc 135) on dev-ff5515d8 at 1, 2 and 6 threads,
   on dev-7ad5a152 and f9d2d9fc at 6; official: SIGSEGV (rc 139). memleak with
   ff5515d8, now that detach runs: the harness path dies at detach 3/3 on 1
   and 6 threads (kprobes: `force_sig_fault` SIGBUS, BUS_ADRALN, address
   0x917a90363f7, on the worker thread); started apart (4/4, 1 and 6 threads)
   and under strace (3/3) it survives: timing. Earlier builds hid this for memleak only because
   detach never ran.
