# Problems 10 and 11 (2026-11-05)

Problems 10 and 11 of `uprobe/harness-gaps-20261104`. dev b46dde06 (problem 10) and 74d0ab8b
(problem 11), on 6a15a271, local. Frozen build: `uprobe/ssl-nginx-2x2-20261023/builds/dev-74d0ab8b`
(with its CLI and transformer). Gate (`gate.log`): runtime 173 passed, 1 skipped (two new); Frida
22/22; text segment transformer 49/50 (1 fails as expected); real-tool smoke funclatency 8,296,000
of 8,296,000, memleak rc 0, detach rc 0.

## 10: a probe logging during exit crashed the traced process (fixed)

The default logger is destroyed with spdlog's registry at exit while probes still run.
`bpftime_set_logger` now registers `bpftime_quiet_logger_at_exit` once the registry exists; exit
handlers run in reverse order, so it runs before the registry's destructor, keeps the logger alive
(a reference never released) and turns it off. Test "A log after spdlog's teardown at exit is
dropped" (a helper process logs from an exit handler registered before the registry): exit 0;
without the handler, 3 (the logger and its sinks destroyed) in 3/3.

Not seen in the real run again: problem 9's fix removed the error the example logged at exit (no
error, no log). get_stack_id 3 rounds on dev-74d0ab8b (`stackid/`): 70/70/70 events, rc 124 3/3.

official's crash at the end of every get_stack_id run is another one (`official-exit-crash.gdb.txt`):
`bpftime_ringbuf_reserve` -> `is_map_fd` after the shm was unmapped at exit, which dev already
skips (`global_shm_released`, CLAUDE.md "Process Exit").

## 11: libbpf's poisoned calls in dead code (fixed), opensnoop still blocked

`bpftime_prog_load` points calls libbpf poisoned (2001000000 + map index, 2002000000 + extern
index; dead code the kernel's verifier removes) at a private helper slot (8188) that reports it ran
and returns -EINVAL. Test "Calls libbpf poisoned in dead code do not disable the program": without
the rewrite the program is disabled (-ENOEXEC).

opensnoop still reports nothing (`audit/`): its exit program now compiles past the poison and stops
at helper 158 (`bpf_get_current_task_btf`), then 115 (`bpf_probe_read_kernel_str`): both inside
`if (full_path && ...)`, where `full_path` is a `const volatile` (frozen rodata, false). The kernel's
verifier knows the value and drops the branch; bpftime does no such elimination and disables a
program that calls any helper it does not register. syscount likewise (helper 35 in its
`sys_exit`).

Experiments, not committed (`experiments/`, builds `exp-74d0ab8b-task158`, `exp-74d0ab8b-missing`):

| agent | opensnoop | syscount (openat) |
|---|---|---|
| dev-74d0ab8b | 0 (helper 158) | none (helper 35) |
| + helper 158 as a stub returning 0 | 0 (helper 115) | - |
| + every unregistered helper pointed at a stub returning -ENOSYS | 18,461 of 38,700 | 38,800 of 38,800 |

With every helper stubbed, opensnoop's `bpf_get_stack` in the syscall tracepoint fails ("There is
no frida hook running": the stack walk needs Frida's context) and it delivers 48% of the opens
(not investigated). bpf: opensnoop 38,800 of 38,800, syscount 41,475 (all processes).
