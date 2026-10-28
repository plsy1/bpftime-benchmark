# Why bpftime's probe path slows down with threads (2026-10-28)

Diagnosis of problem 2 in `../dimension-problems.md`: with no map operation at all, bpftime's
cost per probe hit grows from about 0.2 us (1 thread) to 2.3 (4 threads) and 4.5 (6), worse than
the kernel at 6 threads. Diagnosis only, nothing changed.

`profile.py`: the bpftime loader (dev HEAD frozen build 09421773, symbols and debug info)
attaches `count-control.bpf.o` from `../map-table-20261028` (probe without map work) to
`count_event` of countloop, all threads calling it with one key. After 3 s warm-up,
`perf record -F 1999 -g` samples the workload for 4 s and `perf stat` counts for 3 s. Hit counts
matched the calls in every configuration. jetson_clocks on, cpu5 at 1728000 before and after.
perf 6.8.12 (`/usr/lib/linux-nvidia-tegra-tools-6.8.0-1020/perf`); no ARM SPE on this SoC, so no
`perf c2c`.

| Config | CPUs | us/hit/thread | `_gum_function_context_begin_invocation` | IPC | backend stalls |
|---|---|---:|---:|---:|---:|
| t1 | 5 | 0.21 | 27% | 2.82 | 11% |
| t2 | 5,4 | 0.28 | 51% | 2.13 | 30% |
| t4-cluster0 | 0-3 (one cluster) | 1.25 | 86% | 0.48 | 78% |
| t4-cross | 5,4,3,2 (both clusters) | 2.25 | 92% | 0.27 | 91% |
| t6 | 5-0 | 4.54 | 97% | 0.14 | 95% |

## Cause: Frida's per-function usage counter (shared atomic on every hit)

At 6 threads 83% of the samples in that function sit on two `ldaxr`/`stlxr` loops on the same
word, `[function_ctx + 0x30]` (`t6.annotate.txt`): an atomic increment at the start of the
function and an atomic decrement at its end. In Frida's interceptor (`gum/guminterceptor.c`)
these are

```c
g_atomic_int_inc (&function_ctx->trampoline_usage_counter);          /* begin_invocation */
...
if (!will_trap_on_leave)
  g_atomic_int_dec_and_test (&function_ctx->trampoline_usage_counter); /* or at leave */
```

`trampoline_usage_counter` exists only so that detaching can wait until no thread is inside
the trampoline (`gum_interceptor_transaction_end` frees it when the counter is 0). It is one
word per hooked function, so every thread hitting the same function writes the same cache
line twice per hit; the line moves between cores, more slowly across the two clusters (separate
L3: 1.25 us in one cluster vs 2.25 across). Source read from frida-gum main (bpftime pins the
16.1.2 devkit, which has no source; the disassembly shows the same inc/dec pair). Kernel uprobes
also slow down with threads (1.2 -> 4.2 us), from their own shared state in 6.8 (upstream
reworked the uprobe locking in 6.10+), but bpftime starts 6x lower and ends higher.

## Also seen: per-hit clearing of a ~3 KB invocation stack entry

At one thread 14.5% of the time is `memset` called from `g_array_set_size` <-
`gum_invocation_stack_push`, plus 3.7% `g_array_set_size`, 1.7% `g_array_remove_range` and
1.4% `g_array_maybe_expand` (`t1.report.txt`, `t1.callers.txt`). The per-thread invocation
stack is a `GArray` created with `clear = TRUE`, so every push zeroes a whole
`GumInvocationStackEntry` (a CPU context and `listener_invocation_data[2][1024]`) although
`gum_invocation_stack_push` then sets the few fields it uses. About a fifth of the
single-thread probe cost (0.2 us) goes to this bookkeeping.

## Files

`profile.py`; per config `<name>.report.txt` (flat by symbol), `<name>.callers.txt`,
`<name>.stat.txt`; `t1.annotate.txt`, `t6.annotate.txt`; `results.jsonl`, `profile.log`.
The `perf.data` files (15 MB) are kept on the Jetson only.
