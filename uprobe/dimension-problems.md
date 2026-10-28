# Dimension coverage: problems found (running list)

Collected while running the dimension benchmarks (plan: `map-usage-20261027/README.md`); fixes
are planned together once the matrix is done. Each entry: what, evidence, where in the code.

| # | Problem | Evidence | Code |
|---|---|---|---|
| 1 | Hash maps serialize all threads on one spinlock, lookups included; cost per call grows from 0.12 us (1 thread) to 6.3 (4 threads, two clusters) and 10.7 (6); kernel 0.2-0.4 | `map-threads-20261028` | `bpf_map_handler::map_lock`, `map_lookup_hash` / `map_update_hash` / `map_delete_hash` |
| 2 | The probe path without maps degrades with threads (0.44 -> 2.70 us at 4 threads across clusters, 0.91 in one cluster): shared state touched on every hit, cause unknown | `map-threads-20261028` (control) | to profile |
| 3 | Lookup returns a pointer after releasing the map lock; a concurrent delete or update can change the value the program then reads (kernel: RCU keeps it valid) | code reading, not measured | `map_lookup_hash` |
| 4 | Per-CPU hash/array take no lock in the handler (`should_lock = false`) and none was found in the per-CPU hash; concurrent inserts of new keys may corrupt the container | code reading, not measured | `per_cpu_hash_map.*` |
