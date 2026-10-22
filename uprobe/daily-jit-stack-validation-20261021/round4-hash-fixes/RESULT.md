# Round 4: daily stack + hash fixes replay (2026-10-22)

`run-validation.sh`: detached worktree `worktrees/177a1dc465ff/jit-stack-validation-4`
at official `177a1dc465ff`, cherry-pick of the 42 configured commits plus
`f24dc9b`, `baf0cc85`, `c3090b59`, `1cb20807`, `f26cc335` (`stack.txt`),
`tracker.py build` (GCC 13, LLVM 15, RelWithDebInfo, LTO; `cmake-cache.txt`),
then the configured test filter on candidate and official builds.

| Check | Result |
|---|---|
| Replay | 47/47 applied, no conflicts; candidate `candidate-sha.txt` |
| Tree vs `f26cc335` | identical (`tree-diff-vs-f26cc335.txt` empty) |
| LLVM 15 build incl. updated overlay patches | success (`build.log`) |
| Candidate filter | 74 cases / 239322 assertions passed |
| Official filter | 26 cases / 17913 assertions passed |

Run 17:34-17:41 JST, exit 0. The daily `config.json` was edited locally
afterwards (five commits appended); not committed or pushed at the time of
writing.
