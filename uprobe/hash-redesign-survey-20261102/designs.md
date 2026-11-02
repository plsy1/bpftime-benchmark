# Hash map redesign survey: mature designs against bpftime's constraints

Date: 2026-11-02. Read-only survey: no benchmarks were run and no repository was changed.

## 0. Sources and abbreviations

| Tag | Source |
|---|---|
| BT | `/home/jetson/src/bpftime-daily-state/worktrees/177a1dc465ff/candidate/runtime/src/bpf_map/bpftime_hash_map.hpp` (1064 lines) |
| BT-MD | `.../candidate/CLAUDE.md`, sections "Hash Map Concurrency", "Hash Map Update Flags", "Shared Memory Size And bpf() Failures" |
| K | `/home/jetson/src/l4t-r39.2.1/Linux_for_Tegra/source/kernel/kernel-noble/kernel/bpf/hashtab.c` (L4T r39.2.1, Ubuntu noble 6.8) |
| K-FL | `.../kernel/bpf/percpu_freelist.c` |
| K-NULLS | `.../kernel-noble/Documentation/RCU/rculist_nulls.rst` |
| K-JH | `.../kernel-noble/include/linux/jhash.h` |
| DP | DPDK 26.11.0-rc1 (git e3e29edb, 2026-10-23), `lib/hash/rte_cuckoo_hash.{c,h}`, `doc/guides/prog_guide/{hash_lib,multi_proc_support,ring_lib}.rst`. The local checkout is in a session scratch dir, so line numbers refer to that commit. Upstream: https://github.com/DPDK/dpdk/tree/main/lib/hash |
| BO | Boost 1.83 installed headers, `/usr/include/boost/unordered/detail/foa/{core,concurrent_table}.hpp`, `.../detail/mulx.hpp` |
| LC | libcuckoo git 0dd0b4d6 (2026-10-17), `libcuckoo/cuckoohash_map.hh`, `bucket_container.hh`, `cuckoohash_config.hh`. Upstream: https://github.com/efficient/libcuckoo |
| FO | folly git d9b5d352 (2026-10-29), `folly/AtomicHashMap.h`, `AtomicHashArray{,-inl}.h`, `AtomicUnorderedMap.h`. Upstream: https://github.com/facebook/folly |
| F14 | https://github.com/facebook/folly/blob/main/folly/container/F14.md and `folly/container/detail/F14Table.h` (fetched, main branch) |
| AB | https://github.com/abseil/abseil-cpp/blob/master/absl/container/internal/raw_hash_set.h (fetched, master) |
| TBB | oneTBB git af14bb56 (2026-10-16), `include/oneapi/tbb/concurrent_hash_map.h` |
| SOG | Arm Cortex-A78 Core Software Optimization Guide, Issue 4.0, https://documentation-service.arm.com/static/60a5413bd63d3c31550c391e. The Orin's A78AE is assumed to have A78 instruction timings. This is not verified. |
| XXH | https://github.com/Cyan4973/xxHash/blob/dev/xxhash.h (fetched) |

"Literature only" marks a claim that was not checked against source in this survey.

## 1. Constraints (C1-C5) and what bpftime does today

| # | Constraint | Today (BT) |
|---|---|---|
| C1 | Shared memory mapped at different addresses: offsets only, allocation only from the segment, preallocated, fixed capacity, exact E2BIG | Two `boost::interprocess` vectors (buckets, stripes). Each operation resolves them to plain pointers once (`view`, BT 158-176). Exact capacity uses `max_entries` tokens held by stripes plus a shared pool (BT 106-110, BT-MD). |
| C2 | Value pointers stay valid while the key exists; elements never move | Filled buckets never move (BT 93). Values are updated in place, so readers may see a torn value, as in the kernel without `BPF_F_LOCK` (BT-MD). |
| C3 | Lookups take no lock and write nothing shared; writers may lock; note tolerance of a writer that dies | Per-bucket seqlock word. A lookup reads the word, compares the key and re-checks the word (BT 91-97, 417-470). It also reads the home stripe's probe bound (BT 425). A writer killed while holding stripe locks hangs later writers to those stripes. Readers are not affected (BT-MD). |
| C4 | Kernel semantics for ANY/NOEXIST/EXIST, delete, get_next_key, lookup_and_delete | Implemented (BT-MD "Hash Map Update Flags"). `next_key` scans bucket order and restarts from bucket 0 when the key is missing (BT 973-992), as the kernel does. |
| C5 | Any key size (4/8 are hot) and value size (8-8192) | Elements inline: `[word 4][key][pad][value]`, with `_num_buckets = next_prime(2*max_entries)` (BT 584-600). |

Known costs of today's design (from BT-MD):
- Every value is stored twice over: there are 2x buckets and each bucket holds a value. memleak's 1M-entry map needs 64-96 MB "with the doubled hash buckets".
- Tombstones plus linear probing need the probe-bound machinery, multi-stripe range locking, and "unlock and retry with a larger range".
- Tokens can park in stripes, so inserts near capacity hit a cliff where they lock every stripe.
- `hash_func` clusters structured keys (a probe bound of 9,945 for 10k keys). It also never uses buckets above about 7.85M as homes.

## 2. Per-design survey

### 2.1 Linux kernel BPF htab (preallocated mode, the default)

| Aspect | Detail (citation) |
|---|---|
| Layout | `struct bucket { hlist_nulls_head head; raw_spinlock_t raw_lock; }` (K 79-82). `n_buckets = roundup_pow_of_two(max_entries)` (K ~512). Elements are `htab_elem` (hash node, union with LRU node or per-CPU pointer, `u32 hash`, key aligned to 8), then the value at `key + round_up(key_size, 8)` (K 112-130, 704). All elements live in one array, `elems`, of `max_entries + num_possible_cpus()` entries (K 318-330). |
| Collision | Separate chaining. The stored 32-bit hash is compared before `memcmp` (K 638-653). |
| Delete | Unlink under the bucket lock and push the element to the per-CPU freelist at once, with no RCU grace period (K 1425-1459, 944-956). No tombstones. |
| Readers | RCU and no lock. Because preallocated elements are reused at once, a reader can follow a recycled element into another chain. The chain terminator is a "nulls" value that encodes the bucket index. If the walk ends on another bucket's nulls, the lookup restarts (`lookup_nulls_elem_raw`, K 655-676; K-NULLS 143-190). The key compare itself is not re-validated, so a lookup racing a delete plus reuse can match an element just reassigned (accepted kernel behaviour). |
| Writers | Per-bucket raw spinlock, plus 8 per-CPU `map_locked` recursion counters that return `-EBUSY` on re-entry (K 84-85, 150-182). |
| Update | Replaces rather than overwrites. A new element (taken from a per-CPU `extra_elems` spare) is added at the chain head, then the old element is unlinked and becomes the spare (K 1017-1021, 1180-1196). An old value pointer therefore stops being the key's value. bpftime overwrites in place, a deliberate difference. |
| Capacity | `__pcpu_freelist_pop` fails, so `-E2BIG` (K 1023-1028). The freelist is the counter, which makes the limit exact. The freelist is per CPU, each list locked by a raw spinlock; pop walks every CPU's list, then an extralist (K-FL 120-148). |
| get_next_key | Looks up the key. If found, returns the next element in its chain, otherwise the first element of the next non-empty bucket. A missing key restarts from bucket 0 (K 835-890). |
| lookup_and_delete | Copy and unlink under the bucket lock (K 1606-1668). |
| Hash | `jhash2` for key sizes that are multiples of 4, else `jhash`, seeded with a random `hashrnd` (K 620-625). |
| Memory/entry | `sizeof(htab_elem)` = 48 (BT-MD map-create note), so 48 + round8(K) + round8(V), plus 16 B x (1-2) of bucket per entry. |
| Value stability | Memory stays valid for the map's lifetime. The pointer stays the key's only until the next update of that key (replace semantics). |
| High load | Chains average at most 1 element, since `n_buckets >= max_entries`. Performance stays flat up to 100%. |
| Death tolerance | Not applicable in the kernel: no process dies holding a raw spinlock. |

Fit to bpftime:

| C1 | C2 | C3 | C4 | C5 |
|---|---|---|---|---|
| With changes: pointers become 32-bit indexes, `elems` is a segment array | Yes, with in-place update (better than the kernel) | With changes: no cross-process RCU, so a per-element generation re-check is needed (today's protocol) | Yes (it is the reference) | Yes |

What must change:
- Use u32 `next` and head indexes, with nulls-encoded terminators: for example the high bit set plus the bucket index.
- Add a per-element seqlock word, since there is no RCU.
- Replace per-CPU raw-spinlock freelists, which a dead process can wedge. Options are an index Treiber stack with a {index, tag} 64-bit CAS, or a few locked shards.
- Drop `extra_elems`; in-place update does not need it.

### 2.2 DPDK `rte_hash` (cuckoo with signatures and a key store)

| Aspect | Detail (citation) |
|---|---|
| Layout | Buckets of 8 entries, one cache line each: `uint16_t sig_current[8]; uint32_t key_idx[8]; uint8_t flag[8]; void *next;` (DP .h `RTE_HASH_BUCKET_ENTRIES 8`, `struct rte_hash_bucket`). `num_buckets = align32pow2(entries)/8` (DP .c 546). A separate key store holds `entries + 1` slots (slot 0 is a dummy), with more slots when there are per-lcore caches (DP .c 520-531). Each slot is `{pdata (8 B) ; key[]}` (DP .h `struct rte_hash_key`). Values larger than 8 B live outside, behind `pdata` (hash_lib.rst 45-46). |
| Collision | Bucketized cuckoo with 2 candidate buckets. The primary is `hash & mask`; the alternate is `(prim ^ short_sig) & mask` (partial-key cuckoo). `short_sig = hash >> 16` (DP .c 392-410). Insert displaces along a BFS path; only `key_idx` and `sig` move, never the key store (hash_lib.rst 153-164). |
| Delete | Clears `sig` and `key_idx` (DP .c ~2005-2045). No tombstones. With `EXT_TABLE`, an entry from the chained extension buckets is moved back (hash_lib.rst 191-193). |
| Readers (LF mode) | `RW_CONCURRENCY_LF`: no lock. The reader compares sigs, acquire-loads `key_idx`, compares the key, and retries a miss if the global `tbl_chng_cnt` changed during the search, because a cuckoo move may have carried the key across (DP .c 1599-1737). Key-store slots must not be reused while readers may hold them, so `NO_FREE_ON_DEL` is forced and reclamation is left to the application or the integrated RCU QSBR (hash_lib.rst 96-105). |
| Writers | One global rwlock (`__hash_rw_writer_lock`, DP .c 868-878), taken only when `MULTI_WRITER_ADD` or `RW_CONCURRENCY` is set (DP .c 496-519). The LF reader protocol assumes a single serialized writer: "Since there is one writer, load acquires on tbl_chng_cnt are not required" (DP .c ~1184-1188). Hit updates exchange `pdata` atomically (DP .c 1013-1052). |
| Capacity | Free key-store indexes sit in an `rte_ring`, with per-lcore caches of 64 (DP .c 1297-1327). A failed allocation returns `-ENOSPC` (DP .c 1390). Per-lcore caches strand up to 63 x lcores slots, so `entries` are over-provisioned (DP .c 528). The cuckoo index can fail before the store is full (~95% with jhash, hash_lib.rst 210-253), which the `EXT_TABLE` linked buckets fix. |
| Iteration | `rte_hash_iterate` uses a positional cursor over bucket slots (DP .c 2845-2890). No key-based next. |
| Multi-process | Relies on identical virtual mappings in every process: "all pointers to that memory are valid ... in both processes" (multi_proc_support.rst 55-61). The `hash_func` pointer breaks across different binaries (multi_proc_support.rst 181-187). Raw pointers are stored (`key_store`, `buckets`, `bucket->next`, `free_slots`). |
| Hash | Default `rte_jhash`; `rte_hash_crc` (arm64 `crc32c[bhwx]`) when available (DP .c 437, 683-686; `rte_crc_arm64.h` 9-42). |
| Memory/entry | Index: 64 B per 8 slots = 8 B per index slot. Key store: 8 + key, aligned to 16 (DP .h `KEY_ALIGNMENT 16`). |
| Value stability | Key-store slots never move. Stable while the key exists (C2 yes). |
| High load | Average maximum load with random keys is ~94-96% before an insert fails without ext buckets (hash_lib.rst 210-253). The secondary-bucket share rises to 25-35%, so misses always read 2 buckets. |
| Death tolerance | A writer dying while holding the global rwlock blocks all writers. LF readers do not block. A producer dying between ring head and tail updates blocks the other producers: "A core can only update [prod_tail] if ring->prod_tail is equal to the prod_head local variable" (ring_lib.rst 242-254). |

Fit to bpftime:

| C1 | C2 | C3 | C4 | C5 |
|---|---|---|---|---|
| With changes: every pointer becomes an index, no function pointers | Yes | Yes for readers, but only with a single writer plus deferred slot reuse; multi-writer LF is not provided | With changes: key-based get_next_key (scan the key store by slot index), EXIST/NOEXIST under lock | With changes: value stored inline in the key-store slot |

What must change:
- Per-slot generation words replace RCU and `NO_FREE_ON_DEL`.
- Writers move from one global lock to per-bucket-pair ordered locks (libcuckoo style).
- The global `tbl_chng_cnt` becomes per-bucket change counters; the global one is a written hot line shared by all writers.
- A 2x index or ext buckets keep exact 100% capacity.
- The `rte_ring` is replaced by a freelist that survives a dying process.

### 2.3 Abseil SwissTable (`raw_hash_set`)

| Aspect | Detail |
|---|---|
| Layout | A control-byte array plus a slot array. Each control byte is empty, deleted (tombstone), sentinel, or a 7-bit H2. H1 picks the starting group; H2 filters slots with SIMD over a group of a platform-specific width (AB header comment). |
| Collision | Open addressing, quadratic (triangular) probing over groups (AB "probe_seq"). |
| Delete | A tombstone, unless "we can prove that the slot would not appear in a probe sequence", in which case the slot becomes empty (AB erase comment). Tombstones are cleared by an in-place rehash when growth is due (`drop_deletes_without_resize`; function name in AB, literature-level for this survey). |
| Concurrency | None. Not a concurrent container. |
| Memory/entry | 1 control byte + sizeof(slot), max load 7/8 (AB). |
| Stability | "All iterators and pointers are invalidated after a new element is inserted" (AB). This is a growth or rehash property; at fixed capacity with no rehash, slots never move. |
| High load | Grows at 7/8, so a fixed table must be sized so that `max_entries` is at most 7/8 of the slots. Tombstones accumulate without the in-place rehash. |

Fit: C1 with changes (plain arrays work in the segment). C2 yes as long as no rehash or `drop_deletes` ever runs. C3 no: a concurrency protocol must be added (a per-slot or per-group seqlock), and the control byte cannot also carry a generation. C4 with changes. C5 yes.

Main lesson: SIMD tag filtering, and the empty-vs-deleted rule on erase that avoids most tombstones. In a never-rehashed table, the remaining tombstones still need a cleanup pass under an exclusive lock, which is the same problem bpftime solves today with `release_deleted_run`.

### 2.4 folly F14 (F14Value/Node/Vector/Fast)

| Aspect | Detail |
|---|---|
| Layout | Chunks of 14 slots (12 when the item is 4 bytes). The header holds `tags_[14]` (1 byte each, 0 = empty), `control_` (bits 4-7: `hostedOverflowCount`) and `outboundOverflowCount_` (F14Table.h `F14Chunk`). |
| Collision | Double hashing over chunks with `probeDelta = 2*hp.second + 1` (odd, so it is coprime with the power-of-two chunk count) (F14Table.h). Expected probe length at max load: 1.04 for a hit, 1.275 for a miss (F14.md). |
| Delete | No tombstones. `outboundOverflowCount` "can be decremented on erase", so a chunk recovers in steady insert/erase workloads (F14.md). Misses stop at the first chunk whose outbound count is 0. |
| Concurrency | None. |
| Memory/entry | Max load 12/14 (F14.md). Tags cost about 16/14 B per slot. F14VectorMap stores values packed with 32-bit indexes in the hash array (F14.md). |
| Stability | "F14NodeMap ... is the only F14 variant that doesn't move its elements" (F14.md). Again a rehash statement: with fixed chunks and no rehash, F14Value slots do not move either. |
| High load | Sized for 12/14 = 85.7%. |

Fit: C1 with changes. C2 yes with no rehash. C3 no as-is: a seqlock would need to be added; the tags are single bytes, and the overflow counts are written by inserts that pass through. C4 with changes. C5 yes.

Main lesson: decrementing overflow counts are the only tombstone-free deletion scheme here that cleans itself up at fixed capacity. F14VectorMap's "index array of u32 plus a dense value store" has the same shape as DPDK's index plus key store.

### 2.5 Boost `unordered_flat_map` / `concurrent_flat_map` (FOA, 1.83)

| Aspect | Detail (citation) |
|---|---|
| Layout | Groups of 15 slots with a 16 B metadata word: 15 reduced hashes and 1 overflow byte. The group count is a power of two (BO core.hpp 150-166, 168-200). |
| Collision | Quadratic probing over groups. Overflow byte bit `h%8` is set when an insert passes over a full group (BO core.hpp 178-195). |
| Delete | The reduced hash is reset to 0 (no tombstone value). Overflow bits are sticky. `recover_slot` lowers `size_ctrl.ml` (the max load) on each erase that may have caused overflow, "so that average probe length won't increase unboundedly in repeated insert/erase cycles (drift)" (BO core.hpp 1961-1968). Only a rehash restores it. |
| Readers | Two levels. A container-level rw lock is sharded per thread and taken shared on every operation. On a reduced-hash match, the group's rw spinlock is taken shared (BO concurrent_table.hpp 367-378, 857-862, 889-892). A lookup therefore writes two shared lock words. |
| Writers | Group-exclusive locks plus an optimistic insert using per-group insertion counters (BO concurrent_table.hpp 379-392, 1206-1220). |
| Hash | Non-avalanching hashes are post-mixed with `mulx` (128-bit multiply, xor of the high and low halves) (BO core.hpp 872-894; mulx.hpp). |
| Memory/entry | ~1.07 B metadata + slot. Max load 0.875 (BO core.hpp 1122). Concurrent: plus one group_access (lock and counter) per group. |
| Stability | flat: invalidated on rehash. With no rehash, slots stay put. |
| High load | At fixed capacity, the anti-drift max-load decrement keeps shrinking the usable capacity under churn, which breaks C1's exact E2BIG unless the overflow bits are rebuilt. |

Fit: C1 with changes (allocator only; the per-thread `thread_local` shard id is process-local, which is fine). C2 yes with no rehash. C3 no, because readers take shared locks. C4 with changes. C5 yes. Death tolerance: a reader dying while holding a shared group lock blocks writers to that group.

Latch-free lookups were reported as explored for future releases (https://www.boost.org/news/entry/joaquins-update-on-boostunordered-evolution/). They were not confirmed as shipped.

### 2.6 libcuckoo (`cuckoohash_map`)

| Aspect | Detail (citation) |
|---|---|
| Layout | Buckets of `SLOT_PER_BUCKET = 4` (LC config) holding key and value inline, plus `partials_` (8-bit tags) and `occupied_` (LC bucket_container.hh 59-100). |
| Collision | 2-choice bucketized cuckoo with BFS paths of at most 5 (LC 1376-1378). |
| Delete | Clears `occupied`. No tombstones. |
| Readers | `find_fn` locks both buckets' spinlocks (`snapshot_and_lock_two`, LC 466-476). Striped spinlocks, at most 2^16 (LC 2085). |
| Writers | Ordered two-lock acquisition. A full table triggers `cuckoo_fast_double` (automatic resize, LC 1230-1250). |
| Stability | No: a cuckoo displacement moves key and value between buckets. |

Fit: C1 with changes. C2 no (values move). C3 no (readers lock). C4 with changes. C5 yes, but every displacement moves large values.

Main lesson for bpftime: the striped two-bucket lock ordering is the multi-writer piece DPDK lacks.

### 2.7 folly AtomicHashArray / AtomicHashMap

| Aspect | Detail (citation) |
|---|---|
| Layout | A fixed array of `{key, value}` cells. The key must be an int32 or int64 with reserved empty, locked and erased sentinel keys (FO AtomicHashMap.h 17-60). |
| Collision | Linear or quadratic probing (FO AtomicHashArray.h 40-58). |
| Delete | CAS the key to `kErasedKey`. "the cell containing the key will never be reused" (FO AtomicHashArray-inl.h 278-327). Permanent tombstones. |
| Readers | "find() is completely wait-free and doesn't require any non-relaxed atomic operations" (FO AtomicHashMap.h). |
| Writers | CAS empty to locked, write the value, publish the key (FO AtomicHashArray-inl.h ~170-240). |
| Capacity | Fixed per sub-array. AHM chains up to ~18x of extra sub-maps (FO AtomicHashMap.h). The published numbers are flat up to a 95% load (0.10 us per find). |

Fit: C1 yes (a fixed array). C2 yes. C3 yes. C4 no: an erased cell is never reused, so delete-heavy maps (every BPF map with churn) exhaust it, and keys must be integers with sentinels. C5 no (integer keys only).

Main lesson: the cheapest possible reader, but deletion is the reason it cannot serve as a BPF htab.

### 2.8 folly AtomicUnorderedInsertMap

| Aspect | Detail (citation) |
|---|---|
| Layout | One `Slot` array. Each slot holds `headAndState_` (2 state bits plus the 30-bit index of its chain head), `next_` (u32 index) and the key/value storage. Chain heads are colocated with the storage so that a lookup "is likely to stay within the same cache line" (FO AtomicUnorderedMap.h 352-368). |
| Collision | Separate chaining through 32-bit indexes inside the same array. `allocateNear(home)` tries home..home+7, then random slots (FO 438-466). Capacity is `maxSize/maxLoadFactor + 128`, with a mask of `nextPowTwo(capacity*4)-1` (FO 207-223). |
| Delete | None ("Insert only", FO header). |
| Readers | Wait-free, one acquire load of the head, then plain `next_` reads (FO 424-432). |
| Writers | Lock-free CAS on the slot state and on the head word (FO 263-300). |
| Stability | "Keys and values in the map won't be moved" (FO header). |

Fit: C1 yes in shape (indexes, fixed array). C2 yes. C3 yes. C4 no deletes as-is. C5 yes.

Main lesson: index chaining with heads colocated in the storage slots is an offset-only, fixed-capacity form of the kernel's design. Deletes need what the kernel adds: unlink, the nulls-terminator restart, and reuse after a generation bump.

### 2.9 Intel oneTBB `concurrent_hash_map`

Chained nodes in segment-allocated buckets. Each bucket has a `spin_rw_mutex` (TBB 88-95). `find(const_accessor&)` takes the bucket lock and then holds a read lock on the item for the accessor's lifetime (TBB 708-722, 773-792, 1110-1114). The table grows by segments with lazy rehash markers (TBB 62-68).

Fit: C1 no (node allocation, growth). C2 yes (nodes). C3 no (readers lock). C4 with changes. C5 yes. Not a candidate.

### 2.10 Hopscotch hashing (literature only)

Herlihy, Shavit and Tzafrir, "Hopscotch Hashing", DISC 2008. Open addressing in which every key sits within H (typically 32) slots of home, tracked by a per-bucket hop bitmap. An insert moves existing keys closer to their homes to make room, and the concurrent version uses per-segment locks plus timestamps for readers.

Fit: C2 no, because elements move on insert (it would need an index indirection, which turns it into another index-plus-store design). C3 yes in the concurrent variant (timestamp re-check), C1 yes. Not pursued.

## 3. Summary matrix

Y = yes as-is, W = yes with the changes listed above, N = no without becoming a different design.

| Design | C1 offsets/fixed/exact | C2 stable values | C3 lock-free, write-free readers | C4 kernel ops | C5 any sizes | Delete without tombstones | Exact capacity from structure | Death: writer in lock |
|---|---|---|---|---|---|---|---|---|
| bpftime today | Y (tokens) | Y | Y (seqlock) | Y | Y | N (tombstones + bounds) | N (token side mechanism) | stripe writers hang |
| Kernel htab | W | Y (in place: better) | W (RCU -> generation) | Y | Y | Y | Y (freelist) | per-bucket writers hang |
| DPDK rte_hash LF | W | Y | W (single writer + RCU) | W | W | Y | W (ring + caches, ext buckets) | all writers hang (global lock, ring) |
| Abseil | W | Y if no rehash | N | W | Y | partial | N | n/a |
| F14 | W | Y if no rehash | N | W | Y | Y (counts) | N | n/a |
| Boost FOA / concurrent | W | Y if no rehash | N (shared locks) | W | Y | drifts at fixed size | N | readers can block writers |
| libcuckoo | W | N | N | W | Y | Y | N (resizes) | lock pair |
| folly AHA/AHM | Y | Y | Y | N (no reuse) | N | N | Y | lock-free |
| folly AUM | Y | Y | Y | N (no delete) | Y | n/a | Y | lock-free |
| TBB | N | Y | N | W | Y | Y | N | bucket |
| Hopscotch | Y | N | Y | W | Y | Y | N | segment |

## 4. Memory per entry (structure only, excluding per-map constants)

Assumptions:
- Today: `2*max_entries` buckets (prime), element as in BT 584-600.
- Kernel: 48 B header, plus `n_buckets = pow2 >= max_entries` (1-2 buckets of 16 B per entry).
- Pool (Candidate A, §5): u32 heads (pow2 >= max_entries, so 4-8 B per entry) and element `{word 4, next 4, key, value aligned 8}`.
- DPDK-like (Candidate B): 2x index at 8 B per slot (16 B per entry), store slot `{word 4, key, value aligned 8}`.

| (key, value) bytes | Today | Kernel prealloc | Candidate A: pool + u32 chains | Candidate B: cuckoo index + store | Candidate C: inline + tags at 7/8 |
|---|---|---|---|---|---|
| (4, 8) | 2 x 16 = 32 | 64 + 16..32 = 80-96 | 16 + 4..8 = 20-24 | 16 + 16 = 32 | (16+1) x 8/7 ~ 19 |
| (8, 8) | 2 x 24 = 48 | 64 + 16..32 = 80-96 | 24 + 4..8 = 28-32 | 24 + 16 = 40 | (24+1) x 8/7 ~ 29 |
| (16, 8192) | 2 x 8216 = 16432 | 8256 + 16..32 | 8216 + 4..8 ~ 8224 | 8216 + 16 ~ 8232 | 8217 x 8/7 ~ 9391 |

For (4, 8), Candidate A packs `{word, next, key}` into 12 B and puts the value at offset 16, so the element is 24 B. With a 4-byte key and a 4-byte value it drops to 16 B. Every index-plus-pool design stores each value once. Today's design and any open-addressing design store `1/load` values per entry.

## 5. Ranked recommendation (candidates to verify, not decisions)

The test that ranks them: does exact E2BIG and tombstone-free deletion follow from the data structure itself, or from a side mechanism with its own locks? Only Candidate A answers "the structure" to both.

### Candidate A (first): preallocated element pool + 32-bit index chaining (kernel htab, offset form)

Layout, all in the segment:
- A `u32 head[n_buckets]`, with `n_buckets` a power of two >= `max_entries`.
- `elem[max_entries]`, each `{u32 gen_state; u32 next; key; value}` with the value 8-aligned, as today.
- An index freelist.
- Striped writer spinlocks (a lock per bucket group, one cache line per lock as today).

Protocol:
- Lookup. Hash, load `head[b]` (acquire), walk `next`. On a key match, re-check the element's gen word (today's seqlock protocol, moved from bucket to element). If the walk ends on a terminator that is not `NULLS(b)` (high bit set plus the bucket index, as with kernel nulls, K-NULLS 143-190), restart.
- Insert. Pop from the freelist (empty means E2BIG, exactly, K 1023-1028), write key and value, publish with release, link at the head under bucket b's lock.
- Delete. Unlink under the lock, bump gen, push to the freelist.

What falls out:
- No tombstones, no probe bound, no multi-stripe range locks, no token pool.
- Constant cost up to 100% load (chain average at most 1).
- get_next_key and lookup_and_delete follow the kernel line by line (K 835-890, 1606-1668).
- Values stored once (§4).
- The lock-free reader protocol is the existing one, applied per element.

The freelist must survive a process dying inside an operation. Options:
- A Treiber stack of indexes with a 64-bit `{index, ABA tag}` CAS: lock-free, and a death leaks at most one element.
- Per-CPU shards with batch refill, which costs a lock per shard.
Today's token batching (BT-MD) can be reused as the shard refill policy.

Risks:
- Lookups read `head[b]` and then the element: 2 lines. Today reads the stripe bound and then the bucket, also 2 lines (BT 425-430), so it should break even; this must be measured on 4- and 8-byte keys, hit and miss, at 50% and 90% fill.
- The 3-line case (chain length 2) appears when `n_buckets = max_entries`. Mitigations: 2x heads (4 B per entry), or folly AUM's colocated head-in-slot with `allocateNear` (FO 352-368, 438-466), which conflicts with a LIFO freelist.
- A recycled element may move to another chain mid-walk. Nulls restart plus the gen re-check cover it, but this is new code in bpftime and needs the torn-key and reuse stress tests ported (`test_hash_map_lockfree_lookup.cpp`).
- Unbounded walk: a reader looping on a cycle formed by rapid reuse. The kernel tolerates this under RCU; here a walk cap (`> max_entries` steps means restart) is needed.
- The kernel's replace-on-update (K 1180-1196) is deliberately not copied. Values stay updated in place, as BT-MD documents.

### Candidate B (second): DPDK-style cuckoo signature index + fixed key/value store

Layout: an index of 8-entry, 64-B buckets of `{u16 sig[8]; u32 idx[8]}`. The store holds `max_entries` slots `{gen; key; value}`, plus the same freelist as Candidate A.

Lookup: hash, then 2 buckets compared with NEON over 8 sigs, then `idx`, then the store slot. A per-slot gen re-check replaces RCU. A miss retries when the home buckets' change counters moved, so a counter per bucket rather than DPDK's global one (DP .c 1700-1736).

Writers lock the two candidate buckets in order (LC-style striping, LC 2085). A cuckoo path move locks every bucket on the path and bumps their counters.

Strengths: stable values; at most 2 buckets on any lookup; signature filtering helps long keys (12-192 B); the index is cheap to over-provision (2x index at 8 B per slot), which keeps the 100% fill exact without ext buckets.

Risks:
- DPDK's LF readers are proven only against a single writer (DP .c ~1184-1188). Multi-writer cuckoo moves with lock-free readers are new ground: a reader can miss a key in flight between buckets.
- Every miss reads 2 index lines plus a counter line.
- Short-key hits read an index line and a store line, so no gain over Candidate A.
- More code than Candidate A: BFS path, path validation, ordered multi-bucket locks.

### Candidate C (third, lowest risk): keep the inline open-addressing table, replace its weak parts

- Replace `hash_func` with a strong mixer (§6). This fixes the clustering and the 7.85M home ceiling.
- Add a 1-byte tag per slot in groups (SwissTable/F14 SIMD filtering).
- Replace tombstones with F14-style decrementing overflow counts (F14.md), which clean themselves up at fixed capacity. Boost's sticky bits plus max-load decrement (BO core.hpp 1961-1968) would break exact capacity.

Gains: shorter miss probes, no tombstone cleanup. The load can rise from 50% toward 12/14, which cuts the memory overhead in §4.

Risks:
- Stripes, tokens and probe-range locking all stay, along with their single-thread costs and the near-capacity cliff.
- Overflow counts become another word that writers modify and readers depend on.
- At 12/14 load, a fixed table filled to exactly `max_entries` has to be sized `14/12 x max_entries`.

### Shared risk (all three)

Any well-mixed hash loses the address locality that `hash_func` gives u64 address keys. The MurmurHash3 64-bit finalizer cost 3.1% on the 900K-entry memleak-style table for that reason (BT-MD). This is a property of the hash, not of the table, so it should be measured once, separately from the table change.

## 6. Hash functions for 4- and 8-byte keys on the A78 (cost by instruction count; not measured)

SOG timings used: CRC32/CRC32C latency 2, throughput 1 (Table 3-36). MUL latency 2. MADD latency 2(1). UMULH latency 3 (Table "AArch64 integer multiply"). Basic ALU latency 1.

| Hash | Work for a 4- or 8-byte key | Dependent-latency estimate | Quality / notes |
|---|---|---|---|
| bpftime `hash_func` (4 B) | 3 MUL/MADD + adds by constants 29791, 961, 31 (BT 72-85) | ~4-6 cycles | Poor: linear in the bytes. Clusters structured keys and caps homes at 7,849,920 (BT-MD). |
| jhash2 / jhash_1word | `__jhash_final`: 7 steps of xor + sub + rol, about 21 dependent ALU ops (K-JH 46-55, 171-173) | ~14-21 cycles | Good. Seeded. The kernel uses it (K 620-625). Generic jhash with variable length adds a loop. |
| CRC32C (`crc32cw`/`crc32cx`) | 1 instruction per 4 or 8 bytes (DP `rte_crc_arm64.h` 29-42) | 2 cycles, plus a mixing step if needed | Linear over GF(2): structured keys keep structure in the low bits. Use it with a multiply or xorshift finalizer before taking `& mask`. DPDK defaults to it when available (DP .c 683-686). |
| Murmur3 fmix32 / fmix64 | 2 MUL + 3 xor-shift (smhasher `MurmurHash3.cpp`; literature) | ~7-9 cycles | Good avalanche. Measured in bpftime: +1.2-1.4% on 6-thread churn, -3.1% on u64 address keys (locality loss) (BT-MD). |
| Boost `mulx` | 1 MUL + 1 UMULH (parallel) + 1 EOR (BO mulx.hpp) | ~4 cycles | Good for a single word multiplied by a constant. Cheapest strong mixer for 8-byte keys. |
| xxh3 (4-8 B) | `XXH3_rrmxmx`: 2 64-bit MUL plus rotates and xors (XXH) | ~8-10 cycles | Very good. 9-16 B: one 128-bit multiply plus avalanche. 17-128 B: one 128-bit multiply per 16 B (`XXH3_mix16B`; literature). |
| wyhash (<=16 B) | 2 x 128-bit multiply (`_wymix`; literature, https://github.com/wangyi-fudan/wyhash) | ~8 cycles | Very good. Similar in cost to xxh3. |

Suggested shortlist to A/B later:
- `mulx(key * C)` for 4- and 8-byte keys.
- `crc32cx` followed by one multiply-xorshift for 4- and 8-byte keys.
- xxh3 for 12-192 B.
All are seeded per map, like the kernel's `hashrnd`. Every candidate in §5 takes its bucket index from the high bits, or from a mixed value, never from the low bits of a linear hash.
