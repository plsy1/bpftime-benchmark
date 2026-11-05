# Event output by dimension: kernel bpf vs bpftime (2026-11-05/06)

Item 3 of the next-direction list: the dimension method of the hash map work
(`../map-threads-20261028`, `../hash-keypattern-20261103`) applied to the event output
paths. Why output: of the 57 libbpf-tools here, 27 send events to user space (perf event
array or ring buffer); none uses LRU hash or map-in-map (hash: 80 map definitions in 48
tools, perf event array 22, cgroup array 13, stack trace 7, per-CPU 3).

bpftime: frozen build `fix-final` (dev d46ceb8f); kernel `6.8.12-1021-tegra-btf`. cpu5 at
1728000 before and after; 2026-11-05 23:56 to 11-06 00:47; no other runs; 446 runs, all
checked (hits == the workload's calls, received + failed == hits).

## Setup (`src/`, `run.py`, `go.sh`)

- `out.bpf.c`: a uprobe on `count_event` sends a SIZE-byte record (8, 256, 4096) by
  `bpf_perf_event_output` (perf), `bpf_ringbuf_output` (rbout) or `bpf_ringbuf_reserve` +
  submit (rbres, opensnoop's path); records above 256 B are built in a per-CPU array, as
  libbpf-tools' `bpf_buffer` does. Ring buffer 256 KiB and perf buffer 64 pages per CPU, as
  libbpf-tools. `out-control` = the probe without output.
- `outattach.c`: loads, attaches, a consumer thread in `perf_buffer__poll` /
  `ring_buffer__poll` (100 ms); `CONSUMER_NS` spins per record (a tool formatting a line).
- Workloads: `countloop` (steady, as fast as the probe allows; thread i on CPU 5 - i) and
  `burstloop THREADS SECS BURST PAUSE_US GAP_NS` (bursts every 10 ms, calls GAP_NS apart).
- Matrices: M1 steady, all paths and sizes, 1/2/4/6 threads, 3 reps, consumer 0; M2 bursts of
  10/100/1000, gap 0 or 2 us, consumer 2 us per record, 1/4 threads, 2 reps; M3 steady with
  that consumer. Tables: `summary.md` (`summarize.py`).

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

**3. Bursts lose more events under bpftime, mostly because its probes are faster.** M2,
4 KB records, consumer 2 us: the ring buffer holds 63 of them, and a burst of 100 takes
50 us under bpftime (146 us under the kernel), so the consumer has a third of the time to
wake and drain. Delivered:

| setting (ringbuf reserve, 4 KB) | kernel | bpftime | burst time kernel / bpftime |
|---|---:|---:|---:|
| burst 100, back to back, 1 thread | 100% | 77.1% | 146 / 50 us |
| burst 100, back to back, 4 threads | 89.8% | 64.5% | 196 / 71 us |
| burst 100, 2 us apart, 1 thread | 100% | 98.8% | 442 / 277 us |
| burst 100, 2 us apart, 4 threads | 100% | 90.0% | 436 / 302 us |

perf at 4 KB behaves alike at larger bursts (burst 1000 back to back: kernel 100% / 76.6%,
bpftime 25.2% / 22.5% at 1 / 4 threads); 256-byte records lose nothing below bursts of 1000.
The 2 us gap did not equalize the arrival rate (the kernel's probe still adds about 2 us per
call), so this does not yet separate the consumer's wakeup latency from the arrival rate.
Next: bursts at matched arrival rates (a gap well above the probe cost) and a direct
measurement of the time from a burst's first record to its delivery: the candidate cause of
opensnoop's remaining 5% (`reviews-20261105/fixes`, finding F4).

**4. With all six CPUs producing, both lose most ring buffer events** (M1, 6 threads, 256 B
and up: the kernel 1.1-10.6%, bpftime 46-60% delivered; perf 4 KB 44-46% both): the consumer thread has no CPU of its own. Not a
bpftime problem; a limit of the test at 6 threads on this 6-core board.

Raw: `m1-steady.jsonl`, `m2-burst.jsonl`, `m3-overload.jsonl`, logs `m*.log`, smoke runs in
`smoke/`. Binaries are rebuilt by `src/build.sh` (not kept).
