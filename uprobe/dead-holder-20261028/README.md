# Killing a traced process can leave a map lock held forever (2026-10-28)

Problem 5 of `../dimension-problems.md`. After the per-CPU hash fix (dev `c98951a4`) the crash
that first showed it no longer happens, so this measures the trigger that remains: a traced
process killed (SIGKILL: `kill -9`, the OOM killer) while it is inside a map operation.

`kill_holder.py`: the bpftime loader (`builds/c98951a4`) attaches `count-hash.bpf.o` from
`../map-table-20261028` (hash lookup + atomic add on every call, so every call takes the map's
spinlock) to `count_event` of countloop (1 thread, 64 keys). After a random 0.3-2.0 s the
workload gets SIGKILL; then the loader gets SIGINT and reads the whole map (get_next_key +
lookup, both under the map lock). A loader still running 10 s later counts as hung.
jetson_clocks on, cpu5 at 1728000.

| Run | Trials | Loader read the map and exited | Loader hung |
|---|---:|---:|---:|
| `run1.jsonl` | 40 | 33 | 7 (17.5%) |
| `stack-run.jsonl` (stop at first hang, gdb) | 16 | 15 | 1 |

The hung loader's stack (`stacks/`): `__pthread_spin_lock` <- `bpftime_lock_guard` <-
`bpf_map_handler::bpf_map_get_next_key` <- `handle_sysbpf` (`BPF_MAP_GET_NEXT_KEY`). The map
lock is a plain `pthread_spinlock_t` in shared memory; the killed process held it, nobody
releases it, and every later user of that map (the loader, other traced processes) spins at
100% CPU forever. A probe holds the lock for a short part of each call, but at millions of
calls per second a kill lands inside it often: about one kill in six here.

The shared segment's Boost mutexes (allocator, pid set) are robust mutexes: Boost turns an
owner death into a permanent `lock_exception` on every later lock, so the segment can no longer
allocate. They are held far less often (allocations), and not measured here.

## Options (not fixed: user decision 2026-10-28, recorded only)

Detection: replace the map spinlock with a robust process-shared mutex (`PTHREAD_MUTEX_ROBUST`;
the kernel marks the locks a dying thread holds, the next locker gets `EOWNERDEAD`), instead of
spinning forever. What to do with the map then:

- disable it (operations fail, logged): safe, the tool loses that map for the rest of the run;
  PostgreSQL does the same for its whole shared memory when a backend dies;
- take the lock over and continue: safe only if every map update is crash-consistent, as in
  LMDB (robust writer mutex; a dead writer's uncommitted changes are invisible). For the
  preallocated `bpftime_hash_map`, write key and value first and publish the slot state last,
  delete with a single store; Boost-container maps (per-CPU hash) cannot, so disable those.

Not suitable: OS file locks (released on death, as SQLite uses) cost a system call per map
operation. Lock-free maps (as LTTng-UST buffers) avoid the problem but are the deferred
multi-thread redesign. Boost's own segment mutexes would need a replacement mutex family or a
PostgreSQL-style rebuild.
