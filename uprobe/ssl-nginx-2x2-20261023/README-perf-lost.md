# Finding 3 follow-up: why bpftime lost half the 256 KB events, and the fix (2026-10-23)

Dev worktree `bpftime-daily-state/worktrees/177a1dc465ff/candidate`, on top of `a90fa447`.
Same container, runner and sslsniff as the README in this directory.

## Why the kernel keeps up and bpftime did not

Ring size was not the difference: both sides give sslsniff a 64 KB (16-page) ring per CPU. The
difference was **when the consumer is woken**.

- Kernel: libbpf's perf buffer sets `wakeup_events=1`, so every sample wakes the sslsniff thread
  that is blocked in `epoll_wait`, within microseconds, while nginx is still writing.
- bpftime: nginx's thread writes into its own 64 KB *producer shard* (a per-thread ring in shared
  memory). The shard is copied into the ring sslsniff reads only when sslsniff calls `epoll_wait`.
  bpftime's emulated `epoll_wait` found nothing → slept 1 ms (`sigtimedwait`) → polled again.
  Nothing woke it early.
- One 256 KB response is ~17 `SSL_write` events of ~8.3 KB each, written in ~0.7 ms. The shard
  holds 7 of them, so whatever arrived during a 1 ms sleep beyond 7 events was dropped, and
  `output_data` still returned 0 with no PERF_RECORD_LOST: sslsniff never knew.

## Changes

1. **Losses are visible, the kernel way.** A dropped sample is counted per shard; the count is
   published as a 24-byte `PERF_RECORD_LOST` atomically with the next sample that fits (as
   `perf_output_begin` does). `bpf_perf_event_output` returns `-ENOSPC`; output with no mapped
   ring returns `-ENOSPC` too.
2. **Consumer wakeup.** `bpftime_epoll_wait` sleeps on a shared futex word in the bpftime shm
   (1 ms timeout kept as fallback); a producer calls `FUTEX_WAKE` only if a consumer is sleeping.
3. **Wakeup only at half a ring (final).** Waking on every sample costs the producer ~3.5 us per
   event (futex syscall + cross-core wake). The producer instead asks for a wakeup only when its
   shard is at least half full right after publishing, or a sample was dropped: the kernel's
   `wakeup_watermark` idea. Small events are delivered by the 1 ms timeout as before.

## Builds (`builds/`, see `builds/README.txt`)

| Build | What it is |
|---|---|
| opt-noaff-fork | `a90fa447`, before this work |
| lost-report | change 1 only (consumer still polls every 1 ms) |
| lost-wakeup | changes 1 + 2 (wake on every sample) |
| lost-watermark | changes 1 + 2 + 3 (**the committed version**) |

## Event delivery at 256 KB (audit: wrk -c 10 -d 3)

`bench/audit_lost.py` = `run_fair.py --audit` plus sslsniff stderr, summing libbpf's
"lost N events" lines. Per request, two or more rounds with rotated leg order:

| Leg | delivered/req | reported lost/req |
|---|---:|---:|
| kernel | 17.90, 17.96, 17.94, 17.98 | 0.14, 0.08, 0.10, 0.05 |
| opt-noaff-fork | 9.33, 9.67 | 0 (silently dropped) |
| lost-report | 9.49, 9.49 | 8.54, 8.53 |
| lost-wakeup | 17.99, 17.87, 17.07, 17.99 | 0.04, 0.16, 0.96, 0.04 |
| lost-watermark | 18.04, 17.85 | 0.005, 0.17 |

lost-report proves the diagnosis: delivered + reported = 18.0/req, i.e. every missing event was a
ring-full drop, now reported. The required audit command
(`run_fair.py --payload 262144 --legs kernel,<build> --audit`):
`bench/audit-256k-lost-wakeup.json` kernel 17.86 / lost-wakeup 17.78, and
`bench/audit-256k-lost-watermark.json` kernel 17.72 / lost-watermark 17.95 events/request.
At 16 B both kernel and lost-watermark give 2.000 events/request with 0 lost (`audit-lost-16.json`).

## Throughput (run_fair.py, wrk -c 100 -d 10, 5 reps per leg, order rotated)

`bench/fair-wm-{16,262144}.json` (one same-run set; `fair-lost-*.json` is an earlier set with
lost-report and lost-wakeup that agrees):

| Leg | 16 B req/s | vs kernel | 256 KB req/s | vs kernel |
|---|---:|---:|---:|---:|
| kernel | 11,934 | 0 | 1,214.7 | 0 |
| opt-noaff-fork | 12,960 | +8.6% | 1,312.4 | +8.0% (delivers ~half the events) |
| lost-wakeup | 11,787 | -1.2% | 1,220.3 | +0.5% |
| lost-watermark | 12,860 | +7.8% | 1,274.5 | **+4.9%, same events as kernel** |

Paired per rep: lost-watermark vs lost-wakeup +8.9% (16 B, range +7.9..+11.0) and +4.4% (256 KB);
lost-watermark vs opt-noaff-fork -1.3% at 16 B (range -1.8..+6.7, within noise; the watermark
never fires there) and -2.9% at 256 KB, the cost of now delivering all the data.

**The fair 256 KB comparison is now bpftime +4.9% over kernel** (it was +10.8% when bpftime
dropped half the events).

## Caveats

- Small events (below half a ring) can now wait up to 1 ms for delivery, as before this change;
  the kernel delivers each one immediately. A consumer that needs per-event latency would need
  per-event wakeup (lost-wakeup), at ~9% throughput on this workload.
- The futex word is global in the shm segment: one wake may wake every sleeping consumer (they
  re-check and sleep again). Ring buffers do not notify; they still rely on the 1 ms poll.
- If a consumer is killed while registered, `waiters` stays non-zero and producers pay a futex
  syscall on every watermark crossing until the segment is removed.
- Unit tests: `[perf-lost]`, `[perf-wakeup]` tags in `runtime/unit-test`. Daily filter passes
  (78 test cases). Full runtime suite: 159/160 pass; `test_user_to_kernel_tailcall` needs a
  kernel BPF map (root), and `Test attach replace with ebpf` segfaults, as it did earlier on
  daily candidate `8da867d541f8`; neither involves perf output.
