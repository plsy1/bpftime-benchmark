# S0 rules (written 2026-11-27, before any S0 run)

Plan: `PLAN-stage1.md` revision 4, section 3, S0.

## S0.1: the hash snapshot's acquire load, compile only

**What runs.**
- A draft of the hash snapshot module (`s0/hash_snapshot_draft.ptx`, `.version 7.0 .target sm_70`) is
  compiled with `/usr/local/cuda/bin/ptxas -arch=sm_87`. It loads each header with
  `ld.acquire.gpu.global.b64`, then for a live header sums the row's shards with
  `ld.relaxed.gpu.global.u64`.
- `cuobjdump -sass` of the result is kept, with ptxas `-v` output.

**What is recorded.**
- Whether ptxas accepts the module as written.
- The SASS of the header load and of what follows it: the load form and any `CCTL`, `MEMBAR` or
  `ERRBAR`.
- Registers.

**Decision:** none. The acquire rests on PTX ISA 9.5 section 8.8 case 1 and 8.9.4. If ptxas rejects
`ld.acquire` at `.version 7.0`, the lowest `.version` that accepts it is used, and that is recorded.

## S0.2: the device functions called for real (runs after S1 and S3's first draft)

**What runs.**
- The `lat`-shaped hash tool's probe on mul_mat_vec_q is hand-routed the way round one's C2 `hcall`
  form was made, from the current by-value build's dump:
  - the inline hit path;
  - the `__bpftime_devhash_lookup` fallback call;
  - a `__bpftime_devhash_insert` call;
  - the devhash text placed at the start of the module body (plan D1).
- ptxas at sm_87 for every probed kernel, against the same tool on 1531.

**Recorded:** registers, stack and warps per SM of each probed kernel, and the SASS forms in the
called functions.

**Rule.** A probed kernel that loses warps per SM against the 1531 tool is reported to the user, with
its register counts, before S6 is built. Nothing else decides anything here.

**Mutation:** the devhash text appended after the probes. ptxas must fail with "requires call
prototype".

**Timing** (GPU):
- tg128 with mul_mat_vec_q probed, the hand-routed form against the 1531 tool, two passes, with C2's
  harness;
- before each pass: jetson_clocks checked (cpu5 1728000, GPU 1020 MHz), and no leftover processes;
- each pass under the GPU lock (`flock` on the scratchpad `gpu.lock`), with a total timeout.

**Validity** (added 11-27, after the compile half and before any timed run). The routed form writes
its adds to a block of its own (a module-scope array), so C2's check of the 1531 sum does not apply
to it. Items of a pass: `base`, `tvf-lat_mmvq@hd_t0` (today's 1531 form), `tvf-lat_mmvq@dh_route`
(timed), `tvf-lat_mmvq@dh_check` (the routed form plus a 1531 add for every call that found its key,
or inserted or lost the race: correctness only, its time is not reported as a cost).
- every override item: "PTX override" in its client log;
- `hd_t0`: the 1531 sum equals C2's WANT (5 x 3137339392 + 3137339392 / 128), so the base address
  baked into the dump still holds;
- `dh_check`: the 1531 sum equals WANT, so every probe call took the routed lookup's hit or its
  insert with 0 or -17;
- `dh_route`: the 1531 sum is 0 (nothing reached the 1531 path);
- bucket 194 of the thread-local histogram is 0 in all three.

A pass with any check failing is reported and not used.

The result is S8's expected cost. No decision follows from it.
