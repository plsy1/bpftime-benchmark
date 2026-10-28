# Map tables under threads: tracking table and shared counters (2026-10-28)

Second and third benchmark of the dimension coverage (plan: `../map-usage-20261027/README.md`,
first: `../map-threads-20261028`). Both reproduce the map patterns the survey of 16 real tools
found, with the thread count varied (1, 2, 4, 6; thread i pinned to CPU 5 - i, so 4+ threads
span both clusters) and checked results.

Backends: `none` (no probe), `kernel` (libbpf loader, kernel uprobes, BTF kernel 6.8.12),
`09421773` (dev HEAD, frozen build `../ssl-nginx-2x2-20261023/builds/09421773`, LLVM JIT,
syscall-server + agent; shm 1024 MB for the tracking table, 256 MB for the counters). jetson_clocks on, cpu5 at 1728000 before and after.
Map cost = program minus the same program without map operations (control) on the same
backend; times are per call per thread (threads / calls per second), medians of 3 reps.

Sources in `src/` (`build.sh`): `attach.c` generic loader (prints the per-thread .bss slots, and
with `SUM_MAP` the sum and entry count of a map at the end), `trackloop.c` + `track.bpf.c`,
`countloop.c` + `count.bpf.c`.

## Tracking table (memleak pattern)

`trackloop THREADS SECS LIVE`: each thread owns LIVE/THREADS outstanding unique 64-bit
"addresses"; one op = `free_event(oldest)` + `alloc_event(new)`. The program inserts
`addr -> {size, ts, stack}` (8 B key, 24 B value, hash, max 1M) at alloc and does lookup +
delete at free, so the table stays at LIVE entries. Checked: probe-seen allocs and frees equal
the workload's, no missed free. Run 2: 180 runs, 0 failed (`run2-track.jsonl`,
`summary-track-run2.md`). Run 1 (`failed-run1-duplicate-addresses/`) drew random addresses that
repeated about once per run at 100K+ entries; every failure was one missed free on both
backends, so the generator became a bijection.

Map cost per op (3 map operations), us:

| Live | Threads | kernel | bpftime | kernel probe w/o map | bpftime probe w/o map |
|---:|---:|---:|---:|---:|---:|
| 1,000 | 1 | 0.51 | 0.50 | 2.53 | 0.49 |
| | 2 | 0.41 | 0.78 | 3.64 | 0.75 |
| | 4 | 0.25 | 5.23 | 6.79 | 4.23 |
| | 6 | 0.03 | 10.49 | 7.61 | 5.62 |
| 100,000 | 1 | 0.65 | 0.60 | 2.52 | 0.48 |
| | 6 | 0.25 | 11.17 | 7.59 | 5.75 |
| 900,000 | 1 | 1.08 | 0.76 | 2.52 | 0.48 |
| | 2 | 0.91 | 1.47 | 3.73 | 0.73 |
| | 4 | 0.83 | 5.51 | 6.70 | 4.18 |
| | 6 | 0.75 | 11.62 | 7.74 | 5.69 |

Totals per op (us), 1K live: kernel 3.04 / 4.05 / 7.04 / 7.64, bpftime 0.99 / 1.53 / 9.46 /
16.11 for 1 / 2 / 4 / 6 threads.

- One thread: bpftime's map cost equals the kernel's at 1K entries and is lower at 900K
  (0.76 vs 1.08 us); both grow with the table (cache misses).
- Threads: bpftime's map cost grows to 5.2 (4) and 10.5-11.6 us (6) at every table size: the
  whole-map spinlock (problem 1). Above 2 threads the bpftime total is worse than the kernel's.
- The kernel's map cost shrinks with threads. Its uprobe path is itself contended (probe w/o map
  2.5 -> 7.6 us) and the map work overlaps that waiting, so the subtraction understates it;
  read it as "not the bottleneck", not as a real speedup.

## Shared counters (counter pattern)

`countloop THREADS SECS NKEYS`: every thread calls `count_event(key)` with keys cycling over
0..NKEYS-1, so all threads update the same counters. The program does what tools do (`KIND`):
hash with lookup + atomic add and a `BPF_NOEXIST` insert on a miss, per-CPU hash with lookup +
plain add and insert, per-CPU array, ordinary array with atomic add, control without map.
Checked: probe hits = calls, no failed update, sum of the map (per-CPU values summed over all
CPUs) = calls. 264 runs, 6 failed (`run1-count.jsonl`, `summary-count-run1.md`).

Map cost per call, us (NKEYS 1 / NKEYS 64):

| Threads | kernel hash | bpftime hash | bpftime percpu-hash | bpftime percpu-array | bpftime array |
|---:|---:|---:|---:|---:|---:|
| 1 | 0.02 / 0.01 | 0.03 / 0.03 | 0.04 / 0.04 | 0.02 / 0.02 | 0.01 / 0.01 |
| 2 | 0.00 / -0.01 | 0.20 / 0.21 | 0.08 / crash | 0.04 / 0.04 | 0.05 / 0.03 |
| 4 | -0.04 / -0.08 | 1.21 / 0.49 | 0.02 / crash | 0.01 / 0.02 | -0.03 / -0.02 |
| 6 | 0.02 / 0.10 | 1.36 / 1.04 | 0.11 / crash | 0.01 / 0.10 | 0.08 / 0.05 |

Kernel per-CPU hash, per-CPU array and array are within +-0.2 us of zero at all thread counts
(noise of the subtraction). Probe w/o map: kernel 1.24 / 1.85 / 3.35 / 4.19 us, bpftime
0.21 / 0.28 / 2.25 / 4.54 us (NKEYS 1).

- Hash counters: the hits are lookups plus an atomic add in the program, no update helper, yet
  bpftime's cost grows to 1.2-1.4 us at 4-6 threads: the lookup takes the whole-map lock
  (problem 1). Arrays and per-CPU maps without inserts stay flat on bpftime.
- **Per-CPU hash crashes with concurrent inserts** (problem 4, now measured): with 64 keys and
  2+ threads, 6 of 9 bpftime runs failed: the traced process died with SIGSEGV (5 runs) or hung
  until the timeout (1). With 1 key (one insert per run) and on one thread, all runs were
  exact. `per_cpu_hash_map_impl::elem_update` finds and inserts into the shared container
  with no lock (`should_lock = false`, also in official 177a1dc465ff); the kernel uses bucket
  locks. Successful 64-key runs had exact sums.
- After the crash the loader could not read the map (`failed to read .bss`) and logged
  `Unable to lock bpftime pid set: boost::interprocess::lock_exception`, or itself terminated
  with that exception: a process dying inside shared memory leaves the segment's locks
  unusable for every other process (problem 5).

## Files

- `run_track.py`, `run_count.py`: runners (backends, objects, threads, sizes, reps; each run
  bounded by a timeout; the loader creates the shared segment at each start, and the runs
  after each crash passed).
- `summarize.py`, `summarize_count.py`: tables above.
- `run2-track.*`, `run1-count.*`: raw results and logs. `smoke/`: smoke runs.
