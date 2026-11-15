# C5 (step 0b), optional check: does the documented capture-mode remedy keep the copier from breaking GLOBAL-mode captures?

Part of the guarantee audit `../README.md` (claim 10). **Status: built and smoke-tested twice (SECS=1, shared GPU); the second smoke ran after the adversarial review tightened the rules (see "Review changes"). Main-session runs pending.**

## Question

Step 0's check 3 (B0) found that an agent copier thread, issuing `cuMemcpyDtoHAsync` on its own non-blocking stream, invalidated an application's captures in GLOBAL mode. Its thread had been left in the default capture interaction mode.

The Driver API defines that mode per calling thread (`cuThreadExchangeStreamCaptureMode`):
- in GLOBAL mode, a thread "is prohibited from potentially unsafe API calls" while any other thread captures in GLOBAL mode;
- in RELAXED mode, "The local thread is not prohibited from potentially unsafe API calls."

If the copier thread switches itself to RELAXED (or THREAD_LOCAL), do the application's GLOBAL-mode captures stay valid?

## Method

- **Programs.** Step 0's harness is copied into `src/`. `src/captest.cu` is unchanged except for its first line. `src/step0b_copier.c` is step 0's shim plus one change, shown by `diff` against `step0/e3-copyout/partB/step0_copier.c`.
- **The change.** With `STEP0B_COPIER_MODE=relaxed|thread_local|global`, the copier thread calls `cuThreadExchangeStreamCaptureMode` right after making the application's primary context current, before its own set-up calls. It logs the result (`r_mode`) and the previous mode.
- **Memory.** Registered memory uses shm names `/step0b_c5_*`, unlinked right after mapping (`common/regmem.h`).
- **Workload.** captest captures 200 tiny kernels per iteration in GLOBAL mode on a non-blocking stream for SECS seconds, then instantiates or updates the graph and launches it. It counts every call's errors and every capture seen as invalidated.
- **Cases** (`run_all.sh`):

| case | copier | its thread's mode | role |
|---|---|---|---|
| `noshim-global` | none | - | sanity: must be clean |
| `async-global-default` | 1 MiB DtoH async, back to back, `cuStreamSynchronize` | left alone (step 0) | control: must fail |
| `async-global-global` | same | set explicitly to GLOBAL with the same call the remedy uses | control 2: must fail (differs from the remedy only in the mode value) |
| `async-global-relaxed` | same | RELAXED | the remedy |
| `async-global-tlocal` | same | THREAD_LOCAL | reported |
| `asyncevent-global-default` | same, event polling instead of a stream sync | left alone | control: must fail |
| `asyncevent-global-relaxed` | same | RELAXED | the remedy |
| `memalloc-global-relaxed` | `cuMemAlloc`/`cuMemFree` before each copy (the documented example of an unsafe call) | RELAXED | reported |
| `ctxsync-global-relaxed` | `cuCtxSynchronize` before each copy | RELAXED | reported: documented invalid in every mode |

- **Compilation check.** There is no device-side mechanism here; the trivial kernel is step 0's. `nm -D build/libstep0b_copier.so` shows the shim imports `cuThreadExchangeStreamCaptureMode`, `cuMemcpyDtoHAsync_v2`, `cuMemAlloc_v2` and `cuCtxSynchronize`.

## Decision rules (fixed before the main-session runs; also in `analyze.py`)

Tightened by the adversarial review before any real run (only smokes existed); see "Review changes".

- **All errors 0** means, for captest: 0 begin, kernel-launch, end, instantiate, exec-update, graph-launch, sync and status errors, 0 invalidations seen, and exit status 0.
- **Sanity.** `noshim-global` has all errors 0, and every control and remedy case whose shim log exists shows the copier mode its name says (-1 = no call, 0 GLOBAL, 2 RELAXED). Otherwise the result is BROKEN HARNESS. Reported-only cases are not part of sanity, so their crash cannot void a verdict.
- **Control.** Each `*-default` case must have run (captest result present, rc 0, shim log present) and show end errors or invalidations > 0. A crashed or missing control makes its pair UNDECIDED (control unusable). A control that ran clean makes its pair UNDECIDED too: the driver no longer treats these calls as unsafe, so the remedy cannot be told apart.
- **Control 2.** `async-global-global` (the copier calls `cuThreadExchangeStreamCaptureMode` with GLOBAL, `r_mode` == 0) must also have run and show end errors or invalidations > 0. Otherwise the async pair is UNDECIDED: making the call at all, rather than the RELAXED value, could explain a clean remedy.
- **Remedy holds** for a `*-relaxed` case iff all of the following hold:
  - captest: all errors 0;
  - copier: `r_mode` == 0 and `prev_mode` == 0 (the documented default, GLOBAL, was in force before the call);
  - copier: every set-up call returned 0, 0 copy errors, **0 wait errors**, 0 poll cap hits. (In the controls every copy succeeds and the waits fail with error 900. In the sync variant the wait is `cuStreamSynchronize`. In the event variant the shim counts `cuEventRecord`, `cuEventQuery` and its fallback `cuStreamSynchronize` together, so which of them failed is not recorded.)
  - at least 100 copies issued or finished while a capture was open, so that collisions were possible;
  - a remedy case that crashed (rc != 0, no captest result) or wrote no shim log FAILS.
- **Reported, not decided:**
  - THREAD_LOCAL copier: the documented prediction is the remedy's criteria.
  - `cuMemAlloc` from a RELAXED thread: the same, plus 0 errors from the unsafe calls themselves.
  - `cuCtxSynchronize` during a capture: the documented prediction is invalid. A crash is one form of it, as step 0 saw rc 139.
- **The analyzer can fail.** `python3 -I selftest_analyze.py` (no GPU, about 1 s) feeds altered copies of `smoke/run1` to `analyze.py`: the default-mode data in place of the remedy's, wait errors in the remedy's copier, a clean explicit-GLOBAL control, a missing case, clean controls, a wrong logged mode. It also crashes a remedy case (FAILS), a control (UNDECIDED) and a reported-only case (verdicts unchanged). It checks that each verdict comes out FAILS, UNDECIDED or BROKEN HARNESS as it must, and that the unaltered data comes out HOLDS.

**Use in the design.** If both pairs hold in both main-session runs, then:
- "agent threads that call CUDA switch themselves to RELAXED" is a documented rule whose effect was also observed;
- step 0's "an asynchronous copier breaks applications that capture in global mode" becomes "a copier left in the default mode does";
- the list of unsafe calls stays undocumented, so the agent still counts any capture-related error (claim 10 of `../README.md`).

## Commands (main session, quiet board)

```
cd /home/jetson/src/bpftime-gpu-work/step0b/c5-guarantees/capture-remedy
./build.sh                                   # about 20 s, no GPU
python3 -I selftest_analyze.py               # about 1 s, no GPU: the analyzer's verdicts can fail
timeout -k 5 300 ./run_all.sh OUT/run1 3     # 9 cases x 3 s, about 40-60 s wall
timeout -k 5 300 ./run_all.sh OUT/run2 3     # the second, serial run
```

Each run writes `captest.jsonl`, `runs.csv`, `<case>.shim.json`, `<case>.out/.err` and `verdict.txt` into its output directory.

## Smokes (SECS=1, GPU shared with other agents; not results)

`smoke/run1/verdict.txt` (8 cases, before the review, old rules):
- sanity ok;
- `async-global-default`: 599 of 1,703 captures invalidated;
- `asyncevent-global-default`: 2,811 of 2,811 invalidated;
- `async-global-relaxed`: 0 errors in 1,342 iterations, 6,060 copies during open captures;
- `asyncevent-global-relaxed`: 0 errors in 1,391 iterations;
- `async-global-tlocal` and `memalloc-global-relaxed`: clean;
- `ctxsync-global-relaxed`: crashed (rc 139), as in step 0.

`smoke/run2/verdict.txt` (9 cases, after the review, the rules above; 15 s wall):
- sanity ok, every logged copier mode as named;
- `async-global-default`: 601 of 1,700 captures invalidated; 21,789 of the copier's `cuStreamSynchronize` calls failed with 900 (`CUDA_ERROR_STREAM_CAPTURE_UNSUPPORTED`) and its 31,681 copies all succeeded;
- `async-global-global` (explicit GLOBAL, `r_mode` 0): 580 of 1,647 invalidated, 21,759 failed waits; it behaves like the default;
- `asyncevent-global-default`: 2,886 of 2,886 invalidated, 26,071 failed waits (first error 900; record, query and fallback sync are counted together);
- `async-global-relaxed`: all errors 0 in 1,373 iterations, 0 wait errors, 5,923 copies during open captures: REMEDY HOLDS;
- `asyncevent-global-relaxed`: all errors 0 in 1,413 iterations, 5,149 copies during open captures: REMEDY HOLDS;
- `async-global-tlocal` and `memalloc-global-relaxed`: clean;
- `ctxsync-global-relaxed`: crashed (rc 139).
- Afterwards: no captest process left, no `/dev/shm/step0b_*` segment, no core file.

Both smokes agree with the documented prediction. They are short runs on a shared board, so they decide nothing.

## Known limits

- One application pattern only: a single capturing thread, GLOBAL mode, a non-blocking capture stream. Two capturing threads, or a capture whose stream is blocking (legacy-stream interaction), are not covered. The legacy-stream rule is documented as invalid anyway.
- The copier's mode is set once at thread start. An agent that creates CUDA threads later must set it in each.
- The copier's own calls are what is tested; the unsafe-call list is still not published, and it may change with the driver (13.2 / 595.78 here).
- Relies on: Driver API reference v13.5, 6.37 (`cuThreadExchangeStreamCaptureMode`, mode definitions), and CUDA Programming Guide v13.5, 4.2.2.1.2.2-3 (prohibited operations, invalidation). The quotes are in `../README.md`, claim 10.

## Review changes (adversarial review, 2026-11-15, before any real run)

- **The remedy rule checked the wrong copier counter.** In the controls the copies succeed and the waits fail (error 900), so a remedy whose waits failed would still have passed. It now requires 0 wait errors, 0 poll cap hits, no set-up error and `prev_mode` == GLOBAL. It also requires all of captest's error counters to be 0; `launch_err` was loaded but unused before.
- **Control 2 added** (`async-global-global`), so that control and remedy differ only in the mode value passed to the same call.
- **Sanity** now also checks the logged copier mode of every control and remedy case that has a log. A crashed remedy case FAILS, a crashed control makes its pair UNDECIDED, and a crashed reported-only case changes no verdict.
- **`selftest_analyze.py`** shows each verdict can come out other than HOLDS.
- **Second smoke** `smoke/run2` with the new rules; `smoke/run1` is kept as the record of the first.
