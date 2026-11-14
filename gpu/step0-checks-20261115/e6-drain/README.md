# step0 e6: drain throughput and segment size

Check 6 of `/home/jetson/src/results/gpu/dir1-design-space-20261115/README.md` section 5 (decides for A, C and D):

> 6. **Drain throughput and segment size** (A, C, D).
>    - A pop or lookup loop through intercepted `bpf()` over a pre-filled pool: bytes/s, and the effect on GPU time (CPU and GPU share DRAM).
>    - Whether `BPFTIME_SHM_MEMORY_MB=1024` registers and prefaults.
>    - Decides the sustainable record rate (decode offers 77.3M warp records/s) and the largest budget.

Code under test: dev 52126f33 (`/home/jetson/src/bpftime-daily-state/worktrees/4708f4092d1e/candidate`, its `build-cuda`),
run through `/home/jetson/src/bpftime-gpu-work/run.sh`. Nothing in the dev tree was changed or built.

## Files

| file | what |
|---|---|
| `e6_drain.c` | the reader, a loader under the syscall server: `bench` (part a), `toggle` (part b), `const MODE` (b with llama) |
| `e6_gpubw.cu` | the GPU workload of part b: device-memory copy (float4 LDG/STG.128, `build-check/e6_gpubw.sass`), event-timed batches stamped with CLOCK_MONOTONIC |
| `run_a.sh`, `run_b.sh`, `run_b_llama.sh` (prepared, wiring smoked with a stand-in victim), `run_c.sh` | one script per part, each with a total timeout; each runs `make` (incremental) first |
| `test-fixtures/` | `fake_llama_bench.sh` + a saved llama-bench JSON (pp128, graphs): the stand-in victim for `run_b_llama.sh`'s wiring smoke |
| `summarize_e6.py` (a, c), `analyze_b.py`, `analyze_b_llama.py` | summaries and the decision rules below |
| `Makefile` | builds `e6_drain` (libbpf.a copied from `example/gpu/threadhist-llama/.output`) and `e6_gpubw` (`-keep`) |

Part c reuses `../e5-atomics` (`e5_loader`, `e5_fadd.bpf.o`, `e5_victim --report-mem --alloc-mib`).

## Method

**(a) Drain rate.** `e6_drain bench` runs as the loader under the syscall server, so every `bpf()` call takes the
intercepted path a tool would take (`syscall_context::handle_sysbpf`). The victim is `sleep` with `NO_AGENT=1`, which only
holds run.sh open. The segment is `BPFTIME_SHM_MEMORY_MB=512`.

For each map type (BPF_MAP_TYPE_ARRAY, and 1503, which the server creates as 1513, as `map-type-lines.txt` records), it
does the following with a pool of 64 MiB of 40-B values, 32 MiB of 4-KiB values and 32 MiB of 1-MiB values:
1. Create the map and fill it with `bpf_map_update_elem`; each value carries its key in its first and last 8 bytes.
2. Verify every key once.
3. Loop `bpf_map_lookup_elem` over the keys in order for `E6_SECONDS`, checking the first word of every value.
4. Report records/s, bytes/s and ns per lookup.

It also tries `BPF_MAP_LOOKUP_BATCH`. The server has no case for it (runtime/syscall-server/syscall_context.cpp:515-856: the switch
ends in `default:`, which calls the kernel), so the script records the errno. Last, it runs a pop loop on a
BPF_MAP_TYPE_QUEUE of 40-B values (`bpf_map_lookup_and_delete_elem`, the one-record-per-call pop C would use), refilling
untimed.

**(b) Effect on GPU time.** One run.sh run:
- The loader `e6_drain toggle` fills a 64 MiB pool of 40-B values and a 64 MiB pool of 1-MiB values (both 1513), then waits
  until the GPU workload creates `E6_GO_FILE`.
- It then runs the schedule `[off, d40, off, d1m, off, hog1, off, hog4]` x `E6_ROUNDS`, `E6_PHASE_S` s per phase, and logs
  each phase's CLOCK_MONOTONIC bounds and the work it did.
- The modes:
  - d40: 40-B lookups as in (a);
  - d1m: 1-MiB lookups;
  - hog1 and hog4: plain memcpy between two 64 MiB malloc buffers per thread (over 10x the CPU caches, so it streams
    DRAM), in 1 or 4 threads, with no bpf. They are the positive control: they must slow the GPU. Hog threads and buffers
    exist only in runs that use them (toggle: 4; `const hogN`: N; other const modes: none).
- `run_b.sh` runs only with MemAvailable >= 2,560 MiB (segment, hog and GPU buffers, 1 GiB to spare) and records a skip
  otherwise.
- The victim `e6_gpubw` (`NO_AGENT=1`, a plain process) copies 256 MiB to 256 MiB in device memory in batches of 8
  kernels. It times each batch with events and stamps it with CLOCK_MONOTONIC.

`analyze_b.py` keeps the batches wholly inside a phase, with 0.2 s guards at each edge. It reports per mode the median GB/s,
the slowdown against the off phases (pooled, and paired with the off phase just before), and the drain rate reached
meanwhile.

The llama variant (`run_b_llama.sh`, prepared; llama not run) has three arms per round, in rotating order: `e6_drain const off`,
`const d40` and `const hog4`. The GPU workload is llama-bench pp512 `-r 3` with warm-up, CUDA graphs on (the script unsets
`GGML_CUDA_DISABLE_GRAPHS`, which llama.cpp tests only for being set). Each arm runs only with MemAvailable >= 2,560 MiB
(3,072 for hog4), else the arm is recorded as skipped. The loader prints a line a second, so run.sh's ready wait takes its
full 4 s and the drain is running before llama-bench starts; it runs until run.sh's SIGINT after llama-bench exits.

**(c) Segment size.** For each `BPFTIME_SHM_MEMORY_MB` in `128 1024 1536 1792 2047 2048`:
- The run is skipped if MemAvailable - size < 1024 MiB or `/dev/shm` lacks room. `/dev/shm` is a 3,767 MiB tmpfs.
- Loader: `e5_loader e5_fadd.bpf.o`. Victim, under the agent: `e5_victim 48 256 10 --report-mem --alloc-mib 256`.
- The agent registers the segment before `main` (runtime/src/bpftime_shm_internal.cpp:1414-1469: prefault, then
  `cuMemHostRegister`).
- At `main` the victim prints:
  - the time from process start (`/proc/self/stat` starttime, 10 ms ticks) to `main`;
  - MemAvailable and Shmem;
  - VmRSS, VmPin and VmLck;
  - Size, Rss and Locked of the segment's own mapping, from `/proc/self/smaps`.
- The script also keeps the agent's "Registered shared memory with CUDA: ... size=" line or its error, and samples
  MemAvailable every 0.2 s.
- "Works" means three things: the whole segment registered, the e5 fetch-add check passed (122,880 firings, counter ==
  calls, no gaps), and the victim's 256 MiB cudaMalloc + memset succeeded.

## Decision rules (written before the real run)

- **(a)** The drain rate R40 is the records/s of 1513 lookups of 40-B values over one `E6_SECONDS` window (one window
  per run; repeat `run_a.sh` for a spread). A run counts only if
  `verify_bad == lookup_bad == fill_fail == 0` and the window was not interrupted (`r40_valid`). Batch reads are **unsupported** unless `BPF_MAP_LOOKUP_BATCH` returns 0
  with count > 0.
- **(b)** The rule needs the control first (the gates in the first and fourth items were added in review, before the real run):
  - **Inconclusive** if any phase holds fewer than 3 whole GPU batches inside its 0.2 s guards, or the schedule did not
    finish.
  - The test **can fail** only if hog4 slows the copy kernel by more than T = max(5 %, 3 x the spread between the off
    phases' medians). Otherwise the result is **inconclusive**.
  - **Inconclusive** also if any d40 lookup failed or read a wrong value, if no d40 phase has an off phase just before it,
    or if the off phases spread by 5 % or more (the resolution must be finer than the limit).
  - With the control valid: **sustainable record rate = the d40 phases' median drain rate** (one reader; about R40 when
    the reader keeps its idle-GPU speed) if the d40 phases slow the GPU copy by < 5 % (median against off, and every paired
    value < 5 %).
  - If d40 slows it by >= 5 %, the sustainable rate is **below that rate, and not determined here**: it would need a
    throttled drain, which this test does not have.
  - d1m gives the same answer for whole-value reads of large records (bytes/s instead of records/s).
  - The llama variant uses the same rules on pp512 tokens/s, per round against that round's off arm, with the median over
    rounds; its resolution gate is the spread of the off rounds' medians.
- **(a) against the demand:** R40 / 77.3M decode warp records/s, and R40 / 29.4M prefill, are the fractions one reader
  thread can keep up with.
- **(c)** The **largest budget** is the largest size that works, as defined above. A size whose registration fails (agent
  log "cuMemHostRegister() failed", or a registered size other than the segment's) counts as **no**, even though the victim
  still runs, unprobed. A complete run (run.sh `client_rc` and `server_rc` 0, the victim's line present, 122,880 calls) that
  fails the e5 check or the 256 MiB allocation is also **no**. A size that registered whole but whose run was cut short, or
  with no registration line, is **inconclusive**; a skipped size is **untested**. Neither counts as no
  (`summarize_e6.py c`, three-state `verdict`; added in review, before the real run).

## Commands (main session, quiet board, after the clock check)

```bash
cd /home/jetson/src/bpftime-gpu-work/step0/e6-drain
timeout -k 5 300 ./run_a.sh          # E6_SECONDS=5: ~95 s (the sleep victim holds 83 s)
timeout -k 5 400 ./run_b.sh          # E6_PHASE_S=5 E6_ROUNDS=4: ~185 s
timeout -k 5 400 ./run_c.sh          # sizes 128 1024 1536 1792 2047 2048: ~2-3 min (skips what memory does not allow)
timeout -k 5 900 ./run_b_llama.sh    # optional, llama never run here: 3 arms x 6 rounds, ~8 min
# wiring check only (no GPU, ~45 s): E6_LLAMA_ROUNDS=1 E6_LLAMA_CMD=./test-fixtures/fake_llama_bench.sh timeout -k 5 300 ./run_b_llama.sh
```
Results go to `results/{a,b,c,b-llama}-<TS>/`, with `summary*.txt` and `summary*.json`. The run logs are in
`/home/jetson/src/bpftime-gpu-work/runs/step0-e6{a,b,c,bl}-<TS>*/`.

## Smoke results (2026-11-15, board shared with other agents; the numbers show that the pipeline works and are not measurements)

**(a)** `results/a-smoke1`, with `E6_SECONDS=1`:

| map | value | records/s | GB/s | ns/lookup |
|---|---|---|---|---|
| ARRAY | 40 B | 7.17M | 0.29 | 139 |
| 1513 | 40 B | 6.72M | 0.27 | 149 |
| ARRAY / 1513 | 4 KiB | 2.48M / 2.45M | 10.2 / 10.0 | 403 / 409 |
| ARRAY / 1513 | 1 MiB | 9.9k / 9.9k | 10.4 / 10.3 | 1.0e5 |

- `BPF_MAP_LOOKUP_BATCH` falls through to the kernel and returns EINVAL (22): **unsupported**.
- QUEUE pop: 7.48M pops/s, 0 bad; an empty pop gives ENOENT.
- One reader thread at R40 = 6.7M/s covers 8.7 % of decode's 77.3M warp records/s and 22.9 % of prefill's 29.4M.

**(b)** `results/b-smoke2`, with `E6_PHASE_S=1 E6_ROUNDS=1`, one sample per mode:

| mode | GPU copy GB/s | slowdown vs off | drain meanwhile |
|---|---|---|---|
| off | 93.1 (phase spread 0.07 %) | - | - |
| d40 | 91.5 | 1.7 % | 6.72M records/s |
| d1m | 82.3 | 11.6 % | 7.6 GB/s |
| hog1 | 76.0 | 18.4 % | 6.2 GB/s memcpy |
| hog4 | 56.2 | 39.7 % | 10.4 GB/s memcpy |

The loader mode the llama variant uses (`e6_drain const d40` / `const hog4`) was smoked separately, with a `sleep 5` victim
and no GPU work (`results/const-smoke1/`). d40 kept 6.70-6.73M records/s per 1-s window; hog4 copied 20.8-20.9 GB/s with
the GPU idle (10.4 GB/s while the GPU copy ran in `b-smoke2`). The llama variant itself was not run.

The control is valid. Under the rule, the 40-B drain is sustainable at its full rate of about 6.7M records/s. Draining
large values cost the GPU about 1.5 % per GB/s drained (11.6 % at 7.6 GB/s).

Review re-smoke with the gated analyzer and 64 MiB hog buffers (`results/b-review2`, same settings): off 93.1 GB/s (spread
0.11 %), d40 -1.4 % at 6.72M records/s, d1m -11.6 % at 7.8 GB/s, hog1 -18.1 %, hog4 -38.9 %; every phase had 7-48 batches;
verdict "sustainable" at 6.72M records/s. The 128 MiB buffers (`b-review1`) gave the same picture.

Llama-variant wiring (`results/b-llama-wiring2`, `E6_LLAMA_ROUNDS=1` with `test-fixtures/fake_llama_bench.sh` as the victim,
no GPU): all three arms ran, each JSON was extracted from client.log, the drain logs were read, and the analyzer returned
"inconclusive" because the stand-in cannot slow down (the control gate works). An earlier try (`b-llama-wiring1`) skipped
hog4 by its memory rule. llama-bench itself was never run under this script.

**(c)** `results/c-smoke1`, `c-smoke3`, `c-smoke2` (re-summarized in review; `c-review1` repeats 128 MiB):

| size | registered | start to main | segment Rss at main | MemAvailable before -> at server ready -> at main (MiB) | works |
|---|---|---|---|---|---|
| 128 MiB | yes, whole | 469 ms | 100 % | 3,902 -> 3,764 -> 3,720 | yes |
| 1,024 MiB | yes, whole | 835 ms | 100 % | 3,850 -> 2,800 -> 2,768 | yes |
| 1,536 MiB | yes, whole | 1,152 ms | 100 % | 3,793 -> 2,267 -> 2,223 | yes |
| 2,048 MiB | **no**: `cuMemHostRegister() failed: 2` (OUT_OF_MEMORY) | 1,282 ms | 100 % (prefaulted anyway) | 3,798 -> 1,745 -> 1,687 | **no**: CUDA attach disabled, 0 firings |

- The memory goes when the **server creates** the segment: Boost commits the whole segment at creation (posix_fallocate;
  dev tree CLAUDE.md, "Shared Memory Size"), so MemAvailable falls by the segment size before the victim starts
  ("at server ready", from `mem-samples-*.txt` at run.sh's "server ready" time). The agent's prefault and registration
  then cost 32-58 MiB more. The agent prefaults the whole segment (Rss = Size at main).
- Registration plus prefault took about 0.4 s per GiB on top of the 128 MiB start-up.
- The 2,048 MiB registration failed with 1,745 MiB available at the time (the segment itself had already taken 2 GiB of
  the 3.8 GB available before the start). So plain exhaustion is not excluded; the cause was not identified.
  RLIMIT_MEMLOCK is 941 MiB, and 1,536 MiB registered past it with Locked = 0, so it is not that limit.
- In this smoke the largest budget is 1,536 MiB. The main session's list adds 1,792 and 2,047. 2,048 MiB is exactly 2^31 bytes, so 2,047 tells a 2^31-byte boundary apart from a limit that scales with memory.

## Known limits

- One reader thread, in-order keys, values copied into one buffer. Several threads, or random keys, may differ: the server
  copies the value outside any lock (runtime/syscall-server/syscall_context.cpp:600-622), so readers may scale; untested.
  - The bench copies the whole value the way the syscall server does (`memcpy` of the value size).
  - No record format, accept rule or fence is applied, so this is an upper bound for A's drain and D's poll.
- The C design's chunk pop has no map type yet. The QUEUE pop shows only the cost of one `bpf()` pop per record.
- (b)'s GPU workload is a pure bandwidth kernel, close to the worst case for DRAM sharing. llama.cpp's mix (the prepared
  variant) may be less sensitive.
- Phase alignment is by CLOCK_MONOTONIC across two processes, with 0.2 s guards. Batches spanning a phase edge are dropped.
- (b) and (c) need the pools and segments in shared memory. They are sized for the board with about 3.8 GB available. (c)
  skips sizes by rule, so a busy board yields "untested", not "no".
- The 2,048 MiB registration failure was seen once, on a busy board. The skip rule (MemAvailable before the start - size
  >= 1 GiB) does not separate a size cap from low memory at registration time; compare 2,047 and 2,048 on the quiet board.
- (c) measures start-to-main with 10 ms resolution, and includes everything the agent does before `main`, not only the
  registration. The 128 MiB row is the baseline to subtract.
