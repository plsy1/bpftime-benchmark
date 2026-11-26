# E5 predictions, addendum v4: P5a Dview spike (I9; written before any Dview build or run)

Written 2026-11-26, before building tools/view, Dview/Dviewpc and the new e5target, and before any run. Does not replace
v1–v3. Covers the P5a spike legs and two new scenarios.

## Changes

- e5target: scenarios appended (ids 16, 17); existing scenario code and ids unchanged. The new binary's sha is recorded in
  src/sha256.txt by `./build.sh target` (old binary kept as src/e5target.prev, sha cde15786…).
  - W7t: in a fork child (like W7r), a 2-thread process; p0 worker audits (0/0); the worker drops with raw
    syscall(SYS_setresgid 4343)/(SYS_setresuid 4242); p1 worker audits (4242/4343); p2 the main thread audits (0/0) while
    the worker is still dropped; the worker restores with raw calls.
  - W5r: p0 parent audits; child of raw syscall(SYS_clone, SIGCHLD, 0, 0, 0, 0) audits p1 and _exits; p2 parent audits.
- Dview / Dviewpc: Dnext (6b6b247e) + the view read side (runtime/src/kernel_view.*, view branch first in
  bpf_get_current_uid_gid and bpf_get_current_comm), behind BPFTIME_VIEW=1 (read at load). Kernel side: tools/view
  (view.bpf.c hooks task_newtask, commit_creds (fexit, else fentry, else kprobe), task_rename, sched_process_exec,
  sched_process_exit; per-thread {tid, tgid, uid, gid, comm} slots in a BPF_F_MMAPABLE array indexed by tid & mask, seqlock;
  tern_viewd start/status/stop with pinned links). The runner starts viewd before the loaders and stops it after the targets.
  On a view miss (empty slot, other tid/tgid, odd seq, stale) the helpers query the kernel (getuid/getgid, prctl); pid/tid
  still come from Tern's (careful) identity cache, which is also the view key.

## Legs (one Dviewpc binary)

1. K
2. Dviewpc+BPFTIME_CAREFUL=1
3. Dviewpc+BPFTIME_CAREFUL=1+BPFTIME_VIEW=1
4. Dviewpc+BPFTIME_CAREFUL=1+BPFTIME_VIEW=1+VIEWD_NSLOTS_LOG2=4 (16 slots: forced collisions → misses, never wrong values)

## Predictions (fields that mismatch truth; "-" = none; other fields match)

| Scen | K | 2: CAREFUL | 3: CAREFUL+VIEW | 4: CAREFUL+VIEW, 16 slots |
|---|---|---|---|---|
| W1, W2, W3, W3c, W5, W6, W6t, W6tc, W7g, W8a, W8b, W12 | - | - (as I8) | - | - |
| W7r (raw setresuid, single thread) | - | p1 uid, gid (as I8) | - (commit_creds hook) | - |
| W7t (raw setresuid in one of 2 threads) | - | p1 uid, gid (process-wide cache holds 0/0; raw call not seen) | - | - |
| W8c, W8d (/proc comm write) | - | p1 comm (as I8) | - (task_rename hook) | - |
| W5r (raw clone child) | - | p1 tgid, tid (child runs on the parent's cached identity; no interposer sees a raw clone) | p1 tgid, tid (the view key is the wrong tid; the parent's slot is read, whose uid/gid/comm equal the child's, so those fields match) | p1 tgid, tid |

Counts: N/N at a predicted point, 0/N (≤ 3/N) elsewhere. Additional expectations:
- Every target of legs 3–4 logs "View: mapped"; viewd status: hook counters newtask, creds, rename, exec, exit grow during
  the leg; lock_fail 0 on leg 3; leg 4 shows view misses > 0 and still no wrong value.
- If viewd cannot attach commit_creds by any of the three modes, or the view cannot be mapped, the leg is void (no-go
  material, not a prediction failure).
- Uncertain: W5r runs Frida hooks in a raw-clone child with no fork handlers; a crash or hang there (target rc != 0) would
  be a finding on every bpftime leg, not a view result.
