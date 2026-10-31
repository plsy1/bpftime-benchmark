# Third review: the second round's fixes (2026-10-31, dev 9aeb2863)

Three Fable 5.1 reviews, scoped to the 16 commits f2d6ae7d..9aeb2863 (the
fixes of `../rereview/`, recorded in `../refixes/`). Same rules as before:
only the code, the requirements and the platform; read-only; standalone
experiments in a scratch directory (sources in `experiments/`).
`build-jetson-daily` was rebuilt at 9aeb2863 before the reviews.

Every finding was checked here against the code. "Reproduced" means the
experiment was rebuilt from its source and rerun independently on the same
build (`experiments/*`); the outputs below are from those reruns.

"Mine" marks problems introduced or left half-done by those 16 commits.

## Verdict on what held

- Fork handler merge (lazy mode): the lock order is right, no path takes the
  patch lock and then `rescan_mutex`; the old deadlock test passes.
- The 0xb7e5 seccomp discrimination: right for both install orders;
  application handlers receive the original siginfo and ucontext; x0, mask,
  SA_RESETHAND honored.
- The `note_clone_syscall` assembly: registers, NZCV, FP state and CFI
  preserved; the child depends on nothing in the hook frame.
- Two-page mprotect and the SIGSYS restore on failure paths.
- Ringbuf recovery never discards the record of a correctly identified live
  owner (CAS on the header; 64-bit positions, no ABA); multiple consumers.
- Perf drain rotation; bad_alloc for the shard node.
- vfork/posix_spawn identity in syscall-trace mode.
- Fork table: all registrations are static initializers; publication order.
- Per-CPU packed layout through bpf() (value sizes 4 and 12, no guard
  overrun); lookup_and_delete atomicity with valid pointers; overflow checks.

## Problems found

### Default paths

1. **clone3's flags read with `process_vm_readv` is subject to the
   application's seccomp filter** (mine, dd3ee34d; default mode too).
   Reproduced (`syscall/exp_pvr.cpp`):

   | Application filter on process_vm_readv | epoch after posix_spawn | after vfork |
   |---|---|---|
   | none | 1 (right) | 2 |
   | ERRNO(EPERM), as Docker's default profile without CAP_SYS_PTRACE | 0 (fix silently off) | 1 |
   | KILL_PROCESS | process dies (SIGSYS, core) at posix_spawn | - |

   glibc 2.39's pthread_create and posix_spawn both use clone3, so a sandbox
   that kills on process_vm_readv now kills the process at thread creation,
   for a call the application never made.
2. **Per-CPU hash maps with larger keys refused with ENOMEM** (mine,
   bf1af650). `check_map_attr` multiplies `(key + value)` by the CPU count;
   one key is stored per entry. Reproduced (`maps/exp.c percpu_key`, default
   128 MB segment): PERCPU_HASH key 256 / value 8 / 100,000 entries and key
   64 / 400,000 entries both ENOMEM (kernel-legal, created before bf1af650);
   the same HASH is created.
3. **A map just over the segment leaves a handler without a map in the
   table** (mine, half-done in bf1af650/7553abb8). Boost's
   `boost::container::length_error` is not a `std::length_error` (Boost
   built without `BOOST_CONTAINER_USE_STD_EXCEPTIONS`), so `map_init` does
   not catch it and `set_handler` never clears the slot. The striped hash
   allocates about 3x the lower bound `check_map_attr` checks. Reproduced
   (`maps/exp.c lenerr`, 16 MB segment, hash 8/8/600,000): ENOMEM to the
   tool, one fd leaked, a lookup on the leaked fd SIGSEGVs the tool. With the
   default 128 MB segment, u64/u64 hashes of about 2.8M-8M entries.
4. **Unsupported map types fail with errno 0** (mine, bf1af650: before, they
   succeeded with an empty handler). Reproduced (`maps/exp.c unsupported`):
   SOCKHASH, LRU_PERCPU_HASH, BLOOM_FILTER with key 4: -1, errno 0. libbpf's
   `bpf_map_create` turns that into fd 0, which `bpf_object__create_map`
   takes as success (by reading libbpf).
5. **One fd leaked per rejected map creation** (mine: the failure path is
   new). Reproduced (`maps/exp.c fdleak`): 200 rejected creations, fds 6 ->
   206. libbpf retries a failed create once, so two per map.
6. **`BPF_MAP_LOOKUP_AND_DELETE_ELEM` with a NULL key or value** (mine,
   a6fdd740). Reproduced (`maps/nullp.c`): HASH key NULL SIGSEGV; HASH value
   NULL returns 0 and deletes the key without returning the value; PERCPU_HASH
   either NULL SIGSEGV; LRU_HASH key NULL returns 0. The kernel returns EFAULT.
7. **Mapped-file check by device and inode fails on btrfs** (mine,
   41eb8e9d). By reading: `/proc/self/maps` shows the superblock's device,
   `stat` on btrfs the subvolume's anonymous device, so every library is
   skipped (untraced) on a btrfs root; overlayfs on kernels before
   `file_user_inode` (pre-6.8) likewise. The function's own comment says
   "not the device". Not reproducible here (ext4, no btrfs/overlay mounts).
8. **Header-only perf shard: a thread never outputs again** (half-done,
   e3e7d721). The shard constructor ignores `ensure_mmap_buffer` failing
   (it catches the allocation error itself), so a shard with no data area is
   kept and cached. Reproduced (`shm/exp.cpp headeronly`): 24,560 B free:
   output ENOSPC, 1 shard, data size 0; after freeing to 974,192 B: still
   ENOSPC, still 1 shard, no lost count ever.
9. **Ringbuf `submit` reads the owner slot after publishing** (mine,
   5920d5f9). By reading: once BUSY is cleared the consumer may move on, the
   ring may lap and another reservation (or a padding record, which stores
   `self_fd`) may write that header's second word before the submitter
   reads it; the submitter then frees a live reservation's slot. Effect: that
   record cannot be recovered if its owner dies (the pre-fix wedge); no wrong
   discard (the position check holds).
10. **A stale cached identity now drives discards** (pre-existing stale
    identity; mine: the new consequence). A child made by a raw
    `clone(SIGCHLD)` or glibc's `clone()` runs no fork handler and keeps the
    parent thread's (pid, tid). Reproduced (`shm/exp.cpp rawclone`): the
    child reports the parent's cached pid. By reading: if that parent thread
    exits while the child holds a reservation, the tool discards the child's
    live record; if the ring then laps before the child submits, the child's
    submit clears BUSY on another record's header (a torn record) and frees
    its slot.
11. **Recovery waits for a dead main thread to be reaped** (mine, 5920d5f9,
    incomplete). `tgkill(pid, tid, 0)` succeeds on a zombie thread-group
    leader. Reproduced (`shm/exp.cpp zombie`): recover before reap 0, after
    reap 1. A parent that never waits keeps the ring wedged.
12. **Fork child of a pid-1 producer uses the parent's perf shard**
    (pre-existing: the thread's shard cache was never cleared in a fork child,
    before or after these commits). By reading: matched by (pid, tid) only,
    which a child of pid 1 that becomes pid 1 of a new namespace keeps. Needs
    root to reproduce; not run.
13. **Namespace id 0 ("no /proc") compares equal** (mine, e3e7d721,
    5920d5f9). By reading: producers in different namespaces without /proc
    share a shard by (pid, tid); a tool without /proc discards other
    namespaces' records.

### Lazy mode (opt-in research mode)

14. **The shared-mapping cache is stale after `mremap`** (mine, 632969a8).
    Only mmap/mprotect/pkey_mprotect clear it on the hooked path; mremap,
    munmap, shmat do not (the emulation path clears for mremap/munmap).
    Reproduced (`syscall/exp_mremap.cpp`): a shared memfd mremap-ed over a
    rewritten private JIT page, then called: `FILE word at 4 = 1407cc0b (NOT
    svc: THE SHARED FILE WAS PATCHED)`, shared_skipped 0.
15. **Forwarded SIGSYS lacks SA_RESTART** (mine, de09b3dc). The kernel
    decides restart from our action. Reproduced (`syscall/exp_restart.cpp`,
    tgkill SIGSYS to a thread blocked in read): native and rewritten-only
    `read -> 1`; lazy `read -> -1 EINTR` for both an SA_RESTART handler and
    SIG_IGN.
16. Lesser, by reading:
    - `app_sigsys_action` is copied and read without synchronization
      (torn action under a concurrent sigaction(SIGSYS));
    - fork from an application SIGSYS handler that interrupted a rescan
      self-deadlocks on `rescan_mutex` (newly reachable: the application's
      handler did not run before de09b3dc);
    - the refused-execve warning appears only if a later rescan happens (the
      count is in the stats).

### Minor (errno exactness, comments)

- lookup_and_delete: unsupported types ENOTSUP (95), kernel ENOTSUPP (524);
  `flags` ignored (kernel: EINVAL for anything but BPF_F_LOCK, and for
  BPF_F_LOCK without a spin lock). Reproduced (`maps/exp.c lad`).
- Creation: hash value limit ignores the key; per-CPU array value > 32 KiB
  E2BIG where the kernel gives ENOMEM; array key != 4 and queue/stack key
  != 0 accepted; types outside the switch accept what the kernel rejects
  (pre-existing).
- Fork table: when the bounded spin gives up, every later registration is
  missing too, not only that entry (comment and CLAUDE.md).
- `claim_owner_slot` now makes getpid/gettid/stat calls (first per thread,
  after an epoch bump) inside the cross-process reserve lock.
- The ringbuf WARN reads the owner after freeing the slot (may name the
  wrong thread).
- Comments contradict the code: `open_mapped_file` ("not the device"), the
  shared-mapping cache ("cleared symmetrically").
- `PR_SET_NO_NEW_PRIVS` survives a failed lazy enable (pre-existing).

## Tally

15 numbered problems plus the lesser (16) and minor items, all checked
against the code:
- reproduced here: 1-6, 8, 10 (the stale identity), 11, 14, 15, and the
  lookup_and_delete errno item;
- by reading: 7, 9, 10 (the discard), 12, 13, 16.
Almost all come from the 16 commits under review (the scope); 12 and the
stale identity behind 10 predate them.
