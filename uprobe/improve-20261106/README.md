# Improvements from the 11-06 measurements (2026-11-06)

The plan: the problem list at the end of `../more-dims-20261106/README.md`, one dev commit per
step on the candidate worktree (`bpftime-daily-state/worktrees/177a1dc465ff/candidate`, from
d46ceb8f), each with a test and a mutation check (commands in the commit messages). Frozen
builds in `../ssl-nginx-2x2-20261023/builds` (`dev-<sha>` and `dev-<sha>-probecheck`, see its
README.txt); from item 2 on, every build has the probe read and write checks on (the daily
build keeps them off: decided 11-07). cpu5 at 1728000 before and after every batch; no two batches
at once; no build during a batch.

| item | commits | directory |
|---|---|---|
| 1 ring buffer padding wakeup | f4d505b3 | (smoke below; end to end in the final batch) |
| 2 checked probe reads | a48f6777 | `n2-probe-read` |
| 3 perf output wakes as the consumer asks; the consumer naps | f5b388cd, 213ede4b | `n3-perf-wakeup` |
| 4 ring buffer reserve lock | 26e7c994, 2a0ed6ff, d08ac155, 8a87aaed, b6da99bf | `n4-reserve-lock` |
| 6 perf shards and idle sweeps (a-c) | 5e7eaf9f | (final batch) |
| 5 start-up (a, b) | dc10a160, 0f9c3654 | (final batch) |
| 6 idle consumer sleeps to the caller's timeout (d) | 3260b94c | (final batch) |
| 7 hash map comments; the soak's hit counter | 3f8981e4; output-dims `src/out.bpf.c` | |
| 8 the consumer leaves signals to the kernel (found in the investigations) | 9c088441 | `sigint-fix` |

Regression (`regression/`, 23:40-23:50, dev 3f8981e4 built with checks on, `build-probecheck`):
every runtime test case in a process of its own as root (`each.sh`): 272 cases, 269 passed, the
3 map usage counter cases skipped in this build (as on 11-05); transformer 54 passed and the 1
expected failure; Frida 26/26; syscall trace 7/7 (the daily filter); daemon 10/10 and
`[kernel_user]` 2/2 as root; the daily smoke (`tracker.smoke_real_tools`): funclatency
7,610,000 calls counted exactly, memleak 9 outstanding allocations, a SIGUSR1 detach under
four calling threads with the workload running to its end. Root-owned test segments removed
afterwards.

The soak's harness: `output-dims-20261106/src/out.bpf.c` counts hits with an atomic add under
`ATOMIC_HITS`, which `build.sh` sets for `out-soak.bpf.o` only (more-dims n6: a plain
increment of per-`tid & 31` slots shared by threads of different processes lost 0.087%); the
other objects' instructions are unchanged.

## 1. A consumer at the end padding has caught up (f4d505b3)

Smoke (20:17, output-dims `run.py`, 4 KB and 256 B records by reserve, bursts of 100 at 4 us,
consumer 0, one rep; fix-final against dev-f4d505b3, both checks off): 4 KB on 4 threads
delivered 64.7% -> 99.8%, every record's p99 1,050 -> 10 us; 4 KB on 1 thread 94.3% -> 100%,
1,052 -> 10.6 us; 256 B (no loss before) p99 977 -> 9.2 us on 1 thread, 979 -> 305 us on 4.

## 2. Checked probe reads in one system call per chunk (a48f6777)

`n2-probe-read` (20:31-20:38): more-dims n1's cost matrix (3 reps of 3 s) with the kernel,
fix-final-probecheck (d46ceb8f, checks on) and dev-a48f6777-probecheck; ns per call (median),
in brackets over the control:

| read | kernel | fix-final-probecheck | dev-a48f6777-probecheck |
|---|---:|---:|---:|
| control | 1,246 | 228 | 227 |
| read 64 B | 1,283 (+37) | 1,218 (+990) | 1,215 (+988) |
| read 256 B | 1,291 (+45) | 1,227 (+999) | 1,207 (+980) |
| read 4,096 B | 1,439 (+193) | 1,602 (+1,374) | 1,556 (+1,329) |
| string 16 B | 1,293 (+47) | 2,227 (+1,999) | 1,244 (+1,017) |
| string 255 B | 1,380 (+134) | 2,416 (+2,188) | 1,254 (+1,027) |
| string 1,024 B | 1,603 (+357) | 9,598 (+9,370) | 2,411 (+2,184) |
| string 4,095 B | 2,642 (+1,396) | 32,643 (+32,415) | 3,522 (+3,295) |

Strings now cost one system call per chunk (the first 256 bytes, then to the end of each
page): a probe reading a 255-byte name costs 1,254 ns in all, the kernel's 1,380. Reads are
unchanged (one system call each before too). Bad pointers (`bad.jsonl`, NULL / unmapped /
kernel address, strings and reads, 2 reps): -EFAULT on every call, the process runs.

## 3. perf output wakes the consumer as its attr asks (f5b388cd)

`n3-perf-wakeup` (20:47-20:53), dev-a48f6777-probecheck (half-shard rule) against
dev-f5b388cd-probecheck (the kernel's rule from the consumer's perf_event_attr):

- gethostlatency (output-dims r8 harness, bursts of 50 lookups 20 us apart every 10 ms, 3
  reps): line delay p50 / p90 / p99 kernel 18.7-19.3 / 30-51 / 84-142 us; a48f6777 536-540 /
  969-971 / 1,065-1,069 us; f5b388cd 20.3-20.7 / 21.6-70.7 / 47-116 us. All 22,650 events
  delivered in every run.
- ssl-nginx (more-dims n3 ab harness, wrk -c 100, 16-byte page, 5 reps, rotated): kernel 11,987
  req/s, a48f6777 12,859 (+6.7% paired over the kernel), f5b388cd 11,032 (-7.8%); f5b388cd
  against a48f6777 -9.6 to -14.6% per rep. sslsniff's consumer is now woken for each of the two
  samples per request.

Over the plan's 3% stop; the decision was to find where the cost goes first.

**Where it goes** (`prof/`, 21:56-21:58: `prof.sh` runs one 10 s wrk per leg in the same
container and records system-wide on the host while wrk runs, a cycles profile with call graphs
for 4 s and sched_switch/sched_wakeup for 3 s; perf.data kept outside the repository; this run:
kernel 11,820, a48f6777 12,789, f5b388cd 11,434 req/s):

- The producer pays. nginx's single worker is the bottleneck (one CPU, CPU 2 in every leg).
  `bpf_perf_event_output` with its callees was 1.6% of nginx's samples under a48f6777 and 8.2%
  under f5b388cd; `futex_wake` alone 4.7% of nginx's samples (the FUTEX_WAKE system call in the
  probe waking sslsniff on another CPU), the rest its system call entry and the wakeup path
  (`wake_up_q` 2%). That is about 9 us more per request (78 -> 88 us), two samples per request.
- The consumer wakes for nearly every sample (sslsniff 66,313 context switches in 3 s, 22k/s
  against 23k samples/s; a48f6777: 2,675) and its share of all samples rises from 3.4% to
  11.2% (kernel 6.6%), but on other CPUs (mostly 4 and 5): it does not take nginx's CPU. The
  kernel's sslsniff ran on nginx's CPU more often (14,420 switches there in 3 s).
- So the kernel's rule costs bpftime one system call per sample in the traced process, where
  the kernel's wakeup is an irq_work raised in the probe.

**A consumer that polls after events** (experiments on top of 0f9c3654, not committed:
`bpftime_epoll_wait` keeps polling for 200 us after it last found events before it registers
and sleeps on the futex, so producers find nobody waiting and make no system call;
`BPFTIME_CONSUMER_POLL_US`):

| | gethostlatency p50 / p99 (us), 3 reps | ssl-nginx req/s (median of 5) | vs kernel (paired) | sslsniff CPU (`prof/cpu.sh`) |
|---|---|---|---|---|
| kernel | 18.7-21.7 / 42-114 | 11,956-12,024 | | 12% of a core |
| a48f6777 (half shard) | 536-540 / 1,065-1,069 (round 1) | 12,690-12,772 | +5.5 to +8.2% | 5% |
| 0f9c3654 (every sample) | 20.5-26.2 / 42-869 | 11,443-11,452 | -5.3 to -7.8% | 22% |
| exp-poll (64 yield instructions between polls; `go2.sh`) | 12.0-185 / 53-1,007 | 12,621 | +6.3% | |
| exp-poll2 (sched_yield between polls; `go3.sh`) | 12.6-13.2 / 42-98 | 12,562 | +2.8% | 100% |

Polling takes the producers' system calls away (ssl-nginx +10.5% over 0f9c3654, the same as
the half-shard rule within the noise) and the latency is lower than the kernel's, but the
consumer spins on a core for as long as events come more often than every 200 us (ssl-nginx:
every 43 us). Spinning without giving the CPU away (exp-poll) delayed gethostlatency's own
processes when they shared its CPU (2 of 3 runs: p50 80-185 us, p99 1 ms); with sched_yield
every run was fast.

**A consumer that naps after records** (decided to try: a short timed sleep while records
keep coming, during which producers make no system call):

- exp-nap (`go4.sh`, `exp-nap.diff`): for 2 ms after records the consumer sleeps 100 us at a
  time without registering. ssl-nginx 12,689 req/s (head 11,518, half shard 12,801, kernel
  12,083), sslsniff 8% of a core; gethostlatency p50 89 us, p99 170 us (first record of a burst
  31-33 us: the consumer slept registered and was woken). But nothing can wake a napping
  consumer: bursts of ring buffer records were lost (`bursts.jsonl`: 8,240 B, 1 thread 100% ->
  84%, 4 threads 96-100% -> 63-65%).
- exp-nap2 (`go5.sh`, `exp-nap2.diff`): the nap is a futex wait registered in `nappers`; perf
  wakes nappers past the watermark or on a drop, ring buffers at half a ring. ssl-nginx 12,673
  (half shard 12,854, head 11,505, kernel 12,150), sslsniff 10% (head 21%, kernel 12%),
  gethostlatency p50 90 us, p99 170 us (one rep 108 / 682). Ring buffer bursts still lost
  records sometimes (4 threads: one rep each at 89% and 90.5%): half a ring is too late for a
  ring all producers share.
- exp-nap3 (`bursts2.jsonl`, `exp-nap3.diff`): ring buffers wake napping consumers by bpf's
  rule as waiting ones; only perf naps. Ring buffer bursts as the head (p50 3 us, p99 8-26 us,
  delivery the same; one rep of 8,240 B on 4 threads lost 7-20% in every build, the head
  included), perf 4 KB bursts delivered in full with p50 18-87 us, p99 165-171 us.

Committed as 213ede4b (exp-nap3 with the settings `BPFTIME_CONSUMER_NAP_US` = 100,
`BPFTIME_CONSUMER_NAP_WINDOW_US` = 2000, 0 turns the naps off; opt-in
`BPFTIME_CONSUMER_POLL_US`). The final batch measures the committed build.

## 4. The ring buffer reserve lock, step by step

`n4-reserve-lock` (21:42-21:55), output-dims `run.py`, every build with checks on; the kernel
for reference. ns per call per producer (median of 3), back to back (bursts of 10,000):

| | kernel | f5b388cd (before) | 2a0ed6ff (a inline lock, c one handler read) | d08ac155 (b lock line) | 8a87aaed (d positions on the lock's line) | b6da99bf (e yield) |
|---|---:|---:|---:|---:|---:|---:|
| control, 1 thread | 1,303 | 300 | 280 | 279 | 278 | 278 |
| ringbuf 8 B, 1 thread | 2,083 | 737 | 649 | 593 | 490 | 496 |
| control, 4 threads of 1 process | 3,333 | 4,092 | 4,134 | 4,103 | 4,113 | 4,113 |
| ringbuf 8 B, 4 threads of 1 process | 4,255 | 3,874 | 3,912 | 3,053 | 3,107 | 3,125 |
| control, 4 processes | 2,842 | 299 | 278 | 278 | 278 | 277 |
| ringbuf 8 B, 4 processes | 3,200 | 3,846 | 3,783 | 3,077 | 2,985 | 2,980 |

Every record delivered in every cell. One thread: -33% (737 -> 496 ns). Four producer
processes: 3,846 -> 2,980 ns, now below the kernel's 3,200 per call; the output's own cost over
the control 3.5 -> 2.7 us (the kernel's 0.36 us: its producers spend 2.8 us in the uprobe
trap and meet in the ring far less often). Within one process Gum's per-function counter
(control 4.1 us at 4 threads) still dominates.

64 single-thread producer processes on the 6 CPUs (R7b: bursts of 10 at 4 us every 10 ms, 6 s,
ringbuf 256 B, per-call durations; 2 reps):

| | calls made | median process p99 (us) | worst process p99.9 (us) | worst call (ms) | delivery p99 / max (ms) |
|---|---:|---:|---:|---:|---|
| kernel | 378,690-378,900 | 8.2 | 57-66 | 5.3-7.7 | 0.31-0.41 / 7.9-8.1 |
| f5b388cd | 308,750-345,760 | 16.4-20.5 | 31,457-50,332 | 49-79 | 0.46-6.06 / 49-79 |
| 2a0ed6ff | 312,400-334,430 | 15.4-20.5 | 37,749-41,943 | 72-78 | 0.91-5.62 / 72-78 |
| d08ac155 | 326,090-357,890 | 12.3-16.4 | 23,069-41,943 | 58-69 | 0.35-1.10 / 58-69 |
| 8a87aaed | 358,280-358,310 | 12.3-13.3 | 27,263-31,457 | 46-52 | 0.39-0.43 / 46-52 |
| b6da99bf | 376,940-377,260 | 11.3-13.3 | 164-229 | 21-28 | 0.07-0.10 / 21-28 |

The yield (e) is what takes the descheduled holder's tens of milliseconds away: the workload
makes as many calls as under the kernel, the worst process's p99.9 call is 0.2 ms (the
kernel's 0.06), single calls still reach 21-28 ms.

## Final batch (`final/`, 23:44-00:37)

The kernel, fix-final-probecheck (d46ceb8f with checks on: before) and dev-3f8981e4-probecheck
(after items 1-7), `final/go.sh`; every cell checked (hits, deliveries) unless noted.

**Ring buffer bursts** (`padding.jsonl`, bursts of 100 at 4 us, consumer 0, 2 reps; delivered,
all records' p50 / p99 us):

| record | threads | kernel | before | after |
|---|---|---|---|---|
| 4,096 B (pads every 63) | 1 | 100%, 5 / 9 | 94.8-95.2%, 58 / 1,052 | 100%, 3 / 11 |
| | 4 | 98.6-99.6%, 4 / 49 | 59.6-71.0%, 33 / 1,051 | 92.3-98.3%, 3 / 83-394 |
| 8,240 B (opensnoop's; every 31) | 1 | 100%, 5 / 9 | 64.2-65.4%, 124 / 1,054 | 99.9%, 3 / 11 |
| | 4 | 95.4-97.5%, 4 / 51 | 35.9-37.9%, 279 / 1,053 | 95.4-99.9%, 3 / 189 |
| 256 B (every 992) | 1 | 100%, 5 / 11 | 100%, 3 / 983 | 100%, 4 / 11 |
| | 4 | 100%, 4 / 300 | 100%, 3 / 989 | 100%, 4 / 199 |
| 4,088 / 8,184 / 248 B (no padding) | 4 | 95.9-99.4%, 4 / 11-14 | 92.9-99.7%, 3 / 9-382 | 88.1-99.4%, 3 / 8-398 |

Variable sizes (`varsize.jsonl`, bursts of 1,000): reserve 85.2-85.6% -> 99.9-100% on one
thread (kernel 100%), 70.0-71.4% -> 95.9-96.6% on four (98.5-98.9%); `bpf_ringbuf_output`
86.0% -> 100%, 79.2-80.4% -> 98.1-98.4%. opensnoop itself (`opensnoop/results.jsonl`, the
opensnoop-loss harness, 2 reps): bursts of 100 93.9-94.2% -> 99.95-100%, bursts of 50
92.2-95.5% -> 99.9% (kernel 99.9-100%): F4 closed.

Open from this batch: 4-thread ring buffer bursts in the after build had 300-400 us tails in
one of two reps, also for sizes that never pad (248 B: p99 398 us against 8; 8,184 B: 374
against 35); in the nap experiments every build showed such a rep now and then. Rechecked
with more reps in `investigate/` (below).

**perf output latency** (`perf.jsonl`, bursts of 100): first and all records p50 / p99,
256 B: before 528-531 / 1,051-1,060 us, after 63-88 / 157-162 us (kernel 4-5 / 9-10);
4 KB: before 60-93 / 1,057-1,066, after 44-83 / 162-331. gethostlatency (`gethost.jsonl`, 3
reps): before 537-538 / 968-969 / 1,066 us (p50 / p90 / p99), after 90.5-90.7 / 157.5-158.2 /
170.1-170.7, kernel 18.8-19.0 / 20.1-21.8 / 41-45. All delivered.

**Cost per call** (`steady.jsonl`, back to back, median of 3, ns per call per producer):

| | kernel | before | after |
|---|---:|---:|---:|
| control, 1 thread | 1,294 | 281 | 277 |
| ringbuf 8 B, 1 thread | 2,041 | 719 | 518 |
| ringbuf 8 B, 4 threads of 1 process | 4,188 | 3,990 | 3,168 |
| ringbuf 8 B, 4 processes | 3,272 | 4,040 | 2,991 |
| perf 8 B, 1 thread | 2,759 | 538 | 597 |
| perf 8 B, 4 threads of 1 process | 5,229 | 4,020 | 3,819 |
| perf 8 B, 4 processes | 4,324 | 824 | 837 |

perf output on one thread is 59 ns (11%) dearer: every sample now asks for a wakeup
(wakeup_events 1), so every output fences and loads `waiters` (before: only at half a shard).
A cheaper check (on aarch64 a store-release before the load already orders them) is left open.

**64 producer processes on 6 CPUs** (`oversub.jsonl`, 2 reps): ring buffer calls made
290,240-315,560 -> 376,430-378,410 (kernel 378,900-378,990), worst process p99.9 46-55 ms ->
0.18-0.98 ms, delivery p99 13.9-28.3 -> 0.08-0.10 ms; perf worst p99.9 360-459 -> 57-66 us
(kernel 115-197).

**Start-up** (`startup.jsonl`, 200 starts, p50): with the agent preloaded and no tool
3,015-3,016 ms -> 8.4-8.6 ms (the agent still loads; kernel bpf adds nothing); with a tool,
control 22.0 -> 22.8-23.0 ms, ring buffer 23.5 -> 24.4-24.5, perf 26.6-26.9 -> 27.7-28.1 (kernel
0.50-0.54): about 0.9 ms more, not explained yet (`investigate/startup`). Many probes
(`many.jsonl`): 1,000 probes 439 -> 227 ms per start (0.42 -> 0.20 ms per probe), 100 probes
63.4 -> 43.9 ms; one probe 22.4 -> 23.3 ms (the same +0.9 ms); calls unchanged (222-300 ns).

**Footprint** (`footprint.jsonl`, 10 s idle; `footprint-600.jsonl`): 600 perf-producing
threads: 107 failed -> 0, the idle tool's CPU 1,850 ms -> 80 ms over 5 s (kernel 0); idle tool
CPU over 10 s 20-90 ms -> 0-10 ms. The tool's resident memory 1.1-2.7 MB more (the overflow
shards: one per CPU event, the consumer's mapping each).

**ssl-nginx** (`ssl/ab-16.json`, 5 reps): kernel 11,996 req/s, before 13,026 (+8.4% over the
kernel), after 12,612 (+6.2%); after against before -1.7 to -5.8% per rep. Checked again in
`investigate/item3-ab` with the builds between.

**30-minute soak** (`final/soak.jsonl`, 00:37-01:07, more-dims n6's runner with
dev-3f8981e4-probecheck and the soak object now counting hits atomically): 88 traced processes,
286,652,400 calls, as many hits and records received, nothing failed, every process exited 0
(`ok: true`); the tool's VmRSS 24,712-24,720 KB and 0.27-0.28 of a core in every 5-minute
window.

## Investigations (`investigate/`, 01:08-01:37; measured only)

**4-thread ring buffer tails** (`ringbuf-tail.jsonl`, 248 and 8,184-byte records, 4 threads,
6 reps): every build has reps with a 300-400 us p99 (fix-final-probecheck 3 of 6 at 248 B and
2 of 6 at 8,184 B, 0f9c3654 2 and 1, exp-nap3 1 and 2, 3f8981e4 3 and 0): run-to-run variance,
not a regression of the final build. One exp-nap3 cell's tool did not stop (below).

**ssl-nginx again** (`item3-ab/`, 5 reps): kernel 11,980 req/s; fix-final-probecheck 12,729;
a48f6777 12,844; 0f9c3654 (every sample) 11,426; exp-nap3 12,555; 3f8981e4 12,670. Against
fix-final-probecheck, paired: 3f8981e4 -7.8, -1.2, +0.5, +1.4, -0.5% (median -0.5%; the final
batch's -3.2% was mostly noise), exp-nap3 median -1.7%, 0f9c3654 -7.2 to -14.4%.

**fix-final 3% below 7ad5a152** (`gap3/`, checks off, 5 reps, medians against dev-7ad5a152's
14,374): dev-25f4134f +0.7%, dev-6a15a271 +1.1%, dev-74d0ab8b -1.1%, dev-b60b99c3 -1.7%,
fix-rbwake2 -1.8%, fix-final -2.3% (one rep at 12,223). No single step: about 2 points between
6a15a271 and 74d0ab8b (b46dde06 the exit logger, 74d0ab8b the poisoned calls) and 1-2 within
the fifth review's fixes, each near the noise.

**The older sslsniff** (`oldsniff-old/`, `oldsniff-new/`, bridge network, 3 reps): with
ssl-nginx-2x2's sslsniff (built before the BTF kernel) fix-final is +18.5% over the kernel, with
the CO-RE one +16.8%; both 2.000 events per request. The earlier "no lead" (more-dims n3,
`tail-oldsniff.jsonl`) came with `--network host`, not from the binary. The same runs show the
price of checked reads on this benchmark: fix-final (checks off) +16.8-18.5% over the kernel,
3f8981e4 with checks +4.6-6.7%.

**Where a traced process's start goes** (`startup/`, perf over 200 starts of `shortlived`
under the agent with the control program attached; shares of shortlived's samples, about
23-24 ms per start): `bpftime_prog_load` 44% (the program JIT-compiled with LLVM at O3 in every
process, a fresh LLJIT: about 10 ms), the agent's static constructors (`_dl_init`, mostly
LLVM's) 8.5%, `gum_module_find_base_address` 7% (of which 4% `memmem` in Gum's
`gum_read_auxv_from_stack`, run once per process for libc's name), Gum's thread context 3%,
mapping the agent's dependencies 2.7%, attaching 1.6%, `gum_init_embedded` 1.5%; most of the
rest is the kernel (mapping and relocating the 114 MB agent, 202,114 relative relocations,
exec, exit). The agent without a tool: 8.8 ms per start. The +0.9 ms of 3f8981e4 against
fix-final-probecheck is spread evenly over every part (6% more samples, the same shares);
the agent's text is 80 KB larger; not attributed further.

**Found: the consumer takes the process's SIGINT from other threads.** The exp-nap3 cell
above: outattach did not stop on SIGINT within 60 s; gdb (`ringbuf-tail.jsonl`'s `hang_bt`):
the consumer thread in `bpftime_epoll_wait`'s futex wait, the main thread in `pause()`.
`bpftime_epoll_wait` blocks SIGINT and SIGTERM in its thread and collects a pending one with
`sigtimedwait` after each wait, calling the handler itself. A process-directed SIGINT meant for
the main thread (which does not block it) can be dequeued there first: the handler runs in the
consumer thread, and the main thread, woken with nothing pending, goes back to `pause()`
forever. Single-threaded libbpf-tools are not affected (the handler runs in the polling thread
anyway); tools that wait for signals in another thread hang. Probably also the earlier
unreproduced hang (output-dims smoke). Fixed afterwards in 9c088441 (section 8 below).

## 8. The consumer leaves signals to the kernel (9c088441, `sigint-fix/`, 04:56-05:06)

The SIGINT found in the investigations. `bpftime_epoll_wait` no longer blocks or collects any
signal; the nap and the sleep are one registered futex wait, and a handler that runs in the
polling thread ends it with EINTR (a futex wait with a timeout is not restarted even under
SA_RESTART), so the call returns -1 with EINTR, as bpf's epoll_wait. With no wakeup object
(an older segment) it sleeps with nanosleep the same way. The old code had two more faults
than the stolen signal: SIGINT with `SIG_DFL` taken there did nothing (the process lived on),
and `SIG_IGN` was called as a function (SIGBUS on aarch64).

Test "A consumer leaves signals to the threads they are meant for" (`[epoll-signals]`): on
873cfb61 all five sections fail (the consumer took the process's SIGINT and ran the handler;
the process outlived a default-action SIGINT; SIGBUS for an ignored one; no EINTR, the wait ran
its full 2 s, sleeping or napping). Mutations (the wait ignores EINTR; the nap or the sleep
goes on after it) each fail their sections. Not covered: the nanosleep path.

End to end (`stopcheck.py`, frozen builds dev-3f8981e4-probecheck and dev-9c088441-probecheck,
cpu5 1728000 before and after): outattach (main thread in `pause()`, the consumer started with
SIGINT blocked) with a ring buffer or perf object, SIGINT under load (4 threads of bursts) or
idle, 15 reps each, rotated:

| build | ringbuf load | ringbuf idle | perf load | perf idle | hangs |
|---|---|---|---|---|---|
| 3f8981e4 | 80.0 ms | 226.8 ms | 80.9 ms | 227.7 ms | 0 / 60 |
| 9c088441 | 81.4 ms | 227.4 ms | 80.2 ms | 227.4 ms | 0 / 60 |

(medians from SIGINT to the loader's exit; idle includes the consumer's last 100 ms poll and the
final drain.) The stolen signal is a narrow race and did not happen in these 60 runs on the old
build (once in the investigations); the unit test shows it deterministically. sslsniff
(single-threaded, perf buffer polled with 100 ms), SIGINT 2 s after start, 5 reps: 227.4 ms ->
177.8 ms: the old code kept SIGINT blocked until its sleep ended, the new one ends the sleep.

Regression (`sigint-fix/regression`, 9c088441 `build-probecheck`): each.sh 273 cases, the same 3
map usage counter cases skipped as before, the rest passed; the daily smoke: funclatency
4,263,000 calls counted exactly, memleak 9 outstanding allocations, SIGUSR1 detach with the
workload running to its end. Root-owned test segments removed afterwards.

## 9. Before the daily list: the 25e250d6 gate and x86 (11-07, 17:00-17:50, `daily-gate/`)

Gate as the daily would run it, the official SHA (25e250d6) being unchanged so the daily itself
skips: the 16 commits d46ceb8f..9c088441 appended to a temporary copy of the daily list
(`tools/jetson-upstream/config.local-test.json` in the automation checkout, untracked; config.json
untouched), `tracker.py --config <copy> migrate --official-sha 25e250d6...` (216 commits, every
cherry-pick clean, candidate c689eb11 on the local branch `dev/uprobe-official-25e250d651ad`, not
pushed; `runtime/`, `attach/` and `vm/` identical to dev 9c088441), then `test-candidate`
(log `bpftime-daily-state/candidate-test-candidate-20261107-25e2-216.log`, 17:01-17:08, rc 0):

| suite | 200 commits (11-05) | 216 commits |
|---|---|---|
| runtime, daily filter | 183 passed, 1 skipped | 194 passed, 1 skipped (11 new cases) |
| Frida attach | 25 | 26 |
| transformer | 54 + 1 expected failure | 54 + 1 expected failure |
| the other three suites | passed | passed |
| smoke funclatency | 4,271,000 calls, counted exactly | 5,549,000 calls, counted exactly |
| smoke memleak / SIGUSR1 detach | 9 outstanding / detached, rc 0 | 9 outstanding / detached, rc 0 |

x86 (NUC, 17:03-17:48, NUC session told before and after): dev 9c088441 against d46ceb8f, the
daily's build settings (checks off), the NUC session's method (daily filter in one process, every
runtime and Frida case in a process of its own, 60 s each). No new failure: the same 7 cases fail
on both (4 need root and pass 3/3 under sudo on 9c088441, among them "Producers in two pid
namespaces with the same pid write apart"; 3 need the map usage counters build option), the
changed areas (`[ringbuf],[epoll],[perf_event],[allocation]`) pass 3/3, Frida 26/26. The x86
files go to the x86-nuc branch (NUC session).

Found on the way (not caused by this round): the whole runtime suite in ONE process with
Catch2's junit reporter (`-r junit -o x.xml`) dies at the same point of every run (x86: the
last log lines are the LPM trie cases'; Jetson, info logging only: the shm JSON import case's),
glibc reporting "double free or corruption (!prev)" and then aborting, hanging or crashing:
x86 d46ceb8f 1/1, x86 9c088441 3/3, Jetson c689eb11 2/2 (one of them a segmentation fault without
the message). With the console reporter the same binary runs to the end
every time (Jetson 272 cases: 265 passed, 6 skipped, the user-to-kernel tail call failing without
root; x86 the same, 3/3). The daily, `each.sh` and the NUC per-case runs use the console reporter
or one process per case, so none of them sees it. Cause found on 11-08 (section 10): test cases
that call Catch2 assertions from several threads at once, not the bpftime runtime.

Added to the daily list on 11-08 (automation `3b9e9ff7`, 216 commits; the temporary copy deleted).

## 10. The junit crash: assertions from worker threads (11-08, 03:10-03:31, `junit-threads/`)

Same binary as section 9 (candidate c689eb11, `build-jetson-daily`, Catch2 v3.4.0), as jetson.
gdb loses the threads of this suite, so the crash was placed with two reporters at once (junit,
and console with durations to a file): both runs stop in "LPM Trie Multi-thread", whose three
pthreads (update, delete, lookup) each run 2,000 x 4 `REQUIRE`s; the console file records a
`std::bad_alloc` at one of them (`test_lpm_trie_map.cpp:918`) and stderr "double free or
corruption".

Catch2's own docs (`docs/limitations.md`, "Thread safe assertions"): the assertion macros are not
thread safe, only one thread at a time may use them. The junit reporter sets
`shouldReportAllAssertions`, so every passing assertion takes the full path
(`handleExpr` -> `reportExpr` -> `assertionEnded`: an `AssertionResult` with string members, the
message vector copied for the reporter, a move into `RunContext::m_lastResult`); several threads
there at once allocate and free shared `RunContext` and reporter state (which allocation is
freed twice was not traced). The plain console reporter only increments a
counter for a pass (a race on the count, nothing allocated), which is why it runs to the end.
Opt-in thread-safe assertions came with Catch2 3.9.0 (`CATCH_CONFIG_THREAD_SAFE_ASSERTIONS`
since 3.12.0).

Exit codes, each case alone, 5 runs (124: hung until the 180 s timeout):

| case | origin | junit | `-s` (console, passes reported) | console |
|---|---|---|---|---|
| LPM Trie Multi-thread | official 7f41aedd (#401) | 139 x5 | 139 0 139 134 0 | 0 x5 |
| test_map_stress (kernel): 100 threads in `test_hashmap` etc. | official 327ba8db (#354) | 139 139 139 134 124 | | 0 x5 |
| test_map_parallel (kernel): 20 threads in `test_update_delete` | official 327ba8db (#354) | 139 139 134 139 139 | | 0 x5 |
| hash map inserts take free elements from every list: 4 threads | dev 922dde47; fixed in dev 8a0afd36 | 139 x5 (fixed: 0 x10) | | 0 x5 |
| Software perf event shards are matched within a pid namespace | dev 94099872 | 0 x5 | | 0 x5 |

The last one asserts once in one thread while the main thread only spins and then joins, so the
two never overlap. With the five left out, the whole suite with junit ran to the end 3/3 (267
cases, report complete, only the root-only tail call failing, 6 skipped, 33 s each). x86 was not
rerun; its junit runs died right after the LPM trie cases, which fits.

So the runtime is not involved, and plain console runs (the daily, `each.sh`, upstream CI's
`test-runtime.yml`, the NUC per-case runs) are not affected. Left as is: a failing `REQUIRE` in
one of these worker threads also aborts the whole binary on any reporter (the exception leaves
the thread) instead of reporting the failure; this holds for all five cases, the pid namespace
one included. Not fixed; a fix is test-only (count failures in
the threads, assert after the join, or a lock around the assertion macros), or a Catch2 update.

The dev case is fixed in dev 8a0afd36 (11-08, `junit-threads/fix-8a0afd36.txt`): its threads
count failed deletes and the test asserts the count after joining. The case passes 10/10 with
junit, `-s` and console; the whole suite with junit, only the three official cases left out, runs
to the end (only the root-only tail call failing). Mutations: each thread deleting its keys
twice fails the case cleanly with junit 5/5; `REQUIRE` back in the threads crashes it 5/5. The
three official cases are left for an upstream fix.

## Open after this round

- perf output's per-sample fence (+59 ns per call on one thread) now that every sample asks
  for a wakeup.
- Start-up: the per-process JIT compile (about 10 ms of 23), Gum's auxv scan (1 ms), the
  agent's constructors (2 ms); AOT code in the segment would remove the first.
- The daily build keeps the checks off (decided 11-07): checked reads cost ssl-nginx about 10%
  (bpftime +5-7% over the kernel instead of +17-19%); a bad pointer in a probe read still
  crashes the traced process there.
- Three official test cases assert from several threads at once (LPM Trie Multi-thread,
  test_map_stress, test_map_parallel), which Catch2 3.4 does not allow: reporters that report
  every assertion (junit, `-s`) corrupt the heap, and a failure in a worker thread aborts the
  binary (section 10). The dev case is fixed (8a0afd36); the official ones are left for upstream.
