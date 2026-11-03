# Real tools end to end, dev 7ad5a152 (2026-11-04)

The hash map redesign and the R1 fix (dev 7ad5a152, code-identical to ea32ad8b),
checked with real tools against bpf and the build before the redesign
(dev a4ac0d31). Jetson Orin, BTF kernel 6.8.12-1021-tegra-btf, cpu5 at 1728000
before and after (`run-after-daily.log`). Frozen builds:
`uprobe/ssl-nginx-2x2-20261023/builds/dev-{a4ac0d31,7ad5a152}`. Harness:
`/home/jetson/src/realprog-20261026-btf/harness` (`audit.py`, `measure.py`, and
new today `measure_mt.py`, `profile_mt.py`; multi-thread workloads
`workloads-mt/mt.c`, thread i pinned to CPU 5 - i).

Steps (all from `run-after-daily.sh`, started after the daily job):

1. `audit/`: functional audit, every tool, events expected vs reported.
2. `measure/`: single-thread throughput, 3 rounds x 10 s, backends none, kernel
   (bpf), dev-a4ac0d31, dev-7ad5a152.
3. `measure-mt/`: the same tools on 1, 2, 4, 6 threads, 3 rounds x 10 s.
4. `ssl-nginx/`: nginx + sslsniff, 16 B and 256 KiB responses, audit plus two
   fair rounds (`summary.md`).
5. `profile/`: `perf record -e cycles:u` of the workload, dev-7ad5a152
   (`*.symbols.txt`; perf.data kept on the Jetson, not committed).

Raw text outputs over 1 MB were cut to their first and last 200 KB (counts are
in the jsonl files). `sum.py` makes the tables below.

x86 (NUC) counterparts of steps 2-3 and 5 are on the x86-nuc branch:
`uprobe/x86-nuc-realtools-20261104` (5da7ed06, not pushed yet).

## 1. Audit

All event counts of bpftime match the workload, on both builds:
funclatency (libbpf-tools and example), gethostlatency (bpf lost 932 of
240,300; bpftime 0), memleak (1026 outstanding blocks, bpf 1027), malloc
example, bashreadline (both), usdt_minimal, sslsniff example, bpftrace
uprobe count / uretprobe hist / ustack / usdt jvm gc / bashreadline /
gethostlatency.

Not checked on bpftime (harness, not bpftime):
- opensnoop: 0 events, because the harness preloads the agent (uprobe mode);
  syscall tracepoints need the transformer (`bpftime start -s`).
- javagc: needs `bpftime attach`, which the frozen builds lack.
- get_stack_id example: the workload times out on every backend (rc 124,
  victim never loops). bpftime returns stacks (median depth 6-7); bpf fails
  all 26.

## 2. Single thread (ops/s, median of 3)

| tool | bpf | a4ac0d31 | 7ad5a152 | 7ad5/bpf | 7ad5/a4ac |
|---|--:|--:|--:|--:|--:|
| bpftrace-gethostlatency | 61.0k | 86.2k | 86.7k | 1.42 | 1.01 |
| bpftrace-uprobe-count | 797k | 4.22M | 4.27M | 5.36 | 1.01 |
| bpftrace-uretprobe-hist | 465k | 1.33M | 1.38M | 2.96 | 1.04 |
| funclatency-bpftime-example | 507k | 1.89M | 1.84M | 3.62 | 0.97 |
| funclatency-libbpf | 483k | 1.56M | 1.55M | 3.21 | 0.99 |
| gethostlatency | 60.1k | 90.1k | 87.4k | 1.45 | 0.97 |
| malloc-example | 535k | 523k | 540k | 1.01 | 1.03 |
| memleak | 330k | 428k | 426k | 1.29 | 1.00 |

The redesign changes nothing a real tool can see (0.97-1.04x).

## 3. Threads (ops/s summed over threads, median of 3)

| tool | thr | bpf | a4ac0d31 | 7ad5a152 | 7ad5/bpf | 7ad5/a4ac |
|---|--:|--:|--:|--:|--:|--:|
| bpftrace-uretprobe-hist | 1 | 465k | 1.33M | 1.39M | 3.00 | 1.05 |
| | 2 | 638k | 2.29M | 1.87M | 2.93 | 0.82 |
| | 4 | 796k | 1.39M | 1.88M | 2.36 | 1.36 |
| | 6 | 887k | 1.19M | 1.18M | 1.33 | 0.99 |
| funclatency-bpftime-example | 1 | 504k | 1.89M | 1.85M | 3.67 | 0.98 |
| | 2 | 706k | 2.92M | 2.85M | 4.04 | 0.98 |
| | 4 | 784k | 1.28M | 1.31M | 1.67 | 1.03 |
| | 6 | 920k | 1.18M | 1.16M | 1.26 | 0.99 |
| funclatency-libbpf | 1 | 477k | 1.57M | 1.54M | 3.23 | 0.98 |
| | 2 | 659k | 2.56M | 2.53M | 3.84 | 0.99 |
| | 4 | 787k | 1.35M | 2.14M | 2.72 | 1.59 |
| | 6 | 899k | 1.17M | 1.16M | 1.29 | 0.99 |
| malloc-example | 1 | 532k | 521k | 532k | 1.00 | 1.02 |
| | 2 | 802k | 324k | 332k | 0.41 | 1.02 |
| | 4 | 976k | 261k | 264k | 0.27 | 1.01 |
| | 6 | 1.37M | 269k | 274k | 0.20 | 1.02 |
| memleak | 1-6 | 330k-686k | hang | hang | - | - |

At 4 threads (CPUs 5-2, both clusters) uretprobe-hist is bimodal (a4ac
1.39/1.39/1.81M, 7ad5 2.15/1.82/1.88M). funclatency-libbpf is not: a4ac
1.43/1.34/1.35M, 7ad5 2.14/2.43/2.12M in every round, a 1.6x split that the
other tools do not show and that is not explained (not profiled at 4).
bpf grows with every thread; bpftime peaks at 2 threads and falls back to
1.2M at 6 in every uprobe tool.

## 4. ssl-nginx (`ssl-nginx/summary.md`)

| response | bpf | a4ac0d31 | 7ad5a152 | 7ad5 vs bpf (paired) |
|---|--:|--:|--:|--:|
| 16 B | 12,073 req/s | 14,270 | 14,224 | +18.5%, 19/20 faster |
| 256 KiB | 1,256 | 1,395 | 1,387 | +9.9%, 20/20 faster |

Events per request 2.000 (16 B) and 17.7-18.0 (256 KiB) on every backend.
Unchanged against a4ac0d31.

## 5. Profiles (dev-7ad5a152, cycles:u of the workload)

- funclatency-libbpf, 6 threads: `_gum_function_context_end_invocation`
  55.7%, `_gum_function_context_begin_invocation` 24.9% (2 threads: 14.7%
  and 8.6%). The hot instructions are an `ldaxr`/`stlxr` loop decrementing
  the function context's `trampoline_usage_counter` (offset 0x30): 46% +
  32% of end_invocation's samples. frida-gum is built with LL/SC atomics, not
  LSE. The map: elem_update 1.7%, lookup 0.6%, delete 0.6%. Same for the
  funclatency example (57.2% + 31.9%). On the NUC (x86, `lock` atomics) the
  funclatency example stays 4.75-5.73x bpf at every thread count.
- malloc-example, 6 threads: printk. spdlog formatting (`pattern_formatter`,
  `vformat_to`, `v_formatter`...), its mutex (`__aarch64_cas4_acq` 7.9%,
  `__aarch64_swp4_rel` 7.4%, `pthread_mutex_lock` 2.7%), `_IO_fwrite`,
  `_IO_fflush`, the write system call (`[k]` 7.2%). Map lookup 3.4%,
  update 2.0%. 1 thread: the same functions, without the lock samples.

## memleak hangs with a multi-threaded workload

memleak with `workloads-mt/t*/mallocloop` hung in all 12 runs on both
builds (1 to 6 threads, harness timeout 40 s); with the single-thread
`workloads/mallocloop` it passed every run (steps 1 and 2). Reproduced by
hand and attached with gdb (dev-7ad5a152, t1):

- The agent's detach worker (`ensure_detach_worker_started`,
  `agent.cpp:541`) starts with `ignore_probes_on_this_thread()`, which
  creates the thread's Gum interceptor context; that allocates from Gum's
  dlmalloc, which grows its heap with `mmap` (`sys_alloc`) while holding
  the heap's spin lock. memleak probes `mmap`. The probe enters
  `_gum_function_context_begin_invocation` on the same thread, which has no
  context yet, allocates one, and spins on the heap lock it holds itself.
- The IPC server thread and the workload's worker then wait on the same
  lock (in `interceptor_thread_context_new`); main waits in pthread_join.

The thread-start ignore came with 4d9b8a61 ("Keep the agent's own work out
of the probes", 10-26). Frozen builds, t1 workload, 30 s bound
(`memleak-bisect/`): f9d2d9fc (its parent) passed 2/2 (414k, 425k ops/s)
and also 2 threads (626k) and 6 threads twice (562k, 532k); 4d9b8a61 hung
2/2. So 4d9b8a61 introduced it. The hazard itself is Gum's: any thread whose
first interception needs the Gum heap to grow while `mmap` is probed would
take the heap lock again; with f9d2d9fc the workload's own new threads did
not hit it in these runs. bpf runs the same cells (330-686k ops/s). The NUC
run (x86) did not include memleak.

Nothing caught it before: the gate (unit tests, candidate check) runs no
real tool, and the single-thread harness workload never creates a thread.

## Problems found (all steps, Jetson and NUC)

1. memleak deadlock above (correctness; hangs the traced process; from 4d9b8a61).
2. Multi-thread uprobes on aarch64: Frida's per-function usage counter,
   an LL/SC atomic, takes 81-89% of the cycles at 6 threads; bpftime falls
   from 2.5-2.9M (2 threads) to 1.2M ops/s (6), 1.26-1.33x bpf (3.2-3.7x at
   1 thread). Frida's code (user decision 10-28: do not fix Frida inside
   bpftime).
3. bpf_printk does not scale: malloc example 0.41/0.27/0.20x bpf at 2/4/6
   threads on the Jetson, 0.33/0.20/0.14x on the NUC; spdlog formatting and
   one logger lock per message.
4. Same-key update contention (NUC, malloc example without printk, 6 threads:
   `elem_update` 8.7%, 0.91x bpf): every thread updates the same keys under
   one stripe lock. bpf takes the same bucket lock.
5. Harness gaps: opensnoop needs the transformer, javagc `bpftime attach`,
   the get_stack_id workload times out everywhere.
6. Not bpftime: the example tools count without atomics (`hist[slot] += 1`,
   lookup then update), so their counts are wrong under threads on bpf too.

## Plan (one, in order)

1. memleak deadlock (introduced by 4d9b8a61). Reproduce in a test (a probe
   on `mmap`, an agent thread started after attach). Fix candidates: start
   the agent's background threads and their ignore before any probe is
   attached; or keep the ignore but enter it where no probed call can
   allocate Gum's context (a bpftime re-entry flag checked in the listener
   before Gum's context lookup). Check whether Gum guards context creation
   against a probed `mmap` anywhere (it did not here); not changing Gum's
   allocator. Then rerun step 3 memleak.
1b. Gate: add a bounded real-tool smoke run (memleak and funclatency on the
   threaded workload) to the candidate check, so a hang like this fails the
   daily list.
2. bpf_printk scalability. A bounded shared trace ring (no system call, no
   lock between threads, read by `bpftimetool` like trace_pipe), as already
   suggested under "bpf_trace_printk Output"; at least format without
   spdlog. Gate: malloc example 1-6 threads, Jetson and NUC.
3. Frida counter (needs your decision, Frida code): build frida-gum with LSE
   atomics (a build flag, no source change) and measure funclatency at 1-6
   threads. LL/SC retries are worse than LSE under contention, so it may
   help, but six threads sharing one counter's line stays the root cost:
   LSE will not make it free. Otherwise document and report upstream only.
4. Harness: transformer leg for opensnoop/syscount, an attach-capable build
   for javagc, a working get_stack_id workload.
5. Same-key update contention: recorded only (bpf has the same lock).
