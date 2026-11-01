# Hash maps' single-thread cost: research and plan (2026-11-02)

Plan step 7 / problem 8 of `../dimension-plan.md`. Attribution in
`../dev-6ab9cbfe-20261101/hash-bisect`.

## Research (measured on dev 6ab9cbfe, cpu5 1728000)

Tools: the suite's userspace leg (`run_hash.py`, all 17 cases), and a focused victim
(`focus-runner/`, `focus-src/test_focus.c`: only the hash update, delete or per-CPU loops)
under `perf stat`, counters per operation = (52,000 - 2,000 iterations) / 50M operations.
Experiment builds in `../ssl-nginx-2x2-20261023/builds/exp-*`, `wip-*`; diffs in `exp/`.

| Finding | Evidence |
|---|---|
| R1. Lookup +4 ns is the acquire fence (`dmb ishld`) after the key compare. An acquire load of the four-byte key orders the re-read the same way. | `exp/exp.jsonl`: lookup 27.5 -> 22.3 ns (a7d54d7e: 23.0) |
| R2. Update and delete +15 ns are instructions, not memory: update 191 -> 273 instructions per operation, cycles 47 -> 74, L1D/TLB refills unchanged (~0.03). The extra work is the range machinery: bound read before locking, `range_for`, lock and unlock loops, re-check and retry, wrap checks. Most operations need only their home stripe. | `stat/upd-*`, `stat/del-*` |
| R3. Each lock goes through a PLT call, glibc's `pthread_spin_lock` and its out-of-line atomic. Taking the same word inline (glibc's protocol) saves 1-4 ns. An inline `swpa` is no faster than the out-of-line LSE call; an `ldaxr/stxr` loop is 5 ns slower. | `exp/exp.jsonl` (exp-lock), `stat/f3-*` |
| R4. Reading the bound relaxed before locking: no effect. Dropped. | exp-bound |
| R5. Key and value copies call `memcpy` (size known at run time): inlining 4 and 8 bytes saves 15 instructions, 1 ns per update. | `stat/f2-*` |
| R6. Per-CPU hash: what remains after R3 is the map lock itself (lookup 34 vs 28 ns before the lock). The lock is needed because the Boost index rehashes and its free lists are shared; only moving the per-CPU hash onto a table that lookups can read without a lock removes it. | `exp/`, `stat/f3-pcpu-*` |

Prototype of R1-R3 + R5 (`wip-fast2`, `wip-swpa`): update 42.8 -> 29.5 ns (192 instructions,
as before striping), delete 43.6 -> 37.2, lookup 22.3, per-CPU update 57.8 -> 53, lookup
34.9 -> 33.6. Delete's remaining +6 ns over 9c3e0345 is spread over the token and
displacement bookkeeping (no hot spot); not pursued.

## Plan

### Phase 1: hash map and locks (dev commits, each with a test that fails without it)

1. **Inline spin lock** (`bpftime_spin_lock` / `bpftime_spin_unlock` in `map_common_def.hpp`,
   glibc's protocol on the `pthread_spinlock_t` word, other libcs keep pthread) for the hash
   map stripes and the handler's map lock (`bpftime_lock_guard`).
   Test: the lock excludes against itself and against `pthread_spin_lock` on the same word
   (threads incrementing a plain counter; `pthread_spin_trylock` fails while held).
   Mutation: an exchange that does not wait -> counter test fails.
2. **Lookup**: acquire load of the four-byte key instead of the fence (other key sizes keep
   the fence). Test: the existing torn-key test ("hash lookups never match a key being
   overwritten"). Mutation: relaxed key load and no fence (report if the test cannot catch it
   on this machine).
3. **Single-stripe fast paths** for update and delete (`update_in_home_stripe`,
   `delete_in_home_stripe`: the probe bound, an insert's free bucket, and delete's next
   bucket all in the home stripe without wrapping; otherwise the general path, unchanged), and
   inline 4/8-byte copies. Test: deterministic, with a test hook that holds one stripe's lock
   from another thread: an update whose free bucket lies in the next stripe, a delete whose
   next bucket does, and an operation at the end of the last (partial) stripe must wait for
   that lock; a fast-path operation inside its stripe must not. Plus the existing concurrency
   tests. Mutation: let the fast path write past its stripe -> the wait test fails.
4. **CLAUDE.md**: the fast paths, the lock, the fence; costs.

Gate after phase 1:
- `run_hash.py` (suite cases): dev-6ab9cbfe, a7d54d7e, new head, 3 reps.
- Multi-thread must not lose: churn, tracking table (1K, 900K), counters, 1 and 6 threads,
  against dev-6ab9cbfe (`map-threads-20261028/run_churn.py`, `map-table-20261028/run_track.py`,
  `run_count.py`).
- Unit tests: hash, per-CPU, concurrency tags, then the candidate gate.

### Phase 2: per-CPU hash without a lock on lookups (decide after phase 1)

Move `per_cpu_hash_map_impl`'s storage onto `bpftime_hash_map` with a value of
`value_size * cpus` bytes (the kernel's layout per element), so lookups take no lock and
updates use the stripes; drop the handler's map lock for it (`should_lock = false`). Needs an
update variant that writes only the current CPU's value and zero-fills the others for a new
key (kernel: `pcpu_init_value`), and the `bpf()` paths (all CPUs, libbpf layout) on top.
Keeps: kernel flags, E2BIG, value pointers valid while the key stays, CPU bounds checks.
Tests: the existing per-CPU suites, `[percpu-layout]`, `[cpu-bounds]`, concurrency, plus a
lookup racing inserts. Larger change (the 606-line implementation); done only if phase 1
leaves per-CPU hash lookup above the kernel and the code stays reviewable.

### Phase 3: end to end and other machines

- Full ten-round uprobe suite and the ssl-nginx fair round on the new head (as
  `../dev-6ab9cbfe-20261101`).
- Then ask the NUC session (`bpftime-x64环境`) to run ssl-nginx and the uprobe suite on the
  same commit (x86 results only on x86-nuc branches).
- Results here; push and the daily patch list only after asking.
