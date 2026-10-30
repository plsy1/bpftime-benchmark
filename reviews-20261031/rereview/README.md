# Second round of independent reviews (2026-10-31, dev f2d6ae7d)

The same three Fable 5.1 reviews as `../README.md`, rerun on dev f2d6ae7d
(after the 12 fix commits in `../fixes/`). Same rules: only the code, the
requirements and the platform; read-only; standalone experiments in a scratch
directory (sources in `experiments/`). Every finding below was checked again
here; "reproduced" means the experiment was rerun independently on the same
build (`build-jetson-daily`, built from f2d6ae7d).

## Verdict on the core

- Hash map: no defect in the concurrent core. ~210M lookups with torn-key
  tests on key sizes 5/13/33, ~15M present-key lookups under churn and
  capacity waves across forked processes: no false match, no miss, tokens
  conserved (20000/20000). The new value layout is right for key 1..72 x
  value 1..40.
- Syscall rewriting: the gate/hook encodings, the 784-byte frame, FPCR, the
  CFI (every FDE row checked, gdb unwinds through the hook), the forced
  unwind through the agent namespace and the fork hook chain held up. The
  existing `[lazy]` tests pass.
- Shared memory: the perf SPSC protocol, the futex wakeup, ringbuf wrap and
  submit ordering, per-CPU CPU checks, padding, slots, lock coverage, and the
  fork handlers' child actions held up.

## Problems found

"Mine" marks problems introduced or left half-done by the fix commits.

### Lazy mode (opt-in research mode)

1. **dlopen racing fork deadlocks the process** (mine, b9c849fb).
   Reproduced (`syscall/deadlock.cpp`): lazy mode stalls within 3 s; without
   lazy, 20 s of 11k forks and 21k dlopens.
   - The rescan takes `rescan_mutex` and then the patch lock.
   - The patch lock's fork handler is registered later, so fork's prepare
     takes it first and then `rescan_mutex`.
2. **The application's own SECCOMP_RET_TRAP filter crashes the process.**
   Reproduced (`syscall/appfilter.cpp`): with no tracing and in the default
   mode the process dies of SIGSYS (159), as it should; in lazy mode it
   overflows the stack (139).
   - `on_sigsys` takes every seccomp trap as its own and rewrites the hook's
     own `svc` (`syscall_addr`) into a branch to a gate, so the hook recurses.
   - Also, the application's own SIGSYS handler is never run.
3. **Shared executable mappings are written to their file** (mine,
   3d7ada3d covered only the default mode). By reading:
   - the lazy path writes through `/proc/self/mem` with no `s` check;
   - for a writable `MAP_SHARED` code mapping, the branch to a
     process-private gate lands in the file and in every process mapping it.
4. **Logging from a possible signal handler** (mine, b9c849fb). By reading:
   - `before_hooked_syscall` calls `SPDLOG_WARN` on the first hooked call
     after a refused execve in the emulation path;
   - if that call is made from a signal handler that interrupted malloc, the
     thread deadlocks.

### Perf output, ringbuf

5. **Shards are matched by (pid, tid) only** (mine, d0d9adfa recorded the pid
   namespace but does not compare it). Two producers in different pid
   namespaces with the same numbers (containers sharing /dev/shm) write one
   SPSC ring.
   - Reproduced with two threads given the same identity
     (`shm/same_key_producers.cpp`): one shard, 373k torn samples delivered
     as valid, LOST counts of garbage, head < tail errors.
   - The namespace trigger itself needs root and was not run.
6. **posix_spawn/vfork children share the parent thread's TLS identity
   cache.** Reproduced (`shm/vfork_identity.cpp`):
   - a child that outputs first fills the cache with its own pid/tid;
   - the parent keeps writing to a shard keyed to the dead child, and the
     shard is reclaimed and recreated (generation 2);
   - in the window between reclaim and erase, the parent writes into freed
     memory.
   - The same cache makes `bpf_get_current_pid_tgid` report the child's tid
     for that parent thread.
7. **A producer that dies between `bpf_ringbuf_reserve` and submit wedges
   the ring forever.** Reproduced (`shm/ringbuf_wedge.cpp`): 0 of 10 later
   records delivered, `has_data` 0, no error anywhere. The kernel cannot have
   this.
8. **Shards of other pid namespaces are never reclaimed.** By reading:
   - shared memory grows with each short-lived foreign thread;
   - a Boost `bad_alloc` from the shard list escapes the helper into the
     traced process (no catch).
9. Lesser, by reading:
   - drain order starves later shards when the consumer ring is full;
   - tools that use `perf_buffer__consume()` without epoll never trigger a
     drain;
   - a remap of the consumer buffer reallocates it under libbpf.

### Maps and the bpf() server

10. **Possible-CPU count** (mine, 6f1e73d9/31d57486).
    - bpftime counts possible CPUs as highest + 1; libbpf counts the set bits
      (`libbpf.c` `libbpf_num_possible_cpus`).
    - With a sparse mask (`0,2-3`) bpftime copies 4 values into libbpf's
      3-value buffer: a heap overflow in the tool, and values shifted between
      CPUs.
    - Contiguous masks, as on this Jetson, are unaffected.
    - The test "Possible CPUs are counted as libbpf does" asserts the wrong
      definition.
    - Indexing by `sched_getcpu()` still needs highest + 1; only the bpf()
      layout must pack the set CPUs.
11. **`BPF_MAP_LOOKUP_AND_DELETE_ELEM` ignores the key.** By reading
    (`syscall_context.cpp:621`):
    - it is always a queue/stack pop, so hash and per-CPU hash return
      ENOTSUP;
    - the layout code of 31d57486 for this call is only reachable from the
      unit test.
12. **Size arithmetic overflows at map creation.** Reproduced:
    - per-CPU array: 32-bit `max_entries * ncpu * value_size`; for
      65536 x 1 x 65536 the data size is 0, and lookups return addresses off
      a null base (`shm/percpu_array_overflow.cpp`);
    - hash: `_num_buckets * _elem_size` wraps in 64 bits; created with
      97 MiB, the first lookup gets SIGSEGV (`hash/exp_overflow.cpp`);
    - degenerate parameters are accepted where the kernel returns EINVAL:
      `max_entries` 0, `key_size` 0 (every key matches one element),
      `value_size` 0.
13. **A fork during `register_fork_handlers` leaves the child spinning on its
    next registration** (mine, 208c99ca). By reading:
    - `reserved` is bumped before `published`;
    - lazy registrations exist (array binding, map usage stats).
    - Low likelihood.

### Already documented, worth weighing again

- The agent namespace libc's own fork handlers (malloc arena locks) never
  run, so a child can hang at its first agent allocation. The review
  suggests calling that libc's private `__run_prefork_handlers` /
  `__run_postfork_handlers` (dlvsym GLIBC_PRIVATE) from the fork hook.

### Minor (reviewers' notes, not checked further)

- Lazy mode:
  - gates crossing a 4 KiB page are refused on the mprotect fallback (one
    slot in 128 wasted);
  - the site read has no readability check (only matters with Armv8.7
    EPAN);
  - SIGSYS stays installed if the filter install fails.
- svc-like words in literal pools inside executable sections are still
  matched (1 in 65536 random words).
- `open_mapped_file` checks the inode but not the device.
- `run_callbacks` is noexcept: a forced unwind through an interpreter
  backend aborts.
- Per-CPU hash: `flags` is compared in full on the bpf() path but masked on
  the program path.
- Hash map:
  - near capacity, up to 63 tokens park per stripe, then an insert takes
    every lock;
  - the 4-byte hash spreads small u32 keys over about 8160 home buckets.
