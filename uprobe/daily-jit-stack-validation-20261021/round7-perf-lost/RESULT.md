# Round 7: daily stack with perf loss reporting and consumer wakeup (2026-10-23)

`run-validation.sh`: detached worktree `worktrees/177a1dc465ff/jit-stack-validation-7` at
official `177a1dc465ff`, cherry-pick of the 53 commits configured in `config.json` (ending with
`db3d475f`; `stack.txt`), `tracker.py build` (GCC 13, LLVM 15, RelWithDebInfo, LTO;
`cmake-cache.txt`), then the configured test filter, which now also includes
`[perf_event],[epoll]`, on candidate and official builds.

| Check | Result |
|---|---|
| Replay | 53/53 applied, no conflicts; candidate `cb8bb3179ded` |
| Tree vs `db3d475f` | identical (`tree-diff-vs-db3d475f.txt` empty) |
| LLVM 15 build | success (`build.log`) |
| Candidate filter | 87 cases / 254715 assertions passed |
| Official filter | 31 cases / 32806 assertions passed |

Run 19:19-19:27 JST, exit 0.
