# Per-CPU maps with unpinned threads lose counts (2026-10-28)

Every earlier thread benchmark pinned each thread to its own CPU. Real programs do not, so a
thread can be preempted, or moved to another CPU, in the middle of a probe. Per-CPU maps give
each CPU its own copy of the value, and programs update it with a plain read-modify-write
(`*v += 1`), relying on nothing else touching that CPU's copy while the program runs.

Workload: `src/countloop.c` (copy of `../map-table-20261028/src/countloop.c`, CPUS `none` =
not pinned, up to 12 threads), all threads calling `count_event(0)`. Programs
(`src/count.bpf.c`, unchanged): per-CPU array and per-CPU hash with lookup + `*v += 1`, and an
ordinary array with an atomic add as the reference. Configurations: 6 threads pinned (as
before), 6 unpinned, 12 unpinned (more threads than the 6 CPUs, so preemption and migration are
frequent). Kernel (BTF 6.8.12) and bpftime `c98951a4` (dev HEAD, per-CPU hash fixes); 3 reps
of 6 s. Probe hits equalled the calls in all 54 runs; a run fails when the summed map value
(all CPUs) is lower than the calls. jetson_clocks on, cpu5 at 1728000 before and after.
Runner `run_unpinned.py`; `run1.jsonl`, `summary-run1.md`.

| Map | Threads | kernel lost | bpftime lost |
|---|---|---:|---:|
| per-CPU array | 6 pinned | 0 | 0 |
| | 6 unpinned | 0.004% (1 of 3 runs) | 0.078% (all runs) |
| | 12 unpinned | 0.011% | **2.25%** |
| per-CPU hash | 6 pinned | 0 | 0 |
| | 6 unpinned | 0.004% (1 of 3 runs) | 0.059% |
| | 12 unpinned | 0.089% | **1.44%** |
| array, atomic add | all | 0 | 0 |

- The kernel is not exact either: in 6.8 a uprobe program runs with migration disabled but
  preemption enabled, so another task on the same CPU can run the program in between and both
  update that CPU's copy.
- bpftime loses 15-200x more, most likely because it picks the CPU with `sched_getcpu()` at each lookup and does
  not stop the thread from moving. After a migration the thread keeps writing the old CPU's
  copy while another thread, now on that CPU, writes the same copy at the same time (truly in
  parallel, not only interleaved); with 12 threads on 6 CPUs this happens constantly.
- Atomic updates on an ordinary array never lost a count, on either runtime.

Not fixed. Options: give each program run an exclusive slot (claim the current CPU's slot with
one atomic at program entry, try another if it is taken, release at exit), which matches what
the kernel's migration-disable gives; rseq (restartable sequences) only works for a few
instructions of hand-written code, not for a whole BPF program; locking per lookup does not
cover the write that follows it.
