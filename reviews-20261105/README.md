# Fifth review: the 32 commits after the review rounds (2026-11-05, dev 94f9ba8f)

## Scope

- **Commits reviewed**: 6ab9cbfe..94f9ba8f, everything since the fourth review
  (`../reviews-20261031/rereview3`, which ended at 6ab9cbfe): the hash map and
  per-CPU hash rewrite (11-02 to 11-04), the aarch64 hook's own stack, the
  Frida detach/parking work, the stack trace map, the ringbuf tag, the logger at
  exit, libbpf poison calls, and the rodata dead-code pruning.
- **Reviewers**: five Fable 5.1 subagents at xhigh effort, one per area, read-only,
  experiments in a scratch directory. Each got the code, the commit range and
  CLAUDE.md, not the author's opinion. The bar: defects with a concrete failure
  scenario (high: crash, hang, corruption, wrong or lost data on a default path,
  or a tool that worked before and fails; medium: the same on non-default paths
  or unusual but realistic conditions, unbounded leaks, documentation claiming a
  guarantee the code does not give). Known open issue excluded: a ringbuf submit
  does not wake the consumer (`../uprobe/opensnoop-loss-20261105`).
- **Verification**: every high and medium finding was checked against the code at
  94f9ba8f; the ones marked "re-run" were reproduced again by the author on this
  machine with the reviewers' experiments.
- Reports: `report-<area>.md` (verbatim). Experiments: `experiments/<area>/`
  (sources, build scripts and logs; binaries not kept).

## Result

| # | Severity | Area / commit | Finding | Checked |
|---|---|---|---|---|
| H1 | high | Frida, 25f4134f | Detaching an override while threads call the function kills the process: the replacement handler walks the entry's live `user_attaches`, which `detach_by_id` shrinks before publishing the new dispatch; finding no override it throws out of Gum's replacement (std::terminate / SIGILL). | re-run: 2/2 SIGILL |
| H2 | high | transformer, 7d9690c3 | The hook clears `busy` before it reads its frame back. A signal handler on an alternate stack (SA_ONSTACK, e.g. every Go handler) that makes a hooked call in those ~22 instructions builds its frame over the live one: the interrupted thread hangs, or continues on the signal stack. | re-run with the shipped dev-b60b99c3 transformer, plain program: stalled |
| M1 | medium | transformer, 7d9690c3 | `busy` stays set after a vfork-style child (posix_spawn, system, Go os/exec) execs or exits, and after a siglongjmp out of a handler: the thread never uses the hook stack again (the problem the commit fixed comes back). | re-run: 32 -> 944 bytes below sp after posix_spawn |
| M2 | medium | transformer, 7d9690c3 | Hook stacks (two mappings each) are never reclaimed; a process that keeps creating threads exhausts `vm.max_map_count` (stock 65530: ~32k threads), then its own mmap/pthread_create fail. | reviewer's measurement (16,384 threads: 84 -> 32,854 mappings); consistent with "never given back" |
| M3 | medium | Frida, 25f4134f | A function starting with `b target` (thunk) is hooked by Gum at `target`; parking `target`'s slot silences probes still attached through the thunk. | re-run: thunk probe no longer fires |
| M4 | medium | per-CPU hash, a73f6753 | `BPF_NOEXIST` on an existing key takes a slot (the global pool lock) before finding the key; with a full map and in-flight inserts it returns E2BIG where bpf returns EEXIST. The header and CLAUDE.md say existing-key updates never take the pool lock. | by reading (`per_cpu_hash_map.cpp:133-147`); the pre-a73f6753 code checked first |
| M5 | medium (docs) | hash map, d16fcbf5/fbcc1a04 | CLAUDE.md says a `lookup_read` copy is always whole; on 4-byte keys it is only with acquire-load readers (the header says so). The one in-tree reader complies. | by reading (`bpftime_hash_map.hpp:588-596`, CLAUDE.md:667) |
| C1 | concern, not a regression | hash map spin lock | The stripe spin lock is unfair: a thread updating one stripe ~4M times/s starves another writer of that stripe for 10-100+ ms. pthread_spin_lock (before the range) is worse. | reviewer's measurement |

Low findings (details in the reports):

- per-CPU: a process killed between taking a slot and inserting loses the slot
  (not documented for the per-CPU hash).
- hash map: a tool killed or faulting inside `next_key`/`lookup_and_delete`
  leaves a stripe locked (`lookup_and_delete` copies into the caller's buffer
  under the lock); test gap on the memcmp replace path; stale "single probe"
  wording; a test leaves `/dev/shm/bpftime_hash_map_test`.
- Frida: grace-period `enter()`/`leave()` not signal-atomic on the override path;
  the unaligned-literal park relies on hardware behaviour before LSE2;
  `set_parked` state can go out of step on an mprotect failure; `synchronize()`
  waits without bound and the fork prepare handler can deadlock with it;
  membarrier fallback asymmetry; stale slots if Gum's interceptor were ever
  disposed (not reachable now); the agent frees programs after a partially
  failed detach (pre-existing).
- load time: libbpf's third poison form (CO-RE relocation failure,
  `call 0xbad2310`, seen in memleak's tracepoint programs) is not handled;
  kfunc calls in dead code still disable the program; AOT objects from rewritten
  programs reference private slots the AOT loader does not register; a frozen
  map still accepts push/pop/lookup_and_delete and a writable mmap.

No defect found in: the ordinary hash map's lock-free lookup, replace-in-place,
free lists and update flags (~500M cross-process lookups, 0 misses, 0 torn);
the per-CPU lock-free lookup and slot pool; the stack trace map's lock-free find;
the ringbuf record tag; the rodata reachability analysis (41 hand cases and a
differential fuzz against the LLVM JIT over 1,050 programs: 0 contradictions);
the logger at exit; parking of aligned literals and `b` redirects, the grace
period itself, fork handling.
