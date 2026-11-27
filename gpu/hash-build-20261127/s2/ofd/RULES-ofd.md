# OFD lock check (written 2026-11-27, before any run)

Why: the independent read of the S2 fixes (`../review/reread-fixes.json`, 19 findings, all upheld)
shows that liveness by (pid, start time from /proc) leaves windows a third time, and cannot see
exec at all. The question here: does a lock the kernel releases (an open file description lock,
`F_OFD_SETLK`, on a byte of the shared segment's file in /dev/shm, the fd opened `O_CLOEXEC`) give
the properties D8 (writer slots) and D11 (owner) need? CPU only, outside the worktree, no GPU.

## What runs

`ofd_check.c` (built with gcc, run as the user; item 7 under sudo). A scratch file in /dev/shm
(tmpfs, as the segment) of 4 KiB; locks on bytes at offset 2^40 + i (beyond EOF). Each item is a
pass or fail line.

1. A holder's write lock: another process's `F_OFD_SETLK` (write, non-blocking) fails with
   EAGAIN, and `F_OFD_GETLK` reports it (type not `F_UNLCK`).
2. Another open file description of the holder's own process conflicts with the holder's lock (a
   sealer probing with its own fd sees its own process's slot as held).
3. SIGKILL of a holder whose four threads loop on an atomic counter: the lock is free afterwards
   (taken by the checker within 1 s of waitpid).
4. execve of a holder (four threads looping, the main thread execs `/bin/sleep 3`): the lock is free
   while the new image runs (taken by the checker while `sleep` is alive).
5. fork: the child's copy of the fd shares the parent's description. (a) The child closes its copy:
   the lock stays held by the parent while the child lives; after the parent exits (child still
   alive) the lock is free. (b) The child keeps its copy: after the parent exits the lock is still
   held (this is what a fork handler must prevent).
6. vfork child that execs `/bin/true`: the parent's lock is still held afterwards.
7. pid namespace (sudo): a holder in a child of `unshare(CLONE_NEWPID)` (in its own namespace) is
   seen as held by a process outside; after it is killed the lock is free.
8. `close_range(3, ~0U, 0)` in the holder: the lock is released (the known risk: a host closing
   fds it does not own).
9. Cost: uncontended `F_OFD_SETLK` + unlock, `F_OFD_GETLK` on a free and on a held byte, ns per call
   (median of 5 runs of 100,000).

## Decision rule

- Items 1-7 pass: D8 and D11's liveness move to OFD locks (claim = taking the lock; liveness = a
  non-blocking try), and the findings of the pid + start-time class are answered by that design.
  Item 8 passing (released) is expected and is written as a limit (README 10/11), or guarded.
- Any of items 1-7 fails: no move; the findings are fixed as patches on the current design
  (mandatory before S5: slots-1 with a discount that CASes the count it read, owner-4, owner-5,
  slots-8).
- Item 9 is recorded; it is paid once per process (claim) and per slot per seal, never per update.
