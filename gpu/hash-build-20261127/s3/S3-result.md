# S3 result: the device functions, test harness and trampoline branches (2026-11-27)

Plan: `PLAN-stage1.md` rev 4, S3. Code: bpftime-proto-70f7, branch proto/map-model-70f7bc1f, commit
309a9e8c6 (local). Board: Orin Nano, jetson_clocks on (cpu5 1728000, GPU 1020 MHz), CUDA 13.2,
clang++-16 for the device PTX.

## What was built

- `attach/nv_attach_impl/devhash/devhash.cu`: `__bpftime_devhash_lookup`, `__bpftime_devhash_insert`
  (round two's protocol v2), `__bpftime_devhash_layout_fp`. Shipped as `devhash_ptx.h` (3 `.func`,
  no module header, no `.visible`; no stack frame; one generic load, the inserted value).
- `devhash_harness.cu` and `devhash_test_ptx.h`: round two's stress kernels on the shipped functions,
  one module per build (test, controls 1, 2, 5, 6, wait 0, shipped, shipped with control 6).
- The trampoline answers helpers 1, 2 and 3 on a 1533 map with NULL, -1 and -22 and no host call;
  it stays sm_61-legal.
- `runtime/unit-test/cuda/cuda_test_support.hpp` (helpers shared with `test_cuda_host_call.cpp`),
  `test_gpu_devhash_device.cpp` (`[gpu-devhash-device]`, each run in a child with a deadline), and
  two host text checks in `test_devhash_layout.cpp` (`[devhash-layout]`).

## Runs (second full run, `s3-run2.log`; all 8 test cases pass, 91 assertions)

| run | trials | violations | notes |
|---|---:|---:|---|
| a (max 128, 64 keys) | 51 | 0 | E2BIG 0, EEXIST 518,976 |
| b (max 64, 64 keys) | 51 | 0 | EEXIST 518,976 |
| c (max 48, 64 keys) | 51 | 0 | E2BIG 127,499,940; refused at most 10 |
| e (max 1024, one key per thread) | 6 | 0 | E2BIG 59,938,560; refused at most 1,014 |
| g (wait bound 0) | 51 | 0 | unstable EEXIST 328,754, then NULL 1,107 |
| h (with a reader) | 51 | 0 | 399,833 live checks, smallest sum at a transition 216 |
| d-image (S2's image) | 51 | 0 | |
| d-takeover (S2's takeover image) | 51 | 0 | |
| shipped-a, shipped-b | 51 each | 0 | checks without the outcome channel |
| f (directory fills, max 48) | 6 | 0 | lookup full walks 59,931,648 |
| full directory (all opaque, count 0) | 1 | 0 | 512 of 512 inserts E2BIG, 512 unstable |
| opaque on key 0's and key 1000's path | 3 each | 0 | |
| ABI (results as the routing pass calls them) | 1 | 0 wrong | |
| trampoline, 1533 calls | 1 | 0 host calls | |

**Controls** (each must fail the check round two names; 1 + 200 trials):

| control | fired in | check |
|---|---:|---|
| 1 (plain store for the claim) | 201 / 201 | winners_ne_1 |
| 2 (no count check) | 181 / 201 | live_gt_max |
| 5 (precheck E2BIG without the slot re-read), 16 keys, max 16 | 31 / 201 (35 in a second run) | e2big_on_live_key |
| 6 (round one's order) | 201 / 201 | e2big_with_room |
| shipped build with control 6's order (1 + 50) | 51 / 51 | e2big_with_room |

**Control 5 changed configuration (deviates from RULES-round2).** At round two's configuration
(64 keys, max 64) it fired in 2 of 201 trials on this build, a miss in about 1 run of 8; round two
measured 44 of 201 there. At 8 keys and max 8 it fired in 27 of 201, at 16 and 16 in 35 (and 31 in
the run kept). The test now runs 16 and 16. **Open observation:** why 2/201 here against 44/201 in
round two at the same configuration is not explained (the shipped functions' code differs from
round two's kernels: state-first walks, sharded counters, calls).

## Mutations (`mutations.txt`; each must fail a test: 13 of 13 caught)

| mutation | test that failed |
|---|---|
| S3.2 `build_image()` leaves count 0 | d-image (51 of 51 trials) |
| S3.3 `takeover_image()` does not renumber rows | d-takeover (51 of 51) |
| S3.6 no S bound on the walks | f and full directory killed at their deadlines |
| S3.7 unbounded full-directory wait | full directory killed at its deadline |
| S3.8 key compared before state | opaque-0 (3 of 3) |
| S3.9 bad flags not counted | ABI |
| S3.9 an int return (declared int) | ABI (the module does not load) |
| S3.9 an int return (32-bit result, zero-extended) | ABI |
| S3.10 the trampoline's lookup branch removed | trampoline (host calls > 0) |
| S3.11 `.release` changed to `.relaxed` | text check, memory orders |
| S3.11 one walk load made `ld.global` | text check, no weak access |
| S3.11 a stale fingerprint | text check, fingerprint |
| S3.11 a `.relaxed` load in the trampoline's 1533 branch | text check, sm_61 (and ptxas) |

S3.5's mutation (the shipped build with control 6's order) is the configuration
`shipped-checks-ctl6` in the run itself.

## Files

`s3-run1.log` (first run: every configuration failed because a test lambda named `key_of` hid the
header decoder; fixed), `s3-run2.log`, `ctl5-*.log`, `mutate_s3.py`, `mutations.txt`, `m-*.log`.
