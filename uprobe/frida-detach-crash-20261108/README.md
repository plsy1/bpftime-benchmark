# Frida detach tests crash intermittently (2026-11-08/09)

Question: why does `bpftime_frida_uprobe_attach_tests` fail in about a fifth of
full-binary runs ("Detaching probes while threads call the function keeps them
running", child status 139; sometimes "No probe callback runs after its detach
returned")? Branch `dev/uprobe-official-177a1dc465ff` at 9898ed19 (the Frida
impl source is unchanged since 8a0afd36), Jetson Orin, clocks fixed.

## Answer

Not a detach. Gum 16.1.2 writes a function's redirect in place, without
stopping other threads (Linux, RWX pages), and its arm64 writer stores
`ldr x16, [pc+0]` first, fixing the offset to `[pc+8]` only after `br x16`
and the literal are written. A thread entering the function in between loads
the two instruction words `0x58000010 0xd61f0200` into x16 and branches
there: SIGSEGV at `0x1f020058000010` (the top byte is dropped).

The three detach tests started four caller threads and then made the first
attach, so every fork child performed a first hook under load. With the slot
design that first hook is the only time Gum writes a redirect (parking stores
the literal whole), so the crash never comes from a detach.

Evidence (`parsed-diag.tsv`, `logs/diag`): a build whose fork children print
pc and registers on a fatal signal (`diag.patch`), 50 full-binary runs: 15
failed (override test 6, detach test 5, callbacks test 4). All 15 crashes:
SIGSEGV, `x16 = 0xd61f0200_58000010` (or `..._58000050`: the instruction
fetched before the offset fix, its load after it), pc = x16. Every crash whose
cycle was recorded was in cycle 0 (step 2 = main thread already in the
uretprobe attach, step 12 = in the 200 us sleep after the first attach).
An earlier session's gdb log (2026-11-05, `evidence/`) shows the same pc.

Gum source read: `gum_interceptor_transaction_end` (RWX: mprotect RWX, write,
no suspension; `gum_thread_suspend` on Linux is `tgkill(SIGSTOP)`, which
stops the whole process), `_gum_interceptor_backend_activate_trampoline`
(16-byte form: `put_ldr_reg_address` + `put_br_reg`, then `flush`),
`gum_arm64_writer_commit_literals` (patches the ldr's imm19 after writing
the literal). frida-gum main (copy of 2026-11-04) writes the same way.

## Fix (bpftime test code)

The detach tests now hook their function once before starting the callers
(`hook_before_callers`); the slot keeps the redirect for the life of the
process, so what they race is the detaches and the re-attaches. Children
reset fatal signals to default (Catch's inherited handler printed a child's
crash as a fatal error of the parent's test case: the "SIGSEGV at :297
in-process" report). New hidden test "First hook of a function while threads
call it" (`[attach-window]`) measures the window and fails while it exists.

| binary | runs | failed |
|---|---|---|
| diag (HEAD test + crash handler) | 50 | 15 |
| head (9898ed19 as is) | 50 | 6 |
| fix (tests hook first) | 100 | 0 |
| final (fix + hidden test at 200 children) | 50 | 0 |

Hidden test (per first hook under four callers): 0/100, 4/100, 5/100,
5/200, 3/200 = 17 of 700, all SIGSEGV, with Gum set up in the parent first.
With Gum set up in each child itself (no hook in the parent): 0 of 300.

Mutations on the fix (`mutations.sh`, 5 runs of the three detach cases each):

| mutation | Detaching probes | No probe callback | Detaching an override |
|---|---|---|---|
| gum-detach (Gum detaches, slot dropped) | 5/5 SIGBUS | 5/5 SIGBUS | 5/5 status 5 (replace ALREADY_ATTACHED) |
| no-grace (`synchronize` returns at once) | 5/5 SIGSEGV | 1/5 | pass |
| live-lookup (override from live attaches) | pass | pass | 5/5 SIGABRT |

no-grace on "No probe callback" alone, 10 runs: 4/10 new order, 3/10 old
order (`cb-*`): not weakened by the reordering.

## Exposure and options

Any first hook of a function other threads are running: `bpftime attach
<pid>` (injected agent), or an agent loaded after the application's threads
started. A preloaded agent hooks before them. Ways around it (not done):
write the redirect in an order whose every state is valid (4-byte `b` to a
nearby detour first, rest of the redirect, `ldr` last) with a signal-based
check that no thread is inside the first four instructions, or stop all
threads with a signal around Gum's write and move pcs inside the prologue to
Gum's on-invoke copy.

## Files

`build.sh` (own build dir of the `frida-detach-crash` worktree), `loop.sh`
(runs, bounded, yields to other sessions, stops 03:25-04:15), `parse.sh`,
`mutations.sh`, `diag.patch`, `summary.tsv`, `parsed-*.tsv`, `logs/`.
Binaries (`bin/`, 393 MB) are not committed.
