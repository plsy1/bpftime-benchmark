# Per-CPU hash without the map lock: plan (2026-11-02)

Phase 2 of `../hash-single-thread-20261102/PLAN.md`; problem 1/8 leftovers of `../dimension-plan.md`.
User decision 11-02: do it (multi-thread item chosen over the Frida counter).

## Why

`per_cpu_hash_map_impl` is a Boost `unordered_map` index (rehashes, shared node and slot free
lists, per-CPU key scratch buffers), so since cdc05f67 (10-28, crash fix) every operation runs
under the handler's map lock (`should_lock = true`).

- Single thread (suite, 68f19c1f): lookup 33.5 ns = 1.14x kernel (a7d54d7e 27.7 without lock),
  update 54.3 (44.5), delete 104.6 (99.6).
- 6 threads (shared counters): map cost 0.25-0.56 us per call, kernel ~0.

## Design

Keys in a `bpftime_hash_map` (the ordinary hash's open-addressing table: lock-free lookups,
striped writers, tokens = max_entries) whose 4-byte value is a **value slot index**; the values
(`value_size * ncpu` bytes per slot, the kernel's per-element layout) stay in a separate pool,
as now: chunks allocated as slots are first used (memory as today, not preallocated), slots
never freed back to the segment (a program's stale value pointer writes into a slot, as with the
kernel's preallocated htab).

- **Pool**: fixed-size chunk pointer table (`ceil(capacity / slots_per_chunk)` entries, set at
  construction) + `free_next` array (capacity entries); free list head, slot count, a spinlock,
  on a cache line of their own. Capacity = max_entries + ncpu (in-flight inserts that lose a race
  hold a slot briefly). Taken only by inserts of a new key and by deletes, never by lookups or
  updates of an existing key. A chunk pointer is written once, under the pool lock, before any
  slot in it is handed out, and never changes.
- **Lookup (program)**: lock-free table lookup that reads the slot index **inside** the
  seqlock validation (new `bpftime_hash_map::lookup_read(key, reader)`: the reader runs after the
  key compare and before the word re-read, with acquire loads), then
  `chunk[slot >> shift] + (slot & mask) * stride + cpu * value_size`. Reading the slot after the
  lookup returned would race a delete + re-insert of the bucket and hand out another key's value.
- **Update (program)**, kernel percpu htab semantics (this CPU's value only; a new key zeroes the
  other CPUs, `pcpu_init_value`):
  1. unless NOEXIST: `modify_existing(key, fn)` (new; under the stripe locks covering the key's
     probe bound, fast path = home stripe only) writes this CPU's value into the key's slot;
     found -> 0; EXIST and not found -> ENOENT;
  2. take a slot (none -> E2BIG), zero it, write this CPU's value, `update(key, &slot,
     no_exist)`: ok -> 0; full -> give the slot back, E2BIG; exists -> give it back, NOEXIST ->
     EEXIST, ANY -> back to 1 (another thread inserted it meanwhile).
  An existing key's value is written under its stripe lock, so a concurrent delete cannot free
  (and another insert reuse) the slot while it is being written.
- **Delete**: `elem_delete(key, &slot)` (copies the slot index under the stripe lock), then give
  the slot back.
- **bpf() paths**: lookup copies to libbpf's layout (or returns the slot when the layout is
  packed, as now); update = the same steps with all CPUs' values; lookup-and-delete = delete
  with slot out, copy the values out, then give the slot back; get_next_key =
  `bpftime_hash_map::next_key` (bucket order, missing key -> first key, as the hash map).
- **Handler**: `should_lock = false`; the per-CPU-hash entry points that lock unconditionally
  (`map_lookup_percpu_hash`, `map_update_percpu_hash`, `diagnostic_direct_*`,
  lookup-and-delete) lock only `if constexpr (should_lock)`.
- Kept: kernel flags (EEXIST/ENOENT/E2BIG), CPU bounds (`valid_cpu`), libbpf layout, value
  pointers valid while the key stays, `check_map_attr` (the table's buckets are preallocated as
  for HASH: 2 x max_entries x (4 + key + 4); a per-CPU hash with 256-byte keys and 100,000
  entries = 53 MB of buckets, within the default 128 MB segment).
- Dropped: the Boost index, the per-CPU key scratch buffers, the per-CPU-hash
  `BPFTIME_MAP_PRODUCTION_AB` diagnostic modes (they measured the Boost index; off by default).

Cost to expect: lookup = hash lookup (22.3 ns in the suite) + slot load + chunk pointer + CPU
number; the pool lock only on insert/delete of new keys (per-CPU churn from many threads would
contend on it; per-CPU hashes are used for counters, not churn).

## Commits (dev, each with a test that fails without it)

1. `bpftime_hash_map`: `lookup_read` (reader inside the validation) and `modify_existing`
   (callback under the stripe locks), elem_lookup unchanged in code size.
   Tests: a reader racing delete/re-insert of the same bucket with another key only ever copies
   the key's own value (mutation: read after the re-check; report if the Orin cannot catch it);
   `modify_existing` waits for a held stripe lock beyond the home stripe and not for others
   (test hooks as `[home-stripe]`; mutation: fast path past the stripe).
2. `per_cpu_hash_map_impl` on the table + slot pool, `should_lock = false`, handler locks
   conditional. Tests: a lookup and an update of an existing key complete while the pool lock is
   held (mutation: lookup takes the pool lock); an existing-key update waits for its stripe's
   lock (mutation: write through a lock-free lookup); the existing per-CPU suites
   (`[percpu-hash]`, `[percpu-layout]`, `[cpu-bounds]`, concurrency, value slots, map-create).
3. CLAUDE.md.

## Gate

- Unit: per-CPU, hash, concurrency tags, then the candidate gate (root, each case alone).
- `run_hash.py` suite cases (a7d54d7e, 68f19c1f, new head; 3 reps): per-CPU lookup/update/delete,
  ordinary hash unchanged.
- Counters (`run_count.py`): per-CPU hash and hash, NKEYS 1/64, threads 1/2/4/6, against
  68f19c1f and kernel; churn and tracking table (hash, must be unchanged).
- Then the full ten-round suite and the ssl-nginx fair round; push and the daily list only after
  asking.
