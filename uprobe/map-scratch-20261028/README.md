# Large per-CPU scratch value under threads (2026-10-28)

Fourth benchmark of the dimension coverage (plan: `../map-usage-20261027/README.md`). It
reproduces sslsniff's event record: a per-CPU array with one 8 KB value (8256 B with the
header), looked up on every call, header filled with the same helpers (time, pid/tid, uid,
comm), and min(len, 8192) bytes of the caller's buffer copied in with `bpf_probe_read_user`.
No output (perf output is a separate path), so the lookup and the copy are measured.

`scratchloop THREADS SECS LEN`: THREADS threads (thread i pinned to CPU 5 - i), each calling
`send_event(buf, LEN, tag)` on its own 8 KB buffer; before each call it writes a new tag into
the first and last 8-byte word, and the program checks both in its copy (a record shared with
another thread or a stale copy would differ). A build passing a wrong tag made every call fail
the check on both backends (smoke), so the check works. Control: same header helpers into a
stack record, no map, no copy. Scratch cost = scratch minus control on the same backend.

Backends `none`, `kernel` (BTF kernel 6.8.12), `09421773` (dev HEAD frozen build, LLVM JIT,
shm 256 MB). LEN 0 / 256 / 8192, threads 1 / 2 / 4 / 6, 3 reps, 6 s. jetson_clocks on, cpu5 at
1728000 before and after. 180 runs, 0 failed checks (`run1-scratch.jsonl`, `summary-run1.md`).

Scratch cost per call (us), kernel / bpftime:

| Threads | LEN 0 | LEN 256 | LEN 8192 | probe w/o map (LEN 0) |
|---:|---:|---:|---:|---:|
| 1 | 0.01 / 0.02 | 0.04 / 0.03 | 0.35 / 0.22 | 1.32 / 0.28 |
| 2 | 0.01 / 0.02 | 0.19 / 0.03 | 0.28 / 0.23 | 1.91 / 0.35 |
| 4 | 0.15 / 0.05 | -0.06 / 0.02 | 0.20 / 0.76 | 3.36 / 2.28 |
| 6 | 0.00 / -0.07 | 0.04 / -0.02 | 0.11 / 0.26 | 4.12 / 4.65 |

- The per-CPU array lookup costs next to nothing on both (LEN 0), at every thread count: no
  lock on this path, and every copy was correct, so bpftime's per-CPU array gives each pinned
  thread its own record.
- The 8 KB copy costs 0.22 us on bpftime and 0.35 on the kernel (1 thread).
- bpftime at 4 threads, 8 KB: 0.76 us, reproducible (3.02-3.10 vs control 2.26-2.28 in all 3
  reps); 0.22-0.26 at 1, 2 and 6 threads. Not explained yet; small next to the probe path.
- Totals follow the probe path, not the map: bpftime 0.31 -> 4.58 us (LEN 0, 1 -> 6 threads),
  kernel 1.33 -> 4.12. At 6 threads bpftime is slower than the kernel with no map work at all
  (problem 2 in `../dimension-problems.md`).

No new map problem. Files: `src/` (`build.sh`, `scratch.bpf.c`, `scratchloop.c`, `attach.c`
from `../map-table-20261028`), `run_scratch.py`, `summarize.py`, `run1-scratch.*`, `smoke/`.
