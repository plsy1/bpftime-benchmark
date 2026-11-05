# Event output by dimension: kernel bpf vs bpftime (2026-11-05/06)

Item 3 of the next-direction list: the dimension method of the hash map work
(`../map-threads-20261028`, `../hash-keypattern-20261103`) applied to the event output
paths. Why output: of the 57 libbpf-tools here, 27 send events to user space (perf event
array or ring buffer); none uses LRU hash or map-in-map (hash: 80 map definitions in 48
tools, perf event array 22, cgroup array 13, stack trace 7, per-CPU 3).

bpftime: frozen build `fix-final` (dev d46ceb8f); kernel `6.8.12-1021-tegra-btf`. cpu5 at
1728000 before and after; 2026-11-05 23:56 to 11-06 00:47 (M1-M3, 446 runs) and 00:57 to
01:31 (L1-L3, 352 runs); no other runs; all checked (hits == the workload's calls, received +
failed == hits).

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

Raw: `m1-steady.jsonl`, `m2-burst.jsonl`, `m3-overload.jsonl`, `l1-matched.jsonl`,
`l2-backtoback.jsonl`, `l3-nopad.jsonl`, logs `m*.log`, `l*.log`, smoke runs in `smoke/`.
Binaries are rebuilt by `src/build.sh` (not kept).
