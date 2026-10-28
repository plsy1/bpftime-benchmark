# Round 14: daily stack with the per-CPU hash fixes (2026-10-28)

Stack: the 86 commits of `config.json` (`stack.txt`; the three new ones are `09421773`,
`cdc05f67`, `c98951a4`).

| Check | Result |
|---|---|
| Replay | 86/86 cherry-picked (`-x`) onto official `177a1dc465ff` in a scratch worktree (removed afterwards), no conflicts; tree identical to `dev/uprobe-official-177a1dc465ff` `c98951a4` (empty `git diff`) |
| Candidate gate | `tracker.py test-candidate` on the dev worktree (same tree), GCC 13, LLVM 15, LTO, `-DBUILD_BPFTIME_DAEMON=ON`; `candidate-gate.log` |
| Focused filter | 113 cases / 255970 assertions passed (round 13: 111; the two new `[percpu-hash]` cases: concurrent inserts and deletes, program-path flags and size limit) |
| `[kernel_user]` (sudo) | 2 cases / 31 assertions passed |
| daemon tests (sudo) | 10 cases / 12466 assertions passed |
| `Test attach replace with ebpf` alone | 1 case / 9 assertions passed |
| Frida attach tests | 18 cases / 1078 assertions passed |

The test filter is unchanged, so the official build was not re-run. The new tests fail without
their fixes (concurrency test: SIGSEGV 3/3; flags test: at `BPF_NOEXIST`, and at `E2BIG` for
eight-byte keys without only the size check). Benchmark evidence: `../../percpu-hash-fix-20261028`.
