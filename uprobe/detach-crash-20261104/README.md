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
