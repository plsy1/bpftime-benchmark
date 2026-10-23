# Round 6: daily stack + affinity removal and fork fix replay (2026-10-23)

`run-validation.sh`: detached worktree `worktrees/177a1dc465ff/jit-stack-validation-6`
at official `177a1dc465ff`, cherry-pick of the 50 configured commits plus
`20870a03` (perf output without per-event affinity pinning) and `a90fa447`
(fork children execute inherited array JIT programs) (`stack.txt`),
`tracker.py build` (GCC 13, LLVM 15, RelWithDebInfo, LTO; `cmake-cache.txt`),
then the configured test filter on candidate and official builds.

| Check | Result |
|---|---|
| Replay | 52/52 applied, no conflicts; candidate `4e82c3b11cd1` |
| Tree vs `a90fa447` | identical (`tree-diff-vs-a90fa447.txt` empty) |
| LLVM 15 build | success (`build.log`) |
| Candidate filter | 78 cases / 239653 assertions passed |
| Official filter | 26 cases / 17913 assertions passed |

Run 15:18-15:27 JST, exit 0. The two commits were then appended to the
daily `config.json` (52 commits).
