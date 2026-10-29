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

## Open

- Fix the pthread key overlap, then a corrected run: syscount first, nginx with the transformer, counts checked
  against the kernel, configurations rotated per round. In that setup "other process" still runs the programs in
  nginx (they filter by pid), so "bpftime does not affect untraced processes" would be nginx without bpftime.
