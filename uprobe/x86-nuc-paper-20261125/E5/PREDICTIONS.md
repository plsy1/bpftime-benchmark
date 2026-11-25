# E5 predictions (plan item A7), written before any E5 code ran

Written 2026-11-25, before the target, tools or any smoke run existed. Sources read:
`/home/y/src/bpftime-paper/dnew-c71a9987` (Tern, Dn/Dnpc/Dn730), `dxnew-c71a9987` (Dxn),
`olatest-70f7bc1f` (Ol), `official-4708f409` (O). Paths below are relative to those trees.

## Fields and truth

Per audit point the target records truth T1 (raw syscalls immediately before `audit_point_k`)
and T2 (immediately after `audit_point_u`): `SYS_getpid`, `SYS_gettid`, `SYS_getuid`,
`SYS_getgid`, `prctl(PR_GET_NAME)`. Compared fields: `tgid` (pid_tgid >> 32), `tid`
(pid_tgid low 32), `uid` (uid_gid low 32), `gid` (uid_gid >> 32), `comm` (16 bytes, up to NUL).
T1 != T2 on a field = ambiguous (not counted as match or mismatch). Kernel helpers:
`bpf_get_current_pid_tgid` = (tgid << 32 | pid) of `current` in the initial pid namespace,
`bpf_get_current_uid_gid` = real uid/gid of the calling task's cred (per thread),
`bpf_get_current_comm` = `current->comm` (per thread). All scenarios run in the initial pid
namespace, so the kernel leg is predicted to match truth everywhere (0 mismatches).

## Mechanisms in each tree (x86, agent by LD_PRELOAD, Frida uprobes)

- **O, official 4708f409** `runtime/src/bpf_helper.cpp`: tgid cached per process
  (`static pid_t bpftime_cached_tgid = getpid()`, :212), tid per thread (`thread_local`, :214),
  refreshed only by a `pthread_atfork` child handler (:219-231, sets tgid=getpid(), tid=-1 for
  the forking thread). Helper :233-257 returns cached values. uid_gid: `getgid()/getuid()` on
  every call (:259-266), exact. comm: basename of `/proc/self/exe`, cached once in a static
  `std::string` (:276-294); never the thread's comm.
- **Ol, upstream 70f7bc1f** `runtime/src/bpf_helper.cpp`: same identity cache as O (tgid :380,
  tid :382, atfork refresh :387-399, helper :401-439, BPFTIME_HOST_PROCFS unset so
  `get_host_nspid` returns the fallback, :341-344). uid_gid exact (:610-616). comm = exe
  basename cached with `pthread_once`, reset in fork children (:671-745, atfork :706).
- **Dn / Dnpc, Tern c71a9987**: identity cache `runtime/src/producer_identity.hpp:29-42` (one
  `__thread` struct {pid, tid, epoch}, hit when pid != -1 and epoch == `bpftime_identity_epoch`);
  miss path `producer_identity.cpp:141-155` caches only if `getpid() == process_pid`
  (`process_pid` refreshed in fork children only, :31-43); fork child handler clears the cache
  (:36-43, run through glibc `pthread_atfork` via `fork_handlers.cpp:34-52`). The epoch is
  bumped only by the aarch64 syscall-trace hook (`text_segment_transformer.cpp` `note_clone_syscall`);
  CLAUDE.md:547-550 states x86 and Frida uprobe mode have no such marker: "a vfork or raw-clone
  child there still uses its parent thread's identity". uid_gid: process-wide cache
  (`bpf_helper.cpp:700-750`) enabled only if all eight set*id interposers bind
  (`runtime/agent/credential_hooks.cpp:54-56, 62-87`), invalidated by the libc interposers and by
  the fork child handler (`bpf_helper.cpp:492-494`); raw syscalls are not observed
  (CLAUDE.md:688-705). comm: exe basename via `pthread_once` (`bpf_helper.cpp:802-842`, helper
  :844-880), reset in fork children (:839). Dnpc differs from Dn only in checked probe reads,
  not in these helpers.
- **Dxn with BPFTIME_DIAG_EXACT=1** (dxnew f39f7362): `producer_identity.hpp` returns
  `getpid()`/`gettid` on every call (glibc 2.41 does not cache getpid), uid_gid skips the cache,
  comm = `prctl(PR_GET_NAME)` of the calling thread (`bpf_helper.cpp` diff vs Dn, +846-861).

## Scenarios (per iteration; points: p0 parent before, p1 child/renamed, p2 parent after)

| Scen | What one iteration does |
|---|---|
| W1 | main: p0; `fork()`; child: p1, `_exit`; parent waits; p2 |
| W2 | as W1 with `_Fork()` (glibc 2.34+, runs no atfork handlers) |
| W3 | warm vfork on main thread: p0; `vfork()`; child: p1, `_exit`; parent: p2 |
| W3c | cold vfork: a fresh pthread (no helper call yet) calls `vfork()`; child: p1, `_exit`; that thread then: p2 |
| W7g | single-threaded process as root: p0; glibc `setresgid(4343,4343,0)`, `setresuid(4242,4242,0)`; p1; glibc restore to 0; p2 |
| W7r | as W7g with `syscall(SYS_setresgid/SYS_setresuid)` |
| W8a | main: `prctl(PR_SET_NAME,"w8a-<i>")`; p1 (name restored at the end) |
| W8b | main: `pthread_setname_np(worker,"w8b-<i>")`; worker: p1 |
| W8c | main writes "w8c-<i>" to `/proc/self/task/<worker tid>/comm`; worker: p1 |
| W12 | batches of 4 threads created/joined; each thread: p0 once (glibc reuses stacks/TLS) |

## Predictions: fields that mismatch truth (all other fields match; "-" = none)

| Scen | K | O | Ol | Dn / Dnpc | Dxn (EXACT=1) |
|---|---|---|---|---|---|
| W1 | - | - (atfork refresh, O :219-231) | - (:387-399) | - (producer_identity.cpp:36-43) | - |
| W2 | - | p1: tgid, tid (parent's: no atfork in `_Fork`) | p1: tgid, tid | p1: tgid, tid (TLS cache copied, epoch never bumped on x86) | - |
| W3 | - | p1: tgid, tid (vfork child shares the static tgid and the thread's TLS tid) | p1: tgid, tid | p1: tgid, tid (shared TLS cache, CLAUDE.md:547-550) | - |
| W3c | - | p1: tgid (static tgid is the parent's; tid read fresh = child's, correct); p2: tid (the child filled the shared TLS tid, parent thread now reports the dead child's tid) | same as O | - (child: pid != process_pid so not cached, producer_identity.cpp:147-154; parent fills its own) | - |
| W7g | - | - (uid exact) | - | - (libc interposers invalidate) | - |
| W7r | - | - | - | p1: uid, gid (stale 0/0 cached at p0; raw syscall not observed). p2 matches again (cache 0/0 = truth) | - |
| W8a | - | p1: comm (exe basename "e5target") | p1: comm | p1: comm | - (prctl) |
| W8b | - | p1: comm | p1: comm | p1: comm | - |
| W8c | - | p1: comm | p1: comm | p1: comm | - |
| W12 | - | - (thread_local tid starts at -1 in each new thread; glibc re-initializes TLS of reused stacks) | - | - | - |

Expected counts: for a scenario with N iterations each predicted-mismatching point mismatches
in all N iterations (deterministic mechanisms, not races); a predicted "-" should give 0/N,
reported as an upper bound of 3/N (rule of three, 95%). Ambiguous: 0 expected everywhere (no
concurrent identity change). Missing: 0 expected for the kernel; for bpftime the fork/_Fork/vfork
children run the inherited Frida hooks against the inherited MAP_SHARED shm, so records are
expected (Tern CLAUDE.md:342-351 states fork children execute inherited bound programs);
uncertain for O/Ol in `_Fork`/vfork children (a crash or hang there would show as missing p1
and/or missing later points; any such outcome is itself a finding).

Not predicted here (phase 2, only if implemented): W5 glibc `clone(SIGCHLD)` child: same as W2
for O/Ol/Dn (no atfork handler runs); W6 `clone(CLONE_VM|CLONE_VFORK|SIGCHLD)` on a separate
stack without CLONE_SETTLS: same as W3 (warm) / W3c (cold); W10 `unshare(CLONE_NEWPID)` child:
all user-space legs including Dxn report the namespace-local tgid/tid (1) while the kernel
reports the initial-namespace pid (bpftime only translates with BPFTIME_HOST_PROCFS, Ol
:341-344; Tern bpf_helper.cpp:466-486); W11 non-leader thread exec: the new image reloads the
agent, caches start empty, predicted match for all legs.
