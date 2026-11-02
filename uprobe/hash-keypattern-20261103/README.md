# Hash map churn by key pattern, capacity and load, through BPF programs (2026-11-03 00:13-00:47)

Part 3 of the hash map survey (`../hash-redesign-survey-20261102`). `src/keyloop.c`: threads
each own LIVE/THREADS keys; one op = delete_event(oldest) (lookup + delete) + insert_event(new
key) (update BPF_ANY), uprobes on both (`src/keymap.bpf.c`, 24 B value as memleak's allocs).
Capacities 10240 (the most common max_entries) and 102400, loads 50% and 90%, keys: 8 B `addr`
(scattered 16 B aligned addresses), 8 B `seq64` (pid_tgid-like), 4 B `rand32`, 4 B `shl12`
(id << 12). Backends kernel, a7d54d7e (one map lock, before the 10-30 striped writers),
dev-a4ac0d31 (now). Map cost = object minus `keymap-control.bpf.o` on the same backend and thread
count, medians of 3; 306 runs, all counts exact (`run_keys.py`, `summarize.py`, `summary.md`).
BTF kernel, cpu5 1728000 before and after.

## 1 thread (map cost, us per op)

| capacity, load | kernel | a7d54d7e | dev | dev / kernel |
|---|---:|---:|---:|---:|
| 10240, 50% | 0.28-0.29 | 0.16-0.22 | 0.15-0.24 | 0.52-0.83x |
| 10240, 90% | 0.30-0.32 | 0.18-0.32 | **2.9-3.4** | **10-11x** |
| 102400, 50% | 0.46-0.49 | 0.21-0.35 | 0.18-0.39 | 0.37-0.85x |
| 102400, 90% | 0.61-0.63 | 0.33-0.53; shl12 **13.7** | **32.6-34.5** | **52-56x** |

## 6 threads (map cost per op per thread; the probe path itself is ~6 us here)

| capacity, load | kernel | a7d54d7e | dev |
|---|---:|---:|---:|
| 10240, 50% | 0.18-0.40 | 9.4-9.9 (map lock) | ~0 (rand32, addr); **4.2-4.3** (seq64, shl12) |
| 10240, 90% | 0.19-0.45 | 9.4-10.5 | **19.9-24.0** |
| 102400, 50% | 0.08-0.42 | 9.8-10.2 | ~0; rand32 3.1 |
| 102400, 90% | 0.32-0.59 | 10.2-11.0; shl12 **95.8** | **231-235** |

## Reading

- At half load the textbook table is fine: one thread 0.4-0.9x the kernel; six threads no
  measurable map cost for most patterns (the 10-30 lock-free lookups and striped writers removed
  a7d54d7e's 10 us map lock).
- At 90% load the cost is the token cliff (`../hash-redesign-survey-20261102` finding 1), every
  key pattern: 10x the kernel at 10240, 52-56x at 102400 single-thread, 230 us per op at six
  threads (all inserts take every stripe lock). The stripes of a 102400 map are 3,125, of a
  10240 map 320: the more stripes, the deeper the cliff. a7d54d7e has no cliff.
- The structure's own weakness shows with clustered keys: `shl12` at 102400 / 90% costs a7d54d7e
  13.7 us single-thread and 95.8 us at six threads (long probe runs under one lock); in dev it is
  hidden behind the cliff. Half-loaded small maps with seq64/shl12 keys at six threads: +4 us
  (probe runs cross stripes, writers lock more of them).
- Memory and tombstones (survey findings 3-4) do not show in these runs at this length.
