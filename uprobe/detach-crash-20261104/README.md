# Problem 7: detach crashes a thread entering a probed function (2026-11-04)

Problem 7 of `uprobe/realtools-20261104` was recorded as "SIGUSR1 detach with uretprobes in
flight crashes the workload". The cause is not the uretprobe: it is a thread **entering** a
probed function while Gum restores that function's first instructions.

## Evidence

- `run_gdb.sh dev-efe08cbd 1` (funclatency on `callloop:target_func`, one worker thread, the
  workload under gdb, SIGUSR1 after 6 s; `dev-efe08cbd.t1.gdb.txt`): the worker gets SIGBUS
  at pc `0xd2800000d2800001`, with x30 = its own return address in `worker` (it had just
  called `target_func`); the detach worker is in `~bpftime_prog`, i.e. the detach ran.
- While attached, `target_func` starts with Gum's redirect (gdb on the running process):
  `ldr x16, target_func+8; br x16; .quad 0x0000ffff855ff100` (16 bytes; the trampoline is
  out of `b` range, so a literal load).
- The original bytes at `target_func+8` are `d2800001 d2800000` (`mov x1, #0; mov x0, #0`):
  read as the literal, they are exactly `0xd2800000d2800001`. The thread executed the old
  `ldr x16` while the literal had already been restored, then `br x16` to the instruction
  words.
- memleak's earlier SIGBUS (`uprobe/memleak-fix-20261104/detach-strace/exit-trace.txt`,
  BUS_ADRALN at `0x917a90363f7`): malloc's bytes at +8 are `a90363f7 90000917`, read as the
  literal `0x90000917a90363f7`; the kernel reports it with the top byte cleared (TBI). Same
  mechanism, in malloc.

## Why

Gum detaches by copying the saved prologue back over the 16-byte redirect while other
threads keep running. On arm64 only a few single instructions (B, BL, BRK, NOP, ...) may be
changed while another core may execute them; a four-instruction sequence cannot, and a core
that already fetched the old `ldr` loads the new data. Kernel uprobes swap one BRK. Every
build crashes this way (official too, SIGSEGV when the restored bytes form an aligned
address). Attaching to a running multi-threaded process has the same window (not observed:
bpftime attaches at start, before the workload's threads run the function).

## Fix attempt 1: keep the redirect (not committed)

A no-op listener attached before a function's last bpftime hook goes and never detached
(`keep_gum_redirect`), so Gum never restores the prologue. Test "Detaching probes while
threads call the function keeps them running" (four threads call a hooked function, the
main thread attaches a uprobe and a uretprobe and detaches them every 200 us for 3 s, in a
child): before, SIGBUS 3/3; with the keeper, still a crash in 5/5 runs, now mostly SIGSEGV.

`test-with-keeper.gdb.txt`: the thread faults in `_gum_function_context_begin_invocation`
(guminterceptor.c:1576), dereferencing a listener entry. Gum 16.1.2's
`gum_function_context_remove_listener` frees the entry (`listener_entry_free (*slot)`) and
then clears the slot in the live array, while other threads iterate that array without a
lock: detaching a listener from a function other threads are running is a use-after-free in
Gum itself (its deferred destroy covers the listener object, not the entry).

A third hazard by reading bpftime (not yet seen in a crash): `detach_by_id` erases the
attach and its callback, and changes `user_attaches`, while other threads may be iterating
it in `uprobe_listener_on_enter`; the agent's detach then destroys the bpf programs those
callbacks run.

So no detach is safe while other threads run the function: avoiding it means never asking
Gum to detach or revert at run time (listeners stay attached and become inert), and freeing
bpftime's own callback data only after the threads that may use it have left.

## Upstream Frida and the cost of never detaching

- frida-gum main (`gum/guminterceptor.c`): `gum_function_context_remove_listener` now defers
  `listener_entry_free` until no thread is inside (7822612ffd, 2026-10-14, "Defer freeing
  detached listener entries"); first release with it: 17.24.0 (17.23.3 does not have it).
  bpftime pins 16.1.2. The arm64 prologue restore is unchanged upstream
  (`_gum_interceptor_backend_deactivate_trampoline`: `gum_memcpy` of the 16 bytes), so
  hazard (a) remains in current Frida.
- Cost of keeping Gum's path after a detach (hidden case "Kept redirect cost", the test's
  function, 2 s per point, cpu5 1728000): native 236M calls/s on 1 thread, 1,406M on 6; with
  only the no-op listener left 6.25M (about 160 ns a call) and 1.04M on 6 threads (Gum's usage
  counter); with an empty uprobe as well 5.26M and 1.11M. A process traced once would keep
  that cost on every probed function until it exits.

## Fix: dev 25f4134f (on efe08cbd, local)

Gum is never asked to detach or revert while the process runs. Each hooked function has a
`frida_hook_slot` for the life of the process: Gum's enter listener (and a leave listener once a
uretprobe came, the replacement once an override came) stays attached; bpftime swaps the slot's
dispatch and waits a grace period before freeing what it removed (per-thread counters odd inside
a callback; `membarrier(MEMBARRIER_CMD_PRIVATE_EXPEDITED)`, then wait for odd counters to move),
so `detach_by_id` returns only when no callback can still run the attach. With nothing attached
the slot is parked: the redirect's literal (one aligned 8-byte store, or the low word when
unaligned) or its single `b` points at Gum's on-invoke code (the displaced first instructions,
then back into the function), so calls bypass Gum. The on-invoke address is the
`original_function` of a replacement installed and reverted inside the transaction that first
hooks the function (Gum writes the redirect when that transaction ends, so it is unreachable).
Frozen build: `uprobe/ssl-nginx-2x2-20261023/builds/dev-25f4134f`.

Tests (Frida suite 22/22, two more loops of 45 and 60 runs all passing; one failing run of 108
right after restoring a mutation, cause not found, see below):

| test | before | after | mutation |
|---|---|---|---|
| Detaching probes while threads call the function keeps them running | SIGBUS 3/3 (keeper only: SIGSEGV 5/5) | 5/5 | - |
| No probe callback runs after its detach returned | - | 5/5 | `synchronize` returns at once: SIGSEGV 3/3 (and the test above 2/2) |
| Probes attached again after a detach run | - | 5/5 | no unpark: fails 2/2 |

The one failing run: the first full-suite run after restoring the unpark mutation reported 21/22
with an extra one-test summary line; the forked children then let an exception reach Catch. The
children now catch and report exceptions; 105 full runs after it passed. Gate (`gate.log`):
runtime 170 passed, 1 skipped; Frida 22/22; text segment transformer 49/50 (1 fails as expected);
the rest pass.

Hidden "Hook cost attached and parked" (the test function, 2 s per point, cpu5 1728000):

| | 1 thread | 6 threads |
|---|--:|--:|
| native | 231M calls/s | 1,380M |
| attached (empty uprobe) | 5.2M | 1.1M |
| detached, parked | 157M | 1,317M |
| Gum path kept (keeper listener, before parking) | 6.25M | 1.04M |

## Real tools after the fix (`after/run.sh dev-25f4134f`)

- SIGUSR1 detach, tool and workload apart (`after/detach-manual/`): funclatency 1/2/6 threads
  and memleak 1/6 threads, 3 rounds each: the workload exits 0 in 15/15 (before: SIGBUS or
  SIGSEGV on every build at 1, 2 and 6 threads). Its per-second median after the detach:
  memleak 42.1M ops/s on 1 thread and 249-250M on 6 (native 42.1M and 249.9M: malloc parked);
  funclatency's callloop 75-89M on 1 thread and 517-519M on 6 (native 91.7M and 553M; its target
  is about 11 ns, the parked path adds about 2 ns).
- memleak `-c` through the harness, SIGUSR1 after 4 s (`after/detach-harness.jsonl`): 6/6 finish
  (tool rc 0, the workload's ops reported; before: the workload died 3/3 at 1 and 6 threads).
- Attached cost (`after/measure-mt/`, 3 rounds x 10 s, ops/s summed over threads, median, no
  errors):

| tool | thr | bpf | dev-efe08cbd | dev-25f4134f | 25f4/efe0 | 25f4/bpf |
|---|--:|--:|--:|--:|--:|--:|
| bpftrace-uretprobe-hist | 1 | 465k | 1.37M | 1.35M | 0.984 | 2.90 |
| | 2 | 650k | 1.79M | 2.28M | 1.271 | 3.50 |
| | 4 | 775k | 2.50M | 2.07M | 0.827 | 2.66 |
| | 6 | 910k | 1.29M | 1.24M | 0.962 | 1.36 |
| funclatency-libbpf | 1 | 479k | 1.52M | 1.49M | 0.980 | 3.11 |
| | 2 | 659k | 2.44M | 2.41M | 0.988 | 3.66 |
| | 4 | 791k | 1.61M | 1.88M | 1.170 | 2.38 |
| | 6 | 891k | 1.21M | 1.22M | 1.011 | 1.37 |
| memleak | 1 | 328k | 655k | 643k | 0.982 | 1.96 |
| | 2 | 475k | 1.02M | 1.04M | 1.017 | 2.19 |
| | 4 | 573k | 708k | 706k | 0.997 | 1.23 |
| | 6 | 684k | 516k | 539k | 1.044 | 0.79 |

  One thread: 1.6-2.0% slower (the grace period's TLS access and two stores per callback). Two to
  six threads: within the rounds' spread (2 and 4 threads cross the clusters; rounds differ by up
  to 25%).

Left as before: the attach-time window (Gum writes the redirect over the first instructions while
other threads may run them; bpftime attaches at start); Gum's 8-byte form, deflectors and x86
are not parked (Gum's path after a detach); a function once hooked keeps Gum's context, a past
uretprobe keeps trapping returns while unparked, a past override keeps the replacement (it calls
the original).
