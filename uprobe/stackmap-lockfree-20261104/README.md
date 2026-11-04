# Stack trace map found without the lock, dev efe08cbd (2026-11-04)

Problem 8 of `uprobe/vs-pre-mt-20261104`: `bpf_get_stackid` took the stack
trace map's spin lock on every call (a boost `unordered_map` of shared-memory
vectors filled through one shared `key_buf`), and memleak on six threads
allocating from one stack spent 42% of its cycles spinning on it. dev efe08cbd
(on ff5515d8, local, not pushed) finds a stored trace without the lock.
Frozen build: `uprobe/ssl-nginx-2x2-20261023/builds/dev-efe08cbd`.

## Change

- Each id has a slot holding the offset of its record (hash, frame count,
  frames). A record is allocated under the lock on the slot's first fill and
  kept until the map is destroyed (a delete clears the count). Not
  preallocated: bpftrace's `ustack` map is 131,072 x 1,016 bytes (bpftool on
  bpf), more than the default 128 MB segment.
- Writers (under the lock) clear the count, write frames and hash, store the
  count last (release). `find_stack_trace` loads the count (acquire), compares
  the hash, then the frames, and returns only a match; anything else takes the
  lock and `fill_stack_trace` decides as before (`-EEXIST`, REUSE, FAST_CMP,
  zero padding; ids unchanged).
- No frame after the skip is `-EFAULT`, as the kernel (it stored an empty
  trace); a failed record allocation is `-ENOMEM`.

## Tests

- "Stack trace lookups without the lock find stored traces only" (single
  thread: hit, miss, other trace, truncation, padding, delete, `-EFAULT`).
- "Stack trace lookups without the lock never take a mix of two traces for a
  third": one bucket, A and B differ in the first and last of 127 frames, H
  has A's first and B's last; one writer replaces A with B and back under a
  lock, four readers look H up without it for 2 s (~11M lookups, ~2M
  replacements). Passes 5/5; with the hash comparison removed from the
  lock-free lookup it fails 3/3.
- `get_stackid returns errors as negative 64-bit values` also finds the
  stored trace again and checks `-EFAULT`.
- Gate (`gate.log`): runtime 171 cases, 170 passed, 1 skipped (two new); Frida
  19/19; text segment transformer 49/50 (1 fails as expected); the rest pass.

## memleak (`run.sh dev-efe08cbd`)

Audit, single thread: 1,026 outstanding blocks (bpf 1,027, as ff5515d8), no
stack errors or lookup failures.

Threaded workloads, 3 rounds x 10 s, ops/s summed over threads, median
(`measure-mt/`), no errors:

| threads | bpf | dev-ff5515d8 | dev-efe08cbd | ff5515d8/bpf | efe08cbd/bpf | efe08cbd/ff5515d8 |
|--:|--:|--:|--:|--:|--:|--:|
| 1 | 328k | 430k | 651k | 1.31 | 1.99 | 1.52 |
| 2 | 475k | 652k | 857k | 1.37 | 1.80 | 1.31 |
| 4 | 570k | 470k | 698k | 0.82 | 1.22 | 1.48 |
| 6 | 682k | 555k | 537k | 0.81 | 0.79 | 0.97 |

- One thread is 1.52x faster too: the lock, two vector copies and a
  malloc/free per call are gone.
- Six threads do not gain: the profile (`profile/`, perf.data kept on the
  Jetson) moved from the stack trace map's lock
  (`bpftime_spin_lock_outlined` 40.7% + `__aarch64_swp4_acq` 24.6% on
  ff5515d8) to Gum: `_gum_function_context_begin_invocation` 68.8% and
  `end_invocation` 14.9%, hot on an `ldaxr`/`add`/`stlxr` loop (the
  function context's usage counter; problem 2 of `uprobe/realtools-20261104`,
  Frida's code, LL/SC atomics). The map lock is gone from the profile
  (`__aarch64_swp4_acq` 4.1%).
- Not tested: bpftrace `ustack` on bpftime, x86 (NUC).
