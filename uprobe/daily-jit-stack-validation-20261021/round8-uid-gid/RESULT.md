# Round 8: daily stack with the uid_gid cache (2026-10-24)

`run-validation.sh`: detached worktree `worktrees/177a1dc465ff/jit-stack-validation-8` at
official `177a1dc465ff`, cherry-pick of the 54 commits configured in `config.json` (ending with
`7ed339ad`; `stack.txt`), `tracker.py build` (GCC 13, LLVM 15, RelWithDebInfo, LTO;
`cmake-cache.txt`), then the configured test filter, which now also includes `[uid_gid]`, on
candidate and official builds.

| Check | Result |
|---|---|
| Replay | 54/54 applied, no conflicts; candidate `2581452c68d8` |
| Tree vs `7ed339ad` | identical (`tree-diff-vs-7ed339ad.txt` empty) |
| LLVM 15 build | success (`build.log`) |
| Candidate filter | 91 cases / 254742 assertions passed |
| Official filter | 32 cases / 32814 assertions passed |

Run 16:55-17:02 JST, exit 0.
