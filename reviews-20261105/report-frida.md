## Findings (most severe first)

### 1. Detaching an override while threads call the function aborts the host: the replacement handler iterates the entry's live `user_attaches`, not the snapshot
- Severity: **high** (host SIGABRT/SIGILL on the detach-under-load path this commit exists to fix; attach type uprobe_override / ureplace)
- Commit: 25f4134f
- Files: `attach/frida_uprobe_attach_impl/src/frida_internal_attach_entry.cpp:257-269` (handler takes `override_owner` from the old dispatch, then `hook_entry->run_filter_callback(regs)`), `:204-217` (`run_filter_callback` walks the live `user_attaches` and `throw std::runtime_error("Unable to find filter callback")` when no override entry is there), `attach/frida_uprobe_attach_impl/src/frida_uprobe_attach_impl.cpp:273-281` (`detach_by_id` shrinks `user_attaches` with `remove_if`/`resize` *before* `p->republish()`).
- What is wrong: the grace period protects only what the dispatch snapshot points at. `run_uprobes`/`run_uretprobes` use the snapshot's vectors, but the override path dereferences the internal entry's mutable vector. Between `resize` and `dispatch.exchange` (a `new`, `collect`, `set_parked` with two `mprotect`s and a cache flush) any call on another thread loads the old dispatch, finds the vector empty and throws out of the Gum replacement, whose return address is Gum's on_leave trampoline (no unwind info) -> `std::terminate`.
- Failure scenario: an override/ureplace program attached (`bpf_override_return` tools) and detached by SIGUSR1 or tool exit while the traced process calls the function: the traced process dies.
- Evidence: **reproduced**, `exp5_override_detach.cpp` (`timeout 60 stdbuf -o0 ./exp5_override_detach 3`: four callers, attach override / sleep 200 us / detach): "terminate called after throwing an instance of 'std::runtime_error'" then "child killed by signal 6 (Aborted)" 3/3 runs within the first 1000 cycles (`exp5.out`); a fourth run died with signal 4 (`exp5_full.out`).
- Fix direction: put the override's `frida_attach_entry *` into the dispatch snapshot and have the handler call it directly (no walk of the live vector), with per-call (thread-local) `is_overrided`/`user_ret` instead of fields on the shared internal entry; never throw from the replacement.

### 2. Parking a function's slot silences probes attached through a `b` thunk to it
- Severity: medium (silently lost events; needs two attach points Gum resolves to one function)
- Commit: 25f4134f
- Files: `attach/frida_uprobe_attach_impl/src/frida_hook_slot.cpp:44-54` (`looks_redirected`), `:140-175` (`create_slot` keys the slot by the given address), `:207-229` (`republish_locked` parks when this slot alone is empty)
- What is wrong: `gum_interceptor_attach` resolves through `gum_interceptor_resolve` -> `gum_arm64_reader_try_get_relative_jump_target` (Gum 16.1.2 `guminterceptor.c:2018-2041`, `guminterceptor-arm64.c:999-1002`): a function whose first instruction is `b target` (GCC sibling-call forwarders, PLT stubs) is instrumented at `target`. bpftime makes two slots whose listeners sit on one Gum context. The target's slot is parkable; the thunk's is not (its first word is a `b`). Parking the target's slot retargets the only redirect, so the thunk's listener, still holding live uprobes, never runs. Before 25f4134f a Gum detach of the target's listener kept the context (and the thunk's listener) alive.
- Failure scenario: session A probes `bar` and `foo` (`foo: b bar`) and detaches `bar` first (the agent detaches links in order), or session B probes only `foo` after A parked `bar`: `foo`'s probe fires never again.
- Evidence: **reproduced**, `exp4_thunk.cpp`: "after one call of thunk: hits_target=1 hits_thunk=1" (Gum follows the `b`), "target probe detached: target parked=1 thunk parked=0", "after one call of thunk with its probe still attached: hits_thunk=2" (unchanged; a firing probe would make it 3).
- Fix direction: key slots by the address Gum will instrument (follow `b`/PLT thunks as `gum_arm64_reader_try_get_relative_jump_target` does before the lookup), so both attach points share one slot.

### 3. Hook slots keep patch addresses and literals that become stale if Gum's interceptor is ever disposed
- Severity: low (not reachable today; condition identified)
- Commit: 25f4134f
- File: `frida_hook_slot.cpp:140-175, 232-251`; `frida_uprobe_attach_impl.cpp:168-181`
- What is wrong: the slot's only Gum reference is the per-impl `gum_interceptor_obtain()`. If the singleton's refcount reached zero, `gum_interceptor_do_dispose` (Gum `guminterceptor.c:300-311`) would restore every function's first instructions and free the slices while `slots()` keeps `patch_address`/`enter_value`; the next `frida_hook_add_owner` finds the stale slot, skips `create_slot`, and `set_parked(s, false)` writes the old literal over the restored function body. It does not happen now only because `gum_init_embedded` holds a second reference that is never released (refcount 2 with one impl alive, 1 after it dies, with no slots or anchor call: `exp3_override_gc 2`), so `~frida_attach_impl` never disposes it (`exp1_impl_lifetime`: code not restored, re-attach in a second impl works).
- Evidence: by reading plus the refcount measurement.
- Fix direction: `create_slot` keeps its own `gum_interceptor_obtain()` reference for the life of the process.

### 4. The override handler's `enter()`/`leave()` can run a nested callback outside the grace period
- Severity: low (window of a few instructions, override path only)
- Commit: 25f4134f
- File: `attach/frida_uprobe_attach_impl/src/frida_probe_grace.cpp:102-126`
- What is wrong: the flip of `count` is decided on `depth` and is not one step. `enter()`: a signal between `depth++` and the store whose handler calls a probed function nests `enter()`, sees `depth != 0`, stores nothing: the nested callback runs with an even count. `leave()`: a signal after `--depth` and before the store nests `enter()` which stores `odd+1`, even again. Listeners (`frida_slot_on_enter/on_leave`) are not exposed: Gum's guard key is set around them (`guminterceptor.c:1493-1498, 1597`), so a nested probed call bypasses; the guard is cleared before `*next_hop = replacement_function`, so only `__bpftime_frida_attach_manager__override_handler`'s `enter()`/`leave()` are.
- Failure scenario: override on F; a SIGPROF/SIGALRM handler calls probed G in that window while another thread detaches G: callback on a freed attach.
- Evidence: by reading.
- Fix direction: decide on the counter's parity with idempotent stores (read `c`; enter stores `c+1` only if `c` is even; leave only if odd after `--depth == 0`), or mask signals around the two writes.

### 5. `literal_low32` relies on an unaligned 64-bit `ldr` reading its 4-byte halves atomically
- Severity: low (works on A78AE in practice; the "reads whole"/"valid at any moment" claim is not architecturally backed)
- Commit: 25f4134f (code and CLAUDE.md)
- File: `frida_hook_slot.cpp:63-67, 85-86, 120-122`; CLAUDE.md "or the low word when the literal is unaligned ... both targets are valid at any moment"
- What is wrong: for an entry at 4 mod 8 (clang, `-Os`/`-O0`, asm) the literal at +8 is 4-aligned and the thread's `ldr x16, [pc, #8]` is an unaligned 8-byte load; before FEAT_LSE2 (Armv8.4; Orin is Armv8.2) the ISA guarantees single-copy atomicity only per byte, so a torn low word is permitted.
- Evidence: by reading; `exp2_forms` `f_unaligned` (literal_low32, 4 callers, 4200 park/unpark cycles) showed no crash.
- Fix direction: state it as hardware behaviour, or do not park unaligned literals on cores without LSE2 (`HWCAP_USCAT`).

### 6. `set_parked` leaves `parked` out of step when `gum_memory_patch_code`'s final `mprotect` fails
- Severity: low
- Commit: 25f4134f
- File: `frida_hook_slot.cpp:125-138`; Gum `gummemory.c:262-270`
- What is wrong: Gum stores and flushes before its second `gum_try_mprotect(RX)`; if that fails it returns FALSE after the store. `s.parked` stays, so the next republish's `set_parked(s, !park)` returns early: a function parked for real is believed unparked and re-attached probes never fire (silently), or the reverse.
- Evidence: by reading.
- Fix direction: set `parked` from the apply callback (the store happened), log the protection failure separately.

### 7. `synchronize()` waits without bound on counters that cannot move; the fork prepare handler can deadlock against it
- Severity: low (unusual; a detach hangs, the host keeps running)
- Commit: 25f4134f
- File: `frida_probe_grace.cpp:45-71, 128-164`
- By reading: a thread that leaves a callback without `leave()` (a handler `siglongjmp`ing out of a program) stays odd forever; a cancelled thread is caught by `tgkill` unless its tid was reused; a raw `clone(CLONE_VM)` child without `CLONE_SETTLS` shares the parent thread's `self_reader` and, killed inside a callback, leaves the parent's counter odd with the parent thread alive; `lock_for_fork` takes `registry_mutex` in prepare, so a reader whose signal handler forks while `synchronize()` holds the mutex waiting for that reader deadlocks both. By design but worth knowing: a callback blocked indefinitely (`bpf_trace_printk` to a full pipe) blocks the SIGUSR1/tool-exit detach.
- Fix direction: bound the wait (log, leak the old dispatch instead of hanging); `try_lock` in the fork prepare handler.

### 8. membarrier fallback asymmetry if the expedited command starts failing after registration
- Severity: low (needs a seccomp filter installed after the agent loaded)
- Commit: 25f4134f
- File: `frida_probe_grace.cpp:42-43, 91-99, 112-115, 130-131`
- `membarrier_registered` is fixed at static init, so readers use only a compiler fence; if `MEMBARRIER_CMD_PRIVATE_EXPEDITED` is later refused (re-registration too), `synchronize()` fences on the writer side only: the grace period is then unsound. By reading. Fix: on failure flip a flag that makes `enter()` fence, then wait one more pass.

### 9. (agent, pre-existing path) programs freed after a partially failed `destroy_all_attach_links`
- Severity: low (not reachable through the Frida impl, whose `detach_by_id` returns 0; the syscall impl returns -ENOENT only for unknown ids)
- Files: `runtime/src/attach/bpf_attach_ctx.cpp:776-790` (returns at the first failing link), `runtime/agent/agent.cpp:462-468`, `:720-721` (`reset_instantiated_state` runs regardless)
- Links after a failed one stay attached while `instantiated_progs.clear()` frees the programs their callbacks run. By reading. Fix: keep detaching the remaining links on error, then reset.

## Checked, no defect found
- Parking patch on aarch64: the 16-byte form's literal is data read by `ldr` through the data side (no I-cache maintenance needed; Gum flushes anyway); aligned 8-byte store/load are single-copy atomic; `b` -> `b` is a permitted concurrent modification; both targets exist for the process's life (listeners never detached, slice never freed); a thread between `ldr` and `br` or inside on_enter sees either target and the dispatch swap is covered by the grace period. Reproduced working under four callers for literal64 (suite test), literal_low32 and `b` forms (`exp2_forms`: 4200 and 5396 attach/detach cycles, parked after each detach), and `adrp; br` correctly cannot_park (probes keep firing, `parked=0`). Deflector redirects fail the span and reach checks.
- Unreachable replacement probe in `create_slot`: everything is inside one outer transaction, so the redirect is written only at its end (`gum_interceptor_transaction_end` returns while `level > 0`); `revert` keeps the context because the enter listener makes it non-empty; `original_function` is `on_invoke_trampoline` (`guminterceptor.c:589-590`), in the same 1024-byte slice (`GUM_INTERCEPTOR_CODE_SLICE_SIZE`), matching `SLICE_SPAN`.
- Grace period (membarrier flavour): reader store + compiler fence + acquire load; writer exchange(acq_rel) + `MEMBARRIER_CMD_PRIVATE_EXPEDITED` + acquire loads; liburcu's algorithm; without membarrier both sides use seq_cst fences. Fork: handlers via `register_fork_handlers` (agent) or `pthread_atfork` (tests); the child keeps the forking thread's reader with its new tid and re-registers membarrier before issuing it. Reproduced: a child forked while four parent threads were in callbacks detaches and parks promptly (3/3, `exp2_forms 1`). Readers of exited threads are collected past 64 (`exp3_override_gc 1`: 900 short-lived threads, no crash). New threads block in `register_self` during a wait (latency only). `synchronize` skips its own reader.
- Order in `republish_locked`: park before publishing the empty dispatch; publish, wait, then unpark; `detach_by_id` republishes before erasing the attach and the entry's destructor republishes again; `detach_by_func_addr` destroys the entry before erasing attaches. `bpf_attach_ctx` destroys `attach_impl_holders` (declared after `instantiated_progs`) first, so impls wait before programs are freed; `perform_detach` frees programs after `destroy_all_attach_links` (success path).
- Override after parking: `ensure_locked` installs the replacement before `republish` unparks; override -> parked -> uprobe session -> override again give the right results (`exp3_override_gc 0`). The documented forwarder limit (5 integer args, no FP/struct) happened not to bite with 6 integer args and a double in this build; documented, not understated.
- Thread context fix (ff5515d8): Gum sets the guard key before `get_interceptor_thread_context()` (`guminterceptor.c:1493-1500`), so the anchor call creates the context under the guard; `gum_interceptor_attach` ignores the current thread first (`:430`, unguarded context), hence the `ensure_gum_thread_context` before every attach/replace/ignore. The anchor is hooked once on the first call, which precedes any probe; it keeps its hook when impls die (see finding 3's condition).
- Tests exercise their claims (bounded children, >100 cycles under four callers, per-cycle late flag, parked-state and callback checks after unpark); 4 new tests pass 3/3 runs. x86: `looks_redirected` true -> no replace probing, `cannot_park`, Gum's path with an empty dispatch (documented). CLAUDE.md otherwise accurate (grace semantics, which forms park, forwarder limit, attach-time window, not-thread-safe note).

## Experiments
All under `/tmp/claude-2002/-home-jetson-src/6ac0883f-4479-466c-bec5-5fa31cffe4de/scratchpad/review-20261105/frida/` (`build.sh <name>` links against `build-jetson-daily`'s static libs and the Gum 16.1.2 devkit; each run forks a child and reports its exit status or signal):
- `exp5_override_detach.cpp` / `exp5.out`, `exp5_full.out`: finding 1 (SIGABRT 3/3, SIGILL 1/1).
- `exp4_thunk.cpp` / `exp4.out`: finding 2.
- `exp1_impl_lifetime.cpp` / `exp1_v0.out`, `exp1_v1.out`: slot and hook survive `~frida_attach_impl`; re-attach in a second impl works.
- `exp2_forms.cpp` / `exp2_forms.out`, `exp2_fork.out`: literal_low32, `b`, `adrp` forms under four callers; fork child detaching while parent threads are inside callbacks (3 runs).
- `exp3_override_gc.cpp` / `exp3.out`: override sessions after parking (0), 900 short-lived threads with detaches (1), interceptor refcount with and without an impl (2).
- `suite_new_tests.out`: the four new suite tests, 3 runs each.
No repo was modified; no sudo was used; no leftover processes.
