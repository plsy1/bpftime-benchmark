# Dead code by frozen read-only constants, dev 94f9ba8f (2026-11-05)

The layer behind problem 11 (`uprobe/exit-log-poison-20261105`): libbpf-tools keep their options in
`.rodata` (a `BPF_F_RDONLY_PROG` array libbpf freezes before loading), and the code of an option that
is off calls helpers bpftime does not have (opensnoop's `full_path`: 158, 115; syscount's `-P`: 35).
The kernel's verifier drops that code; bpftime disabled the whole program.

dev 94f9ba8f (on 74d0ab8b, local; first committed as b60b99c3, then amended with CLAUDE.md lines only: the
frozen build `dev-b60b99c3` is the same code):

- `BPF_MAP_FREEZE` is recorded (it was a no-op): later syscall updates and deletes are EPERM, a
  second freeze EBUSY. The flag sits in padding; `bpf_map_handler` stays 160 bytes (the first try
  added 8 bytes, and "Test basic operations of external hash map ops", 6144 handlers in a 1 MB
  segment, ran out of memory: the gate caught it).
- When a program calls a helper nobody registered, `reachable_instructions` (a forward analysis of
  scalars and pointers into frozen read-only array values) prunes branches they decide; calls of
  missing helpers in unreachable code go to a private slot that reports it ran. A reachable call
  still fails the compile. Conservative: unmodeled instructions leave registers unknown, joins of
  differing values are unknown, calls clobber r0-r5, callees and callbacks are entries, no stack
  tracking.

Tests: "Branches on read-only constants are followed one way", "Reachability keeps subprograms,
callbacks and joined paths", "A missing helper behind a read-only constant does not disable the
program" (with EPERM/EBUSY after freeze, and a not-frozen or writable map leaving the program
disabled). Mutations (each fails a test): no constants; calls keeping r1-r5; joins keeping a value.
Gate (`gate.log`, second run): runtime 176 passed, 1 skipped (three new); Frida 22/22; text segment
transformer 49/50 (1 fails as expected); smoke funclatency 6,865,000 of 6,865,000, memleak rc 0,
detach rc 0.

## The whole audit (`run.sh dev-b60b99c3`, `audit/`)

Every harness tool, single thread, on bpf, dev-74d0ab8b and dev-b60b99c3:

| tool | bpf | dev-74d0ab8b | dev-b60b99c3 |
|---|---|---|---|
| syscount (openat) | 41,418 (all processes) | no counts | **38,900 of 38,900** |
| opensnoop | 38,729 of 38,800 | 0 | **18,838 of 38,800** |
| everything else (18 tools) | | as before | same counts as dev-74d0ab8b |

The other tools all match: funclatency (both) exact, gethostlatency 352,100 of 352,100, memleak
1,026 (bpf 1,027), sslsniff 76,000 of 76,000, bashreadline (all) 200, usdt 8, bpftrace (count,
hist, ustack, usdt jvm, bashreadline, gethostlatency) exact, javagc 50/50, get_stack_id 70 events.

## Left

- **opensnoop delivers 48.5% of the opens** (18,838 of 38,800; bpf 99.8%). Not investigated. Its
  `bpf_get_stack` in the syscall tracepoint also fails ("There is no frida hook running": the stack
  walk needs Frida's context), which only zeroes its `callers` field.
- syscount `-P` (count by process) and opensnoop `-F` (full path) run the code that needs those
  kernel-only helpers: there the program is still disabled, as before (by construction; not run).
