# Round 2: daily stack with array JIT + map-name test fix (2026-10-21)

Fixes the round-1 failure (`../RESULT.md`) with
`2eea622f97688b82a5c39402f631e79b5eb538f8` on
`codex/uprobe-official-8da867d541f8`: the map-name collision test now creates
the segment-wide map-binding mutex before its baseline, as `bpftime_shm` does.
Only the test changed; runtime code and performance results are unaffected.

`run-validation.sh` used the edited daily `config.json` (copied here): 41
commits = `8da867d541f8..2eea622` in order, extended `test_filter`
(`test-filter.txt`).

| Check | Result |
|---|---|
| Replay onto official 177a1dc (fresh worktree `jit-stack-validation-2`) | 41/41, no conflicts; candidate `0fb1fd5b7181` |
| Tree vs `2eea622` | only the upstream ringbuf tail-wrap commit differs |
| `tracker.py build` (GCC 13, LLVM/clang 15, RelWithDebInfo, LTO ON) | success, 20:10–20:17 JST including tests |
| Candidate, configured filter | 66 cases / 128,293 assertions, all passed |
| Official 177a1dc build, same filter | 26 cases / 17,913 assertions, all passed |
| `exit-status.txt` | 0 |

No benchmark was run: the daily job only builds and tests candidates.
