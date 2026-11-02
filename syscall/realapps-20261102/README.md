# System call rewriting on real runtimes: Go, Node, Java (2026-11-02)

Plan and survey: `PLAN.md`. Dev on a4ac0d31 (`build-jetson-daily`), Go 1.22.2, Node 18.19.1,
OpenJDK 17.0.20.1, BTF kernel, cpu5 1728000. Step 1 of the plan (the hook on its own stack) is
done; steps 2-3 (JIT stack size, lazy-mode masks) are not started.

## Survey

- Go (`go/`): every dynamically linked Go program crashed with the real agent loaded, with or
  without programs (6/6 and 6/6); fine with a dummy agent or with goroutine stacks grown first
  (`go/grow`). The hook wrote 880 / 1,376 / 18,144 bytes below the caller's sp (`stackuse/`:
  dummy agent / agent / agent + syscount), goroutine stacks guarantee about 900.
- Node and Java (`jit/`): pass in default and lazy mode, calls traced (syscount output in
  `jit/sc-*.out`). Neither JIT emits raw `svc`: lazy mode's rewriting of real JIT code was not
  exercised.
- Lazy mode's temporary masks: code reading only (`PLAN.md` finding 3).

## The fix (dev commit after a4ac0d31)

`syscall_hooker_asm` switches to a per-thread hook stack (128 KiB above a 64 KiB guard, mapped
at the thread's first traced call) before building its frame; see CLAUDE.md "aarch64 System
Call Rewriting". After it, `stackuse` with syscount attached: 32 bytes below sp (first call of
the thread 18,272: it still runs on the caller's stack).

Tests (transformer, each with a mutation that fails it):

| Test | Mutation | Result |
|---|---|---|
| a hooked call uses only a few bytes of the caller's stack | never switch (`cbz` -> `b`) | fails (exit 4) |
| a handler on an alternate stack leaves the hook's live frame alone | ignore `busy` | fails |
| a handler on the hook stack builds its frame below the live one | always switch (no stay checks) | fails (both handler tests) |

All 50 transformer cases pass (the one expected failure as before), including the unwind tests
(cancel through a hooked call, through the agent namespace, backtraces from a handler).

## Cost (`hook-perf/`, run_layers, 10 rounds alternating, ns per write)

| Leg | a4ac0d31 | hook stack |
|---|---:|---:|
| rewrite (dummy agent) | 397.2 | 406.2 |
| agent, no program for write | 426.0 | 436.3 |
| agent, empty program on sys_exit_write | 519.9 | 539.6 |

perf stat (writeloop, 1M writes): +39 user instructions and +14-15 user cycles per call, kernel
and L1D misses unchanged: the switch's instructions (TLS lookup, bounds and busy tests, the
saved sp, the busy clear, two sp moves) and `ensure_hook_stack`.

## End to end (`e2e/`)

See `e2e/summary.md`.
