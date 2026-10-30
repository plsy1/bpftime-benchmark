# Fixes for the review findings (2026-10-31)

Plan: every finding of `../README.md` fixed where possible, one dev commit and
a test each, every test checked to fail without its fix. Dev branch
`dev/uprobe-official-177a1dc465ff`, on top of 5062cb97.

## Commits

| Commit | Finding | Fix | Test (fails without the fix) |
|---|---|---|---|
| 208c99ca | Agent fork handlers never ran under dlmopen | `bpftime::register_fork_handlers` table, run from the main libc through the agent's `bpftime_agent_fork_hook` | stand-in agent in its own namespace: fork child sees its own pid (else the parent's) |
| 56b1bcf9 | Hash values misaligned, SIGBUS on atomic add | values of 8+ bytes at 8-byte offsets, elements a multiple of 8 | `[alignment]`: atomic add on every value, key 1-64 x value 4-24 |
| 6f1e73d9 | Per-CPU maps: unchecked CPU number | possible-CPU count (as libbpf), every CPU-indexed path checked | `[cpu-bounds]`: maps made for one CPU, thread on CPU 1 |
| 913870ed | Per-CPU hash, non-4-byte keys: use after free | every key size uses the value slots; delete frees nothing | blocks allocated after a delete stay intact when the stale pointer is written (old code: overwritten on every CPU, `b4-old-code/`) |
| 31d57486 | (new) Per-CPU values through bpf() not in libbpf's layout | one slot per possible CPU, each rounded to 8 bytes, converted at the copy | `[percpu-layout]`: values of 4/8/12 bytes through the userspace paths |
| d0d9adfa | Perf shards of other pid namespaces reclaimed | shards record the producer's pid namespace; reclaim only own | same-namespace dead shard reclaimed, other kept |
| eb26ba43 | Transformer exit(1), NULL dereference, stdout log | log and return; guard; debug level | preloaded shell with a missing / non-agent library: own output only, exit 0 |
| 3d7ada3d | Deleted libraries skipped; shared code written to its file | `/proc/self/map_files` for "(deleted)" (root), inode check otherwise; `s` mappings skipped | unlinked module rewritten as root; MAP_SHARED code file unchanged |
| 4bafc514 | No unwind info in the hook; FPCR not kept | CFI (return column x17, DWARF expression decoding the gate), agent unwinds once at setup and rethrows forced unwind; FPCR saved, restored if changed | cancel runs destructors (also through a try/catch hook in another namespace); backtrace from a handler reaches the caller; rounding mode kept |
| b9c849fb | Lazy mode: fork deadlock, pre-blocked SIGSYS, stack, page-end IP, not dumpable, spin | atfork for the patch lock, refuse while a thread blocks SIGSYS, SA_ONSTACK, ranges as [lo+4, hi+4), mprotect fallback, yield | one test each (hang, SIGSYS 159, SIGSEGV 139, a trap, emulated) |
| 8c108309 | Hash tokens one at a time near capacity | move a batch to the home stripe | `[tokens]` |
| f2d6ae7d | (refinement) | one unsigned compare for the CPU check | `[cpu-bounds]` |

Not fixed, documented in CLAUDE.md: the agent namespace libc's own fork
handling (its malloc locks); a cancel landing while a program runs (JIT frames
have no unwind info); `in_hook` sticking after siglongjmp out of a hook;
misaligned sp; mremap of rewritten code; hook stack use (Go); dead lock
holders (known problem 5); consumer death redelivery. The review's
"replaced file gets the new file's sections" does not happen: a file renamed
over shows "(deleted)" in the maps.

Not committed: MurmurHash3 home hashing for the hash map (fixes a 9,945 probe
bound for `id << 16` keys) cost 1.2-1.4% at 6-thread churn (4-byte keys) and
3.1% on a 900K-entry u64 address table (8-byte keys): `hash-ab/`, `hash-ab2/`.

## Verification

- Candidate gate on every step and on the final HEAD: all suites green.
  Runtime cases each in its own process (as root): 205 run, all passed except
  the 3 map-usage-counter cases (not built: skipped).
- `fork-e2e/`: statsnoop (perf output) with a parent and 4 fork children
  under the syscall-trace agent. Before (f4fdcbcd agent): all 5,710 child
  events carried the parent's pid. After: 14,827 events, every pid right;
  statfs events now arrive too (syscall table fix).
- `cancel-e2e/`: pthread_cancel of a thread blocked in a hooked read under the
  real agent: cleanup skipped before, abort with CFI alone, runs now.
- `fuse-final.*`: the fuse benchmark on the final build: getdents64 cache
  11.35x, denied open 14.68x faster than native (as before the fixes).

## Cost (cpu5 at 1728000, medians)

- Hook (`hook-perf/`): the hook alone unchanged (397.6 -> 397.4 ns per write);
  12 more user instructions per call (FPCR, execve compares), +2 user cycles
  with the agent. The wall-time medians of the agent legs moved 3-9 ns within
  their known bimodality.
- Hash map (`hash-ab2/`, the committed token change): churn and tracking, 1
  and 6 threads, within the run spread.
- Map program paths (`counters-ab/`, f4fdcbcd vs 8c108309, single thread):
  per-CPU array unchanged; hash +1 instruction (0.233 -> 0.237 us); per-CPU
  hash 0.252 -> 0.259 us (+11 user cycles, +3 instructions after f2d6ae7d);
  the control (no map) +3 cycles.
