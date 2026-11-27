# S2 rework: writer slots and the owner's claim on OFD locks (2026-11-27)

Commit 84ce9813d (local), on top of S3 (309a9e8c6). Plan: D8, D11b-d rewritten in place, section 10.

## Why

The independent read of the S2 review fixes (c84dd0ff, 0b9b02af; `review/reread-fixes.json`: two
lenses, every finding checked by a refuter) upheld 19 findings. They are one class: liveness from
(pid, start time from /proc) cannot be read together with the slot or claim it belongs to, and
cannot see exec at all.
- **Blocker (slots-1, reproduced):** a writer slot was taken by a CAS on the owner word and its start
  time stored afterwards. In between, a second claimant took the live process's slot. The seal then
  discounted a writer that was still writing, returned true, and the image missed that write; the
  writer's decrement wrapped the count to 2^64-1.
- **Exec (slots-2, owner-1, owner-8):** exec keeps the pid and the start time, so counts left by the
  threads exec killed were never discounted, and an owner that exec'd stayed the owner.
- The others: a sealer killed mid-discount left a count nothing could clear; dead in-flight slots
  were not reusable and other namespaces' slots never freed, so writers ended in the overflow slot;
  64 liveness probes per update once slots ran out (1.3 ms per update); no test reached pass 1, the
  overflow slot or the start-time match; and more (`reread-fixes.json`).

## The check first (`ofd/RULES-ofd.md`, rules written before the run)

Open file description locks (`F_OFD_SETLK`) on bytes of a /dev/shm file, the descriptor opened
`O_CLOEXEC`, on this kernel (6.8, aarch64, tmpfs). Every item passed:
1. held against other processes' tries, reported by `F_OFD_GETLK`;
2. another description of the holder's own process conflicts too;
3. released on SIGKILL with four threads running;
4. released by exec while the new image runs (same pid);
5. a fork child's copy shares the lock: closing it leaves the parent's lock; keeping it keeps the
   lock after the parent dies (so a fork handler must close it);
6. a vfork child that execs leaves the parent's lock;
7. (root) a holder that is pid 1 of its own pid namespace is seen from outside, and released when
   killed;
8. `close_range` in the holder releases it (the limit written into the plan);
9. cost: lock or unlock 715 ns, `F_OFD_GETLK` 436-444 ns (paid once per process at its first write,
   and per slot with a count per seal; never per update).

## What changed

- A writer slot (256, one per 64-byte line, plus the overflow slot) is held by the lock of its byte
  in the segment's file (found from the mapping that holds the map, opened `O_RDWR | O_CLOEXEC`;
  bytes at 2^44 + a generation-derived offset). A process takes the first slot it can lock from its
  pid's, clears what a gone holder left there, and caches the slot (a lock-free table of 1024
  entries keyed by the map object). With all slots held it writes in the overflow slot and tries
  again after 100 ms.
- The sealer probes each slot with a count through a description of its own (which conflicts with
  its own process's writers too, so there is no "self" case); taking the lock means the holder is
  gone, and it clears the count while holding the lock.
- The claim is the lock of the byte after the slots'. `still_holds_claim()` checks that the
  description still refers to the segment's file (`fstat`), for the owner to check before each
  delivery.
- A fork handler (`register_fork_handlers`) closes the child's inherited descriptions; the map's
  destructor closes this process's description for it.
- `make_image` catches an exception after the seal, unseals, gives the claim back and rethrows.
- Removed: owner records, the liveness probes and their stubs, the start-time words.

## Tests

`[gpu-devhash-cpu]`: 15 cases pass (6,603 assertions). New: "The owner's path" (the path table; claim,
cached copy, release) and "Writer slots and the owner's claim follow the processes": exec in flight
(the seal discounts the slot while `sleep` runs), a fork child of a writer killed in flight, slot
reuse with a test limit of one slot, slots exhausted (two held, a third process: overflow, one scan
per 100 ms), 200 rounds of two processes claiming at once, an owner killed, exec'd or with a forked
child, an owner whose description was closed (`close_range`), an exception between the seal and the
copy. The object-code order check (`check_devhash_orders.sh`) passes. Fork-handler and fork tests of
the runtime pass.

## Mutations (`mutate_s2_ofd.py`, `mutations-ofd.txt`: 15 of 15 caught)

| mutation | test that failed |
|---|---|
| the description opened without `O_CLOEXEC` | exec in flight |
| no fork handler | killed in flight (the writer, a fork child of the test process, wrote in its parent's slot) |
| the leftover count not cleared | slot reuse |
| the sealer probes through the writers' own description | the seal waits for a writer in flight (same process) |
| the overflow decision not cached | slots exhausted |
| a claim that always succeeds | the owner's path (another process's claim) |
| no `fstat` check | closed description |
| no catch after the seal | exception between the seal and the copy |
| a cached copy treated as a takeover | the owner's path |
| the writer skips its count | the seal waits for a writer in flight |
| no discount of dead writers | killed in flight; exit; fork child |
| `SEAL_DEAD` not stored | stopped in flight |
| `SEAL_DEAD` unconditional | stopped in flight |
| the claim kept after a failed seal | stopped in flight |
| the PHASE-1 path skips the poll | recovery from a dead sealer |

## Limits (as of this first version; superseded below)

This first version put a host that closes the agent's descriptors on the dead side. The second fix
(below) moved it to the alive side with holder hints; the current limits are README 10 and 11 in the
plan.

## Second review and second fix (11-28; 3b6842fc8, local)

A four-lens review of 84ce9813d, every finding checked by a refuter: 38 findings, 37 upheld
(`review/review-ofd-rework.json`; answers in plan section 11). The main ones:
- a host closing the agent's description (the limit the first version put on the dead side) left
  every later write in a slot whose lock was free, and `still_holds_claim()` could be fooled by
  the number coming back as another description of the same file;
- a description could land on fd 0-2;
- a vfork-style child's first write recorded its own description in the parent's table, and the
  parent then wrote with no lock (reproduced: the seal discounted the parent's live writer);
- the fork handler, destructor and new-map path could close the host's files.

The fix:
- holder hints (pid namespace << 32 | pid) and an image nonce in each slot and for the claim. A free
  lock whose holder still runs and maps the segment counts as held. A free slot of this pid but
  another image (an exec) is cleared by the new image.
- the description itself must hold its lock (`/proc/self/fdinfo`); a process takes its slot and
  claim back after losing them.
- descriptions at 3 or above; raw system calls; writers only try the process-local mutex.
- numbers closed only while they name the segment's file; vfork-style children record nothing.
- the seal checks each slot for zero.

Tests: 20 cases (6,689 assertions), each run twice; the order check passes.

Mutations of the second fix, run on 6a2850c0b (before the image nonce, the try-lock and the per-slot
check; `mutate_s2_ofd2.py`, `mutations-ofd2.txt`; baseline run first): 19 of 20 caught; the one missed
("own slot cleared by its own rescan") showed the exec case above. The final code (3b6842fc8: the
nonce, the try-lock, the per-slot check, task maps for a holder whose main thread exited, the vfork
check before the mutex) has its own run: `mutate_s2_ofd3.py`, `mutations-ofd3.txt` (below).

| mutation | caught by |
|---|---|
| hint ignored by the discount | a writer whose description the host closed is still counted |
| hint ignored by a scanning writer | the same (the second writer's scan) |
| hint ignored by a claimant | a free claim of an owner that runs is not taken over |
| the file alone taken for the description | an owner whose description the host closed takes its claim back |
| a description at fd 0-2 kept | the description never takes a standard stream's number |
| numbers closed without checking | a fork child leaves a host file at the description's old number |
| a vfork child records in the table | a vfork child's first write leaves the parent's table alone |
| description without `O_CLOEXEC` | exec in flight |
| no fork handler | killed in flight (a fork child of the test process) |
| the leftover count of a gone holder not cleared | slot reuse |
| the overflow decision not cached | slots exhausted |
| a claim that always succeeds | two processes claiming at once |
| no catch after the seal | an exception between the seal and the copy |
| no discount of dead writers | killed in flight |
| no discount in the `SEAL_DEAD` branch | after `SEAL_DEAD`, a stalled writer that dies is discounted |
| no periodic discount | a writer killed while the sealer polls |
| the overflow slot left out of the seal | a writer in flight in the overflow slot |
| the destructor closes nothing | the destructor closes this process's description |
| a stale description kept at a new map's address | a new map at a destroyed map's address |
| an earlier image's count kept (run by hand) | an exec'd image of a writer clears what its earlier image left |

Not covered by a test: writers waiting on the mutex (only reachable from a probe or a signal handler
inside these paths), the seal's per-slot zero check (needs a wrapped count), a seccomp filter
refusing the locks.

## Final code (9bb80ba16)

The final fix adds the image nonce, the try-lock for writers, the per-slot zero check, task maps
for a holder whose main thread exited (found by an advisor's read: `/proc/<pid>/maps` is empty then,
checked on this kernel), the vfork pid check before the mutex, and a fork from inside these paths
that does not wait for its own mutex. Tests: 21 cases (6,702 assertions).

Mutations on 3b6842fc8 (`mutate_s2_ofd3.py`, `mutations-ofd3.txt`, baseline first): 19 of 22 caught.
- "a claim that always succeeds" survived: the holder-hint guard alone kept the second claimant out.
  A new case, "The claim's lock alone keeps a second claimant out" (a test-only clear of the hint, as
  for an owner of another pid namespace), catches it (run by hand on 9bb80ba16).
- "a vfork child takes the mutex and the table" and "a vfork child records in the table" each
  survived alone: the two checks cover each other. Both removed together, the vfork case fails (run
  by hand).

The narrow independent read of the final code (stop rule: a wrong image, two owners, a hang) did
not run: the agents hit the account's weekly limit (resets 2026-11-30 06:00 JST). To run then.
