# Hash map redesign: survey (2026-11-02)

User 11-02: the ordinary hash map (`bpftime_hash_map`) is a textbook linear-probing table;
research before any redesign. Two parts: the structure of today's table under realistic keys
(`structure/`, measured), and mature designs against bpftime's constraints (`designs.md`,
literature and source reading, nothing benchmarked). Dev a4ac0d31 (= 7d9690c3 for the map),
Jetson Orin, cpu5 1728000, single thread on cpu5 unless noted.

## 1. Today's table under realistic keys (`structure/structure.cpp`, `table.txt`)

100k max_entries (200,003 buckets), 8 B value; 12 key patterns x loads 10/50/90/100% of
max_entries; measured after filling and after churn (2 x max_entries delete-a-random-live-key +
insert-a-new-key). Harness compiled -O2 without LTO, random access over 100k entries: absolute
ns are higher than the suite's (22 ns lookup), compare within the table.

### Finding 1 (severe): churn near capacity costs 30-66 us per delete + insert, every key pattern

| Load | today (striped writers) | a7d54d7e (one lock, before 10-30) | kernel via bpf() (2 system calls) |
|---:|---:|---:|---:|
| 10% | 235 ns | 154 ns | 1,527 ns |
| 50% | 315 ns | 203 ns | 1,672 ns |
| 70% | 4,624 ns | 236 ns | 1,783 ns |
| 80% | 16,864 ns | - | - |
| 90% | 30,570 ns | 296 ns | 1,871 ns |
| 100% | 42,145 ns | 320 ns | 1,945 ns |

(`structure/churn_only.cpp`, `churn_only_old` built from a7d54d7e's header, `kernel_churn.c`.)
perf at 90%: 63% `__aarch64_swp4_acq` (spin lock exchanges), 33% `update`: inserts lock every
stripe to borrow element tokens. Cause: each of the 3,125 stripes may park up to 63 tokens, more
than the free capacity once the map is ~60-70% full; deletes return tokens to the deleted key's
stripe, inserts take them from the new key's, so the shared pool runs dry and inserts take the
all-stripes path. Documented as "a cliff near capacity (second review)" in CLAUDE.md, but its
size was never measured: 100x a7d54d7e and 16x the kernel including its system calls. Hidden in
earlier benchmarks: churn and tracking reuse keys (same home stripe, tokens stay local) or run
at low load. Affects any map kept above ~2/3 full whose keys change (new pids/tids, connection
tuples, request ids).

### Finding 2: keys with fixed low bits cluster (hash function)

`u32 id<<12` at 90% load: mean displacement 613 buckets (max 10,647), hit 1.7 us, miss 6.9 us
(other patterns: 30-150 ns). The 4-byte `hash_func` (29791, 961, 31, 1 per byte) puts such keys
on few adjacent homes. `id<<8`, sequential, pid_tgid, heap addresses (16 B / 4 KiB apart),
12/16/64 B struct keys: displacement < 25 mean (16 B {addr, pid}: 19-24 at 90-100%).

### Finding 3: tombstones accumulate under churn

After churn at 90% load, empty buckets fall from 55% to 21-43% and tombstones rise to 12-34%;
the probe run of a miss grows from 1-3 to 2-16 buckets (max 40-270). Bounded by the stripe probe
bound, so not a cliff, but hit and miss cost rise 1.3-2x (e.g. u32 random miss 50 -> 92 ns).

### Finding 4: memory

Segment bytes per max_entry: (4, 8) 34 B, (8, 8) 50 B, (12/16, 8) 50/66 B, (64, 8) 162 B: every
value stored twice (half the buckets empty). Below the kernel's (48 B element header + key + value
+ 16-32 B of buckets) for small keys; large values double.

## 2. Designs (`designs.md`)

Constraints: shared memory at different addresses (offsets only, preallocated, exact capacity),
stable value pointers, lock-free lookups that write nothing, kernel semantics (flags,
get_next_key), any key size.

| Candidate | Idea | Fixes | Main risks |
|---|---|---|---|
| A (first) | kernel htab in offset form: preallocated element pool `{gen, next, key, value}`, u32 chain heads (pow2), seqlock per element, nulls-style chain terminators, freelist = exact capacity | 1 (no tokens), 3 (no tombstones), 4 (value once); 2 with a better hash | two cache lines per lookup (head, element): measure; freelist robust to a dying process (64-bit {index, tag} CAS stack); reader walk cap |
| B | DPDK-style cuckoo index (8 x 2 B signatures + u32 indexes per bucket, NEON) over a fixed store | at most two buckets per lookup | DPDK's lock-free readers assume one writer under a global lock: multi-writer cuckoo moves under lock-free readers would be new |
| C (lowest risk) | keep the inline table: strong hash, 1-byte tags, F14-style overflow counts instead of tombstones | 2, 3 | keeps stripes, tokens (finding 1 needs its own fix) and 2x memory |

Ruled out: Abseil, F14, Boost (lookups lock or no concurrent protocol), libcuckoo (values move),
folly AtomicHashArray (no reuse of erased cells), AtomicUnorderedMap (no delete). Hash: A/B a
one-multiply mixer and `crc32cx` + multiply for 4/8 B keys, xxh3 for longer, seeded per map;
measure separately from the table (better mixing loses the address locality worth 3.1% on u64
addresses, `reviews-20261031/fixes/hash-ab2`).

## 3. Not done yet

- Kernel side by side through BPF programs (uprobes) per key pattern and load: feasible by giving
  `map-table-20261028/src/trackloop.c` a key-pattern option and `track.bpf.c` key-size variants;
  the bpf() numbers above include system calls and only show the kernel's cost is flat in load.
- Multi-thread versions of findings 1-3.
- No fix: per the measure-all-then-plan rule, findings go to one plan.
