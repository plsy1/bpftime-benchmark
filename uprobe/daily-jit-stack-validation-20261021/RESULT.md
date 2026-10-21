# Daily patch stack + array JIT replay validation (2026-10-21)

Goal: check whether `9ffbffb` (opt-in ordinary array JIT) and `c0939bd`
(default ordinary/per-CPU array JIT + delete fix) can be appended to the daily
patch stack in `automation/jetson-daily`. The daily `config.json` was NOT
changed by this run.

Procedure (`run-validation.sh`): detached worktree at official `177a1dc465ff`
(`worktrees/177a1dc465ff/jit-stack-validation`), `git cherry-pick -x` of the 38
configured commits plus the two JIT commits, then `tracker.py build` with the
daily configuration (GCC 13, LLVM/clang 15, RelWithDebInfo, LTO ON) and the
daily focused test filter plus `[array-jit-lifetime],[array-jit-poc],[array-offset-branch]`.

| Check | Result |
|---|---|
| Replay onto 177a1dc | 40/40 applied, no conflicts; candidate `4dc498af5c1d` |
| Tree vs `c0939bd` | differs only by the upstream ringbuf tail-wrap commit (3 files) |
| LLVM 15 configure/build incl. llvmbpf overlay patches | success (`build.log`) |
| Candidate focused + JIT tests | **48 cases, 47 passed, 1 failed** (`candidate-tests.log`) |
| Candidate host-contract filter | 18 cases / 201 assertions passed |
| Official 177a1dc binary with extended filter | 8 cases / 17712 assertions passed (unmatched JIT tags do not break the official gate) |

`exit-status.txt` is 1 because the script stops
at the failing candidate test; the remaining checks were rerun manually with
the same binaries and filters.

## Failure

`Hash maps with colliding public names remain independent`
(`runtime/unit-test/maps/test_shm_hash_maps.cpp:214`, tag `[maps][map_name]`):
`segment.get_num_named_objects() == named_objects_before_maps` expands to
`2 == 1`.

- Passes on the pre-JIT daily candidate build (`177a1dc465ff/candidate`).
- Fails on the LLVM 16 hot build (`bpftime-percpu-array-jit-20261020/build-jit`)
  and on this LLVM 15 build, so it is caused by the JIT commits, not LLVM 15.
- Cause: `9ffbffb` makes `handler_manager::set_handler*`/`clear_*` take
  `map_binding_mutex(memory)`, which lazily `find_or_construct`s one named
  mutex in the segment. `bpftime_shm` creates it eagerly, but this test builds
  a raw segment + `handler_manager` directly, so the first `set_handler`
  creates the named object.
- The array JIT experiment's own test filters did not include `[maps][map_name]`,
  so `array-jit-default-delete-fix-20261021` never ran this case.

Resolved by `round2/` (test fix `2eea622`, all gates pass).
