# Real-program functional audit (2026-10-25)

Question: which real uprobe tools run on bpftime on this Jetson (stock kernel 6.8.12-1021-tegra,
no BTF), and when one does not, does it already fail on the official baseline or only after our
changes?

Backends:

- `kernel`: kernel eBPF.
- `official`: upstream 177a1dc unpatched, CPU affinity pinning included (`builds/official` of
  `ssl-nginx-2x2-20261023`; corrected 2026-10-25, an earlier version said `official-noaff`).
- `bca2ebce`: the dev branch head `codex/uprobe-official-177a1dc465ff` at bca2ebce.

Tools are built without BTF/CO-RE (`src/libbpf-tools-nobtf.diff`,
`src/bpftime-examples-nobtf-makefile.diff`): clang `-DBPF_NO_PRESERVE_ACCESS_INDEX`, `.BTF.ext`
removed, and libbpf-tools' `ensure_core_btf` made a no-op when no min-core BTF exists. Only tools
that read pt_regs and user memory work this way. Workloads are in `src/workloads`.

## Result

`audit2.jsonl` is the final run (harness fixed, see below); `audit.jsonl` is the first run and is
kept only for the record.

| Tool | kernel | official | bca2ebce | Verdict |
|---|---|---|---|---|
| bashreadline (200 lines via pty) | 200/200 | 200/200 | 200/200 | works |
| gethostlatency (getaddrinfo loop) | 239,656 / 240,903 events | 213,133 / 214,388 | 346,800 / 351,595 | works (0.5-1.4% tail loss at stop) |
| funclatency, bpftime example | histogram count matches | matches | matches | works |
| malloc example (counts per pid) | 2,144,050 / 2,142,166 | 7,652,003 / 7,664,343 | 8,640,011 / 8,643,814 | runs; hash-map flag bug, see below |
| usdt_minimal (2 probes per line) | 8/8 | 7/8 | 7/8 | works (last probe cut by `timeout`) |
| funclatency, libbpf-tools | works | workload SIGSEGV | workload SIGSEGV | fails on baseline too |
| memleak (`-c`, 1024 outstanding blocks) | 1,027 outstanding allocations | 0 reported | 0 reported | fails on baseline too |

No tool works on `official` and fails on `bca2ebce`.

Throughput numbers in the jsonl files are from single 4 s runs and are not measurements; see
`../realprog-measure-20261025`.

## Failures and bugs found

1. **funclatency (libbpf-tools)**: uses a `BPF_MAP_TYPE_CGROUP_ARRAY` map and helper 37
   (`bpf_current_task_under_cgroup`), neither supported by bpftime. The missing helper makes the
   LLVM JIT crash inside the traced process (null dereference in `generateModule`). Same on
   `official`.
2. **memleak**: allocations are tracked, but every `bpf_get_stackid` in the uretprobe fails, so
   memleak cannot show any stack ("N stack traces could not be displayed") and reports
   "Top 0 stacks". Cause: `current_thread_gum_cpu_context` is only set in
   `uprobe_listener_on_enter`, not in `on_leave`. Same on `official`. Also needs
   `BPFTIME_SHM_MEMORY_MB` well above the 50 MB default (1024 used).
   Separately, only in `-c` mode, the agent logs "Expected fd 4/5 to be a map fd" 48 times at
   start-up (0 times with `-p`). Running the command through a static wrapper (no inherited
   syscall-server) does not change it; the shm export taken while it runs shows all referenced
   fds as maps. Cause not found yet.
3. **Hash map ignores update flags** (`fix_size_hash_map_impl::elem_update` drops `flags`; the
   flag-checking `var_size_hash_map` is only used with `BPFTIME_USE_VAR_HASH_MAP`, which nothing
   defines). The malloc example does lookup → `update(BPF_EXIST)`; when the reader deletes the
   key in between, the kernel rejects the update, bpftime re-inserts the stale count. In the first
   run one `official` interval showed two seconds of calls (3,834,491); a smoke run of 2 s
   reported 5,767,128 against 3,843,512 expected. Present in both builds; how often it hits is
   timing dependent. `BPF_NOEXIST` is ignored the same way, and `elem_delete` always returns 0.
4. **bpf_printk** writes every call to the traced process's stdout under bpftime (the kernel uses
   its trace ring). The malloc example prints once per malloc: ~230 MB per 4 s. Raw outputs here
   are trimmed to a count.

## Harness problems fixed between the two runs

- Tool stdout was a pipe read only at the end: after 64 KB the tool blocked in `printf`, so
  gethostlatency reported ~1,200 events on every backend. Tool output now goes to a file.
- usdt_minimal hits were counted with `"bpf: "` over the tool output too, which matched libbpf
  debug lines ("libbpf: ...", 78 of them). Now only lines starting with `bpf: ` in the traced
  process's output count.
- malloc example used the largest one-second value; it now sums each pid's counts over the run.
- memleak `-c` now runs the workload through `src/workloads/staticwrap.c` (static, ignores the
  inherited LD_PRELOAD) instead of a shell script.

## Files

- `audit.jsonl`, `audit/`: first run. `audit2.jsonl`, `audit2/`: final run. Per tool and
  backend: `<tool>.<backend>.workload.txt` and `.tool.txt`; large ones gzipped or trimmed.
- `src/harness/audit.py`, `measure.py`, `bash_driver.py`: harness (copies of
  `/home/jetson/src/realprog-20261025/harness`).
- `src/workloads/*.c`, `src/*.diff`, `src/build-logs/`.

## Follow-up (2026-10-25)

Fixed on the dev branch (df865b3e), see `../realprog-fixes-20261025`: uretprobe stacks, the exit
crash they exposed, the memleak `-c` map errors (cause: the forked child started a second
syscall server whose new session reset the parent's maps; the static wrapper was not the cause),
the hash map flags, and the JIT crash (an off-by-one for a bpf-to-bpf call at pc 0, not the
missing helper itself).
