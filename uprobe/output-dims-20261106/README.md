# Event output by dimension: kernel bpf vs bpftime (2026-11-05/06)

Item 3 of the next-direction list: the dimension method of the hash map work
(`../map-threads-20261028`, `../hash-keypattern-20261103`) applied to the event output
paths. Why output: of the 57 libbpf-tools here, 27 send events to user space (perf event
array or ring buffer); none uses LRU hash or map-in-map (hash: 80 map definitions in 48
tools, perf event array 22, cgroup array 13, stack trace 7, per-CPU 3).

bpftime: frozen build `fix-final` (dev d46ceb8f); kernel `6.8.12-1021-tegra-btf`. cpu5 at
1728000 before and after; 2026-11-05 23:56 to 11-06 00:47 (M1-M3, 446 runs), 00:57 to 01:31
(L1-L3, 352 runs), 13:42 to 15:16 (rounds 3-4, R1-R8 and the profile, 426 runs; one ~5 s
compile overlapped R3 around 13:56); no other runs; all checked (hits == the workload's calls,
received + failed == hits; exceptions named where they occur).

## Setup (`src/`, `run.py`, `go.sh`)

- `out.bpf.c`: a uprobe on `count_event` sends a SIZE-byte record (8, 256, 4096) by
  `bpf_perf_event_output` (perf), `bpf_ringbuf_output` (rbout) or `bpf_ringbuf_reserve` +
  submit (rbres, opensnoop's path); records above 256 B are built in a per-CPU array, as
  libbpf-tools' `bpf_buffer` does. Ring buffer 256 KiB and perf buffer 64 pages per CPU, as
  libbpf-tools. `out-control` = the probe without output.
- `outattach.c`: loads, attaches, a consumer thread in `perf_buffer__poll` /
  `ring_buffer__poll` (100 ms); `CONSUMER_NS` spins per record (a tool formatting a line).
- Workloads: `countloop` (steady, as fast as the probe allows; thread i on CPU 5 - i) and
  `burstloop THREADS SECS BURST PAUSE_US GAP_NS PERIOD_NS` (bursts every 10 ms, calls GAP_NS
  apart, or with PERIOD_NS call j at the burst's start + j * PERIOD_NS by the clock: the same
  arrival rate whatever the probe costs). Each call passes its CLOCK_MONOTONIC send time as
  the record's first word; with `LATENCY=1` the consumer takes every record's delay (its clock
  on entering the callback minus the send time) and that of the first record of each burst
  (sent more than 1 ms after every record seen before it: it reaches a consumer that caught
  up and slept, so its delay is the wakeup latency), and prints percentiles.
- Matrices: M1 steady, all paths and sizes, 1/2/4/6 threads, 3 reps, consumer 0; M2 bursts of
  10/100/1000, gap 0 or 2 us, consumer 2 us per record, 1/4 threads, 2 reps; M3 steady with
  that consumer. Tables: `summary.md` (`summarize.py`).
- Latency matrices (`go-lat.sh`, `go-lat3.sh`): L1 bursts of 10/100/1000 at matched arrival
  (calls 4 us apart, above both probes' cost), perf and ringbuf reserve at 256 B and 4 KB,
  consumer 0 or 2 us per record, 1/4 threads, 2 reps of 4 s; L2 M2's back-to-back bursts of
  100 with the delays; L3 ringbuf records of 248 and 4,088 bytes against 256 and 4,096 (L1's
  settings, bursts of 100/1000). Tables: `summary-lat.md` (`summarize_lat.py`).

## Findings

**1. Ring buffer output does not scale with producers.** Output cost per event (ns per call
minus the control's), bpftime / kernel, 8-byte records (both sides deliver 100%, so the
cost is not distorted by drops):

| path | 1 thread | 2 threads | 4 threads | 6 threads |
|---|---:|---:|---:|---:|
| perf | 0.16× | 0.15× | 0.14× | 0.06× |
| ringbuf output | 0.60× | 1.47× | 2.22× | 7.14× |
| ringbuf reserve + submit | 0.64× | 1.54× | 2.49× | 8.96× |

In ns: ringbuf reserve 455 / 778 / 1608 / 1935 (bpftime) against 712 / 505 / 647 / 216
(kernel). The perf path scales (bpftime's per-thread shards); the ring buffer is one ring
with one reserve lock in both, but bpftime's cost per event grows with every producer while
the kernel's does not. Not diagnosed yet: candidates are the reserve spin lock (pthread spin
lock, unfair) and the work under it (the owner-slot claim and identity reads added for dead
producers), and cache lines shared by producers (positions, headers).

**2. The consumer side is not slower.** With a consumer spending 2 us per record and steady
overload (M3), both deliver what the consumer can take: perf 256 B, one thread, kernel
100% of 434k/s, bpftime 16.2% of 2.61M/s = 423k/s; ringbuf 256 B, kernel 64.5% of 694k/s =
448k/s, bpftime 14.6% of 3.06M/s = 446k/s. Steady without consumer cost (M1), bpftime
delivers more events per second than the kernel (perf 4 KB, 2 threads: 1.39M/s against
463k/s), although a smaller fraction of its (3x more) events.

**3. Bursts lose more 4 KB events under bpftime.** M2, 4 KB records, consumer 2 us: the ring
buffer holds 63 of them, and a burst of 100 takes 50 us under bpftime (146 us under the
kernel): bpftime's faster probes give the consumer a third of the time. But that is not the
main cause: at matched arrival rates the ring buffer still loses them (finding 5). Delivered:

| setting (ringbuf reserve, 4 KB) | kernel | bpftime | burst time kernel / bpftime |
|---|---:|---:|---:|
| burst 100, back to back, 1 thread | 100% | 77.1% | 146 / 50 us |
| burst 100, back to back, 4 threads | 89.8% | 64.5% | 196 / 71 us |
| burst 100, 2 us apart, 1 thread | 100% | 98.8% | 442 / 277 us |
| burst 100, 2 us apart, 4 threads | 100% | 90.0% | 436 / 302 us |

perf at 4 KB behaves alike at larger bursts (burst 1000 back to back: kernel 100% / 76.6%,
bpftime 25.2% / 22.5% at 1 / 4 threads); 256-byte records lose nothing below bursts of 1000.
The 2 us gap did not equalize the arrival rate (the kernel's probe still adds about 2 us per
call); findings 5 and 6 compare at matched rates and measure the delays.

**4. With all six CPUs producing, both lose most ring buffer events** (M1, 6 threads, 256 B
and up: the kernel 1.1-10.6%, bpftime 46-60% delivered; perf 4 KB 44-46% both): the consumer thread has no CPU of its own. Not a
bpftime problem; a limit of the test at 6 threads on this 6-core board.

**5. At matched arrival rates bpftime's ring buffer still loses 4 KB records: its consumer
sleeps through each wrap of the ring.** L1, ringbuf reserve, calls 4 us apart in both (a
burst of 100 takes 397-400 us in both), consumer 0:

| setting | delivered kernel / bpftime | first record p50 / p90 us, kernel | bpftime | all records p99 us kernel / bpftime |
|---|---|---|---|---|
| 4 KB, burst 100, 1 thread | 100% / 94.9% | 7.6 / 13.5 | 10.9 / 206 | 9.0 / 1,052 |
| 4 KB, burst 100, 4 threads | 99.4% / 60.2% | 9.7 / 13.9 | 31.0 / 836 | 9.9 / 1,050 |
| 4 KB, burst 1000, 1 thread | 99.9% / 60.6% | 7.6 / 13.7 | 10.0 / 843 | 6.1 / 1,046 |
| 256 B, burst 100, 1 thread | 100% / 100% | 7.5 / 14.0 | 8.7 / 11.7 | 8.9 / 973 |

The wakeup itself is as fast (first record p50, one thread: kernel 7.5-9.8 us, bpftime
8.3-10.9 us, futex wake and scheduling); what differs is a tail at bpftime_epoll_wait's 1 ms
timeout. Cause: a record that does not fit before the end of bpftime's data area goes to the
start of the next lap, behind a padding record (a discard) filling the end
(`ringbuf::reserve`); the kernel maps its data pages twice, so its records cross the end and
nothing pads. The wakeup rule (`consumer_at`, 665aaa38) compares the consumer's position with
the record's: a consumer that caught up stands at the padding, not at the record, so the
record wakes nobody, and neither does any record after it (the consumer is behind them all).
It sleeps until the timeout while the ring fills. 4 KB records (4,104 bytes with the header)
pad every 63 records, so nearly every burst of 100 crosses a padding; 256 B ones every 992 (no
loss in a ring of 992, only the 1 ms delay). L3 confirms it: records that divide the ring
(248 and 4,088 bytes plus the 8-byte header) never pad, and bpftime delivers as the kernel.
Consumer 0, bursts of 100, delivered and all records' p99:

| | 4,096 B | 4,088 B | 256 B | 248 B |
|---|---|---|---|---|
| kernel, 1 thread | 100%, 8.7 us | 100%, 7.7 us | 100%, 8.2 us | 100%, 9.8 us |
| bpftime, 1 thread | 95.0%, 1,051 us | 100%, 9.2 us | 100%, 986 us | 100%, 10.5 us |
| kernel, 4 threads | 99.9%, 8.2 us | 99.9%, 8.0 us | 100%, 9.1 us | 100%, 11.0 us |
| bpftime, 4 threads | 55.4%, 1,050 us | 99.7%, 18.3 us | 100%, 975 us | 100%, 10.8 us |

(bursts of 1000, one thread: 4,096 B 58.2%, 4,088 B 100%.) This is very likely the rest of
opensnoop's loss (`reviews-20261105/fixes`, finding F4: 8,240-byte records, 8,248 with the
header, pad every 31). Without padding, bursts of 1000 on 4 threads still lose a little more
than the kernel (4,088 B: 96.6% against 99.0%; 248 B: 98.6% against 99.7%, with a 2 ms p99),
probably the multi-producer cost of finding 1 (not looked into).

**6. bpftime's perf output wakes its consumer only at half a shard; the kernel wakes it for
every record.** libbpf's `perf_buffer__new` opens its events with `wakeup_events = 1`, and the
kernel wakes a waiting consumer for each sample. bpftime does not read the attribute: a
producer wakes the consumer when its shard is half full or it drops a sample (the
`wakeup_watermark` rule of the perf output work: waking on every sample cost ssl-nginx 8% of
its requests per second), and leaves the rest to the 1 ms timeout. It loses nothing by it here
(L1: 98.9-100% in both, except bursts of 1000 at 4 KB on 4 threads with the 2 us consumer,
kernel 68.7%, bpftime 77.3%), but delivers late: first record of a burst, p50 kernel 8.4-12.4
us; bpftime at 256 B 505-550 us on one thread, 188-595 us on four (a burst never reaches half
a shard: uniform over the timeout), at 4 KB 148-158 us in bursts of 100 and 1000 (the time to
fill half a shard at 4 us per record; bursts of 10: 328-541 us); all records p99 at 256 B on
one thread 0.75-1.06 ms against 7-20 us: a tool shows events up to 1 ms late. Back to back
(L2), perf
4 KB on 4 threads delivered 86.9% against 99.6%.

## Round 3 (`go-r3.sh`, 13:42-14:03; more-dims-20261106 list)

**7. opensnoop's own record size loses its records; one that divides the ring does not.** R1,
ringbuf reserve, calls 4 us apart, bursts every 10 ms, 2 reps of 4 s. 8,240-byte records (8,248
with the header) leave a padding every 31 records; 8,184-byte ones (8,192) never pad.
Delivered, kernel / bpftime:

| burst, consumer, threads | 8,240 B | 8,184 B |
|---|---|---|
| 50, 0, 1 | 100% / 76.4% | 100% / 100% |
| 50, 0, 4 | 100% / 55.1% | 99.8% / 99.7% |
| 50, 2 us, 1 | 100% / 88.9% | 100% / 100% |
| 100, 0, 1 | 100% / 63.8% | 100% / 99.8% |
| 100, 0, 4 | 98.2% / 33.8% | 97.6% / 99.4% |
| 100, 2 us, 4 | 99.2% / 57.2% | 98.4% / 99.9% |

With 8,240 B bpftime's first records of a burst wait 12-356 us at the median and 0.35-0.92 ms at
p90, every run's p99 is at the 1 ms timeout (1,052-1,054 us); with 8,184 B the first record
takes 9.0-9.6 us (kernel 7.5-8.6) and p99 of all records stays below 56 us. This settles F4
(`reviews-20261105/fixes`): opensnoop loses its records to finding 5, not to the consumer's
system calls. (opensnoop itself delivered 94-95%: its bursts of `open` calls are slower than
4 us apart.)

**8. The perf lost counts that fell short of the failed outputs were pending, not lost.** R2,
perf 256 B and 4 KB, bursts of 1,000 back to back every 1 ms, consumer 2 us per record, 1 and 4
threads, 3 reps; each thread makes 3 more calls 50 ms after the run (`FLUSH=1`): the lost
callback's count equals the producers' failed outputs in all 24 runs, kernel and bpftime
(e.g. bpftime 4 threads 6,177,295 = 6,177,295). The shortfalls of rounds 1-2 (22 of 144
bpftime perf runs, 8 of 144 kernel) are counts still waiting for a sample to carry them when the
loader stopped (both write a lost record only with the next sample that fits); bpftime has one
shard per thread, so more of them wait.

**9. Waking a consumer that slept longer costs both the same; perf output is always late.**
R3, bursts of 100 at matched arrival, one thread, pauses from 0.5 ms to 1 s (deeper idle
states), first record of each burst (more than half a pause after the previous record):

| pause | kernel first p50 / p90 us | bpftime ringbuf 4,088 B | bpftime ringbuf 4,096 B (pads) | bpftime perf 256 B |
|---|---|---|---|---|
| 0.5 ms | 7.6 / 8.1 (perf), 6.3 / 6.8 (rb) | 7.8 / 8.7 | 8.0 / 833, delivered 70.6% | 539 / 957 |
| 10 ms | 8.6 / 14.8, 9.3 / 15.2 | 9.2 / 13.9 | 10.3 / 202, 94.8% | 550 / 959 |
| 100 ms | 19.5 / 24.9, 20.8 / 29.3 | 13.7 / 26.8 | 156 / 892, 82.5% | 527 / 961 |
| 1 s | 28.6 / 35.2, 31.5 / 36.5 | 36.7 / 42.7 | 36.2 / 529, 90.4% | 488 / 934 |

The futex wakeup follows the kernel's at every idle depth (8 us after 0.5 ms, 30-37 us after 1
s, the CPU's wake-up from idle). perf output never wakes the consumer for these bursts (half a
shard is never reached) and the timeout delivers them, uniformly over 0-1 ms; the padding loss
of finding 5 shows at every pause. (1 s pauses, 4,088 B: both lose 0.6-0.9% to the slower wake-up
with a 400 us burst into a 64-record ring.)

## Round 4 (`go-r4.sh`, 14:08-14:29; tables `summary-r4.md`)

**10. The traced call itself: cheaper in bpftime except when it wakes the consumer, which costs
both about the same.** R4, `CALL_HIST=1`: each `count_event` call timed by the workload (clock
before and after, 12.5% buckets, lower bounds), bursts of 100 at 4 us, 2 reps. ns:

| output, threads | kernel p50 / p99.9 / max | bpftime p50 / p99.9 / max |
|---|---|---|
| none (untraced) 1 | 64 / 80 / 6,241 | |
| control 1 / 4 | 1,280 / 3,840 / 48k; 1,280 / 4,608 / 48k | 288 / 1,856 / 108k; 288 / 2,560 / 138k |
| perf 256 B 1 / 4 | 3,584 / 9,728 / 51k; 3,072 / 10,752 / 57k | 432 / 3,712 / 315k; 576 / 4,096 / 286k |
| perf 4 KB 1 / 4 | 3,584 / 11,776 / 52k; 3,328 / 12,288 / 79k | 928 / 6,400 / 334k; 1,152 / 9,472 / 335k |
| ringbuf 256 B 1 / 4 | 2,432 / 7,680 / 45k; 2,944 / 9,216 / 105k | 1,792 / 7,936 / 180k; 1,152 / 15,360 / 218k |
| ringbuf 4,088 B 1 / 4 | 2,432 / 8,704 / 47k; 2,432 / 9,216 / 1.2M | 1,856 / 8,192 / 171k; 1,536 / 7,936 / 186k |
| ringbuf 4,096 B 1 / 4 | 2,432 / 7,936 / 48k; 2,560 / 8,192 / 48k | 544 / 6,400 / 189k; 512 / 5,632 / 189k |

A ring buffer record that finds the consumer asleep and caught up makes the producer wake it: in
bpftime a `FUTEX_WAKE` inside the probe, about 1.5 us of the call (256 B and 4,088 B: 1.8 us
against the control's 0.29); in the kernel the irq_work raised in the probe (2.4 against 1.3
us). With 4,096-byte records the call is cheap (0.54 us) because of finding 5: the producer does
not wake a consumer standing at the padding. perf output never wakes in these bursts (finding
6): 0.4-1.2 us against the kernel's 3.1-3.6. The tails (p99.9) are no worse than the kernel's;
each run's maximum is 0.1-0.3 ms under bpftime against 45-105 us (one 1.2 ms), the first calls
of a thread (Gum's per-thread context, the perf shard).

**11. Variable record sizes lose ring buffer records too.** R5, sizes 72/520/1,480/3,992 bytes
chosen per record (as tools that send what they filled), matched arrival. Delivered, kernel /
bpftime: ringbuf reserve bursts of 1,000 100% / 85.4% (1 thread), 98.6% / 70.8% (4); bursts of
100 on 4 threads 99.9% / 95.1%; `bpf_ringbuf_output` the same (85.7%, 81.1%, 96.5%). Any size
that does not divide the ring pads at some wrap, so finding 5 is not limited to fixed sizes that
happen to pad; it is every ring that wraps with sizes that do not divide it. perf with variable
sizes loses nothing and is late as in finding 6 (first record p50 181-376 us, kernel 8.9-10.3).

**12. In separate processes the ring buffer's cost per event stands alone: 11x the kernel's.**
R6, 4 producers as 4 threads of one process or 4 single-thread processes. Paced (bursts of 100 at
4 us) the layout changes nothing (`summary-r4.md`). Back to back (bursts of 10,000, no pause, 3
reps), ns per call:

| output | kernel 1 proc x 4 thr / 4 proc | bpftime 1 proc x 4 thr / 4 proc |
|---|---|---|
| control | 3,438 / 2,875 | 4,192 / 282 |
| perf 8 B | 5,400 / 4,204 | 4,044 / 846 |
| ringbuf reserve 8 B | 4,094 / 3,202 | 3,948 / 3,865 |

Within one process bpftime's probe itself is the bottleneck at 4 threads (Gum's per-function
usage counter, `vs-pre-mt-20261104`): the control costs 4.2 us, and the ring buffer run is even
cheaper than its control, because the ring's lock serializes the threads and relieves the counter.
So finding 1's "output cost per event" (ns per call minus the control's) mixed the two. In
separate processes the probe is 0.28 us, and the ring buffer adds 3.6 us per event (kernel 0.33
us: 11x); perf adds 0.56 us (kernel 1.33). The ring buffer's multi-producer cost is real and
large; the profile (below) looks at where it goes.

**13. Where the ring buffer's producers spend it: the reserve lock, then an acquire that waits
for the last record's stores.** `prof.py` (14:29-14:32): out-rbres-8, burstloop back to back,
threads on CPUs 5 / 5,4 (one cluster) / 5,3 (two clusters) / 5,4,3,2 / 5..0, ns per call 2 reps
without perf, then a flat `perf record -F 4999` profile (`prof/<backend>-<config>.txt`). ns per
call, kernel / bpftime: 1 thread 2,078 / 743; 2 same cluster 2,421 / 1,102; 2 across clusters
3,696 / 1,567; 4 4,104 / 3,865; 6 4,380 / 6,513. bpftime's samples:

| symbol | 1t | 2t same | 2t cross | 4t | 6t |
|---|---:|---:|---:|---:|---:|
| `pthread_spin_lock` + libc's `cas4_acq` + `swp4_acq` (the reserve lock, glibc's, out of line) | 11.4% | 23.4% | 22.4% | 67.2% | 74.9% |
| `handler_manager::get_handler` | 22.8% | 20.5% | 18.4% | 9.1% | 7.0% |
| `_gum_function_context_begin_invocation` (Gum's per-function counter) | 8.9% | 15.4% | 25.5% | 6.5% | 5.9% |
| `bpftime_ringbuf_reserve` + `submit_flags` + `try_get_ringbuf_map_impl` | 19.6% | 15.7% | 17.0% | 10.0% | 7.5% |

From 4 threads the lock is the cost. What it covers (`ringbuf::reserve`): loads of
`consumer_pos` (written by the consumer for every record) and `producer_pos`, the header stores
(a line the consumer reads), the owner-slot claim for dead producers (an acquire load, maybe a
CAS, three stores and a release store on the slot's line), the release store of `producer_pos`:
several lines that bounce to the consumer, all inside the critical section. The lock is glibc's
`pthread_spin_lock` through the PLT (the hash maps moved to the inline `bpftime_spin_lock` on
11-02; the ring buffer did not). `get_handler`'s share is almost all one instruction: the
acquire load (`ldar`) of the init guard of its function-local static `oob_handler`, whose cache
line nobody writes in these runs. An Armv8 `ldar` waits for the thread's earlier store-releases
to complete, so this is most likely the previous record's release stores to lines the consumer
holds being paid here; removing the guard may move the wait to the next ordered access rather
than remove it (to check in the plan). The kernel's samples are its uprobe machinery
(`caches_clean_inval_pou`, `el0_dbg`, the uprobe rwsem at 6 threads); `bpf_ringbuf_commit`
is 3-5%.

**14. With more producer processes than CPUs, a ring buffer call can wait tens of
milliseconds: the reserve lock's holder gets descheduled.** R7 (`go-r4.sh`) and R7b
(`go-r7b.sh`, 15:11-15:16): 6, 16 or 64 single-thread processes on the 6 CPUs, bursts of 10 at
4 us every 10 ms, 6 s, 2 reps, with the per-call durations (each process's p99, p99.9, max; the
worst and the median process):

| procs, output | calls made kernel / bpftime | median process p99 (us) | worst process p99.9 (us) | worst call (ms) | delivery p99 (ms) |
|---|---|---|---|---|---|
| 6, ringbuf 256 B | 35,675 / 35,620 | 8.2 / 10.8 | 14.8 / 21.5 | 0.05 / 2.4 | 0.015 / 0.047 |
| 16, ringbuf 256 B | 95,065 / 94,955 | 8.2 / 9.2 | 23.6 / 25.6 | 1.3 / 0.7 | 0.021 / 0.147 |
| 64, ringbuf 256 B | 378,895 / 323,745 | 8.2 / 16.9 | 70 / 39,846 | 9.1 / 72.4 | 0.29 / 8.7 |
| 64, perf 256 B | 378,470 / 375,490 | 9.2 / 6.7 | 164 / 229 | 7.2 / 32.9 | 0.067 / 1.08 |
| 64, control | 379,290 / 378,910 | 3.8 / 4.1 | 17 / 22 | 2.7 / 0.3 | - |

At 64 processes bpftime's ring buffer producers lose 15% of their calls' time (the workload
makes 14.6% fewer calls in the same 6 s), the worst process's p99.9 call takes 40 ms and the
worst record waits 72 ms. The kernel's ring buffer lock is a raw spin lock taken with
preemption off, so its holder cannot be descheduled while others spin; bpftime's is a
user-space spin lock, and with 64 runnable processes on 6 CPUs the holder is preempted for a
time slice now and then while the others spin. The control (no shared lock) and perf (a shard
per thread, no shared lock on the produce path) do not show it to this degree. R7 (no
per-call times): the same picture, delivery exact on both (`received` = the calls; with more
than 32 producers the BPF side's per-thread hit slots, `tid & 31`, can be shared and lose
increments, on both backends, so those runs are checked by the outputs instead).

**15. A real perf-buffer tool shows finding 6 as it is.** `r8-gethostlatency/` (14:32-14:34):
libbpf-tools gethostlatency (CO-RE build) with `resolveburst` making uniquely named
`getaddrinfo` calls (numeric hosts, no lookup), its stdout through `stdbuf -oL` on a pipe whose
lines are time-stamped on arrival; delay = line arrival minus the call's start, 3 reps:

| workload | delivered | kernel p50 / p90 / p99 (us) | bpftime p50 / p90 / p99 (us) |
|---|---|---|---|
| bursts of 50, 20 us apart, every 10 ms | 100% both | 18.7-18.9 / 20.7-46.3 / 53-86 | 540-558 / 970-980 / 1,070-1,079 |
| single lookups every 10 ms | 100% both | 40-44 / 58-61 / 100-133 | 564-572 / 989-998 / 1,073-1,081 |

Every event reaches the tool on both; under bpftime each line appears half a millisecond
later on average, up to the 1 ms timeout.

Raw: `m1-steady.jsonl`, `m2-burst.jsonl`, `m3-overload.jsonl`, `l1-matched.jsonl`,
`l2-backtoback.jsonl`, `l3-nopad.jsonl`, `r1-opensnoop-size.jsonl`, `r2-lost-flush.jsonl`,
`r3-pause.jsonl`, `r4-call-tail.jsonl`, `r5-varsize.jsonl`, `r6-procs-paced.jsonl`,
`r6-procs-steady.jsonl`, `r7-many-procs.jsonl`, `r7b-oversub.jsonl`, `prof/` (runs and flat
profiles; perf.data not kept), `r8-gethostlatency/` (`burst50.jsonl`, `single.jsonl`), logs
`m*.log`, `l*.log`, `r*.log`, smoke runs in `smoke/`.
Binaries are rebuilt by `src/build.sh` (not kept).
