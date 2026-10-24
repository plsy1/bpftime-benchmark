# Round 9: daily stack with the perf output identity cache (2026-10-24)

`run-validation.sh`: detached worktree `worktrees/177a1dc465ff/jit-stack-validation-9` at official
`177a1dc465ff`, cherry-pick of the 55 commits configured in `config.json` (ending with `bca2ebce`;
`stack.txt`), `tracker.py build` (GCC 13, LLVM 15, RelWithDebInfo, LTO), then the configured test
filter (unchanged from round 8) on candidate and official builds.

| Check | Result |
|---|---|
| Replay | 55/55 applied, no conflicts; candidate `1d754f728968` |
| Tree vs `bca2ebce` | identical |
| LLVM 15 build | success |
| Candidate filter | 92 cases / 254753 assertions passed |
| Official filter | 32 cases / 32814 assertions passed |

Run 19:46-19:52 JST, exit 0.
