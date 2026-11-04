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
