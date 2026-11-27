# Device hash map, stage one: the build, steps S0.1-S2 (2026-11-27)

The build of the plan in `gpu/hash-build-plan-20261127` (revision 4, with section 9 added after
the S2 code review). Steps done: S0.1, S1 (one layout header), S2 (the CPU side: map type 1533,
phase-0 table, seal, image, delivered mirror, takeover image, owner path). The GPU side (S3-S6)
is not built yet.

Code: branch proto/map-model-70f7bc1f, local commits cce426af (S1), 192f08d7 (S2), c84dd0ff
(review fixes); not pushed.

- **S0.1:** the hash snapshot's acquire load compiles at `.version 7.0` to
  `LDG.E.64.STRONG.GPU` + `CCTL.IVALL` (`s0/`). Recorded only.
- **S1:** 4 cases pass; 4 mutations each fail one.
- **S2:** 14 cases pass (about 1.5 s); 28 mutations each fail at least one; the object-code order
  check passes and fails each of its 5 mutations; the non-CUDA build compiles.
- **Code review of S1-S2:** 22 findings, 21 held, all answered. The measured one: an ordinary exit
  of a multi-threaded writer process left the single in-flight counter raised in 2-23 of 40 exits,
  after which the map could never get a device copy. The seal now counts writers per process and
  discounts processes that are gone.

Details: `s2/S2-result.md`. Rules: `S0-RULES.md`. Mutation scripts and outputs: `s2/`. Review:
`s2/review/code-review.json`.
