# Fourth review: the third round's fixes, with a severity bar (2026-11-01, dev 6ab9cbfe)

## Scope

- **Commits reviewed**: the 16 commits from 9aeb2863 to 6ab9cbfe. These are the 15 fixes of `../refixes2/`, plus 6ab9cbfe, which put the ringbuf submit back to one exchange.
- **Reviewers**: three Fable 5.1 reviews, read-only, with experiments in a scratch directory. The experiment sources are in `experiments/`.
- **What was new this round**: a stopping rule agreed beforehand.
  - Report only high-severity defects on default paths: a crash, hang, deadlock, corruption, wrong or lost data, an unbounded leak, or a real tool that worked before and now fails.
  - Not reported: errno exactness, theoretical races that need a full ring lap, and minor issues in lazy mode.
  - If nothing meets the bar, the review rounds end.

## Result

**Nothing met the bar in any of the three areas.** The review rounds end here.

## Maps and the bpf() server

Commits d93a0f00, 06fdaa07, d1abdc50 and 6ae782c8.

- **Map creation**: a libbpf 1.7 program, run through the syscall server, created 22 kinds of map with the attributes real tools use (`maps/maps_e2e.c`). Among them:
  - memleak's hash of 1M entries;
  - per-CPU hashes with 256- and 64-byte keys;
  - libbpf's .rodata and .bss arrays, and an array with a 32 MiB value;
  - queue, stack, ringbuf, perf event array, stack trace, LPM trie, bloom filter, cgroup array, map-in-map and prog array.
- **Lookup and delete**: correct for the three hash types, and for queues and stacks with the NULL key libbpf passes.
- **Rejected creations**:
  - invalid sizes and unsupported types return EINVAL;
  - three 6M-entry hashes return ENOMEM;
  - the number of open descriptors is unchanged before and after.
- **Callers**: every creation path was checked (syscall server, daemon, kernel mirror, JSON import). None creates what the new rules reject.

## Perf output, ringbuf and identity

Commits 2839d98a, a646180d together with 6ab9cbfe, b8af155a, ce11e384 and 58622f72.

- **Ringbuf stress** (`shm/rb_stress.cpp`): 16 runs of 6 s each, 3 producers writing sequenced, checksummed records, and about 700 producers dying per run (fork-and-exit, SIGKILL, zombies, exiting threads).
  - No torn, duplicated or lost live record.
  - Every dead reservation was recovered within 2 s, and no live record was discarded.
- **Perf fork storm** (`shm/perf_storm.cpp`): about 1,600 fork children and grandchildren. Every record was delivered exactly once, every shard was reclaimed, and free shared memory returned to its starting value.
- **Shared memory full**: 4 threads output with the segment nearly full. 41,602 outputs failed and no shard was kept. Once memory was freed, 3.1M records were delivered intact.
- **Producer hot path**: no new allocation, lock or system call.

## System call rewriting

Commits 2555b56d, ae90254b, 59fecdf4, 19a872c1 and ca54fa89.

- **Reading clone3's flags directly**: a valid clone3 call cannot fault. This was checked with pthread_create, the minimal thread stack, posix_spawn, a sigaltstack handler, heap-allocated args, and clone3(0x10), which still returns EFAULT. Go uses clone, not clone3.
- **Assembly**: x2 is `sp + 0x320`, which is the application's sp. No register is clobbered and the CFI is unchanged (checked with objdump).
- **The new special calls**: munmap, mremap, shmat and remap_file_pages are now handled out of line. In the default mode this adds one atomic store and no rescan. Each call was counted exactly once (`syscall/maps_count.cpp`).
- **Fork during rescans**: forks racing dlopen and rescans gave 11,602 forks and about 20,000 children, none of them hung.
- **Real programs**: the following run with the same output as native under the default-mode preload: python3 (threads, fork, subprocess, multiprocessing), Go 1.22 (goroutines, os/exec), OpenJDK 17 (thread pool, ProcessBuilder) and node (worker_threads, execFileSync).

## Below the bar (recorded, not fixed)

- **clone3 bad pointer near the top of the stack**: a clone3 pointer the kernel would reject, lying above the top of a stack mapping whose sp is within 1 KiB of that top, gets SIGSEGV instead of EFAULT. Constructed by hand in `syscall/clone3_exp.cpp`.
- **Go threads**: every Go thread creation bumps the identity epoch, because Go does not use CLONE_SETTLS. The cost is one identity re-read per thread.
- **btrfs**: the inode-only match could accept a file with the same inode on another subvolume mounted over the path.
- **No /proc**: each process is its own namespace, so cross-process shard reclaim and recovery are off. A forking server without /proc keeps one shard per dead thread.
- **Zombie owner of another uid**: tgkill returns EPERM and the owner is taken as alive. While the head record is BUSY, every poll costs the tool a /proc read.
- **Per-CPU over 32 KiB**: the maps reviewer recalled E2BIG for a per-CPU value over 32 KiB. The v6.8 sources fetched for the third round's fixes have no such check, so ENOMEM stays.
- **Lazy mode**:
  - eight concurrent `sigaction(SIGSYS)` writers can lap the 8-copy action ring;
  - the fork-handler flags are process-wide statics shared by concurrently forking threads.
- **munmap and friends**: these calls now make one more out-of-line call and one atomic store. Not measured.
