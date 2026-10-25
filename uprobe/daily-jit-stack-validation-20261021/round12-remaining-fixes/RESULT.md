# Round 12: daily stack with the remaining real-program fixes (2026-10-26)

`run-validation.sh`: detached worktree `worktrees/177a1dc465ff/jit-stack-validation-12` at official
`177a1dc465ff`, cherry-pick of the 73 commits configured in `config.json` (ending with `a7d54d7e`;
`stack.txt`), `tracker.py build` (GCC 13, LLVM 15, RelWithDebInfo, LTO), then the configured test
filter (round 11 plus `[trace_printk]`, `[ratelimit]`) on candidate and official builds.

| Check | Result |
|---|---|
| Replay | 73/73 applied, no conflicts; candidate `60fdb3bc1f54` |
| Tree vs `a7d54d7e` | identical |
| LLVM 15 build | success |
| Candidate filter | 110 cases / 255932 assertions passed |
| Official filter | 34 cases / 32829 assertions passed |

Run 02:09-02:16 JST, exit 0.
