# E5 predictions, addendum v3: Dnext careful mode (I8, written before any Dnext build or run)

Written 2026-11-26, before building Dnext/Dnextpc and before any run with them. It does not replace
`PREDICTIONS.md` (v1) or `PREDICTIONS-v2.md` (v2); the target (src/e5target, sha in src/sha256.txt)
and the scenarios are unchanged from v2. It covers only the two new legs.

## Builds and legs

- Dnext / Dnextpc: Tern c71a9987 plus the careful-mode patch (branch `ladder/dnext-c71a9987`; the
  commit is recorded in `builds/dnextpc/source-state.txt` once built). Same CMake options as
  dnew / dnewpc. The patch (runtime/agent/careful_hooks.c, runtime/src/producer_identity.*,
  runtime/src/bpf_helper.cpp, runtime/agent/credential_hooks.cpp,
  runtime/src/handler/perf_event_handler.cpp) is inert unless `BPFTIME_CAREFUL=1` is set at load.
- Leg `Dnextpc`: switch off. Leg `Dnextpc+BPFTIME_CAREFUL=1`: switch on.
- Mechanism of the switch-on leg: the agent interposes vfork/__vfork (asm stub), _Fork,
  clone/__clone, posix_spawn(p), pidfd_spawn(p), system, popen, prctl, pthread_setname_np. Spawn-like
  calls bump the identity epoch; a clone child without CLONE_VM and a raw _Fork child reset the
  identity state in the child; a CLONE_VM clone without CLONE_SETTLS that runs concurrently marks the
  TLS block shared (no caching). comm: per-thread cache filled with prctl(PR_GET_NAME), invalidated
  around prctl(PR_SET_NAME) and pthread_setname_np. Not seen: raw system calls, writes to
  /proc/.../comm.

## Predictions (fields that mismatch truth; "-" = none; all other fields match)

| Scen | Dnextpc (off) | Dnextpc + BPFTIME_CAREFUL=1 |
|---|---|---|
| W1 (fork) | as Dn | - |
| W2 (_Fork) | as Dn (p1 tgid, tid) | - (_Fork interposer: epoch bump, child resets process_pid and the cache) |
| W3 (vfork warm), W3c (vfork cold) | as Dn (W3 p1 tgid, tid; W3c -) | - (vfork stub bumps the epoch; the child misses, pid != process_pid, nothing cached) |
| W5 (clone SIGCHLD) | as Dn (p1 tgid, tid) | - (child runs through the trampoline: identity reset in the child) |
| W6 (clone VM\|VFORK) | as Dn (p1 tgid, tid) | - (epoch bump + spawning guard; child pid != process_pid) |
| W6t (thread-like clone, no SETTLS, warm) | as Dn (p1 tid) | - (TLS marked shared before the clone: creator and child never cache) |
| W6tc (same, cold) | as Dn (p2 tid) | - (same) |
| W7g (libc set*id, fork child) | - | - |
| W7r (raw setresuid syscall, fork child) | as Dn (p1 uid, gid) | p1: uid, gid (raw system calls are not seen; unchanged) |
| W8a (prctl PR_SET_NAME, main thread) | p0, p1, p2: comm (exe basename) | - (prctl interposer; refill with PR_GET_NAME) |
| W8b (pthread_setname_np on a worker) | p0, p1, p2: comm | - (pthread_setname_np interposer bumps the comm epoch) |
| W8c (worker writes its own /proc comm) | p0, p1, p2: comm | p1: comm (the /proc write is not seen; p0 and p2 hold the base name, which the cache holds) |
| W8d (main writes the worker's /proc comm) | p0, p1, p2: comm | p1: comm (same) |
| W12 (tid reuse) | - | - |

Counts: deterministic, so N/N at a predicted point and 0/N (reported as ≤ 3/N) elsewhere.

Other expectations:
- The target log line "Careful mode: interposers bound, caches on" appears on the switch-on leg
  (agent log), and no such line on the off leg.
- W6t/W6tc: v2's uncertainty about Frida in a TLS-sharing thread still applies on both legs; a crash
  or hang there would be a finding.
- If Dnextpc (off) differs from Dn anywhere, the switch is not inert: a defect of the patch.
