# Runtime map usage of real tools on bpftime (2026-10-27/28)

Step 1 of the dimension coverage plan: how real uprobe tools actually use their maps at run
time (operations per traced call, operation mix, misses, inserts/deletes, threads), to choose
the dimensions of the controlled bpftime-vs-kernel benchmarks. The static survey
(`../jit-eligibility-survey-20261021`) gave map types and key sizes, not frequencies.

## Method

- Build `mapusage` (`../ssl-nginx-2x2-20261023/builds/mapusage`): dev branch `627a2201` plus
  the opt-in counters `BPFTIME_ENABLE_MAP_USAGE_STATS=ON` (map operations counted in the
  `bpf_map_handler` entry points; array lookup JIT off so every access is counted; LLVM 15,
  no LTO). Counting only, never for timing.
- Harness: the BTF-kernel functional audit (`../btf-kernel-20261026/realprog-audit`), copied to
  `src/harness` with one change: the `mapusage` backend passes
  `BPFTIME_MAP_USAGE_STATS_FILE=stats/<tool>.jsonl`; each process (tool loader through the
  syscall server, traced workload through the agent) appends one line per map at exit.
- 16 tools (the audit's 18 minus `javagc`, which needs `bpftime attach`, and `opensnoop`,
  which records nothing on arm64). Every tool's output matched its workload exactly
  (`audit.jsonl`), as in the audit; the counters do not change behaviour.
- `summarize.py` → `summary.md`. "Prog ops/call" = program-side map operations per traced
  call (the workload's exact call count from the audit).

`failed-run1/`: the first run hung for 15 h at memleak / malloc-example. The counters
allocated under their lock, the allocation hit the malloc probe, whose program re-entered the
counters on the same thread (first while counting, then in the exit dump): deadlock at the
traced process's exit. Fixed with a per-thread re-entry guard on every lock holder, each with a
test that hangs without it; the harness had no overall timeout (now always set).

## Results (`summary.md`)

| Tool | Map | Type | Key | Value | Prog ops/call | Pattern |
|---|---|---|---:|---:|---:|---|
| funclatency (libbpf-tools) | starts | hash | 4 | 8 | 3.00 | insert at entry, lookup + delete at return |
| funclatency (example) | starts | hash | 4 | 8 | 2.00 | update existing key at entry, lookup at return |
| gethostlatency | starts | hash | 4 | 112 | 3.00 | insert / lookup + delete |
| sslsniff (example) | start_ns, bufs | hash | 4 | 8 | 3.00 each | insert / lookup + delete |
| sslsniff (example) | ssl_data | percpu_array | 4 | 8256 | 1.00 | scratch buffer lookup |
| memleak | sizes | hash | 4 | 8 | ~3 per malloc | insert / lookup + delete (tid) |
| memleak | allocs | hash | 8 | 24 | ~3 per malloc/free | insert per malloc, lookup + delete per free; ~1,000 live, max 1M |
| malloc example | libc_malloc_cal | hash | 4 | 8 | 2.00 | lookup + update existing (per-pid counter) |
| bpftrace gethostlatency | @start, @name | hash | 8 | 8 | 4 + 3 | insert / lookups + delete |
| bpftrace uretprobe hist | @s | hash | 8 | 8 | 4.00 | insert / lookups + delete |
| bpftrace uretprobe hist | @lat | percpu_hash | 8 | 8 | 1.00 | histogram bucket, 6 keys |
| bpftrace uprobe count | @c | percpu_array | 4 | 8 | 1.00 | counter |
| bpftrace ustack | @ | percpu_hash | 16 | 8 | 1.00 | count per stack id, 1 key |
| perf output (bashreadline, gethostlatency, sslsniff) | events | perf_event_array | 4 | 4 | 1.00 | output |

- **Dominant pattern: entry/exit pairing on a nearly empty hash map.** Insert a start value at
  function entry, look it up and delete it at return: 6 of the 16 tools, 3-4 operations per
  call, keys 4 B (tid) or 8 B (pid_tgid, address), values 8-112 B, maps sized 4K-100K but
  holding only the in-flight calls. The official uprobe benchmark does not measure this
  sequence (it repeats lookup-hit, update-existing or delete-hit separately).
- **Tracking table with many live entries**: memleak `allocs` (8 B address keys, 24 B values,
  about a thousand outstanding entries here, up to 1M).
- **Aggregation**: per-key counters and histograms (hash update-existing, per-CPU hash/array
  lookup-hit) on a handful of keys.
- **Misses are rare**: lookup and delete misses 0% in every map; inserts never failed.
- **Threads: 1 everywhere**, because every workload here is single-threaded. The thread
  dimension, where bpftime's whole-map spinlock (taken even for lookups) differs most from the
  kernel (lock-free lookups, per-bucket locks), is not covered by these tools' workloads.
- Loader-side `bpf()` operations are few (reading results at the end or once a second); memleak
  reads 7,451 `allocs` entries.

## Proposed controlled benchmarks (kernel vs bpftime, same programs)

1. **Entry/exit churn**: uprobe at entry inserts (key = tid or pid_tgid), uretprobe looks up and
   deletes; key 4/8 B, value 8/112 B, max 10240; threads 1/2/4/6, each with its own keys.
2. **Large tracking table**: memleak-like, 8 B random address keys, 24 B values, 1K/100K/1M live
   entries, insert/lookup/delete; threads 1/4.
3. **Shared counters**: update-existing / per-CPU lookup on 1-16 keys shared by 1/2/4/6 threads
   (contention on the same map and key).
4. **Large per-CPU scratch value** (8 KB) lookup and write.

Each with a same-run kernel leg and a checksum of the results; threads are where bpftime is
expected to lose and should come first.

## Files

`audit.jsonl`, `audit.log`, `audit/` (raw outputs; files over 2 MB trimmed to their first and last MB), `stats/<tool>.jsonl` (per process and map),
`summary.md`, `summarize.py`, `src/harness/`, `failed-run1/`, `failed-smoke1/`.
