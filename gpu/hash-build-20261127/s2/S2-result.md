# S0.1, S1 and S2 results (2026-11-27)

Branch proto/map-model-70f7bc1f (worktree bpftime-proto-70f7), local commits, not pushed:
- cce426af S1: `attach/nv_attach_impl/trampoline/gpu_devhash_layout.h`, `[devhash-layout]`.
- 192f08d7 S2: `runtime/src/bpf_map/gpu/nv_gpu_device_hash_map.{hpp,cpp}`, map type 1533 (enum,
  creation checks, dispatch), `bpftime_hash_map::for_each_in_use_unlocked`, `[gpu-devhash-cpu]`,
  `runtime/unit-test/maps/check_devhash_orders.sh`.
- c84dd0ff: fixes after the code review (below).

## S0.1 (`s0/`)
ptxas 13.2 accepts the hash snapshot draft at `.version 7.0 .target sm_70`; the header's
`ld.acquire.gpu.global.b64` is `LDG.E.64.STRONG.GPU` then `CCTL.IVALL`; 28 registers. Recorded only.

## S1
- 4 cases, 461,058 assertions, pass. Mutations failing one case each: R = 513; shift by 63 - logS;
  a 30-bit row field; a fingerprint of the constants only.
- `bpftime_nv_attach_tests` as a whole: 64 of 65 cases pass; `[ptx-failure]` fails as on plain dev
  (results gpu/map-model-move-70f7bc1f-20261126).

## S2, first version (192f08d7)
- 12 cases pass; 19 mutations each failed a case (`mutations.txt`, `mutations-rerun.txt`).
- Two corrections during that run: the first "no guard" mutation removed only half of the guard
  (operator precedence) and passed; with the whole guard off the test fails. "Refused returned as
  live" first made a test reader loop spin; the loop is bounded now.

## Code review of S1-S2 (`review/code-review.json`)
Three lenses (concurrency, plan and tests, integration), each finding checked by a refuter:
22 findings, 21 held, 1 refuted. Two blockers were one fault: the default (non-CUDA) build no
longer compiled (the 1533 creation check outside the CUDA guard). The main measured finding: an
ordinary exit of a multi-threaded writer process left the single in-flight counter raised in 2-23
of 40 exits, after which no seal could succeed. All 21 are answered in c84dd0ff; plan section 9
records the design changes (writer slots per process with dead processes discounted; liveness over
all tasks and the start time; the claim given back on every path without a copy; repair only for
newly seen keys).

## S2 after the review (c84dd0ff)
- 14 cases, about 4,760 assertions, pass in about 1.5 s; three consecutive runs pass.
- 28 mutations (`mutate_s2_v2.py`, `mutations-v2.txt`), each failing at least one case: the 19 of
  the first version (updated to the new code) and: null next_key not checked; no discount of dead
  writers; the claim kept after a failed seal; values written only when the header changes; EPERM
  taken as dead; the leader's state only; the start time ignored; the dispatch's update case removed;
  the dispatch's lookup case removed.
- `check_devhash_orders.sh` (RCpc target): OK on the code (writer `ldaddal` ... `ldar`, sealer
  `stlr` then `ldar` loads, polls `ldar`, reader `ldapr`, the four operations call their helpers);
  five mutations each fail it: an acquire writer load, acquire sealer loads, relaxed polls, a relaxed
  reader load, a lookup that loads PHASE itself.
- The non-CUDA build: `map_handler.cpp` compiles without BPFTIME_ENABLE_CUDA_ATTACH
  (`-fsyntax-only` with the build's flags).
