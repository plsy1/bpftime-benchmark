# More dimensions: what the earlier rounds did not measure (2026-11-06)

The dimensions left after the event output rounds (`../output-dims-20261106`, whose rounds 3-4
ran the same afternoon): helpers' memory reads, process start-up, the traced program's tail
latency, many probes, memory footprint, long runs. Kernel bpf (`6.8.12-1021-tegra-btf`) against
the frozen bpftime build `fix-final` (dev d46ceb8f) unless a part says otherwise. One directory
per part; `common.py` holds the shared runner pieces (the loader is
`output-dims-20261106/src/outattach`). Batches: `n1-probe-read/go*.sh`, `go-rest.sh` (with
output-dims round 4), `go-last.sh`; times in `go-rest.log` and `go-last.log`. cpu5 at 1728000
before and after every batch; no two batches at once.

| part | directory | result in one line |
|---|---|---|
| 1 probe reads | `n1-probe-read` | without checks (every build compared so far) a bad pointer kills the traced process; with them a read is a system call (255-byte string 2.1 us, kernel 0.12) |
| 2 start-up | `n2-startup` | every traced process starts 22-27 ms late (kernel +0.01-0.04 ms); 3 s with the agent preloaded and no tool |
| 3 tail latency | `n3-tail` | nginx under sslsniff: tail no longer, median lower than the kernel's, throughput +14% (+5.5% with checked reads) |
| 4 many probes | `n4-many-probes` | 0.42 ms per probe per process start (1,000 probes: 0.44 s); the call cost does not change |
| 4b many processes | `output-dims-20261106` R6, R7, R7b | in separate processes the ring buffer adds 3.6 us per event (kernel 0.33); with 64 producer processes on 6 CPUs a ring buffer call waits up to 40 ms (p99.9) |
| 5 footprint | `n5-footprint` | +38-40 MB resident per traced process (6.5-8.5 MB private); 256 KB of the 128 MB segment per perf-producing thread (600 threads: 107 samples fail); the idle tool polls |
| 6 long run | `n6-soak` | 2 h, 358 traced processes, 1.17 billion ring buffer events with hash churn: all delivered, none failed; the tool's memory and CPU flat |

Output items done in output-dims rounds 3-4: opensnoop's record size (settles F4), pending lost
counts (not a bug), pause lengths, the traced call's own duration, variable sizes, processes
against threads, the producers' profile, gethostlatency's delays.

## Problems found today, for the one improvement plan

From `output-dims-20261106` (findings 5, 6, 10-15) and the parts here, roughly by weight:

1. **Ring buffer wrap padding loses the wakeup** (output finding 5): every ring whose record
   sizes do not divide it (opensnoop's 8,248, any variable size) loses records in bursts and
   delays the rest to the 1 ms timeout. Fix in `consumer_at`: a consumer standing at the
   padding before the record has caught up.
2. **Probe reads are unchecked in every build compared** (n1): a bad pointer kills the
   application. The checked build is safe but costs a system call per read (two per 256 bytes
   of string): -7.5% nginx throughput, a 255-byte string 2.1 us. Needs a cheap fault-safe copy.
3. **perf output wakes only at half a shard** (output finding 6, R5, gethostlatency): events
   arrive 0.5 ms late on average. Honor `wakeup_events` when someone waits, with the cost
   measured (the reason for the watermark was -8% on ssl-nginx).
4. **Ring buffer reserve lock** (output findings 1, 12, 13, 14): 3.6 us per event with 4
   producer processes (kernel 0.33); glibc's out-of-line spin lock around several cross-core
   lines (positions, header, owner slot); a descheduled holder stalls others for tens of ms
   with more producers than CPUs. Shorter critical section, a better lock, and either a
   bounded spin then yield/futex or a lock-free reserve.
5. **Start-up per process** (n2, n4): 22-27 ms per traced process (16 ms of it loading the
   agent), 0.42 ms per probe, 3 s when no tool runs (fail fast instead of 60 x 50 ms).
6. **perf shards per thread** (n5): 256 KB of the segment per producing thread, ~500 threads
   exhaust 128 MB; the idle consumer scans every shard each millisecond (493 shards: 36% of a
   core). Per-CPU shards, or a shard pool, and an idle back-off.
7. **Smaller**: the 1 ms polling of an idle tool (2-7 ms of CPU per second); `get_handler`'s
   static-local guard as the visible stall after the last record's release stores (to check);
   fix-final 3% below 7ad5a152 on ssl-nginx (not investigated); the older sslsniff build
   removes bpftime's ssl-nginx lead (not investigated); the stale hash map comments the
   lecture session found (`bpftime_hash_map.hpp` lines 115 and 752).

Earlier open items (F1-F3, L11, problem 14, the override forwarder, x86 parking) are in
`../../reviews-20261105/fixes/README.md`.
