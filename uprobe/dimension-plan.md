# Dimension coverage: consolidated problems and improvement plan (draft, 2026-10-28)

Matrix run on the Jetson Orin (BTF kernel 6.8.12, jetson_clocks), bpftime dev HEAD 09421773
(official 177a1dc465ff where noted), same programs on both runtimes, every run checked:

| Benchmark | Directory | Runs |
|---|---|---:|
| Map usage of 16 real tools (what to benchmark) | `map-usage-20261027` | 16 tools |
| Entry/exit churn, key 4/8 B, value 8/112 B, 1-6 threads | `map-threads-20261028` | 120 + cluster run |
| Tracking table (memleak), 1K-900K live entries | `map-table-20261028` | 180 |
| Shared counters: hash, per-CPU hash, per-CPU array, array | `map-table-20261028` | 264 |
| 8 KB per-CPU scratch value (sslsniff) | `map-scratch-20261028` | 180 |
| Probe path profile (perf), 1-6 threads | `probe-path-profile-20261028` | 5 configs |
| Torn reads through a lookup pointer | `map-torn-20261028` | 36 |

What is already fine: one thread, bpftime's map operations cost the same as or less than the
kernel's (churn 0.12 vs 0.20 us, tracking table 0.50 vs 0.51 at 1K and 0.76 vs 1.08 at 900K,
8 KB copy 0.22 vs 0.35); key/value size barely matters (4->8 B key, 8->112 B value: +0.04 us);
arrays and per-CPU arrays stay flat at every thread count and gave correct results. The losses
are all about threads, plus one crash.

## Problems (detail and evidence: `dimension-problems.md`)

| # | Kind | Problem | Effect | Where |
|---|---|---|---|---|
| 4 | crash | Per-CPU hash inserts take no lock | traced process SIGSEGV/hang in 6 of 9 multi-thread runs with 64 keys (official too) | bpftime `per_cpu_hash_map_impl` |
| 5 | robustness | A process dying inside shm leaves the segment's locks unusable | loader cannot read maps or terminates (`lock_exception`) | bpftime shm locks, pid set |
| 1 | performance | Hash maps: one spinlock per map, lookups included | map cost 0.1-0.5 us at 1 thread -> 5-12 us at 4-6 threads (kernel 0.03-0.8) | bpftime `map_lock`, `bpftime_hash_map` |
| 2 | performance | Frida's per-function `trampoline_usage_counter`: atomic inc + dec on every hit | probe without map 0.21 us -> 2.25 (4 threads) -> 4.54 (6), kernel 4.19 at 6 | frida-gum interceptor |
| 6 | performance | Frida zeroes a ~3 KB invocation stack entry on every hit | about a fifth of the 0.21 us single-thread hit | frida-gum `gum_invocation_stack_push` |
| 3 | semantics | Lookup pointer + in-place update: values change under a reader | 6-10 torn reads per M reads; the kernel also tears (0.1-2.8) | bpftime hash update |

Open, small: bpftime 8 KB scratch at exactly 4 threads costs 0.76 us vs 0.22-0.26 at 1, 2 and 6
(reproducible, unexplained).

At 6 threads a hit with three hash operations costs bpftime ~15.5 us vs the kernel's ~6.7:
about 10.7 us is problem 1 and 4.8 us problem 2 (churn, `map-threads-20261028`). Fixing only one of them does not bring
bpftime below the kernel there.

## Plan (in order; each fix gets a test that fails without it and a check against official)

1. **Problem 4, per-CPU hash crash** (small, bpftime only). **Done 2026-10-28** (dev `cdc05f67`, `c98951a4`, plus kernel update flags; `percpu-hash-fix-20261028`). User decision: multi-thread performance (steps 2-3) deferred; Frida problems are Frida's, not bpftime's. Serialize per-CPU hash updates and
   deletes like the hash map, and make lookups safe against a concurrent insert (the
   container can rehash), or move the per-CPU hash onto the fixed-size open-addressing table
   the hash map already uses (no rehash). Test: concurrent-insert unit test (crashes without
   the fix), then the counter benchmark (64 keys, 2-6 threads) must pass every run.
2. **Problem 1, hash map lock** (medium, bpftime only). `bpftime_hash_map` is preallocated
   open addressing with linear probing and tombstones, never rehashed, so:
   a. lock-free lookups (per-bucket sequence counter, retry on change): covers the counter
      pattern entirely and one of the three operations of churn;
   b. then finer write locking (lock stripes over bucket ranges, with care for probe runs that
      cross a stripe) for insert and delete.
   Re-run churn, tracking table and counters after each step; single-thread cost must not get
   worse.
3. **Problems 2 and 6, Frida** (large, outside bpftime). bpftime links the prebuilt frida-gum
   16.1.2 devkit; both costs are in Frida's interceptor (still present in Frida main). First a
   feasibility step: build frida-gum 16.1.2 from source on the Jetson and link bpftime against
   it. Then: (2) replace the shared counter with per-thread or sharded counters that detach sums
   (or an epoch scheme), keeping detach safe; (6) stop clearing the whole stack entry. Measure
   with the probe-path profile and the controls. Candidate for an upstream Frida change.
4. **Problem 5, crash robustness** (medium). **Recorded, not fixed (user decision 2026-10-28)**; measurement and options in `dead-holder-20261028`. Find which shm locks a dead process can hold
   (pid set, handler table, map spinlocks); use robust mutexes with owner-death recovery where
   possible, and at least keep the loader from terminating. Test: kill a traced process inside
   a map operation, the loader must keep working.
5. **Problem 3, torn reads**: no change proposed; it is within kernel semantics. Document it.
   Revisit only if step 2 changes the update path anyway.

6. **Problem 7, per-CPU maps with unpinned threads** (found after this plan): recorded, not fixed (user decision 2026-10-28); option in `percpu-unpinned-20261028`.

Not in this plan: the ARM64 syscall trampoline (next phase), upstream PRs (deferred).
