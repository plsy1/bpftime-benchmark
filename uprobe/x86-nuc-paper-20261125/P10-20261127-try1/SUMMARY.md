# P10 ring-buffer reserve+submit microbenchmark (x86 NUC, 2026-11-27; exploratory, no PREREG entry)

Session P10-20261127-try1, plan plans/P10.txt.
- Pinning: CPUs 2,4,6 at 3.5 GHz, 8–11 at 2.5 GHz. check-before and check-after: PASS.
- Code: p10/ (p10.bpf.c, p10tool.c, p10victim.c, p10run.py). sha256.txt covers the built binaries (p10.bpf.o,
  p10tool, p10victim) and p10run.py; the .c sources are not hashed. The session checked those hashes at start
  (console.log "OK").
- Rows: p10.jsonl.

Not covered: the opensnoop-delivery half of the paper's \pending{P10} (design.tex:59). Only the microbenchmark ran.

## Setup

**Victim.** Calls a noinline `p10_point(i)` 100 000 times untimed, then 2 000 000 times timed (CLOCK_MONOTONIC).
- The body is two stores and one load to a volatile global. A body of only `ret` cannot be hooked by Frida.

**Programs.**
- rbprog: `bpf_ringbuf_reserve(64 B)`, then 2 stores, then `bpf_ringbuf_submit(…, 0)`. Flags 0 means the backend's
  default notification. Ring buffer 16 MiB.
- empty: `return 0`.

**Placement.** The loader/consumer runs on CPU 4 (`ring_buffer__poll`, 100 ms timeout).
- 1t: one victim on CPU 2 (P core).
- 4p: four victim processes at once on E cores 8–11, which share one L2. All write into the same ring buffer.

**Legs.**
- K (kernel), O (official 4708f409), Ol (upstream head 70f7bc1f), Dn (Tern).
- bpftime legs: the loader runs under libbpftime-syscall-server.so; the victims run under libbpftime-agent.so with
  SPDLOG_LEVEL=off.
- B: the victim with no tool. In each rep B runs first (1t, then 4p); then the 16 leg × program × placement
  combinations run in a per-rep shuffled order. 5 reps.

**Delivery.**
- All 90 rows: victim rc 0.
- All 80 tool rows: tool rc 0 and 0 drops.
- All 40 rbprog rows: records equal expected, i.e. 2.1 M per victim (2 M timed + 0.1 M warm-up).
- This is a count only: record contents were not checked.
- The bpftime per-call times (about 234 ns) rule out a kernel uprobe, so records went through bpftime's shared-memory
  ring buffer.

**Notification differs by backend (from the source).** Wakeups were not counted.
- O and Ol never wake the consumer; it polls every 1 ms.
- Dn wakes a caught-up, napping or sleeping consumer with FUTEX_WAKE from inside the probe, by bpf's commit rule.
  The consumer naps 100 µs for up to 2 ms after records.
- K queues irq_work when the consumer has caught up (default rule).

## Results

ns per traced call: median over 5 reps of the per-rep mean over victims.

| | B | K empty | K rb | O empty | O rb | Ol empty | Ol rb | Dn empty | Dn rb |
|---|---|---|---|---|---|---|---|---|---|
| 1t | 0.57 | 1392 | 2262 | 154.1 | 234.0 | 172.3 | 263.1 | 164.9 | 234.1 |
| 4p | 1.63 | 1621 | 2028 | 379.8 | 1069 | 476.1 | 1199 | 406.6 | 939 |

**Per traced call with the ring buffer (rb), the primary figure.**
- 1t: Tern 234.1 = official 234.0 (per-rep ranges overlap: 229.9–235.0 and 227.1–245.2). Upstream head 263.1.
  Kernel 2262, 9.7× Tern.
- 4p: Tern 939 is the lowest. Official 1069 (Tern 12 % lower), upstream head 1199 (22 % lower), kernel 2028
  (2.2× Tern).

**Marginal cost of reserve+submit plus each backend's default notification (rb − empty, paired by rep).**
Median [min, max]:

| | K | O | Ol | Dn (Tern) |
|---|---|---|---|---|
| 1t | 876 [835, 880] | 79.9 [72.7, 91.2] | 90.3 [84.9, 99.5] | 69.6 [64.2, 70.1] |
| 4p | 406 [393, 431] | 692 [649, 782] | 723 [556, 788] | 532 [328, 552] |

- This is not a like-for-like data-structure cost. O and Ol include no wakeups. Dn includes an unknown number of
  futex wakeups. K is dominated by its wakeups.
- At 1t the slow kernel uprobe (1392 ns per call) probably lets the consumer catch up after almost every record, so
  most kernel records pay irq_work plus an epoll wakeup.
- The kernel's fall from 876 to 406 ns at 4p is most plausibly fewer wakeups per record at the higher aggregate rate.
  It was not measured, and 1t vs 4p also changes core type and clock.
- At 1t, Tern's smaller marginal cost than O (70 vs 80) comes with a 10.8 ns higher empty-hook cost: per call they are
  equal.
- Tern's 4p marginal cost: one rep at 328 ns, the other four at 504–552.

**Hook cost (empty), 1t.**
- Tern 164.9 ns: +7 % over official 4708f409 (154.1), but −4 % vs its own base, upstream head (172.3).
- So this does not corroborate the design text's attribution of the +7 % (design.tex:74; 131.2 vs 122.8 ns) to the
  grace-period counter. Part or all of it may come from the upstream base.
- Kernel uprobe: 1392 ns.

## Comparison with the earlier Jetson measurement

Source: DES §3.2 / DEVDOC; ns per call with the hook included. The Jetson figures (Tern vs kernel) were 518 vs 2041 for
one thread and 2991 vs 3272 for 4 processes. On the matching x86 metric (rb per call) the signs agree: 234 vs 2262 and
939 vs 2028. Tern is lower in both.

## Reading

- **Per traced call:** Tern's ring-buffer path is no slower than any userspace build. It ties official with one
  producer and is lowest with four. It is 2.2–9.7× cheaper than the kernel's, including the hook.
- **Not shown:**
  - that Tern's ring buffer as a data structure is cheaper than the kernel's;
  - that it scales worse with several producers.

  The marginal figures mix in different notification protocols. A kernel leg without wakeups (BPF_RB_NO_WAKEUP) and
  wakeup counts would be needed.
- **Exploratory, single session.**

## P10b: no notification on any backend (session P10b-20261127-try1, plans/P10b.txt; check-after PASS)

**Change.**
- Program rbnw submits with BPF_RB_NO_WAKEUP.
- The loader busy-reads the ring with `ring_buffer__consume` in a loop on CPU 4. The plan text still says
  "poll timeout 0": that was changed after a smoke (not kept in raw/), because without wakeups the kernel ring fd
  never becomes ready for epoll and the ring filled. bpftime's mocked epoll checks for data, so it would not have
  needed the change.
- From the sources:
  - Dn honors NO_WAKEUP (bpftime_shm.cpp skips notify).
  - O and Ol ignore the flag; they only run a SPDLOG_WARN level check, which is off. They never notify anyway.
  - So nothing notifies on any backend.

**Run.** 90 rows, all victim rc 0, all 80 tool rows tool rc 0; all 40 rbnw rows deliver the expected records with
0 drops (count only).

**Code.** p10/ after the P10b change. The v1 sources are kept as *.v1, but P10's exact binaries were overwritten; the
victim is unchanged (same sha). sha256.txt was updated and checked at session start. Header comments in p10tool.c and
p10.bpf.c still describe v1.

**What rbnw − empty measures.** Paired by rep, it is the per-implementation reserve+submit cost under a busy-reading
consumer. The BPF operation is the same everywhere, but the implementations do different work:
- O/Ol take a pthread spinlock and load consumer_pos on every reserve.
- Dn caches consumer_pos under an inline lock and records an owner slot.
- The kernel takes an irqsave spinlock.

ns per call; median over 5 reps [min, max]:

| | K | O | Ol | Dn (Tern) |
|---|---|---|---|---|
| 1t rbnw per call | 1505 | 269.7 | 296.0 | 225.8 |
| 1t rbnw − empty | 118.8 [91.7, 122.4] | 117.3 [110.8, 123.6] | 122.5 [118.9, 136.0] | 58.8 [48.4, 77.9] |
| 4p rbnw per call | 1727 | 1144 | 1130 | 980 |
| 4p rbnw − empty | 127.2 [112.1, 139.2] | 772.5 [740.3, 888.6] | 646.6 [501.1, 655.9] | 575.6 [567.3, 587.8] |
| 4p/1t of rbnw − empty | 1.07× | 6.6× | 5.3× | 9.8× |
| 4p/1t of empty (placement alone) | 1.15× | 2.43× | 2.75× | 2.48× |

**One producer.**
- Tern's figure is about half of the others' (59 against 117–123 ns), with a wide spread (48–78).
- Part of the gap comes from this consumer. With P10's 1 ms polling consumer, O's figure was 80 ns, and here 117 ns:
  about +37 ns of consumer cache-line traffic, which Tern largely avoids by caching consumer_pos.
- Real tools use epoll and do not busy-read, so "half" is specific to this setup.

**Four producer processes.**
- Every userspace build's figure rises far more than placement alone explains. Scaling each 1t figure by its own
  empty-hook ratio predicts about 285 (O), 337 (Ol) and 146 (Dn) ns, against the observed 773, 647 and 576. A 2–4×
  excess remains.
- The kernel stays at about 127 ns, but it runs at a lower aggregate rate: its uprobe trap limits 4 producers to about
  2.3 M records/s, against Tern's 4.1 M/s and O/Ol's 3.5 M/s. Whether the kernel ring buffer would pay the same
  contention at Tern's rate was not measured; that would need a rate-matched kernel leg or a profile.
- Tern's median is the lowest of the userspace builds, but Ol is bimodal and ties or beats Dn in 2 of 5 reps
  (per call 978–981 ns; figure 501–506).

**Per traced call.** Tern is still about 1.8× cheaper than the kernel at 4p (980 against 1727), because the kernel's
uprobe trap (about 1.4–1.6 µs) dominates.

## Reading (P10 + P10b)

- Per traced call, Tern's ring-buffer path is no slower than any userspace build: it ties official with one producer
  and has the lowest median with four. Including the hook, it is 1.8–9.7× cheaper than the kernel's.
- With one producer and no notification, Tern's reserve+submit is the cheapest measured, with a busy-reading
  consumer.
- With four producer processes, every userspace ring buffer's per-record cost rises 2–4× beyond what core placement
  explains. Tern's reaches 4.5× the kernel's, at a 1.8× higher aggregate record rate. That contention is the likely
  cause is inferred, not profiled.
- **For the design paragraph:** claim no more than a cheaper per-call path. If the paragraph keeps the ring buffer,
  state the multi-producer cost.
