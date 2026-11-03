# Step 2 design: element pool + u32 chains (Candidate A)

Replaces `bpftime_hash_map` (open addressing, linear probing) keeping its public surface used by
`fix_size_hash_map_impl` and `per_cpu_hash_map_impl`: `elem_lookup`, `lookup_read(key, read)`,
`with_existing_value(key, fn)`, `update(key, value, mode) -> ok|exists|missing|full`,
`elem_update`, `elem_delete(key, value_out)`, `next_key`, `bucket_count`, `get_elem_count`,
`get_index_of_value`; test accessors `stripe_of_key`, `lock_stripe_for_test`,
`unlock_stripe_for_test`. Per-CPU hash internals and the JIT stay untouched.

## As built: where the code differs (review of e5ba4aa7, 11-03)

- Lookup step 6 has no per-step re-check: a stale `next` leads to a free element (not in use:
  restart), a terminator that names another bucket or no bucket (restart), or into another
  chain that ends in another bucket's terminator (restart); a key's element is only in its own
  chain, so no false hit. The re-check cost a fence per step and no test could tell it apart.
  Test (a) below stalls the reader after the key compare (`lookup_read`) instead.
- Delete order is reversed (dev 199e23ec): unlink first (release), then the new generation
  (release). Word first left a deleter stopped between the two stores with an element in its
  chain not in use, and every lookup reaching it restarted (forever after a death).
- Buckets use a per-map random seed (dev 48767d88): XORed into 4/8 B keys, the start of the mix
  for longer ones (the kernel's `hashrnd`).
- Capacity is exact only without concurrent deletes (the `pcpu_freelist` window); a free list
  whose holder died hangs inserts that run out of their own list (pass 1 waits for each lock).
- Updates of an existing key take the stripe's spare element (one per stripe, outside
  max_entries, under the stripe lock), link it at the head and retire the old one as the new
  spare, as the kernel's `extra_elems` (dev 518a6b99); they wrote in place before, and readers
  saw torn values.

Details: `../dev-e5ba4aa7-20261103/REVIEW.md`.

## Layout (one shared-memory buffer, offsets only, resolved once per operation)

- `heads`: u32 per bucket, `n_heads` = power of two >= max_entries (one constant, so 2x can be
  measured). A head holds an element index, or a terminator `NULLS | bucket` (bit 31).
- `elems`: max_entries elements `{u32 word, u32 next, key, pad, value, pad}`; value at
  `round_up(8 + key_size, A)`, element size a multiple of A (A = 8 for values >= 8 B, else 4), the
  buffer 16-aligned: every value of 8 B or more 8-aligned (`[alignment]`, BPF atomics).
  `word` = generation (high bits) | state (in use / free).
- `stripes`: spinlocks, one cache line each, over runs of heads (power of two, at least 16 heads,
  at most 4096 stripes). Writers lock the key's head stripe only: one lock per operation, no
  ranges, no retries, no probe bound.
- Allocator: see below. Free elements are the capacity: an empty allocator means E2BIG (as the
  kernel's prealloc freelist, `hashtab.c` ~1023), so the step-1 count goes away.

## Hash

`bpftime_hasher::hash_func` keeps its byte-wise values for other users; the table takes
`mix(h) = (h * 0x9e3779b97f4a7c15) >> (64 - log2 n_heads)` (Fibonacci hashing: the high bits,
well mixed even when the key's low bits are fixed). 4 and 8 B keys: h is the key itself (one
multiply); longer keys: a word-at-a-time multiply-xorshift over 8-byte chunks. Measured
separately first: step 1's table with the mixer vs step 1 (suite hash cases, u64 address keys:
the 3.1% locality loss of `reviews-20261031/fixes/hash-ab2`).

## Operations

Lookup (no lock, writes nothing):
1. `i = load_acquire(heads[b])`; up to a step cap (max_entries + 1):
2. if `i` is a terminator: if it is `NULLS | b`, miss; else (the chain changed under us) restart.
3. `w = load_acquire(elem[i].word)`; not in use -> restart.
4. compare key (4 B: acquire load of the key, else memcmp then an acquire fence).
5. on a match: `read(value)`, then `elem[i].word == w` (relaxed) -> hit, else restart.
6. no match: `n = elem[i].next` (relaxed), acquire fence, `elem[i].word == w` -> `i = n`, step 2;
   else restart. The re-check on every step is required: `next` is shared with the free list, and
   a delete + push overwrites it while a reader stands on the element.

Insert / update (key's stripe lock):
- walk the chain; key found: NOEXIST -> exists; else write the value in place (as today, no
  replace-on-update: value pointers stay valid while the key exists).
- not found: EXIST -> missing; pop a free element (none -> full); write key and value;
  `next = heads[b]`; `word = gen | IN_USE` (release); `heads[b] = i` (release).

Delete (key's stripe lock):
- find `i` and its predecessor; copy the value out if asked;
  `word = (gen + 1) | FREE` (relaxed: the unlink is the release); unlink: `prev.next = next` or
  `heads[b] = next` (release); push `i` to the allocator.
- Reuse never resets the generation; a reader stalled across delete + reuse sees a different word.

`with_existing_value(key, fn)`: under the stripe lock, fn on the value (per-CPU hash updates).
`lookup_read`: step 5's read runs before the re-check (per-CPU hash slot numbers).

`next_key`: bucket order, chain order within a bucket; a missing key (or null) -> the first key;
each key copied under its stripe lock. Same semantics as today and the kernel.

## Allocator (decided from step 1's six-thread numbers)

- Global lock-free stack: head `{tag:32, index:32}` with a 64-bit CAS; `next` holds the free
  link. One contended line per insert and delete; no lock, so a dying process holds nothing (it
  can leak one element between pop and link or unlink and push: E2BIG one slot early).
- Per-CPU lists (the kernel's `percpu_freelist.c`): pop from this CPU's list under its lock, else
  the others one lock at a time; push to this CPU's. Exact (the free elements are the count),
  bounded stealing, no all-stripes path; one more lock class a dying process can hold.
  Chosen if one contended line costs more than ~100 ns at six threads in step 1.

## Tests

- Removed with the probing: home-stripe fast paths, probe bound (`max_displacement`), the wrap
  past the last stripe, `release_deleted_run`.
- Ported through `stripe_of_key`: `test_percpu_hash_locks`, `test_hash_map_value_access`.
- Ported in intent: torn keys (`test_hash_map_lockfree_lookup`), threads vs a reference with exact
  capacity and processes at different addresses (`test_hash_map_striped_writers`), `[alignment]`,
  the step-1 churn test, fix and per-CPU hash semantics, `[map-create]`.
- New, each with a mutation that fails it:
  (a) a reader stalled on element e while e is deleted and reused by another key in another
      chain must miss (mutation: no per-step re-check);
  (b) a reader whose element went to the free list restarts and never returns a free
      element's value (mutation: terminator check off);
  (c) capacity exact under concurrent inserts and deletes (mutation: pop fails while elements
      are free, e.g. one list only);
  (d) a held stripe lock blocks only writers of that stripe, never lookups.

## Measurements

Legs: kernel, a7d54d7e, dev-a4ac0d31, step 1 (dev-42595149), step 1 + mixer, step 2. The key
pattern matrix (`hash-keypattern-20261103/run_keys.py`), the structure harness (chain lengths
instead of displacement), the suite's hash and per-CPU hash cases, churn, tracking table,
counters, ssl-nginx unchanged; memory per entry. CLAUDE.md memory numbers (per-CPU 1M keys,
memleak's map) and `map_init`'s lower bound updated from them.
