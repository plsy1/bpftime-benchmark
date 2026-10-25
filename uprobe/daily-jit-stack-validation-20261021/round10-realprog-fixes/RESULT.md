# Round 10: daily stack with the real-program fixes (2026-10-25)

`run-validation.sh`: detached worktree `worktrees/177a1dc465ff/jit-stack-validation-10` at official
`177a1dc465ff`, cherry-pick of the 62 commits configured in `config.json` (ending with `2448aeaf`;
`stack.txt`), `tracker.py build` (GCC 13, LLVM 15, RelWithDebInfo, LTO), then the configured test
filter (round 9 plus `[shm_release],[jit_load]`) on candidate and official builds.

| Check | Result |
|---|---|
| Replay | 62/62 applied, no conflicts; candidate `5e409fb08073` |
| Tree vs `2448aeaf` | identical |
| LLVM 15 build | success (includes the llvmbpf local-call overlay patch) |
| Candidate filter | 97 cases / 254800 assertions passed |
| Official filter | 32 cases / 32814 assertions passed |

Run 20:54-21:00 JST, exit 0.
