# Device hash map, stage one: the build plan and its reviews (2026-11-27)

The plan for building the device hash map (`gpu/hash-design-20261126`, checked in
`gpu/hash-round1-20261126` and `gpu/hash-round2-20261127`) into bpftime, on
`proto/map-model-70f7bc1f`. Nothing is built yet.

The user's decision of 11-27 came before the plan: the E2BIG/lookup window stays as bpf's htab has
it, and is documented rather than fixed (DESIGN.md, decision 5).

## How the plan was reviewed

Each round sent the plan to independent reviewers. Each reviewer's findings then went to a refuter,
told to disprove them against the code and the documents.

| round | plan read | reviewers | findings | held | the serious one |
|---|---|---|---:|---:|---|
| 1 | revision 1 | coverage, buildability, cross-process CPU concurrency, device and routing | 83 | 80 | The new GPU functions were placed after their callers in the PTX, so ptxas would reject every routed module. |
| 2 | revision 2 | two checked round 1's answers; two read afresh | 53 | 49 | The seal ran under `map_binding_mutex`, a robust interprocess mutex that a dead process breaks for good. |
| 3 | revision 3 | one checked round 2's answers; one read afresh | 23 | 23 | On the late path, modules were patched before the device copy existed. They kept base 0, and 1533 calls there never reached the device map. |

**Revision 4** (`PLAN-stage1.md`) answers round 3:
- the copies are made in a pre-pass, before any CUDA link exists and outside the mutex;
- the pre-pass is gated on the init instantiating a CUDA link.

Revision 4 had one narrow check of the pre-pass against the code (when the CUDA context exists, and
what triggers the seal), not a fourth round.

Most of the held findings are about tests that could not fail under their own mutations. The plan
now names a mutation, or a reason for none, for every test. The rest are settled when each step's
tests are written.

## What the plan decides that the user has not seen yet

- **The new GPU code stays out of the shared trampoline** (D1). Only modules of a process with a 1533
  map get sm_70 forms. Only constant-fd calls made while a copy existed reach the device map.
- **Each hash tool declares two maps:** the hash and a companion status array named after it (D12).
- **The CPU reads the delivered directory as a repaired mirror** (D10), instead of keeping an index of
  its own as DESIGN.md says.
- **A seal can be delayed.** A CPU writer of a running process stuck for more than 1 s during the
  seal delays the device copy until it has finished (D8). Writers whose process died, was killed or
  exec'd are discounted (changed during the build, plan sections 9 and 10: a writer's slot and the
  owner's claim are now kernel file locks on the segment's file, released at exit, kill and exec in
  any pid namespace; a host that closes the agent's descriptors releases them too).
- **Takeover from a dead owner re-seeds the same slots** (D11). This is the largest optional piece;
  it could be cut to a documented loss.

DESIGN.md is to be changed in five places when the build lands (plan section 6).

## Files

- `PLAN-stage1.md`: revision 4, with each round's findings mapped to their answers (sections 7 and 8),
  and the changes the S2 code review and the independent read of its fixes made during the build
  (sections 9, 10 and 11, rewritten into D8, D11 and S2 in place), and what building S5 found
  (section 12: the claim check on merged locks; the companion's index stored by the owner only, D12;
  the gate's CUDA-attach condition, D11a; test 1c built with a stub attach).
- `review-r1/`, `review-r2/`, `review-r3/`: the plan each round read (`PLAN-stage1-rN.md`), and every
  reviewer's findings with each refuter's verdict (`plan-review.json`).
