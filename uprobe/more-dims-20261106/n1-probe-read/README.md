# bpf_probe_read: cost, bad pointers, containers (2026-11-06)

Item 1 of the extra dimensions (`../README.md`). Kernel `6.8.12-1021-tegra-btf`; bpftime
`fix-final` (dev d46ceb8f) and `fix-final-probecheck` (the same source built with
`ENABLE_PROBE_READ_CHECK=ON` and `ENABLE_PROBE_WRITE_CHECK=ON`, otherwise the daily cmake args;
`../../ssl-nginx-2x2-20261023/builds/README.txt`). cpu5 at 1728000 before and after; 13:21-13:27
(round 1), 13:33-13:42 (round 2); no other runs; every run checked (hits == the workload's calls).

## Setup (`src/`, `run.py`, `go.sh`, `go2.sh`)

- `readstr LEN SECS`: one thread on CPU 5 calls `probe_me(s)`, s a string of LEN bytes starting
  128 bytes into a page. `BAD=1/2/3` calls `probe_bad(p)` instead with p NULL, an unmapped page or
  a kernel address (the function never touches p; only the probe reads it).
- `pr.bpf.c`, a uprobe on it: `pr-str-SIZE` = `bpf_probe_read_user_str(buf, SIZE, s)` (opensnoop,
  execsnoop, bashreadline read strings this way), `pr-read-SIZE` = `bpf_probe_read_user(buf, SIZE,
  s)` (sslsniff's buffers), `pr-control` = no read; the buffer is a per-CPU array value. Loaded
  by `output-dims-20261106/src/outattach`. ns per call = 1e9 / calls per second; a read's cost =
  that minus the control's.
- Container cells: the workload in Docker 29.8 (image `bpftime-ssl-nginx:20261023`, Ubuntu 24.04
  like the host; `src/` and the bpftime build bind-mounted read-only at their host paths,
  `/dev/shm` shared, its own pid namespace), with Docker's default seccomp profile and
  capabilities, `--security-opt seccomp=unconfined`, or `--cap-add SYS_PTRACE`; the loader on the
  host.

## Why

bpftime's `bpf_probe_read*` (`runtime/src/bpf_helper.cpp`) is a plain `memcpy` (a byte loop for
strings) unless built with `ENABLE_PROBE_READ_CHECK`. The CMake option defaults to ON (`make
release` keeps it), but the official `make benchmark` build sets it to 0, and so do the daily's
cmake args (`bpftime-jetson-daily/tools/jetson-upstream/config.json`) and every bpftime build
measured here so far. With it, each read is a `process_vm_readv` of the process itself, and a
string read copies at most 256 bytes (not across a page) per step with a `process_vm_readv` into
a scratch buffer and a `process_vm_writev` into the program's buffer.

## Findings

**1. Without the checks a bad pointer kills the traced process.** `bad.jsonl`, 3 reps each:

| pointer | kernel | fix-final (checks off) | fix-final-probecheck |
|---|---|---|---|
| NULL, unmapped page, kernel address; str and read | -EFAULT (14) on every call, process runs | SIGSEGV at the first call (rc -11, 18 of 18 runs) | -EFAULT on every call, process runs |

A tool that reads a pointer argument the application got wrong (`open(NULL)` under opensnoop,
a freed buffer under sslsniff) crashes the application under bpftime as built for every
comparison so far; under the kernel it gets -EFAULT.

**2. The checks cost a system call per read, two per 256 bytes of string.** `cost2.jsonl` (the
three builds in one batch, 3 reps of 3 s; `cost.jsonl`, round 1 without probecheck, agrees within
2%). ns per call, and in brackets the read's cost over the control:

| read | kernel | fix-final (off) | fix-final-probecheck |
|---|---:|---:|---:|
| control (no read) | 1,249 | 227 | 228 |
| read 64 B | 1,279 (+29) | 250 (+23) | 1,204 (+976) |
| read 256 B | 1,293 (+44) | 255 (+27) | 1,216 (+989) |
| read 4,096 B | 1,438 (+188) | 342 (+115) | 1,579 (+1,351) |
| string 16 B | 1,292 (+42) | 266 (+38) | 2,131 (+1,903) |
| string 255 B | 1,373 (+124) | 495 (+268) | 2,374 (+2,146) |
| string 1,024 B | 1,599 (+349) | 1,253 (+1,026) | 9,446 (+9,218) |
| string 4,095 B | 2,490 (+1,241) | 4,192 (+3,965) | 31,420 (+31,192) |

With checks, a probe reading one file name (255 B) costs 1.7x the kernel's whole uprobe instead
of 0.36x; a 4 KB string 12.6x. Without checks the string copy is a byte loop, about 1 ns per
byte, 3.2x the kernel's `strncpy_from_user` at 4 KB (the kernel's uprobe is still dearer up to
about 1 KB). So every bpftime-vs-kernel number so far for tools that read strings or buffers (opensnoop,
sslsniff) was measured without the safety the kernel always has, and the safe build as it is
would reverse them for tools that read strings.

**3. Containers: no difference.** Docker 29.8's default seccomp profile allows
`process_vm_readv` (`pvr_self` 16 in every cell), so the checked build reads in a container as on
the host (`container.jsonl`, `container-probecheck.jsonl`: 0 failed reads in 24 runs, the same
ns per call). The worry that a container's default profile breaks checked reads does not hold
for this Docker (`CLAUDE.md`'s note on the clone hook names Docker's default profile as denying
it: not on this version).

Side note: a first container attempt chrooted into the host's root inside the container; there
the agent saw the host's `/proc` with the container's pids and Gum refused every hook
(`GUM_ATTACH_WRONG_SIGNATURE`). An artifact of that setup, not of containers; the runs above use
the image's own root and `/proc`.

## For the plan

- Bad pointers must not kill the application: make the safe path the default.
- A cheaper fault-safe copy than two system calls per 256 bytes: one `process_vm_readv` per
  page for strings straight into the program's buffer (no scratch copy, no `process_vm_writev`
  of memory the runtime owns), or a fault-catching copy on the fast path (a per-thread
  SIGSEGV/SIGBUS fixup around a known copy routine, as the kernel's exception tables), with the
  system call only as fallback.

Raw: `cost.jsonl`, `cost2.jsonl`, `bad.jsonl`, `container.jsonl`, `container-probecheck.jsonl`,
logs `*.log`, the build log `build-probecheck.log`, smoke runs in `smoke/`. Binaries are rebuilt
by `src/build.sh` (not kept).
