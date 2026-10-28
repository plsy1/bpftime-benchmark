# Where bpftime's per-syscall cost goes (ARM64, 2026-10-29)

The official `benchmark/syscall` gives kernel +85 ns and bpftime +287 ns per write
(`../official-syscall-20261029`). This splits bpftime's cost into layers by adding them one at a
time. `writeloop` (-O2) issues N = 1,000,000 writes of 7 bytes to fd 42 (`/dev/null`); the time
per write is measured inside it. Legs (`run_layers.py`, sources and build in `src/`):

- `rewrite`: transformer preloaded, `AGENT_SO` = `src/libdummy_agent.so`, which leaves the
  transformer's own hook in place: svc rewritten, trampoline, full register save, the C entry,
  the system call issued from the transformer. No agent, no shared memory.
- `passthrough`: the dummy installs a hook in its `dlmopen` namespace that only calls the previous
  one (the cross-namespace call the real agent adds).
- `agent-other`: the real agent and syscall-server, the loader attaching one empty program to
  `sys_enter_getppid`, which the workload never calls: the agent's dispatch for every system call
  with no program for write.
- `agent-exit`: one empty program on `sys_exit_write` (as the official benchmark).
- `agent-count`: counting programs on `sys_enter_write` and `sys_exit_write` (checked = N).
- `kernel-*`: the same objects on kernel tracepoints.

Dev branch `7a7d29b8`, `build-jetson-daily` (GCC 13, LLVM 15, LTO). BTF kernel 6.8.12,
jetson_clocks on, cpu5 at 1728000 before and after. 5 rounds, order rotated, all 45 runs valid
(counting legs exact). `run1.jsonl`, `summary-run1.md`.

| Leg | median ns/write (min-max) | step | over none |
|---|---|---:|---:|
| none | 352.6 (352.2-353.0) | - | - |
| rewrite | 390.1 (389.8-390.6) | +37.5 | +37.5 |
| passthrough | 390.5 (389.8-390.9) | +0.4 | +37.9 |
| agent-other | 592.6 (585.2-603.8) | **+202.1** | +240.0 |
| agent-exit | 634.8 (628.9-648.3) | +42.2 | +282.2 |
| agent-count | 681.8 (679.1-686.3) | +47.0 | +329.2 |
| kernel-other | 367.4 (366.6-367.5) | +14.8 | +14.8 |
| kernel-exit | 436.7 (435.6-437.1) | +69.3 | +84.1 |
| kernel-count | 523.6 (522.1-529.1) | +86.9 | +171.0 |

- The trampoline (rewritten svc, gate, saving and restoring x0-x30, flags, FPSR and q0-q31) costs
  37.5 ns; the cross-namespace call nothing measurable.
- **The agent's dispatch costs about 200 ns per system call even when no program is attached to
  it** (`syscall_callback` -> `dispatch_syscall`: exception guards, the per-thread
  `std::function` for return override set twice, the context, callback lookups). That is about
  70% of bpftime's cost in the official benchmark, and more than the kernel's whole cost for one
  program (84 ns).
- Per program, bpftime adds 42-47 ns, about half of the kernel's 69-87 ns.
- With programs on the kernel side, enabling any syscall tracepoint costs every system call
  15 ns.

Next: profile `agent-other` (perf) to split the 200 ns.

## Profile of the dispatch (`profile.sh`, `perf/`)

`profile.sh agent-other` and `profile.sh rewrite`: 20,000,000 writes, `perf stat` over the run and
`perf record -F 2999 -g` on writeloop for 5 s (perf 6.8.12). Per write: rewrite 674 cycles /
1207 instructions, agent-other 1032 cycles / 2176 instructions. The agent adds **357 cycles
(207 ns) and about 970 instructions** at an IPC of 2.1: not stalls, just code. In agent-other
(`perf/agent-other.user.txt`), user-space samples outside the system call itself:

| Samples | Function | What it is |
|---:|---|---|
| 8.2% | `dispatch_syscall` | the dispatch body |
| 5.5% | `syscall_hooker_asm` | the trampoline's register save/restore (also in rewrite) |
| 5.3% + 2.4% | `_dl_tlsdesc_dynamic`, TLS init function of `curr_thread_override_return_callback` | thread-local access in the dlmopen-ed agent (dynamic TLS) and the lazy initialization of a non-trivial `thread_local` |
| 4.1% + 4.0% + 1.7% + ~1.5% | `malloc`, `_int_free`, `free`, `operator new`, `std::function` handlers | heap allocation and release of `std::function`s, twice per system call |
| 0.5% | `syscall_callback` | the agent's entry |

The source (`attach/syscall_trace_attach_impl/src/syscall_trace_attach_impl.cpp`,
`attach/base_attach_impl/base_attach_impl.hpp`): for every system call, before checking whether
any program is attached, `dispatch_syscall` assigns
`thread_local std::optional<std::function<void(uint64_t, uint64_t)>>
curr_thread_override_return_callback` a lambda capturing three locals by reference (24 bytes,
above `std::function`'s 16-byte inline buffer, so a heap allocation), resets it, assigns it again
for the exit side and resets it again. Only then are the (empty) callback sets checked. The
callback exists so that `bpf_override_return`-style helpers can replace the result.

Candidate fixes (not done; shared code, so x86 should gain too):
1. Return straight to the system call when no enter, exit or global callbacks exist for the
   number (every system call a tool does not trace pays the 200 ns today).
2. Without the allocation: a callback that fits the inline buffer (one pointer to a small
   struct on the stack), or a plain per-thread pointer to that struct.
3. A trivially constructible `thread_local` (no TLS init function).
