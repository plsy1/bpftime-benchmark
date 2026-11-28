# S5: the device copy and the delivery of 1533 maps in the agent (2026-11-28)

Plan: `PLAN-stage1.md` S5, D11a, D12, D14, D15; working steps `s5/S5-steps.md`. Branch
proto/map-model-70f7bc1f, local commits abc759681 (first part), 41d68ea8c (second part: review
fixes, the remaining tests, the claim check on merged locks) and da4e70c48 (tests of the claim
check's branches, slot 255 end to end, a real exit).

## What was built

- **Delivery** (`nv_gpu_model_maps.cpp`): `registered_hash` beside the sum maps (pinned staging
  of `staging_for(max)`, the companion's words, the sum schedule: 20 ms while changing, else
  200 ms, and every application sync). The hash snapshot has its own module, warm-up launch and
  broken flag (D14), so a hash failure never stops 1531/1532 deliveries. Each delivery of a hash
  map: the claim is checked first (`still_holds_claim`, probes suppressed; lost: the map is
  dropped), snapshot, wait, `apply_delivery` (mirror, counters, count, live, refused),
  SYNCS_COVERED (or EXIT_DELIVERY at exit), DELIVERIES, then status words 0-15 into the
  companion (relaxed stores). `nothing_to_deliver` covers both kinds; the exit delivery and its
  words cover hash maps.
- **The copy** (`gpu_model::make_device_hash_copy`): under a process-wide mutex from the claim to
  the cache insert: `make_image` (claim, then the seal and the CPU table's image, a dead sealer's
  recovery, or the takeover's image), shards = pow2(SMs) * 32 (256 on the Orin), the block built
  on the host (headers, each live slot's value in shard 0 of its row or its single word, count,
  counter totals, geometry words) and uploaded on the agent's own non-blocking stream in relaxed
  capture mode, synchronized under `agent_sync_scope`; first copy: the mirror seeded; registration;
  first copy: PHASE 2; cached by object and generation. **D15:** everything after the image runs
  in one catch-all; any failure frees the block, a first copy also empties the mirror and unseals
  (PHASE 0), a takeover keeps PHASE 2 and the mirror, the claim is given back. The cache's room is
  reserved before PHASE 2, so nothing after it can throw. Registration removes its entry and frees
  its staging if it throws (no thread).
- **The pre-pass** (`bpf_attach_ctx::prepare_device_hash_copies`, called at the start of
  `init_attach_ctx_from_handlers`, inside `ctx_mutex`, before the binding guard). Gate: a CUDA
  context, an attach implementation registered for the CUDA attach types, not the direct run, and a
  link handler whose program's name starts with `cuda__`. Under the guard: every 1533 map without a
  copy pinned once per process (`inc_map_refcount`), its companion chosen and pinned (D12).
  Without the guard: the copies; the process whose copy is made then stores the companion's index
  (or clears a stale one). Nothing it throws leaves init.
- **Accessors** (`map_handler.cpp`): 1533 in `get_gpu_map_extra_buffer` and
  `get_gpu_map_max_thread_count` return the cached base and shards, or 0; they never make a copy
  (they run under the binding guard).
- **`bpftimetool cuda`**: `set_direct_run(true)`; no copy, no claim, no seal.

## Tests (`[gpu-devhash-s5]`, in `[gpu-devhash-device]`; each configuration in a child process)

Each child is an agent built as in production: it creates the segment as the syscall server would
(maps, a `cuda__` program, its perf event and link), reopens it, registers a stub implementation
of the CUDA attach types (it records what init's link loop hands a CUDA link), and runs
`init_attach_ctx_from_handlers`.

| test (plan S5) | child(ren) | checks |
|---|---|---|
| 1, 1b, 1c: first copy, later links | s5-first | prefill (inserted, updated, deleted) at the image's slots with its values, count 30; init's link loop hands the CUDA link the copy's base and 256 shards; `map_basic_info` the same; CPU writes EPERM; the binding mutex free during the copy (another thread takes it); a second init and `create_map_basic_info` keep the base, no second registration or copy |
| 2: not the owner | s5-hold, s5-not-owner, s5-takeover | another agent gets no copy while the owner runs and while it is stopped (OWNER, PHASE 2, lookups read the mirror, EPERM, the companion's link unchanged); after the owner execs (`sleep`, same pid) the next agent takes over |
| 2, 10: an unbound companion | s5-hold (two candidates), s5-close-candidate, s5-not-owner | the owner binds none; one candidate closed; another agent leaves the link at -1 |
| 3: owner deaths | s5-die (4 stop points), s5-next-owner | killed after the claim (PHASE 0), in the seal wait (a writer of its own in flight; PHASE 1), after the seal (PHASE 1), between passes B and C of a delivery that filled slots (the mirror holds the occupancy, no header); the next agent's copy keeps all 20 prefilled keys, and after the pass-B death all 300 delivered GPU keys with their mirror values; after the seal-wait death a map can still be created |
| 4: takeover after a clean exit | s5-owner-exit, s5-takeover | the same live slots and values as the mirror, rows in slot order, count = live (320), companion re-bound from the stored index |
| 5: failures | s5-fail, s5-takeover-fail | a copy failure, a registration failure and an exception in the copy each leave PHASE 0, an empty mirror, OWNER 0, no registration, and a forked process can claim; then a phase-0 delete and a successful copy leave the key absent; a registration failure on a takeover keeps PHASE 2 and every lookup |
| 6: end to end | s5-e2e (and with `BPFTIME_GPU_DELIVERY_AT_SYNC=1`) | 2,048 threads, 600 keys (rows 512..599 single words), NOEXIST inserts of 1 then adds of 2 through a lookup; after the application's sync, by D16's rule (or at once in the sync call): every mirror value exact, companion EEXIST = the -17 results (1,448), E2BIG 0, count 600, live 600, refused 0, owner, syncs covered |
| 7: prefill under GPU inserts | s5-prefill-gpu | 100 keys prefilled at 5, GPU inserts and adds on them and 100 new keys: seeded + adds exact, one slot per key, companion EEXIST exact |
| 8: refused slots | s5-refused | max 64, 200 keys: 64 live, companion E2BIG = the -7 results, word 15 = the refused slots on the device |
| 9: close while delivering | s5-close | the tool closes both fds; no 1533 object destroyed; deliveries go on into the pinned companion |
| 10: companion binding | s5-companion, s5-reuse-companion + s5-takeover | before or after its hash map, a 15-character name's 12-byte prefix: bound; two candidates, two hash maps of one prefix, 4-byte values: none; words readable with `bpf_map_lookup_elem`; a companion closed and its slot reused before a takeover is not bound and not written |
| 11: isolation | s5-isolation | the hash module fails to load; a 1531 map in the same process is delivered (77), the 1533 map has no delivery and stays in PHASE 2 |
| gate (D11a) | s5-gate | no copy without a CUDA link, without a CUDA attach, or in the direct run (PHASE 0, no claim); with them, a copy |
| the claim check, end to end | s5-slot255 | the agent writes before its copy with writer slot 255 (slots 0..254 held by another description): the claim stays held, deliveries go on, the mirror exact |
| a real exit | s5-exit, s5-after-exit | `exit()` with a 1533 map registered (the context a static, as the agent's): exit status 0; the next process reads EXIT_DELIVERY 1, SYNCS_AT_EXIT = SYNCS_COVERED, no launches after the last sync, 100 keys in the mirror |
| (CPU) the claim check's branches | `[gpu-devhash-cpu]` | slot 255 beside the claim; fdinfo unreadable (descriptor limit): held; another file at the number with fdinfo unreadable: giving the claim back leaves that file's lock |

18 test cases. Before the claim-check fix (below) the suite passed 8 of 9 runs (the pass-B death
case failed once; the death case alone passed 8 of 8 more); after it, every run.

**Found while testing: the claim check misread merged locks.** The kernel merges one description's
adjacent locks of one type (`s2/ofd/merge_check.c`): writer slot 255 and the claim's byte 256 show
as one fdinfo line from byte 255. The check compared the line's start with the claim's byte, so an
owner that had written before it claimed and held slot 255 (1 in 256: the scan starts at
pid % 256) took its claim for lost, could not take it again (its own forgotten description held
it) and stopped delivering: the pass-B death test failed once in ten suite runs. The cause was
first inferred from that run's log (the retake failed, so the old description still held the
claim), then reproduced on purpose: s5-slot255 with the old check fails exactly so (claim lost, no
delivery; mutation C1). Fixed (range check; a number naming another file is checked first; an
unreadable fdinfo with the segment's file at that number counts as held); CPU tests for each
branch; plan section 12.

Not handled, as for 1531: the deliveries hold the 1533 object in the segment, so a process that
unmaps its segment while a device map is registered crashes the delivery thread (the agent unmaps
only at exit, after the exit delivery; the gate test keeps its copying case last).

## Mutations (32 of 32 caught)

Each applied alone, the build rebuilt, the named children (or a whole test case) run; scripts
`mutate_s5.py` (batch 1, before the review), `mutate_s5b.py` (batch 2, after it) and
`mutate_s5c.py` (batch 3, the claim check's branches and the exit), outputs `mutations-s5*.txt`.

| mutation | caught by |
|---|---|
| 1533 missing from `get_gpu_map_max_thread_count` | s5-first (the entry's shards) |
| no cache in the pre-pass or the copy | s5-first (a second registration) |
| the copy made under `map_binding_mutex` | s5-first (the mutex not free during the copy) |
| the same, with an owner killed in the seal wait | death test (the next agent's init and map creation fail) |
| takeover rows not renumbered | takeover test (rows not their slot-order rank) |
| PHASE 0 after a failed takeover | takeover-failure child |
| OWNER not released on a failure | s5-fail |
| the delivery does not store SYNCS_COVERED | s5-e2e (D16's wait times out) |
| companion uniqueness checked on arrays only / on hash maps only | s5-companion (two each) |
| one broken flag for 1531 and 1533 | s5-isolation (1531 not delivered) |
| count not copied | s5-first |
| companion shape not checked | s5-companion (the 4-byte array binds) |
| no unseal / mirror not reset after a failed first copy | s5-fail |
| the gate ignores CUDA links / the direct run / whether CUDA attaches are handled | s5-gate (three) |
| a claim that always succeeds (lock and holder check) | not-owner test (a second copy) |
| the path chosen by the claim alone (PHASE 1 taken over) | death test (the prefill lost) |
| the stored companion slot not checked at resolve | reuse test (the other map bound and written) |
| 1533 left out of the pin list | s5-close (the object destroyed, deliveries stop) |
| word 15 not written | s5-refused |
| the copies made at the end of init (plan 1c) | s5-first (the link given base 0) |
| a non-owner records the companion binding | unbound-companion test |
| an exception in the copy not rolled back | s5-fail (PHASE 1 and the claim left) |
| the claim check takes a lock line's start only | CPU test, slot 255 |
| the dead owner's pass-B occupancy dropped by the takeover image | death test (no occupied non-live slot) |
| the claim check takes a lock line's start only, on the GPU | s5-slot255 (claim lost, no delivery) |
| unreadable fdinfo taken as a lost claim | CPU test, fdinfo unreadable |
| the number not checked against the segment file first | CPU test, another file at the number (its lock released) |
| the exit words not written for hash maps | exit test (SYNCS_AT_EXIT 0) |

The S2 lock mutations (`mutate_s2_ofd3.py`) rerun on da4e70c48 against `[gpu-devhash-cpu]`
(`s2rerun/`): 20 of 22 caught; the two missed are the two vfork guards, each redundant with the
other, as on 9bb80ba16 (checked together by hand then).

## Final runs (da4e70c48)

- `[gpu-devhash-device]` (S3, S4, S6 and S5's 18): 29 cases, 211 assertions, all pass
  (`runs/gpu-device-final2.log`).
- `[gpu-devhash-cpu]`: 24 cases, 6,975 assertions, all pass.
- The non-CUDA build: `map_handler.cpp`, `bpf_attach_ctx.cpp` and `bpftimetool/main.cpp` compile
  with the build's flags without `BPFTIME_ENABLE_CUDA_ATTACH` (`-fsyntax-only`; the define comes
  only from the command line).
- 1531 regression (on 41d68ea8c; `lat_mmvq`, tg128, worktree build, clocks checked: cpu5 1,728,000, GPU
  1020 MHz): base 67.96 t/s, lat_mmvq 56.11 t/s, **+0.1267 ns per thread** (S4/S6's run: +0.1273),
  sum exact (`regression-lat_mmvq.*`). The shared delivery changes cost 1531 nothing measurable.
- The whole build: `bpftime_daemon` fails to link for want of `-lcupti` and `-lnvrtc`. S5's
  commits change no CMake file (`git diff c42a843cb..` lists none), so the link line is the
  configuration's; whether it ever linked in this build directory was not checked. The
  measurements do not use it.

## Not done in S5

- Test 9 checks the 1533 object's pin only: the companion array's pin has no check (nothing counts
  arrays destroyed) and no mutation.
- Test 5 does not check that a failed copy's block is freed.

- The narrow independent read of the slots and claim code (9bb80ba16), of the claim check's
  rewrite (41d68ea8c) and of S5: the agents' weekly limit (until 2026-11-30 06:00).
- Plan test 1c with nv_attach_impl's late path in the runtime tests (the stub covers the order;
  S6's GPU test covers routing with a base).
