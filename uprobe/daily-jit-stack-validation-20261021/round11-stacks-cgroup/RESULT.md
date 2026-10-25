# Round 11: daily stack with frame-pointer stacks, stack trace map, cgroup arrays (2026-10-25)

`run-validation.sh`: detached worktree `worktrees/177a1dc465ff/jit-stack-validation-11` at official
`177a1dc465ff`, cherry-pick of the 65 commits configured in `config.json` (ending with `f9d2d9fc`;
`stack.txt`), `tracker.py build` (GCC 13, LLVM 15, RelWithDebInfo, LTO), then the configured test
filter (round 10 plus `[stack_trace]`, `Test stack trace map*`, `[cgroup_array]`) on candidate and
official builds.

| Check | Result |
|---|---|
| Replay | 65/65 applied, no conflicts; candidate `b9ef238acb48` |
| Tree vs `f9d2d9fc` | identical |
| LLVM 15 build | success |
| Candidate filter | 103 cases / 254872 assertions passed |
| Official filter | 34 cases / 32829 assertions passed (the two "Test stack trace map" cases exist upstream too) |

Run 23:01-23:07 JST, exit 0.
