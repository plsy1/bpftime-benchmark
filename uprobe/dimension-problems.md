# Dimension coverage: problems found (running list)

Collected while running the dimension benchmarks (plan: `map-usage-20261027/README.md`); fixes
are planned together once the matrix is done. Each entry: what, evidence, where in the code.

| # | Problem | Evidence | Code |
|---|---|---|---|
| 1 | Hash maps serialize all threads on one spinlock, lookups included; cost per call grows from 0.12 us (1 thread) to 6.3 (4 threads, two clusters) and 10.7 (6); kernel 0.2-0.4; tracking table (memleak pattern, 1K-900K entries) 0.5 -> 10.5-11.6 us at 6 threads; hash counters that only look up + atomic add 0.03 -> 1.4 us | `map-threads-20261028`, `map-table-20261028` | `bpf_map_handler::map_lock`, `map_lookup_hash` / `map_update_hash` / `map_delete_hash` |
| 2 | The probe path without maps degrades with threads (0.44 -> 2.70 us at 4 threads across clusters, 0.91 in one cluster): shared state touched on every hit, cause unknown; same in the other two benchmarks (0.21 -> 4.54 us at 6 threads, kernel 4.19) | `map-threads-20261028`, `map-table-20261028` (controls) | to profile |
| 3 | Lookup returns a pointer after releasing the map lock; a concurrent delete or update can change the value the program then reads (kernel: RCU keeps it valid) | code reading, not measured | `map_lookup_hash` |
| 4 | **Per-CPU hash crashes the traced process on concurrent inserts.** The handler takes no lock (`should_lock = false`, same in official 177a1dc465ff) and `elem_update` finds/inserts into the shared container unlocked: 64 keys, 2-6 threads: 6 of 9 runs died (5 SIGSEGV, 1 hang); 1 key or 1 thread exact | `map-table-20261028` (counters) | `per_cpu_hash_map_impl::elem_update`, `map_update_percpu_hash` |
| 5 | A process that dies inside the shared segment leaves its locks unusable for all others: after the per-CPU hash crash the loader logged `Unable to lock bpftime pid set: boost::interprocess::lock_exception`, could not read the map, or terminated with that exception (kernel: a traced process's crash does not affect the maps or the loader) | `map-table-20261028` (counter failures) | shm mutexes (boost interprocess), pid set |
