# E5 predictions, addendum v2 (written before the v2 smoke run)

Written 2026-11-25, after the review of E5 v1 and before any run of the v2 target. It does not
replace `PREDICTIONS.md` (v1, sha 6720abbb..., kept unchanged). It covers only what v2 changed.
Mechanism citations are the ones in `PREDICTIONS.md` unless given here.

Correction to v1: the Dxn build (`builds/dxnewpc`) is commit **f39f7362** on branch
`ladder/dexact-c71a9987` (recorded in `builds/dxnewpc/source-state.txt`). The source tree is
the directory `/home/y/src/bpftime-paper/dxnew-c71a9987`, which is named after its base commit
c71a9987. v1 line 4 named the directory and line 45 named the commit; both mean the same tree.

## What changed in v2

- One target process per scenario (the runner starts e5target once per scenario, the loaders stay
  attached across them). Caches no longer carry over from one scenario to the next.
- W7g/W7r: the iterations run in a fork child of the target. A fork child has only the forking
  thread (the agent's 2 threads are not copied), so the process doing set*id is single-threaded
  on every leg (`threads_at_start` is logged from the child). Fork child handlers run first, so
  the caches start empty in the child; p0 inside the child fills them.
- W8a/b/c/d: the base comm is "w8base", which is not the exe basename "e5target". Each iteration
  has p0 (base name), p1 (renamed), p2 (after restoring the base name).
  - W8a: main thread, `prctl(PR_SET_NAME)`.
  - W8b: main thread renames a worker with `pthread_setname_np` (glibc writes
    `/proc/self/task/<tid>/comm` for another thread); worker audits.
  - W8c (changed): the worker renames ITSELF by writing `/proc/self/task/<own tid>/comm`.
  - W8d (new, the v1 W8c): main thread writes the worker's `/proc/self/task/<tid>/comm`. W8d
    goes through the same kernel path as W8b; it is a control for W8b, not an independent case.
- W12 (changed): forces tid reuse. Gen 1: N pthreads created and joined one after another, each
  audits p0. Raw trivial threads are then spawned until the cyclic pid allocator wraps around
  pid_max (4194304 here) and stands just before gen 1's first tid. Gen 2: N pthreads, each audits
  p1, and gets gen-1 thread i's tid on the same cached glibc stack (reuse count is logged:
  `W12 tid_reused k/N`). No sysctl is written.
- W6t / W6tc (new): thread-like raw clone `CLONE_VM|CLONE_FS|CLONE_FILES|CLONE_SIGHAND|
  CLONE_THREAD|CLONE_SYSVSEM|CLONE_CHILD_CLEARTID` WITHOUT `CLONE_SETTLS`: a live child in the
  same thread group, on its own stack, sharing the creating thread's TLS (fs base) while that
  thread waits on the clear-tid futex. W6t: warm (creator audited p0 just before; p1 child; p2
  creator). W6tc: cold, from a fresh pthread that has not called a helper (p1 child, p2 creator).
  W5 (`clone(SIGCHLD)`) and W6 (`clone(CLONE_VM|CLONE_VFORK)`) stay, but they are raw-clone
  versions of W2 and W3, not independent cases.

## Predictions (fields that mismatch truth; "-" = none; all other fields match)

| Scen | K | O | Ol | Dn / Dnpc | Dxn (EXACT=1) |
|---|---|---|---|---|---|
| W1, W2, W3, W3c, W5, W6 | as v1 | as v1 | as v1 | as v1 | - |
| W7g (fork child) | - | - (uid read every call) | - | - (cache cleared by the fork child handler, `bpf_helper.cpp:492-494`; libc interposers invalidate) | - |
| W7r (fork child) | - | - | - | p1: uid, gid (p0 in the child caches 0/0 in the process-wide `cached_credentials`, `bpf_helper.cpp:706-750`; the raw syscall is not seen). p2 matches (cache 0/0 = truth again) | - |
| W8a | - | p0, p1, p2: comm (always the exe basename "e5target", O `bpf_helper.cpp:276-294`) | p0, p1, p2: comm (Ol `:671-745`) | p0, p1, p2: comm (Tern `bpf_helper.cpp:802-880`) | - (prctl per call, dxnew `bpf_helper.cpp:851-853`) |
| W8b, W8c, W8d | - | p0, p1, p2: comm | p0, p1, p2: comm | p0, p1, p2: comm | - |
| W12 gen1 p0, gen2 p1 (reused tid) | - | - (new thread: thread_local tid starts at -1 again; glibc re-copies the TLS init image for a cached stack) | - (same, plus thread_local host identity) | - (`__thread identity_cache` default pid=-1, `producer_identity.hpp:14-19`, re-initialized per thread) | - |
| W6t warm | - | p1: tid (child reads the creator's thread_local cached tid; tgid is right because CLONE_THREAD keeps the tgid) | p1: tid | p1: tid (cache hit: pid != -1 and the epoch is never bumped on x86, `producer_identity.hpp:28-41`) | - |
| W6tc cold | - | p2: tid (the child fills the shared thread_local tid with its own tid, then the creator reports the dead child's tid). p1 matches | p2: tid (same, cached host identity) | p2: tid (miss path caches because getpid() == process_pid for a CLONE_THREAD child, `producer_identity.cpp:141-155`; unlike W3c, where the vfork child's pid differs and nothing is cached). p1 matches | - |

Counts: each predicted mismatch is deterministic, so N/N at that point; "-" is 0/N, reported as
≤ 3/N. W12 p1 is the reused-tid case only for the iterations where the reuse happened (logged);
the prediction (no mismatch) is the same for both. Ambiguous: 0 expected everywhere.
Uncertain: W6t/W6tc on bpftime legs run the agent's Frida hook in a thread that shares another
thread's TLS (glibc tcache, Gum thread context). A crash there kills the whole thread group
(missing records and target rc != 0 for that scenario only); a hang shows as a timeout. Either
outcome would be a finding, not a prediction failure for the identity fields.
