# E10-E14 preparation (Jetson, written 2026-11-24 while E3/E4 were measuring)

Nothing here has been executed. The scripts were checked with `bash -n` and the Python files with
`ast.parse`, and that is all. No build, smoke run or sudo call was made. Every `*-build.sh` is
CPU-heavy or writes outside prep/ (it creates `workloads/<exp>/`, and E12 also creates
`example/*-core/` in the official worktree), so run them only between measurement sessions.

## Layout and common rules

- `lib.sh` is sourced by every `*-run.sh`, which is modelled on `scripts/run-e3.sh`. It does the following:
  - `mkdir raw/<SID>`, which refuses an existing output directory.
  - `check_env.sh start <log> <MIN>` and, in the EXIT trap, `check_env.sh end`.
  - In the trap: `sudo -n pkill -9 -x` on the experiment's process names, then `bpftimetool remove` for each arm used. An outer `timeout` cannot reach sudo'd children that run in their own sessions.
  - It unsets `BPFTIME_VM_NAME`, `DISABLE_JIT`, `SYSCALL_LAZY`, `HASH_ZERO_SEED`, `MAP_PRODUCTION_AB`, `ARRAY_JIT_INLINE`, `AOT_CACHE` and `LD_PRELOAD` (plan 1.5).
  - It moves this Claude session's process tree to cpu2 and restores it at the end. It also records the sha256 of the prep scripts.
- Output goes to `raw/J-E1x-20261124/` by default. Pass another session id as `$1` (for example `J-E10-20261126`).
- **Total bound:** `lib.sh` sets a session deadline: start + `MIN` minutes − 120 s, which leaves room for the end guard. Each step runs under `timeout --kill-after=30 $B`, where `budget N` sets `B = min(N, time left)`. A step with under 60 s left is refused (exit 97), so a session never outlives the window `check_env.sh` vetted for timers.
  - There is deliberately no outer `timeout` around a whole script: its kill-after would SIGKILL past the EXIT trap and skip the cleanup and end guard.
- `timeout` and `taskset` always sit outside `sudo env LD_PRELOAD=...`.
- **`bpftimetool remove`:** checked in both trees (plan 1.1 asks for this after the argparse change #719). The subcommand is still `remove` (`tools/bpftimetool/arguments.hpp:18`), and `main.cpp:205-207` always returns 0, even when no segment exists. E10's `check=True` on it is therefore safe.
- Every old runner is **copied** into `prep/E1x/` and patched there. Nothing in `~/src/results` or `~/src/realprog-20261026-btf` is edited or imported, and the copies set `PYTHONDONTWRITEBYTECODE`. The header docstring of each copy lists its changes.
- Frozen builds are flat (`builds/<arm>/libbpftime-*.so`, `bpftimetool`, and for O/D also `bpftime` and the transformer).
  - Runners that already expected a flat `BUILDS/<name>/` only needed `BUILDS` changed: `run_churn.py`, `audit.py`, `measure.py` (via audit) and `common.py`.
  - Runners that expected a build-tree layout had their paths rewritten: `run-ordinary/percpu.py` and `run_layers.py` (`runtime/agent/...`, `tools/bpftimetool/...`, `attach/text_segment_transformer/...`).
- Workload binaries are frozen under `workloads/<exp>/` with a `sha256.txt`. Each run script checks it at start and end. `check_env.sh` does not cover `workloads/`. Consider adding it to the guard's sha list (I did not touch the harness).

## Recommended order

1. Builds, all together in one idle window between sessions: `E10-build.sh`, `E11-build.sh`, `E13-build.sh` (each under a minute) and `E12-build.sh` (6 example builds, each with its own libbpf and bootstrap bpftool; about 10–20 min at `-j2`).
2. Smoke the two uncertain points first (see blockers): an E10 D round, and the E12 javagc and opensnoop cells.
3. E12 (P0 for ATC, planned before CP2) → E13 → E14 → E10 → E11.

## Per experiment

| Exp | Build | Run | Arms / params | Est. run (guard window) |
|---|---|---|---|---|
| E10 consuming lookup | `E10-build.sh` | `E10-run.sh` | kernel, O, D; ordinary + per-CPU; `--runs 6 --iterations 50000`; loader/victim on cpu5 | ~5 min (20) |
| E11 churn + layers | `E11-build.sh` (copy only) | `E11-run.sh` | churn: kernel,O,D × {k4-v8, control} × threads 1,2,4,6 × 3 reps; layers: D, 9 legs × 5 rounds, cpu5; optional O agent legs ×1 | ~15 min (35) |
| E12 18-tool audit | `E12-build.sh` | `E12-run.sh` | kernel,O,D × 18 tools, `--secs 4`, 3 runs | ~32 min (60) |
| E13 start-up | `E13-build.sh` | `E13-run.sh` | burst K=1,16 × {kernel,O,D,D:noaot}, 200 rounds × 2 reps; Dpc at K=1; seq N=200 × 2; P1: memleak-start ×10, n4 M=1..1000 | ~15 min, ~30 with P1 (60) |
| E14 real-tool throughput | `E14-build.sh` (checks E12's) | `E14-run.sh` | none,kernel,O,D × 8 tools × 5 rounds × 10 s | ~50 min (80) |

### E10: `results/uprobe/consuming-official-vs-optimized-20261021/run-{ordinary,percpu}.py`
- **Old build selection:** hardcoded constants.
  - `ROOT=/home/jetson/src/bpftime-baseline-177a1dc-20261021` (workloads). **This tree is gone.**
  - `PREVIOUS=.../official-artifacts` and `BUILD=.../baseline-177a1dc-jit-llvm16-20261021/artifacts` (build-tree layout).
  - bpftimetool always came from `BUILD`.
- **Now:** both scripts are merged into `E10/run_consuming.py --kind ordinary|percpu` with engines kernel/O/D.
  - `ROOT=workloads/E10`, which is also the cwd: the .bpf.o's uprobe SEC path is the relative `benchmark/uprobe/.output/array-jit-poc/victim`, so the build keeps that layout.
  - Flat frozen builds; bpftimetool is the arm's own.
- **Build:** the 10-21 recipe (`baseline-177a1dc-jit-llvm16-20261021/run-build.sh` lines 60–75), using the **D tree's** `benchmark/uprobe/diagnostics`.
  - Official 4708f409 has no diagnostics directory.
  - It reuses the D tree's `benchmark/uprobe/.output` (libbpf.a, bpf headers, bootstrap bpftool) read-only.
  - I checked the 9ccda7e8 sources: the CSV header and cases, and the `RESULT`/`RESULT_CPU` lines, still match the parser.
- **Changed vs 10-21:**
  - `BPFTIME_VM_NAME=llvm` is dropped (plan 1.5; llvm is the compiled-in default when LLVM JIT is on).
  - The D markers are searched in loader.log and victim.log together (see blocker 1).

### E11: `results/uprobe/map-threads-20261028/run_churn.py` and `results/syscall/breakdown-20261029/run_layers.py`
- **churn:**
  - `BUILDS=.../ssl-nginx-2x2-20261023/builds` (flat), now `builds/`.
  - `SRC=HERE/src`, now `workloads/E11/churn`, a frozen copy of the existing prebuilt binaries.
  - Defaults kept: `--secs 8`, threads pinned by churnloop to 5,4,3,… (cross-cluster from 4 threads), loader unpinned. `--cpus` is x86 only.
- **layers:**
  - `BPFTIME_BUILD` defaulted to `bpftime-daily-state/.../build-jetson-daily`, a build-tree layout that is now gone. It is now required and flat (`builds/D`).
  - `SRC` points to `workloads/E11/layers`.
- **Rebuild:** not possible as-is (the libbpf both old `build.sh` used is gone), and not needed: the prebuilt binaries exist and are copied.
- **dummy-agent ABI:** `libdummy_agent.so` (10-29) exports `_bpftime__setup_syscall_trace_callback(hook_t*)` and `injected_with_frida`. D's `agent-transformer.cpp` (L100–118) still calls exactly that, so the risk is low. The missing `bpftime_agent_fork_hook` only gives a warning.

### E12: `~/src/realprog-20261026-btf/harness/audit.py` (identical to the 11-04 harness-gaps copy)
- **Old paths:**
  - `BUILDS=.../ssl-nginx-2x2-20261023/builds` (flat), now `builds/`.
  - `EX=/home/jetson/src/bpftime-official-177a1dc-noaffinity-20261023/example` (**gone**), now `~/src/bpftime-paper/official-4708f409/example`.
  - `ROOT` (libbpf-tools, workloads, bash_driver) and `SHORT=~/src/rp` (`sw`) are unchanged and still present. Their sha256 is recorded at build time and checked at every run (`~/src/rp/sw` by absolute path).
- **Build:** cp the 6 examples to sibling `*-core` directories in the official tree, then `make CLANG=/usr/bin/clang-15 -j2`.
  - The examples are malloc, tracing/funclatency, minimal/usdt_minimal, tracing/bashreadline, tracing/sslsniff and bpf_features/get_stack_id_example.
  - Plain `clang` is not on PATH. `sys/sdt.h` is present (`/usr/include/aarch64-linux-gnu`).
  - The script checks the worktree is at O's commit and that tracked files are unchanged, and records `.BTF.ext` presence.
  - The recipe is reconstructed from `build-examples.log`; the 10-26 script was not found.
- **Run:** `--tools` is passed explicitly: TOOLS without syscount, 18 entries (checked).
- **javagc:** uses `BUILDS/<arm>/bpftime -i BUILDS/<arm> attach PID`, so each arm uses its own CLI (both frozen with it, plan 1.2). The `-i` flat directory works because of how the CLI resolves its libraries (`resolve_library_path_or_exit`). It first walks up to 5 parents from its own path looking for a `runtime/agent/` build tree; `builds/<arm>`, `builds`, `paper-runs-20261124`, `src` and `~` have none. It then falls back to `<install>/libbpftime-agent.so`. Its log line "Using agent from build tree" must not appear.

### E13: `startup-20261108/go.sh` (steps 1–2), `concurrent-start/run.py`, `memleak-start/run.sh`, `more-dims-20261106/{common.py, n2-startup/run.py, n4-many-probes/run.py}`
- **Old paths:** `common.BUILDS` was the ssl-nginx-2x2 builds (flat), now `builds/`. All workload paths now point into `workloads/E13`.
- **Rebuilt helpers:** `outattach`, `out-control.bpf.o`, `shortlived`, `spawnloop`, `many-M` and `stampmain` exist only as sources. `spawnburst` was compiled into a temp dir at import time; it is now prebuilt.
  - The old libbpf is gone, so outattach is linked against `official-4708f409/build-bench-llvm16/libbpf` (also 1.7.0).
  - `inc/bpf` is a copy, not a link.
- **Variants:** `D:noaot` means `BPFTIME_AOT_CACHE=0` in the tool, as on 11-08. `memleak_start.sh` now takes its cells on the command line.

### E14: `~/src/realprog-20261026-btf/harness/measure.py`
- It imports `audit` from its own directory, so the copy in `E14/measure.py` imports the patched `E12/audit.py`. Nothing else changed. Its prerequisite is E12's example build.

## Blockers and open questions (check before the first real session)

1. **E10 D assertion (most likely failure).**
   - The 10-21 runner asserted `[array-jit-inline] inlined` / `program=` in the *victim* log.
   - With D's AOT cache (default on), the syscall server compiles and inlines at BPF_PROG_LOAD (dev CLAUDE.md: "The AOT compile inlines array lookups"). The line may therefore appear only in the loader's log, or not at INFO at all. The `inlined` line is in the llvm-vm patch `array-map-inline.patch`.
   - I accept either log. If neither has it, the run fails with an assertion, not a wrong number.
   - Smoke one D round (`--runs 1`) first. If the marker is missing, decide whether to drop the check or use `BPFTIME_AOT_CACHE=0` as a check-only run.
2. **Rebuilds needed** (old binaries or trees gone):
   - E10 workloads (from the D tree), E12 examples (official tree, `*-core`), E13 helpers.
   - All rebuilt from the same sources with the same commands, but they are new binaries. Say so in the paper's config notes.
3. **E12 javagc wording.** Plan E12 says "javagc 用 D 的 bpftime attach", while plan 1.2 says O and D both get the CLI for E12 javagc. The script uses each arm's own CLI. If the plan means only D's CLI (injecting O's agent with it), change `audit.py` `t_javagc` to use `BUILDS/'D'/'bpftime'` with `-i BUILDS/backend`.
4. **E12/E11 O on syscall tools.**
   - opensnoop (E12) and the O layer legs (E11) run O's transformer on aarch64, where the official build has no svc rewriting. The expected result is 0 events, but a crash (rc ≠ 0) is possible.
   - In E12 a crash is recorded as a row, and the session goes on. In E11 the O legs run in their own non-fatal invocation (`E11_O_LAYERS=0` skips them).
   - Whether to run them at all is open: the plan only says to "record O as 0 events".
5. **E13 "N=200 × 2 reps".**
   - Read as 200 spawnburst rounds per (backend, K) cell: 200 starts at K=1 and 3,200 at K=16. 11-08 used 20 rounds. Change with `E13_ROUNDS`.
   - The "one Dpc point" is taken as K=1 (`E13_DPC_KS`).
   - n2-startup's sequential spawnloop (`--n 200 --reps 2`, builds O,D) is added because the plan names that runner. `E13_SEQ=0` skips it.
   - `D:noaot` is only meaningful for D (O has no AOT cache).
   - The spawnburst per-cell `timeout` was 300 s for 20 rounds. The copy scales it to `max(300, 15 × rounds)`, because O at K=16 × 200 rounds has never been measured and an expiry would only show as `ok=false`.
6. **Pinning (deviations from history, noted in the scripts).**
   - E10 loader/victim are on cpu5, as 10-21.
   - E11 layers are now pinned to cpu5 (10-29 was unpinned).
   - E11 churn, E12, E13 and E14 stay unpinned like their history (`taskset -c 0-5` undoes the cpu2 mask the shell gets in `start_session`).
   - The Claude tree still goes to cpu2, as in every campaign session. It is idle during legs, but cpu2 is also used by churn threads (4–6 threads) and by unpinned workloads.
7. **SHM size.** churn keeps `BPFTIME_SHM_MEMORY_MB=256` (10-28; it is not a real tool). audit and measure use 1024 (plan 1.5).
8. **Expected O failures (not harness bugs):**
   - funclatency-libbpf crashes the traced process (rc -11).
   - memleak reports 0 leaks and about 1M stack errors.
   - get_stack_id traced process segfaults at exit.
   - gethostlatency silently loses events.
9. **E12 example build writes to the official worktree** (untracked `example/*-core/`, like the existing `sslsniff-paper-*`). The script refuses an existing `*-core` and checks that tracked files are unchanged.
10. **realprog inputs** (libbpf-tools, workloads, `~/src/rp/sw`, `stw`) are the 10-26/11-08 binaries and are not copied into the campaign dir. Their sha256 is recorded by the build scripts and verified at each run.
11. The memleak-start copy adds `pkill -9 -x memleak` after each cell. Otherwise its commands are as on 11-08.

## Files

- `lib.sh`
- `E10-build.sh`, `E10-run.sh`, `E10/run_consuming.py`
- `E11-build.sh`, `E11-run.sh`, `E11/run_churn.py`, `E11/run_layers.py`
- `E12-build.sh`, `E12-run.sh`, `E12/audit.py`
- `E13-build.sh`, `E13-run.sh`, `E13/{common.py, run_burst.py, n2_startup.py, n4_many.py, memleak_start.sh}`
- `E14-build.sh` (check only), `E14-run.sh`, `E14/measure.py`
