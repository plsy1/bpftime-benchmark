# Zero-argument CPU alias: rejected

37 correctness cases / 110,253 assertions and 18 host-contract cases / 201
assertions passed. Actual JIT windows confirm elimination of five zero-argument
moves at each of two lookup sites. CPU function and error handling are unchanged.

Six paired consuming rounds completed with artifact and checksum checks passing:
all-hit 6.6640 -> 6.6457 ns (-0.27%, 5/6 wins); mixed 12.4829 -> 12.4634 ns
(-0.16%, 3/6 wins). This fails the predeclared >=2% gain requirement. Unused-result
timings are excluded. The alias-only candidate is NOT accepted or committed.

The shell runner exited 2 after the six-round measurement because it was edited
while active to insert a JIT-code gate. The on-disk script passes `bash -n`, and
the actual JIT check was run independently and passed. This harness error is
preserved in `short.log`/`short-exit-status.txt`; it is not a failed measurement
or the reason for rejecting the candidate. The independently reviewed valid
timings fail the performance threshold regardless. No long suite was started.

The candidate's three source files have been restored to their pre-experiment
contents. Candidate snapshots/binaries are retained in `candidate-source/` and
`artifacts/`. A separate direct-CPU-query experiment follows; it must preserve
the standard helper's failure behavior, not silently return a different CPU.
