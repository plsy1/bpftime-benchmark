# Two hours under bpftime (2026-11-06)

Item 6 of the extra dimensions (`../README.md`): does anything drift, leak or break when one
tool runs for hours while traced processes come and go? bpftime `fix-final` (dev d46ceb8f) with
its default 128 MB segment; bpftime only (the question is bpftime's own state over time). cpu5
at 1728000 before and after (`clock-before.txt`, `clock-after.txt`); 15:24:21-17:24:24
(`../go-last.log`); no other runs.

## Setup (`run.py`)

One tool (outattach with output-dims' `out-soak.bpf.o`, KIND 8) attached for the whole run to
burstloop's `count_event`. Each call:
- updates a 65,536-entry hash map at key `(arg >> 5) & 0xffff` and deletes key `k ^ 0x8000`
  (churn that keeps the map about half full);
- sends a 256-byte ring buffer record by reserve + submit.

Every 20 s a new burstloop process starts: 2 threads, 40 s, bursts of 100 calls at 4 us
spacing every 2 ms, on a CPU pair that rotates. So 2-3 processes (4-6 producing threads) are
live at any time and 358 start and exit over the two hours, each through the agent's start-up,
attach and exit paths. Every 60 s: the tool's VmRSS, thread count and CPU time. At the end:
every process exited 0, the probe hits against the processes' own call counts, the records
received against the hits.

## Findings

**1. Nothing lost, nothing failed over 1.17 billion events.** 358 processes, all exited 0;
1,166,253,700 calls, 1,166,253,700 records received, `lost 0`, 0 failed reserves, 0 failed hash
updates. 162,000 events per second on average for two hours.

**2. No drift.** The tool's VmRSS was 16,664 KB at the start and 16,672 KB in each of the 119
later samples, threads 2 throughout; its CPU use was 0.265-0.274 of a core in every 10-minute
window (about 1.7 us of consumer time per event), the same in the last window as in the
first. After the run the segment and every process were gone (`/dev/shm` and `ps` clean).

**3. The run's own check says `ok: false`, because of the harness, not bpftime.** The BPF
program's hit counter is 1,165,239,567, 1,014,133 (0.087%) below the calls. The counter is the
harness's: a plain `slot->hits++` in a global array at slot `tid & 31`, not atomic. Threads of
different live processes whose thread ids share the low 5 bits share a slot (with 4-6 live
threads that is often), and their unsynchronized increments lose counts, as in output-dims R7
and R7b. Every call did run the program: the records, one per call, add up exactly to the
calls. A later harness should count with atomics or per process.

Not covered: the kernel side (no comparison was the point), the segment's fill over time
(sampled only through the tool's memory), and many tools or reattaching programs during the
run.

Raw: `soak.jsonl` (samples and the end record), `soak.log`, `started`, `finished`, clock files.
