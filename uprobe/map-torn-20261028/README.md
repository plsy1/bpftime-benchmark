# Torn reads through a hash lookup pointer (2026-10-28)

Measures problem 3 of `../dimension-problems.md`, until now from code reading: bpftime's hash
lookup returns a pointer into the map after releasing the map lock, and updates copy the new
value in place, so a program can see a value change while it reads it.

`tornloop THREADS SECS`: thread 0 calls `write_event(seq)`, which updates key 0 of a hash map
(112-byte value, all 14 words = seq, `BPF_ANY`); the other threads call `read_event()`, which
looks the key up, reads the first word, calls `bpf_ktime_get_ns()` (programs call helpers
between lookup and use), then reads the last word and counts a torn read when they differ.
`torn-noprealloc` sets `BPF_F_NO_PREALLOC`. Kernel (BTF 6.8.12) and bpftime 09421773; threads
2, 4, 6 (one writer); 3 reps of 5 s. Probe counts matched the calls in all 36 runs. jetson_clocks
on, cpu5 at 1728000 before and after. (`run1-torn.jsonl`, `summary-run1.md`)

| Map | Threads | kernel torn / M reads | bpftime torn / M reads | kernel torn / M writes | bpftime torn / M writes |
|---|---:|---:|---:|---:|---:|
| prealloc | 2 | 0.11 | 9.95 | 0.12 | 9.9 |
| | 4 | 0.57 | 8.11 | 1.5 | 27 |
| | 6 | 2.79 | 8.11 | 13.5 | 28 |
| no prealloc | 2 | 0.22 | 10.31 | 0.27 | 10.2 |
| | 4 | 0.71 | 5.69 | 2.1 | 19 |
| | 6 | 2.58 | 7.34 | 13.0 | 25 |

- The kernel tears too, with and without preallocation: a replaced element goes back to a
  free list (prealloc) or to the BPF memory allocator's cache (no prealloc) and can be reused
  by the next update while a program still reads it. A lookup pointer is not a stable snapshot
  in either runtime; programs that need one use `bpf_spin_lock` or per-CPU values.
- bpftime tears about 3-90x more often per read (2-80x per write; the gap is largest at 2 threads), because it updates in place
  instead of writing a fresh element and swapping it in.
- So this is a difference of degree within the kernel's own semantics, not a bpftime-only
  correctness bug. Not tested: a pointer used after the key is deleted (bpftime: the slot is
  marked deleted and may be refilled; the kernel likewise reuses the element).

Files: `src/` (`build.sh`, `torn.bpf.c`, `tornloop.c`, `attach.c`), `run_torn.py`,
`summarize.py`, `run1-torn.*`, `smoke/`.
