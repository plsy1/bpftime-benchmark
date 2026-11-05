# opensnoop delivers half the opens under bpftime: ring buffer, no wakeup

Problem 13 of `uprobe/realtools-20261104`: opensnoop under the transformer
reported 18,838 of 38,800 opens (48.5%), bpf 38,729 (99.8%). Diagnosis only;
nothing is fixed here.

## Two places opensnoop drops an open silently

`opensnoop.bpf.c`: `trace_exit` returns when the enter's arguments are not in
`start` ("missed entry"), and skips the event when `reserve_buf` (here
`bpf_ringbuf_reserve`, libbpf-tools' `bpf_buffer` uses a ring buffer whenever
one can be created) fails. Neither is counted or printed.

## Experiment: same rate, different bursts

The audit's `openloop 4 10000` opens 100 times back to back, then sleeps 10 ms
(about 9,600 opens/s, in bursts of 100). `openburst.c` takes the burst size;
`run.py` keeps the average rate near 10,000/s (burst b, pause 100*b us; b=1
sleeps longer than asked, about 6,300/s) and runs the audit harness's
`traced()` (opensnoop through the syscall server, the workload under the
transformer), bpf and frozen build `dev-b60b99c3` (= dev 94f9ba8f), two runs
each. 2026-11-05 12:50, cpu5 at 1728000. `results.jsonl`, raw outputs in
`raw/`.

| burst | burst time (bpftime) | bpf delivered | bpftime delivered | bpftime per burst |
|---|---|---|---|---|
| 100 | 307-320 us | 98.9%, 99.9% | **47.9%, 51.1%** | 48-51 of 100 |
| 50 | 164-166 us | 99.7%, 100% | **66.6%, 66.8%** | 33 of 50 |
| 25 | 93 us | 100%, 99.8% | 100%, 99.8% | 25 of 25 |
| 10 | 38 us | 99.8%, 100% | 100%, 99.8% | 10 of 10 |
| 1 | 5 us | 100%, 100% | 100%, 100% | 1 of 1 |

Every open is delivered when a burst fits the ring buffer and none beyond it is
when it does not: the loss is not enter/exit pairing (it would not depend on
the burst) and not bpftime's speed (the average rate is the same).

## Cause

- **The ring buffer holds 31 opensnoop events.** `struct event` is 8,232 bytes
  (`full_path`: 255 x 32 bytes of path, used only with `-F`), a record 8,240
  with its header, and libbpf-tools' ring buffer 256 KiB: 31 records. bpf has
  the same 31.
- **bpftime does not wake the consumer when a record is submitted.**
  `ringbuf::submit` (`runtime/src/bpf_map/userspace/ringbuf_map.cpp`) only
  clears the busy bit and ignores the flags. The tool's `epoll_wait`
  (`bpftime_epoll_wait`, `runtime/src/bpftime_shm.cpp`) finds ring buffer data
  by polling: with nothing ready it sleeps in a 1 ms futex wait that only perf
  output wakes ("Records below the wakeup watermark and ring buffers are found
  by the timeout"). The bpf ring buffer wakes a waiting consumer on submit
  (unless `BPF_RB_NO_WAKEUP`), so it drains during the burst.
- A burst of 100 takes about 0.3 ms, shorter than the 1 ms poll: when the
  consumer sleeps through it, 31 records fit and `bpf_ringbuf_reserve` fails
  (ENOSPC) for the rest. When the consumer wakes during the burst (roughly
  0.3 ms of each ~1 ms), it drains and more fit: about 49 per burst of 100,
  33 per burst of 50 (0.16 ms).

## Reach

Any program that outputs through a bpftime ring buffer loses the records that
arrive faster than the ring buffer holds within about 1 ms, silently (the
producer only sees the reserve fail). Large events make it worse (opensnoop,
and the other libbpf-tools with `struct full_path`); tools that use perf
buffers are woken (dev's perf wakeup, `notify_perf_event_consumers`) and
count lost records. The uprobe and syscall paths share the ring buffer, so it
is not specific to the transformer.

## Fix direction (not done)

Wake the consumer from `ringbuf::submit` the way perf output does: the shared
`bpftime_perf_wakeup_state` (sequence and waiter count) already exists; a submit
that sees a waiter bumps the sequence and calls FUTEX_WAKE. The bpf rule is to
notify when the consumer has caught up with this record, and to honour
`BPF_RB_NO_WAKEUP` / `BPF_RB_FORCE_WAKEUP`. The cost when nobody waits is a
load of the waiter count per submit; it needs the ring buffer output A/B.
