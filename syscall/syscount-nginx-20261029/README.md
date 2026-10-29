# Official syscount-nginx on the Jetson (2026-10-29)

Dev branch `2e850c45` (ARM64 system call trampoline + dispatch fix), build `build-jetson-daily`, cpu5 at 1728000
before and after. nginx and wrk run in a container from `docker/Dockerfile` (the ssl-nginx image
`bpftime-ssl-nginx:20261023` plus sudo and matplotlib, which the script needs), `--privileged --pid=host`, the dev
worktree mounted read-only at its own path. The container was removed afterwards; the image is kept.

`view/` is the directory the script expects as its working directory: `benchmark/syscount-nginx/` (benchmark.py,
nginx.conf, index.html "Hello, World!" as the Makefile writes it), `build` -> the dev build, and
`example/libbpf-tools/syscount/syscount`, a copy of `example/tracing/syscount/syscount` built unmodified with clang-15
(the script still uses the path from before #451 moved the example; `view-sha256.txt`).

## 1. Does each configuration trace anything? (`audit.py`, `audit-1.jsonl`)

One 5 s wrk load per configuration (`-c 100`, as the script), syscount's output kept (the script sends it to
/dev/null):

| Configuration | Requests | System calls counted |
|---|---:|---:|
| kernel syscount `-p <nginx>` | 122,008 | 1,101,045 (setsockopt 2, and close, recvfrom, sendfile, openat, fstat, writev, write ... 1 per request) |
| **benchmark.py's userbpf leg**: nginx with `LD_PRELOAD=libbpftime-agent.so`, then syscount with the syscall server | 123,093 | **0** |
| nginx with the transformer (`AGENT_SO` + `LD_PRELOAD=libbpftime-agent-transformer.so`), then syscount (the script's order) | 115,359 | **0** |
| syscount first, then nginx with the transformer (as `benchmark/syscall`) | – | **nginx aborts at start** |

- The script's bpftime legs trace nothing: preloading only the agent never rewrites a system call (that needs the
  transformer), and programs loaded after the traced process started are not picked up by it.
- The correct order crashes nginx in `ngx_ssl_init` -> `ERR_clear_error` -> `free(): invalid pointer`
  (`repro-crash.sh`, `repro-crash.log`, gdb in the container).

### Cause of the crash: pthread keys shared by two libcs

The transformer loads the agent with `dlmopen(LM_ID_NEWLM)`, so the agent has its own copy of libc. Both copies
allocate pthread key numbers from 0 in their own tables, but `pthread_setspecific` stores into the same per-thread
array. With programs present at start, the agent (GLib/Frida thread-private data) stores pointers under keys 0-4 on
the main thread; a key the program creates later gets one of those numbers and reads the agent's pointer instead of
NULL. OpenSSL takes it as its error state and frees it.

`src/keycheck.c` shows it without nginx: create 8 keys, check that each reads NULL first, store values, issue 1000
system calls, check the values.

| Run | Result |
|---|---|
| no tracing | keys 0..7, 0 wrong |
| agent preloaded directly (same namespace), syscount programs | keys 5..12 (the agent holds 0..4), 0 wrong |
| transformer, no programs | keys 0..7, 0 wrong |
| **transformer, syscount programs** | keys 0..7, **4 new keys read agent pointers** (0, 1, 2, 4) |

The other direction (the program overwriting the agent's values) follows from the same overlap. The dlmopen path is
upstream's and the same on x86, so this is not specific to the ARM64 trampoline (not yet checked on x86). A simple
program (writeloop, 100,000 writes) under syscount counts every write; programs that create pthread keys after the
agent attaches are the ones affected.

## 2. The official script, unchanged (`official/`)

`benchmark.py` copied unchanged to `view/runbench.py` (its cleanup `pkill -f`s the tool's name, which is in the path
of this directory), 10 runs per configuration, wrk `-c 100 -d 10`, run order as in the script.

| Configuration | Requests/s (mean) | vs no tracing | Paper (Fig. 10, x86 System B) |
|---|---:|---:|---:|
| No tracing | 25,141 | – | 19,705 |
| Kernel syscount, nginx | 23,523 | −6.4% | −10.3% |
| Kernel syscount, other process | 24,210 | −3.7% | −9.6% |
| bpftime syscount, nginx (traces nothing, §1) | 24,594 | −2.2% | −3.36% |
| bpftime syscount, other process (traces nothing) | 23,634 | −6.0% | no effect |

- The two bpftime rows measure nginx with the agent loaded and nothing traced. They differ by 3.8 points
  although they do the same thing, so the run order of the script (one configuration after another, the 600 MB
  access log growing throughout; it was deleted after the run) moves results by several percent: the kernel rows'
  difference (2.7 points) is within that.
- The kernel rows show the expected direction (tracing nginx costs more than tracing another process, and every
  process pays for the system-wide tracepoints).

## 3. Run correctly, after the pthread key fix (`run_correct.py`)

Dev branch `59e5e154` ("text segment transformer: keep the agent's pthread keys apart from the process's") on top
of `2e850c45`. `run_correct.py` keeps benchmark.py's nginx.conf, syscount, wrk (`-c 100 -d 10`) and syscount
duration (`-d 20`, wrk 2 s after it starts) and changes only what made it wrong or noisy: the bpftime legs load
the programs first and start nginx with the transformer; nginx starts as `sh -c 'read; exec env ... nginx'`, so
`syscount -p <nginx>` works for bpftime too; syscount's output is kept and counted; legs rotate every round; the
access log is deleted before each start. `summarize_correct.py` summarizes a run.

**Run 3 (`correct-run3.jsonl`, `summary-correct-run3.md`) is the result.** 16:09-16:28, cpu5 1728000 before and
after, no other process using CPU, 60 runs valid, nginx never died:

| Configuration | Requests/s | sd | vs no tracing | +ns per nginx system call | Calls counted per request |
|---|---:|---:|---:|---:|---|
| No tracing | 27,928 | 241 | – | – | – |
| Kernel syscount, nginx | 26,352 | 377 | −5.6% | +237 | 9.018-9.019 |
| Kernel syscount, other process | 26,918 | 248 | −3.6% | +149 | 0 |
| **bpftime syscount, nginx** | **24,746** | 252 | **−11.4%** | +510 | **9.018-9.019** (= kernel) |
| bpftime syscount, other process | 25,411 | 211 | −9.0% | +393 | 0 |
| nginx with the transformer, no programs | 26,983 | 334 | −3.4% | +139 | – |

"+ns per nginx system call" is the extra time per request divided by 9.02 calls (the kernel count): it charges
all of the difference to system calls.

Earlier runs, not used:
- Run 1 (`correct-run1.jsonl`, 15:05-15:24) ran beside a leftover process of 10-28 (`timeout` with the agent
  preloaded into it, spinning on one CPU for 24 h; killed 15:46). Throughput was 12-14% lower in every leg; the
  percentages were close (kernel −4.6% / −3.2%, bpftime −11.0% / −9.5%, transformer −2.6%). The official
  benchmark.py run of §2 ran beside it too.
- Run 2 (`correct-run2-collided.*`) is invalid: two runners shared the container (a relaunch while the first was
  still running).

What run 3 shows:
- bpftime counts exactly what the kernel counts.
- bpftime is slower than kernel (−11.4% vs −5.6%), as in the system call microbenchmark, and more so: about
  510 ns per traced call, against +247.6 ns for two counting programs in the write loop
  (`../dispatch-fix-20261029`). Not explained yet; to be profiled.
- "Other process" does not make bpftime free: the programs still run in nginx and return at the pid check
  (−9.0%). The paper's "does not affect the unmonitored process" corresponds to nginx without bpftime at all.
- Rewriting with no programs costs 3.4% (+139 ns per call; the write loop showed about +66 ns).

| | Jetson, run correctly (run 3) | Jetson, benchmark.py (§2) | Paper (Fig. 10, x86) |
|---|---:|---:|---:|
| Kernel, nginx | −5.6% | −6.4% | −10.3% |
| Kernel, other process | −3.6% | −3.7% | −9.6% |
| bpftime, nginx | **−11.4%** | −2.2% (nothing traced) | −3.36% |
| bpftime, other process | −9.0% | −6.0% (nothing traced) | no effect |

## 4. perf: where bpftime's cost per traced call goes in nginx (`perf/`)

`profile_nginx.sh` + `profile_leg.py`: each leg is set up as in `run_correct.py`, wrk runs `-c 100 -d 20`, and from
3 s after nginx serves the host counts nginx's cycles, instructions and cache misses for 8 s (`perf stat -p`) and
samples it (`perf record -F 2999 -g`). Per-request figures divide by wrk's requests/s × 8 (throughput under perf is
a few percent lower). All samples are nginx's single thread (no agent background thread shows up). Clean CPU,
cpu5 1728000. Three repetitions of native, transformer_only and bpftime_targeted (`*-r2`, `*-r3`); the split by
DSO is from `perf report --sort dso`:

| Leg (mean ± sd of 3) | cycles/request | instructions/request | kernel | bpftime libraries | nginx + libc | cache misses/request |
|---|---:|---:|---:|---:|---:|---:|
| No tracing | 70,861 ±163 | 59,011 ±137 | 53,796 ±240 | 0 | 17,066 ±83 | 838 ±41 |
| Transformer, no programs | 73,584 ±696 | 60,439 ±198 | 55,049 ±831 | 2,303 ±113 | 16,232 ±528 | 884 ±30 |
| bpftime syscount, nginx | 79,836 ±548 | 66,654 ±293 | 57,776 ±650 | 5,389 ±116 | 16,672 ±66 | 992 ±14 |

"bpftime libraries" = agent, transformer, ld.so (dynamic TLS), JIT code (gates and programs), libstdc++. The
samples at the rewritten `svc` (`syscall_addr`) count here; without bpftime they fall in libc, hence the lower
"nginx + libc".

Per traced system call (÷ 9.02) against no tracing:

| | total | bpftime libraries | kernel | nginx + libc | cache misses |
|---|---:|---:|---:|---:|---:|
| Transformer, no programs | +302 cycles | +255 | +139 | −92 | +5.1 |
| bpftime syscount, nginx | **+995 cycles** (≈576 ns) | **+597** (≈345 ns) | **+441** (≈255 ns) | −44 | +17.1 |

- The work bpftime does itself, about 600 cycles (345 ns) per call, matches the write-loop microbenchmark (two
  counting programs, +329 ns over no tracing, `../dispatch-fix-20261029`).
- The rest is the kernel getting slower: the same system calls take about 440 more cycles each, with 17 more cache
  misses per call. In the write loop everything stays in cache; in nginx, bpftime's per-call user work (768-byte
  register save, agent code and data, the shared-memory maps) evicts what the kernel's network and file paths
  use. This is inferred from the counters, not isolated.
- In the bpftime libraries (first run, `perf/*.report.txt`, cycles per request over the transformer alone):
  `syscall_callback` (dispatch, inlined) +741, dynamic TLS (`_dl_tlsdesc_dynamic` + TLS init of
  `curr_thread_override_return_callback`) +425, `bpftime_prog_exec` +213, JIT code +179, `std::function` call
  +102, `bpf_get_current_pid_tgid` +102, `std::set` iteration +79; tracing nginx adds the hash map
  (`elem_lookup` +420, helper +127, libc compare +190).

## Open

- Smaller per-call footprint: the thread-local return-override callback (dynamic TLS), dispatch over std::set,
  saving all 32 SIMD registers on every call; whether a smaller footprint also removes the kernel slowdown.
