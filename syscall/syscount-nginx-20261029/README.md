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

## 3. Run correctly, after the pthread key fix (`run_correct.py`, `correct-run1.jsonl`)

Dev branch `59e5e154` ("text segment transformer: keep the agent's pthread keys apart from the process's") on top
of `2e850c45`. `run_correct.py` keeps benchmark.py's nginx.conf, syscount, wrk (`-c 100 -d 10`) and syscount
duration (`-d 20`, wrk 2 s after it starts) and changes only what made it wrong or noisy: the bpftime legs load
the programs first and start nginx with the transformer; nginx starts as `sh -c 'read; exec env ... nginx'`, so
`syscount -p <nginx>` works for bpftime too; syscount's output is kept and counted; legs rotate every round; the
access log is deleted before each start. 10 rounds, 15:05-15:24, cpu5 1728000 before and after, 60 runs valid,
nginx never died. `summarize_correct.py` -> `summary-correct-run1.md`:

| Configuration | Requests/s | sd | vs no tracing | +ns per nginx system call | Calls counted per request |
|---|---:|---:|---:|---:|---|
| No tracing | 24,469 | 425 | – | – | – |
| Kernel syscount, nginx | 23,345 | 446 | −4.6% | +218 | 9.019-9.020 |
| Kernel syscount, other process | 23,698 | 521 | −3.2% | +148 | 0 |
| **bpftime syscount, nginx** | **21,788** | 367 | **−11.0%** | +558 | **9.019-9.020** (= kernel) |
| bpftime syscount, other process | 22,151 | 562 | −9.5% | +474 | 0 |
| nginx with the transformer, no programs | 23,820 | 488 | −2.6% | +123 | – |

"+ns per nginx system call" is the extra time per request divided by 9.02 calls (the kernel count): it charges
all of the difference to system calls.

- bpftime now counts exactly what the kernel counts.
- bpftime is slower than kernel here (−11.0% vs −4.6%), as in the system call microbenchmark, and more so:
  about 560 ns per traced call, against +247.6 ns for two counting programs in the write loop
  (`../dispatch-fix-20261029`). Not explained yet; to be profiled.
- "Other process" does not make bpftime free: the programs still run in nginx and return at the pid check
  (−9.5%). The paper's "does not affect the unmonitored process" corresponds to nginx without bpftime at all.
- Rewriting with no programs costs 2.6% (+123 ns per call; the write loop showed about +66 ns).
- benchmark.py's sequential order gave kernel −6.4% / −3.7%; rotated, −4.6% / −3.2%.

| | Jetson, run correctly | Jetson, benchmark.py | Paper (Fig. 10, x86) |
|---|---:|---:|---:|
| Kernel, nginx | −4.6% | −6.4% | −10.3% |
| Kernel, other process | −3.2% | −3.7% | −9.6% |
| bpftime, nginx | **−11.0%** | −2.2% (nothing traced) | −3.36% |
| bpftime, other process | −9.5% | −6.0% (nothing traced) | no effect |

## Open

- Profile bpftime_targeted/untargeted in nginx: why a traced call costs about twice the write loop's.
