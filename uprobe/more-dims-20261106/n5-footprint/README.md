# Memory footprint and idle CPU (2026-11-06)

Item 5 of the extra dimensions (`../README.md`). Kernel `6.8.12-1021-tegra-btf`, bpftime
`fix-final` (dev d46ceb8f) with its default 128 MB segment. cpu5 at 1728000 before and after;
14:48-15:04 (`../go-rest.log`); no other runs; 42 cells, all checked (hits == threads).

## Setup (`src/thr.c`, `run.py`)

The tool (outattach with `out-control`, `out-perf-256` or `out-rbres-256` of output-dims on
`thr`'s `count_event`) is started; its VmRSS and RssShmem after attaching, and for the kernel
the memlock of its BPF objects (`/proc/<tool>/fdinfo`). Then `thr THREADS 20`: each of THREADS
threads makes one call (so per-thread state exists: Gum's context, a perf shard) and blocks;
main prints its own VmRSS / RssAnon / RssFile / RssShmem, idles 20 s and prints its own CPU
time (`getrusage`, all threads, the agent's included); the tool's CPU time over the same window
(consumer polling with no events). THREADS 1, 16, 64; 2 reps.

## Findings

**1. A traced process carries about 38-40 MB more resident memory (56 MB with 64 perf
threads), 6.5-8.5 MB of it private.** VmRSS of
`thr` (MB, mean of 2 reps):

| threads | none | kernel (any output) | bpftime control | bpftime ringbuf 256 B | bpftime perf 256 B |
|---:|---:|---:|---:|---:|---:|
| 1 | 1.4 | 1.4 | 39.4 (anon 6.9, file 31.5, shmem 1.0) | 40.7 (7.8, 31.9, 1.0) | 40.2 (6.9, 32.0, 1.3) |
| 16 | 1.5 | 1.5 | 39.0 (6.5, 31.5, 1.0) | 40.3 (7.4, 31.9, 1.0) | 43.7 (6.6, 32.0, 5.1) |
| 64 | 1.9 | 1.9 | 40.7 (8.2, 31.5, 1.0) | 41.1 (8.2, 31.9, 1.0) | 57.7 (8.5, 32.0, 17.3) |

Most of it (31.5-32 MB) is the agent's code and read-only data, file-backed and shared by every
traced process through the page cache; the private part is 6.5-8.5 MB per process (the agent's
heap, Gum, the JIT). Threads add little (about 20 KB each), except with perf output.

**2. perf output costs 256 KB of the shared segment per producer thread.** RssShmem of a process
with perf output grows 1.3 -> 5.1 -> 17.3 MB with 1 / 16 / 64 threads that each sent one sample:
bpftime gives every producing thread its own shard (64 pages, as libbpf-tools' perf buffer),
where the kernel has one ring per CPU (6 here, whatever the threads). The default segment is
128 MB, committed at creation (Boost `posix_fallocate`): about 500 producing threads fill it.
(600 threads: below.)

**3. Each bpftime tool commits its 128 MB segment; its own memory is 14-19 MB.** The tool
(outattach) under the syscall server: VmRSS 13.6 (control), 14.2 (ringbuf), 15.2-19.4 MB (perf,
growing with the producers' shards it maps); the segment's 131,072 KB are allocated in
`/dev/shm` from the start. The kernel tool: 2.0-2.5 MB, BPF memlock 12 KB (control, perf maps;
the perf rings are its mmap of 64 pages per CPU) and 281 KB (the 256 KiB ring buffer).

**4. Idle: the agent costs nothing, the tool polls.** With every traced thread blocked for 20 s,
`thr`'s CPU time is 0 under both. The tool's: kernel 0 ms; bpftime 0 ms with no output map, 35-40
ms (ringbuf) and 65-140 ms (perf, growing with the shards: 1 / 16 / 64 threads) per 20 s, i.e.
2-7 ms of CPU per second: `bpftime_epoll_wait` wakes every 1 ms and scans the rings and shards.

Raw: `footprint.jsonl`, `footprint.log`. Binaries are rebuilt by `src/build.sh` (not kept).
