# Round 13: daily stack with the daemon-mode and NUC fixes (2026-10-27)

Stack: the 83 commits of `config.json` (`stack.txt`; the ten new ones end with `627a2201`),
automation commit `56ee70f1`.

| Check | Result |
|---|---|
| Replay | 83/83 cherry-picked (`-x`) onto official `177a1dc465ff` in a scratch worktree, no conflicts; tree identical to `dev/uprobe-official-177a1dc465ff` `627a2201` (empty `git diff`) |
| Candidate gate | `tracker.py test-candidate` on the dev worktree (same tree), GCC 13, LLVM 15, LTO, `-DBUILD_BPFTIME_DAEMON=ON`; `candidate-gate.log` |
| Focused filter (+ `get_stackid returns errors as negative 64-bit values`) | 111 cases / 255936 assertions passed |
| `[kernel_user]` (sudo) | 2 cases / 31 assertions passed |
| daemon tests (sudo) | 10 cases / 12466 assertions passed |
| `Test attach replace with ebpf` alone | 1 case / 9 assertions passed |
| Frida attach tests | 18 cases / 1078 assertions passed |
| Official build, new filter | 34 cases / 32829 assertions passed; the new case matches nothing there (`official-tests.log`) |

`candidate-gate-attempt1.log`: the first gate run failed in configuration, not in bpftime: the
gum case was listed under the Frida binary (it is a runtime test), and the root suites used the
default segment name, leaving `/dev/shm/bpftime_maps_shm` owned by root (removed). Root suites now
use `BPFTIME_GLOBAL_SHM_NAME=bpftime_daily_root_tests`, removed after each run.
