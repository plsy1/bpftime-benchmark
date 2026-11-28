# Device hash map, stage one: the build, steps S0-S6 (2026-11-27 to 11-28)

The build of the plan in `gpu/hash-build-plan-20261127` (revision 4; section 9 added after the S2
code review, section 10 after the independent read of its fixes). Steps done: S0.1, S0.2, S1 (one
layout header), S2 (the CPU side: map type 1533, phase-0 table, seal, image, delivered mirror,
takeover image, owner path), S3 (the GPU functions, their test harness, the trampoline's 1533
branches), S4 (hash snapshot), S5 (the copy and the delivery in the agent), S6 (routing pass). Not
built yet: S7 (tools, docs), S8 (measurement).

Code: branch proto/map-model-70f7bc1f, local commits cce426af (S1), 192f08d7 (S2), c84dd0ff
(review fixes), 0b9b02af (claim window), 309a9e8c6 (S3), 84ce9813d (writer slots and claim on OFD
locks), 19f6b777b (S4, S6), 9bb80ba16 (slots and claim after the review of the lock rework),
c42a843cb (S6 mirror tests), abc759681 (S5, first part), 41d68ea8c (S5, second part), da4e70c48 (S5,
the claim check's branches, slot 255 end to end, a real exit); the first four pushed 11-27.

- **S0.1:** the hash snapshot's acquire load compiles at `.version 7.0` to
  `LDG.E.64.STRONG.GPU` + `CCTL.IVALL` (`s0/`). Recorded only.
- **S1:** 4 cases pass; 4 mutations each fail one.
- **S2:** 14 cases pass; 28 mutations each fail at least one; the object-code order check passes
  and fails each of its 5 mutations; the non-CUDA build compiles.
- **Code review of S1-S2:** 22 findings, 21 held, all answered (writer counts per process, dead
  processes discounted; `s2/review/code-review.json`).
- **Independent read of those fixes:** 19 findings, all upheld by a refuter
  (`s2/reread-fixes.json`). One blocker, reproduced: a writer slot was taken by a CAS on (namespace,
  pid) and its start time stored afterwards, so a second claimant took a live process's slot and
  the seal discounted a writer that was still writing. Liveness from pid and start time also cannot
  see exec. **The writer slots and the owner's claim are now open file description locks on bytes
  of the segment's file**, which the kernel releases at exit, kill and exec in any pid namespace.
  The lock semantics were checked first (`s2/ofd/`, rules before the run, every item passed; a
  host's `close_range` releases them, written as a limit). Plan D8 and D11b-d rewritten, section
  10 maps each finding to its answer. Rework: 15 cases, 15 of 15 mutations caught.
- **Review of the lock rework:** 38 findings, 37 upheld (`s2/review/review-ofd-rework.json`; plan
  section 11). The first version left a host closing the agent's description on the dead side for
  good (later writes ran in a slot whose lock was free), could put the description at fd 0-2, let a
  vfork child's first write corrupt the parent's table (reproduced), and could close host fds.
  Second fix: holder hints and an image nonce (a free lock whose holder still runs and maps the
  segment counts as held; an earlier image's leftover count is cleared by the new image), the
  description checked through `/proc/self/fdinfo`, fds at 3 and above, raw system calls, writers
  that never wait on the process-local mutex, a per-slot zero check in the seal, task maps for a
  holder whose main thread exited. 21 cases; on the final code 19 of 22 mutations caught and the
  other three closed (a new case; two redundant checks shown together). The narrow independent read
  of the final code is still to run (agents' weekly limit). `s2/S2-rework-result.md`.
- **S3:** every run clean (round two's runs a, b, c, e, g, h; run d on S2's images; shipped build;
  full directory; opaque; ABI; trampoline without host calls); controls 1, 2, 5, 6 fire; 13 of 13
  mutations caught. Control 5 moved to 16 keys (at 64 keys it fired in 2 of 201 trials here against
  44 of 201 in round two, not explained). `s3/S3-result.md`.
- **S0.2:** llama.cpp's probed `mul_mat_vec_q` with the 1533 lookup hand-routed (inline hit path,
  fallback call, insert call, devhash text at the module start): 56 registers, 600 B stack, 9 blocks
  per SM at 128 threads, the same as today's 1531 form. tg128: 53.02 / 53.16 tokens/s against 55.77 /
  55.76 for 1531 (base 67.57 / 67.54), **+0.038 / +0.036 ns per thread over 1531** (S8's expected
  cost). Two earlier passes were invalid (an inherited lock descriptor shifted the map fds, so the
  PTX override never applied; caught by the validity rule). `s0/S0.2-result.md`.
- **S4:** the hash snapshot (its own PTX module; header acquire, rows and counters relaxed) stages a
  hand-built block exactly. **S6:** constant-fd 1533 lookups become the inline hit path with the
  walk's call behind, updates the insert's call, deletes -22, the devhash text once at the module
  start; on the GPU, 10,000 keys' routed pointers equal the walk's and routed updates return -17,
  -7, -22. 15 of 15 mutations caught (two only by the GPU test). `s46/S4-S6-result.md`.
- **Final runs:** `[gpu-devhash-device]` 11 cases (101 assertions) pass on 3fcc7caab
  (`s46/gpu-device-final.log`); `[gpu-devhash-cpu]` 21 cases on 9bb80ba16; `lat_mmvq` (1531) on the
  worktree build: +0.1273 ns per thread, sum exact, no regression (`s46/regression-lat_mmvq.*`).

- **S5:** the agent copies each 1533 map to the GPU in a pre-pass at the start of
  `init_attach_ctx_from_handlers`, before its binding guard (claim, seal and image, or the
  takeover's image; upload; mirror seeded; registered; PHASE 2; any failure after the seal rolls
  back), and delivers it after the application's syncs (claim checked first; the hash snapshot
  with its own module and broken flag; mirror, status words and the companion status array). The
  handler accessors return the cached base and shards; `bpftimetool cuda` makes no copy. 18 GPU
  test cases, each configuration in a child process built as the agent is (plan S5 tests 1-11 and
  the gate, owner deaths at four points, a real exit included); 32 of 32 mutations caught (three
  batches, the
  second after a review that found an unrolled-back exception path, a non-owner changing the
  companion's link, an untested ordering and a missing gate condition). **Found while testing:**
  the kernel merges one description's adjacent locks, so an owner holding writer slot 255 read its
  claim (byte 256) as lost and stopped delivering (1 in 10 suite runs; then reproduced on purpose);
  the claim check now tests the byte against each lock line's range (plan section 12; `s2/ofd/RULES-ofd.md`, found later).
  1531 regression: +0.1267 ns per thread, sum exact. `s5/S5-result.md`.
- **Final runs (da4e70c48):** `[gpu-devhash-device]` 29 cases (211 assertions), `[gpu-devhash-cpu]`
  24 cases, all pass; the S2 lock mutations rerun: 20 of 22 (the two redundant vfork guards, as
  before); the non-CUDA build compiles the touched files.

Details: `s2/S2-result.md`, `s2/S2-rework-result.md`, `s3/S3-result.md`, `s0/S0.2-result.md`,
`s46/S4-S6-result.md`, `s5/S5-result.md`. Rules: `S0-RULES.md`,
`s2/ofd/RULES-ofd.md`.
