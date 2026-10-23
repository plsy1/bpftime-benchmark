# Round 5: daily stack + hash bound/delete commits replay (2026-10-23)

`run-validation.sh`: detached worktree `worktrees/177a1dc465ff/jit-stack-validation-5`
at official `177a1dc465ff`, cherry-pick of the 47 configured commits plus
`af10949b`, `dffb2fce`, `72aebadd` (`stack.txt`), `tracker.py build` (GCC 13,
LLVM 15, RelWithDebInfo, LTO; `cmake-cache.txt`), then the configured test
filter on candidate and official builds.

| Check | Result |
|---|---|
| Replay | 50/50 applied, no conflicts; candidate `757162f72269` |
| Tree vs `72aebadd` | identical (`tree-diff-vs-72aebadd.txt` empty) |
| LLVM 15 build | success (`build.log`) |
| Candidate filter | 77 cases / 239634 assertions passed |
| Official filter | 26 cases / 17913 assertions passed |

Run 11:47-11:56 JST, exit 0. The three commits were then appended to the
daily `config.json` (50 commits).
