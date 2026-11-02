# Hash map: assessment and plan (2026-11-03)

Inputs: `README.md` (structure under 12 key patterns, designs summary), `designs.md` (mature
designs against bpftime's constraints), `../hash-keypattern-20261103` (kernel side by side
through BPF programs, 1 and 6 threads).

## Assessment: is the textbook table the problem?

Partly. Four problems, of two origins:

| # | Problem | Origin | Size (worst measured) | Kernel |
|---|---|---|---|---|
| 1 | Churn above ~70% full: inserts lock every stripe to borrow tokens | the 10-30 striped writers (tokens for exact capacity) | 1 thread 10x (10240) to 56x (102400) the kernel; 6 threads 230 us per op | flat, 0.3-0.6 us |
| 2 | Keys with fixed low bits cluster (`id << 12`) | the textbook structure: weak hash + linear probing | a7d54d7e 13.7 us (1 thread), 95.8 us (6 threads) at 102400 / 90%; +4 us at 6 threads in half-loaded 10240 maps | jhash + chains: unaffected |
| 3 | Tombstones accumulate under churn | the structure (filled buckets cannot move under lock-free lookups) | hit/miss 1.3-2x slower after churn at 90% | none |
| 4 | Values stored twice (2x buckets) | the structure | 34-162 B per entry; large values double | 48 B header + key + value |

Not a problem: half-loaded maps with ordinary keys. One thread 0.4-0.9x the kernel, six threads
no measurable map cost, all ten tracked suite operations below the kernel.

Problem 1 is ours and the most severe. It is the common case for small maps: max_entries 10240 at
70-90% with keys that change (new pids/tids, connections, requests). Earlier benchmarks missed
it because they reused keys or ran at low load.

## Plan

### Step 1. Remove the token cliff in today's table (regression fix, ~1-2 days)

Replace the per-stripe tokens with an exact shared count: `fetch_add` on insert and `fetch_sub`
on delete, an insert that would pass max_entries undoing its add (E2BIG), on its own cache line.
This removes the all-stripes path, `TOKEN_BATCH` and the parked tokens. Cost: one contended atomic
per insert and delete (a cross-cluster line transfer, ~50-100 ns at six threads, against today's
230 us at 90% and a7d54d7e's 10 us map lock), nothing single-thread.

Alternative if that atomic costs too much at six threads: keep the tokens, but cap what a stripe
parks (return above `TOKEN_BATCH`), and when the pool is empty take tokens from other stripes one
lock at a time (own lock released first). This is more code, and the cliff only gets shallower.

- Test: churn at 90% of a 10240 and a 102400 map, 1 and 6 threads, must stay within 2x of half
  load. Mutation: restore the tokens. The `[tokens]` and exact-capacity tests stay: E2BIG only
  when all max_entries are used, under concurrent inserts.
- Measure: this matrix (`run_keys.py`), churn, tracking table, counters, the suite's hash cases.

### Step 2. Decide on the redesign with step 1's numbers (Candidate A in `designs.md`)

Candidate A: a preallocated element pool `{gen, next, key, value}` plus u32 chain heads
(kernel htab in offset form), a seqlock per element, nulls-style chain terminators, a freelist
for exact capacity (a 64-bit {index, tag} CAS stack that survives a dying process). It fixes 2-4
and makes step 1 unnecessary. Cost: 1-2 weeks, about half of it moving the lock-free lookup,
striped writer, cross-process, fork and alignment tests over. Risk: two cache lines per lookup
(head, element); measure 4 and 8 B keys, hit and miss, at 50% and 90%.

Hash function (problem 2), measured separately: a one-multiply mixer or `crc32cx` plus a multiply
for 4 and 8 B keys, xxh3 above. Better mixing loses the address locality worth 3.1% on u64
addresses (`reviews-20261031/fixes/hash-ab2`).

### Order

Step 1 first: it is a regression on the default path, small, and step 2's comparison needs a
baseline without the cliff. Then a go/no-go on step 2 from the user.
