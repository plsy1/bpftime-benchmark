# Note: Dn inherits upstream's 1 ms epoll_wait wrapper (2026-11-25)

## What

`syscall_context::handle_epoll_wait` (runtime/syscall-server/syscall_context.cpp) differs between the trees:

| tree | line | consumer epoll_wait on a bpftime epoll fd |
|---|---|---|
| D 9ccda7e8 | 1144-1156 | `return bpftime_epoll_wait(epfd, evt, maxevents, timeout);` (libbpf's 100 ms timeout passed through) |
| Ol 70f7bc1f | 1525-1564 | loop: `orig_epoll_wait_fn(epfd, ..., 0)` (kernel, non-blocking), then `bpftime_epoll_wait(..., min(remaining, 1))`, until the timeout |
| Dn c71a9987 | 1595-1635 | identical to Ol |

Source of the change: upstream 82e7dc60 (2026-11-15, #728, "userspace syscall server fd semantics, dispatcher export and tracepoint emulation"). Its purpose is to let one epoll fd mix kernel fds with bpftime's userspace sources.
- 82e7dc60 is an ancestor of Dn (it came with the rebase onto 70f7bc1f).
- It is not in D.
- Dnpc and Dxn have it too, because they are built from the same base.

## Effect on the consumer

Inside `bpftime_epoll_wait` (Dn runtime/src/bpftime_shm.cpp:452-660), the dev wait logic is unchanged:
- In a steady stream, for 2 ms after records were found, it naps 100 us on a futex registered in `nappers`. A producer wakes a napping consumer only when a shard is half full or a sample was dropped, so producers make no futex syscall per sample.
- When idle, it sleeps on a futex registered in `waiters` for up to min(100 ms, timeout). A producer FUTEX_WAKEs a consumer in `waiters` on any wake-eligible sample: every sample with libbpf's wakeup_events = 1, or every 32nd per shard with SSLSNIFF_PB_WAKEUP=32.

The wrapper calls `bpftime_epoll_wait` with timeout 1 ms, so:
1. The idle sleep is cut from up to 100 ms to about 1-2 ms per inner call. `bpftime_epoll_wait` only stops once more than `timeout` ms have elapsed, so each call lasts 1-2 ms. Each return adds one kernel epoll_wait(0) and the loop overhead.
2. Each inner call starts a new wait with a new start time. The nap state (thread-local `last_ready_time`) carries over, so the nap/idle choice is unchanged.

In a dense stream such as ssl-nginx at about 27 k events/s, the consumer should mostly be napping. The wrapper then adds roughly one epoll_wait(0) per return, which is cheap. The 1 ms cap matters when there are gaps: the consumer cycles in and out of `waiters` more often, and every entry into `waiters` makes producers eligible for a FUTEX_WAKE.

## Observation it may explain (not verified)

E18, stock legs, CPU per event on core 3 (sslsniff's core), from `c-percore-E18.txt`:

| build | core 3, us/event | sslsniff process CPU, us/event |
|---|---|---|
| Dn | 6.0 | 2.73 |
| D | 1.4 | 2.54 |
| Dnpc | 1.5 | not listed here |

The Dn values are consistent over all 20 reps.

All-core: Dn/D = 1.093 (+4.7 us/event). nginx cores 4-5: Dn 31.8 vs D 30.5 us/event.

Since the consumer's own CPU barely changes, the extra core-3 time is outside its user/system accounting: irq/softirq, wakeup/IPI or scheduler work on core 3. That fits more futex sleeps and wakeups of the consumer, which the wrapper would cause.

What does not fit: Dnpc and Dxn have the same wrapper and show no excess. So the wrapper is at most part of the story. A timing-dependent regime (napping vs waiters) that flips with the producer's per-event cost is one possibility. It is untested.

## How to check (not done)

- In the C1 H1 data: sniff schedstat, voluntary context switches and core-3 irq/softirq deltas per row for Dn vs D. Futex wake counts would be direct evidence.
- A/B: Dn with `handle_epoll_wait` reverted to D's pass-through (one-line patch), same session as Dn.
- `perf stat -e syscalls:sys_enter_futex,sched:sched_wakeup` on nginx and sslsniff, Dn vs D.
