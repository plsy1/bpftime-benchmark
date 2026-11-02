# Per-CPU hash without the map lock (2026-11-02)

Phase 2 of `../hash-single-thread-20261102/PLAN.md` (plan: `PLAN.md`). Since cdc05f67 (10-28,
crash fix) the per-CPU hash was a Boost `unordered_map` index and every operation, lookups
included, ran under the handler's map lock: single-thread lookup 1.14x the kernel, shared
counters at 2-6 threads 0.18-0.43 us of map cost per call (kernel ~0).

## Dev commits (on 68f19c1f, branch `dev/uprobe-official-177a1dc465ff`)

| Commit | What |
|---|---|
| fbcc1a04 | `bpftime_hash_map`: `lookup_read(key, read)` (read runs after the key compare, before the bucket word is re-checked) and `with_existing_value(key, fn)` (fn under the stripe locks covering the probe; home-stripe fast path) |
| a73f6753 | per-CPU hash: keys in a `bpftime_hash_map` whose value is a u32 slot number; values (`value_size * ncpu` per slot) in a pool of lazily allocated 64-byte aligned chunks, free list under `pool_lock` (own cache line, taken only for new keys and deletes); `should_lock = false`, the handler takes the map lock only `if constexpr (should_lock)` |
| e1677f0d | CLAUDE.md: design, history, tests, `pool_lock` in the dead-holder list, memory |
| a4ac0d31 | `__attribute__((flatten))` on the ordinary hash's `elem_update` / `elem_delete` (see below) |

Memory: the key table is preallocated like the ordinary hash (about 29 MB for 1M entries of
4-byte keys); the old index grew with the keys. The unit test "Test basic operations of hash
map" now uses 1<<16 entries (1M did not fit its 20 MB segment; the first candidate gate failed
on it).

## Tests and mutation checks

| Test | Mutation | Result |
|---|---|---|
| `[value-access]` stalled reader: a delete plus another key's insert into the same bucket while `lookup_read`'s reader is blocked must give "not found" | M1: read after the re-check | fails |
| `[value-access]` `with_existing_value` waits for every stripe its probe reaches | M2: `<=` in the fast-path bound | fails |
| `[percpu-locks]` lookups and existing-key updates complete while `pool_lock` is held; a new key waits | MA: existing-key update through the pool | fails |
| `[percpu-locks]` existing-key update waits for its stripe, not for another; lookup does not wait | MB: update without the stripe lock | fails |
| `[percpu-locks]` no handler operation waits for the map lock | MC: handler keeps the lock | fails |
| slot reuse tests (`test_per_cpu_hash.cpp`) | MD: no memset of a new slot | fails |

Candidate gate (`tracker.py test-candidate`) on a4ac0d31: green (focused 157 cases, 156 passed,
1 skipped; transformer 47 with the 1 expected failure; syscall trace ok); no root-owned shm left.

## First gate on e1677f0d (`gate/`, `go.sh`, `summary.md`)

Counters (with the kernel), churn, tracking table and suite cases, dev-68f19c1f vs dev-e1677f0d;
216 + 24 + 48 runs, all ok. Per-CPU counters' map cost fell to the kernel's level, the ordinary
hash cases unchanged. Suite reruns (`suite2.jsonl`, `suite3.jsonl`, 5 reps, ns per operation):

| Case | 68f19c1f | e1677f0d | exp-flatten (= a4ac0d31) |
|---|---:|---:|---:|
| hash update | 29.50 | 30.06 | 29.05 |
| hash lookup | 22.31 | 22.25 | 22.24 |
| hash delete | 37.53 | 39.07 | 33.94 |
| per-CPU hash update | 54.51 | 34.70 | 36.24 |
| per-CPU hash lookup | 33.53 | 25.13 | 25.26 |
| per-CPU hash delete | 104.40 | 62.49 | 58.16 |

The ordinary hash got slower in e1677f0d without its code changing: with a second caller of
`bpftime_hash_map::update`, LTO stopped inlining it into `fix_size_hash_map_impl::elem_update`
and dropped the constprop clone of `elem_delete`. `flatten` on the two wrappers restores (and for
delete improves) them; committed as a4ac0d31.

Read and write helpers: `bpftime_probe_read` moved in the binary and the suite's `read` case is
4.85 instead of 4.57 ns in every run of every new build; `write` is bimodal (4.28 or 4.85) in the
new builds, 4.27 in 68f19c1f. Their code is unchanged (layout only), not chased. Full ten:
read +6.3%, write +1.0%.

## Final run on a4ac0d31 (`final/`, `summary.md`; `../dev-a4ac0d31-20261102/run-all.sh`)

18:47-19:33 JST, BTF kernel, cpu5 1728000 before and after. 216 + 24 + 48 runs, all ok.

Shared counters, per-CPU hash, us per call per thread (total, map cost = object minus control):

| Threads | kernel total | 68f19c1f | a4ac0d31 | change | kernel map | 68f19c1f map | a4ac0d31 map |
|---:|---:|---:|---:|---:|---:|---:|---:|
| 1 | 1.268 | 0.260 | 0.244 | -6.2% | 0.026 | 0.049 | 0.032 |
| 2 | 1.841 | 0.460 | 0.342 | -25.7% | 0.024 | 0.184 | 0.062 |
| 4 | 3.365 | 2.853 | 2.302 | -19.3% | -0.002 | 0.433 | 0.009 |
| 6 | 4.221 | 4.982 | 4.714 | -5.4% | 0.117 | 0.300 | -0.040 |

(NKEYS 1; NKEYS 64 the same picture: -5.7%, -23.1%, -9.4%, -3.8%.) The ordinary hash counters,
churn and tracking table: within -2.4%..+2.5% of 68f19c1f.

Suite cases (5 reps, ns): per-CPU hash update 54.61 -> 36.33, lookup 32.82 -> 25.17, delete
104.96 -> 58.22; hash update 29.49 -> 29.11, lookup 22.34 -> 22.27, delete 37.53 -> 33.91;
arrays and per-CPU arrays within +-1.2%. Against a7d54d7e (10-26, before the crash fix):
per-CPU update -18.4%, lookup -9.2%, delete -41.5%.
