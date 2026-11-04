# Harness gaps closed, four new problems (2026-11-04)

Problem 5 of `uprobe/realtools-20261104`: opensnoop, javagc and the get_stack_id example were not
checked on bpftime. `audit.py` (the harness copy that ran, here) now:

- runs opensnoop and a new syscount case with the workload under the text segment transformer,
  as `bpftime start -s` (`LD_PRELOAD=libbpftime-agent-transformer.so AGENT_SO=libbpftime-agent.so`);
- runs javagc through `bpftime attach` (frida-core injection into the running JVM, as root; the
  JVM waits 7 s before collecting);
- runs get_stack_id's victim under `stdbuf -oL`: its "continue malloc" lines were lost in the pipe
  buffer when `timeout` ended it, so "victim never loops" was the harness (rc 124 is the normal end).

`dev-25f4134f` was frozen again with its `bpftime` CLI and transformer (built at f0954773,
docs-only after 25f4134f; agent unchanged). `run.sh`: opensnoop/syscount/javagc once on bpf and
dev-25f4134f, get_stack_id 3 rounds on bpf, official and dev-25f4134f.

| tool | bpf | dev-25f4134f | note |
|---|---|---|---|
| javagc | 50/50 | 50/50 (attach rc 0) | works |
| opensnoop | 38,740 of 38,800 | 0 | program disabled: new problem 11 |
| syscount (openat) | 41,171 (all processes) of 39,200 | no counts | program disabled: new problem 12 |
| get_stack_id, victim mallocs | 64, all `-EEXIST` | 14 / 70 / 0, stacks depth 7 | new problems 9 and 10 |

official on get_stack_id: 1,304-1,308 events with stacks (depth 2), no fd errors, but the workload
dies with SIGSEGV 3/3 at its end (not investigated).

On bpf the victim's stacks fail because the example traces malloc in every process: its stack map
(1000 entries, no `BPF_F_REUSE_STACKID`) fills with other processes' stacks (`-EEXIST` 2,536 and
`-ENOMEM` 46 times in the morning run). The example's design, not a bug.

## New problems (not fixed)

9. **Regression in dev since 5920d5f9 (10-31, "ringbuf: skip the record of a producer that died
   before submitting")**: the record header's second word, which `bpf_ringbuf_submit` and
   `bpf_ringbuf_discard` read as the map fd (`ptr[-1]`), now holds the producer's owner slot.
   They then look up a wrong fd ("Expected fd 145 to be an ringbuf map fd", rising by one per
   event), the record is never submitted, and the reclaim later discards it ("discarded a record
   reserved by thread ... which exited before submitting it"). Every program using
   `bpf_ringbuf_reserve` + submit/discard loses its events (get_stack_id: 0-70 of ~1,300 that
   official delivers; libbpf-tools' compat buffer uses this path); `bpf_ringbuf_output` passes the
   fd and is not affected. The runtime tests call `bpftime_ringbuf_submit` with the fd, so they
   never took the helper path. In the daily list since 02d2d7cd ("Replay the review fixes daily (137 commits)").
10. **A probe that logs during process exit crashes the traced process.** dev (efe08cbd and
    25f4134f alike), `timeout` running the victim with the agent: SIGSEGV in 1-2 of 4 runs; gdb:
    `bpf_ringbuf_submit` -> `bpftime_shm::try_get_ringbuf_map_impl` -> `SPDLOG_ERROR` ->
    `spdlog::pattern_formatter::format` on a destroyed logger. Problem 9 makes it log; any error
    log from a probe at exit can do the same.
11. **libbpf's poisoned instructions in dead code disable the program.** opensnoop's
    `tracepoint__sys...`: "Ext func not found: _bpf_helper_ext_2001000001". libbpf marks the
    branch of its compat buffer that it does not use (ringbuf vs perf; the other map is not
    created) with POISON_LDIMM64_MAP_BASE (2001000000 + map index); the kernel's verifier removes
    the dead code, bpftime (no verifier) compiles it and gives up.
12. **`bpf_get_current_task` (helper 35) is not provided**: syscount's `sys_exit` is disabled
    ("Ext func not found: _bpf_helper_ext_0035"); there is no `task_struct` in user space.

Raw outputs: `audit/`, `stackid/`. By hand: `gsi.sh BUILD plain|stdbuf` (crash rate), `gsi-gdb.sh` (the gdb run of problem 10, `gdb-problem10.txt`, on dev-efe08cbd).
