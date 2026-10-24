# Why bpftime's lead over kernel narrows with payload size (2026-10-24)

Question: the fair sweep (`../ssl-nginx-sweep-bca2ebce-20261024`) shows bpftime (bca2ebce) ahead of
kernel by +16-17% at 16 B-1 KB but +11-12% at 128-256 KB; per SSL event bpftime costs 6.4 us for
small events and 8-9 us for large ones, kernel about 13 us at every size.

## 1. Controlled copy size at a fixed 256 KB response (`bench/`, 8 reps, 18 events/request)

sslsniff variants cap the bytes read and output per event (`src/copycap.diff`: 16, 1024, 4096,
8192 B; stage 3 = read without output, stage 4 = full). The 8192 B stage 4 build is the original
sslsniff BPF code. Per event over no tracing (`bench/summary.md`):

| | 16 B | 1 KB | 4 KB | 8 KB | slope us/KB |
|---|---:|---:|---:|---:|---:|
| kernel, full | 12.0 | 12.4 | 13.1 | 14.7 | +0.33 +/- 0.05 |
| bca2ebce, full | 7.4 | 6.9 | 9.0 | 8.7 | +0.22 +/- 0.30 |

Copying more data costs kernel at least as much per KB as bpftime. At 8 KB bpftime is 6.1 us per
event cheaper than kernel, at 16 B 4.6 us. Copy size is not a bpftime-specific penalty.

## 2. perf (`perf/`, nginx worker, 256 KB, bca2ebce, copy 16 B vs 8 KB)

- `perf stat` (two runs each): copying 8 KB instead of 16 B adds 1-2k instructions, ~100 L1d
  misses and 3-6k cycles (2-3 us) per event; IPC of the worker drops from 1.60 (no tracing)
  to 1.43 (16 B) and 1.35 (8 KB).
- System calls (`raw_syscalls:sys_enter`): copy 16 B -> no futex calls; copy 8 KB -> 3.97 futex
  wakes per request (every ~4.5 events), from the half-ring wakeup of db3d475f: 8 KB events fill
  half of a 64 KB shard after a few events. Other system calls are nginx's own and identical.
- Cycles (`perf/leafs.py` on `cyc-*.script`), extra share of worker samples for 8 KB:
  perf output memcpy +1.0% (~0.42 us/event), futex wake path +1.15% (~0.47 us/event; each wake
  ~2 us because a futex in shared memory goes through `get_user_pages_fast`), probe_read memcpy
  +0.8% (~0.33 us/event); the remaining ~1 us/event is indirect (cache misses).

## Conclusion

1. The narrowing is mostly not a bpftime slowdown. Its absolute per-event advantage over kernel
   stays around 4.5-6 us at every payload (within run-to-run noise). The percentage shrinks because
   each event comes with more no-tracing work at large payloads (29 -> 32-34 us per event of TLS
   work) and both tracers pay a size-proportional copy cost, so the same absolute advantage is
   divided by a larger total.
2. Inside bpftime, a large event costs 2-3 us more than a small one: two 8 KB copies (~0.75 us)
   and consumer wakeups (~0.5 us per event, shared-memory futex), plus ~1 us of cache effects.
   Kernel pays the copies too (its per-KB slope is larger); the futex wake is bpftime's own cost.
3. Run-to-run variance of the 256 KB lead is 2-3 points (+11.1% sweep, +11.7% 2x10, +14.7% here);
   per-event decompositions carry about +/-1 us of noise.

Possible bpftime-side reductions for large events: fewer wakeups per KB (e.g. a watermark tied to
records rather than bytes, or a larger shard), and avoiding the per-wake page-pinning cost of a
shared futex. Both trade delivery latency or memory; neither is implemented here.
